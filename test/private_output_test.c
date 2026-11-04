/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "direwolf.h"
#include "private_output.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#if defined(__APPLE__)
#include <sys/acl.h>
#include <membership.h>
#include <grp.h>
#endif
#if __WIN32__
#include <io.h>
#include <direct.h>
#include <process.h>
#include <aclapi.h>
#define mkdir_private(p) _mkdir(p)
#define process_id() _getpid()
#define remove_directory(p) _rmdir(p)
#else
#include <fcntl.h>
#include <unistd.h>
#define mkdir_private(p) mkdir(p, 0700)
#define process_id() getpid()
#define remove_directory(p) rmdir(p)
#endif

static void check_private_fd(int fd)
{
#if __WIN32__
  HANDLE handle = (HANDLE)_get_osfhandle(fd);
  PACL acl = NULL;
  PSECURITY_DESCRIPTOR descriptor = NULL;
  SECURITY_DESCRIPTOR_CONTROL control;
  DWORD revision, flags, needed;
  HANDLE token = NULL;
  ACCESS_ALLOWED_ACE *ace = NULL;
  union { TOKEN_USER user; unsigned char bytes[sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE]; } identity;
  ACL_SIZE_INFORMATION information;
  assert(GetHandleInformation(handle, &flags) && !(flags & HANDLE_FLAG_INHERIT));
  assert(GetSecurityInfo(handle, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                         NULL, NULL, &acl, NULL, &descriptor) == ERROR_SUCCESS);
  assert(acl != NULL && GetAclInformation(acl, &information, sizeof(information), AclSizeInformation));
  assert(information.AceCount == 1 && GetAce(acl, 0, (LPVOID *)&ace));
  assert(ace->Header.AceType == ACCESS_ALLOWED_ACE_TYPE && ace->Header.AceFlags == 0);
  assert(ace->Mask == FILE_ALL_ACCESS);
  assert(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token));
  assert(GetTokenInformation(token, TokenUser, &identity, sizeof(identity), &needed));
  assert(EqualSid(identity.user.User.Sid, (PSID)&ace->SidStart));
  CloseHandle(token);
  assert(GetSecurityDescriptorControl(descriptor, &control, &revision));
  assert(control & SE_DACL_PROTECTED);
  LocalFree(descriptor);
#else
  struct stat status;
  assert(fstat(fd, &status) == 0 && S_ISREG(status.st_mode));
  assert((status.st_mode & 0777) == 0600);
  assert(fcntl(fd, F_GETFD) & FD_CLOEXEC);
#if defined(__APPLE__)
  {
    acl_t acl;
    acl_entry_t entry;
    errno = 0;
    acl = acl_get_fd(fd);
    if (acl == NULL) assert(errno == ENOENT);
    else {
      assert(acl_valid(acl) == 0);
      assert(acl_get_entry(acl, ACL_FIRST_ENTRY, &entry) != 0);
      acl_free(acl);
    }
  }
#endif
#endif
}

static void check_private(FILE *stream)
{
#if __WIN32__
  check_private_fd(_fileno(stream));
#else
  check_private_fd(fileno(stream));
#endif
}

#if !__WIN32__ && defined(DW_OUTPUT_CREATION_TEST)
static unsigned int creation_checks;
static int inject_bad_mode;
static const char *concurrent_destination;
int dw_test_new_open(const char *path, int flags, mode_t mode)
{
  int fd = open(path, flags, mode);
  if (fd >= 0) {
    if (inject_bad_mode) assert(fchmod(fd, 0666) == 0);
    else check_private_fd(fd);
    if (concurrent_destination) {
      FILE *other = fopen(concurrent_destination, "wb");
      assert(other && fputs("KEEP", other) >= 0 && fclose(other) == 0);
    }
    ++creation_checks;
  }
  return fd;
}
#if defined(__APPLE__)
int dw_test_new_openx(const char *path, int flags, filesec_t security)
{
  int fd = openx_np(path, flags, security);
  if (fd >= 0) {
    if (inject_bad_mode) assert(fchmod(fd, 0666) == 0);
    else check_private_fd(fd);
    if (concurrent_destination) {
      FILE *other = fopen(concurrent_destination, "wb");
      assert(other && fputs("KEEP", other) >= 0 && fclose(other) == 0);
    }
    ++creation_checks;
  }
  return fd;
}
#endif
#endif

static void check_bytes(const char *path, const unsigned char *expected, size_t length)
{
  unsigned char buffer[32];
  FILE *stream = fopen(path, "rb");
  assert(stream != NULL && length <= sizeof(buffer));
  assert(fread(buffer, 1, sizeof(buffer), stream) == length);
  assert(memcmp(buffer, expected, length) == 0);
  assert(fclose(stream) == 0);
}

int main(void)
{
  char directory[80], binary[120], text[120], link_path[120], fifo[120];
  unsigned int index;
  FILE *stream;
  int previous_mask;
  const unsigned char bytes[] = {'A', '\n', 0, 'Z'};
  for (index = 0; index < 32; ++index) {
    snprintf(directory, sizeof(directory), "private-output-%ld-%u", (long)process_id(), index);
    if (mkdir_private(directory) == 0) break;
    assert(errno == EEXIST);
  }
  assert(index < 32);
#if defined(__APPLE__)
  /* Inherited Mac ACLs are independent of the 0600 mode bits. */
  {
    struct group *everyone = getgrnam("everyone");
    uuid_t principal;
    acl_t inherited = acl_init(1);
    acl_entry_t entry;
    acl_permset_t permissions;
    acl_flagset_t flags;
    assert(everyone != NULL && mbr_gid_to_uuid(everyone->gr_gid, principal) == 0);
    assert(inherited != NULL && acl_create_entry(&inherited, &entry) == 0);
    assert(acl_set_tag_type(entry, ACL_EXTENDED_ALLOW) == 0);
    assert(acl_set_qualifier(entry, principal) == 0);
    assert(acl_get_permset(entry, &permissions) == 0 && acl_add_perm(permissions, ACL_READ_DATA) == 0);
    assert(acl_get_flagset_np(entry, &flags) == 0);
    assert(acl_add_flag_np(flags, ACL_ENTRY_FILE_INHERIT) == 0);
    assert(acl_add_flag_np(flags, ACL_ENTRY_DIRECTORY_INHERIT) == 0);
    assert(acl_set_file(directory, ACL_TYPE_EXTENDED, inherited) == 0);
    acl_free(inherited);
  }
#endif
  snprintf(binary, sizeof(binary), "%s/binary", directory);
  snprintf(text, sizeof(text), "%s/text", directory);
  snprintf(link_path, sizeof(link_path), "%s/link", directory);
  snprintf(fifo, sizeof(fifo), "%s/fifo", directory);
#if __WIN32__
  previous_mask = _umask(0);
#else
  previous_mask = umask(0);
#endif
  stream = dw_open_output(binary, DW_OUTPUT_BINARY, DW_OUTPUT_REPLACE);
#if __WIN32__
  _umask(previous_mask);
#else
  umask(previous_mask);
#endif
  assert(stream != NULL);
  check_private(stream);
  assert(fwrite(bytes, 1, sizeof(bytes), stream) == sizeof(bytes));
  assert(fclose(stream) == 0);
  check_bytes(binary, bytes, sizeof(bytes));
  assert(dw_open_output(binary, DW_OUTPUT_BINARY, DW_OUTPUT_NEW) == NULL);
  check_bytes(binary, bytes, sizeof(bytes));
  stream = dw_open_output(binary, DW_OUTPUT_BINARY, DW_OUTPUT_REPLACE);
  assert(stream != NULL);
  check_private(stream);
  assert(fputs("OK", stream) >= 0 && fclose(stream) == 0);
  check_bytes(binary, (const unsigned char *)"OK", 2);
  stream = dw_open_output(text, DW_OUTPUT_TEXT, DW_OUTPUT_NEW);
  assert(stream != NULL);
  check_private(stream);
  assert(fputs("A\n", stream) >= 0 && fclose(stream) == 0);
#if __WIN32__
  check_bytes(text, (const unsigned char *)"A\r\n", 3);
#else
  check_bytes(text, (const unsigned char *)"A\n", 2);
  assert(symlink("binary", link_path) == 0);
  assert(dw_open_output(link_path, DW_OUTPUT_BINARY, DW_OUTPUT_REPLACE) == NULL);
  check_bytes(binary, (const unsigned char *)"OK", 2);
  assert(remove(link_path) == 0);
  assert(mkfifo(fifo, 0600) == 0);
  assert(dw_open_output(fifo, DW_OUTPUT_TEXT, DW_OUTPUT_REPLACE) == NULL);
  assert(remove(fifo) == 0);
#endif
  assert(dw_open_output(directory, DW_OUTPUT_BINARY, DW_OUTPUT_REPLACE) == NULL);
  assert(dw_open_output(binary, (dw_output_format_t)99, DW_OUTPUT_REPLACE) == NULL);
  assert(dw_open_output(binary, DW_OUTPUT_BINARY, (dw_output_policy_t)99) == NULL);
#if __WIN32__
  {
    char alias[160], absolute[MAX_PATH];
    snprintf(alias, sizeof(alias), "%s:hidden", binary);
    assert(dw_open_output(alias, DW_OUTPUT_BINARY, DW_OUTPUT_NEW) == NULL);
    snprintf(alias, sizeof(alias), "%s::$DATA", binary);
    assert(dw_open_output(alias, DW_OUTPUT_BINARY, DW_OUTPUT_REPLACE) == NULL);
    check_bytes(binary, (const unsigned char *)"OK", 2);
    assert(GetFullPathNameA(binary, sizeof(absolute), absolute, NULL) > 0);
    stream = dw_open_output(absolute, DW_OUTPUT_BINARY, DW_OUTPUT_REPLACE);
    assert(stream && fclose(stream) == 0);
  }
#endif
#if !__WIN32__ && defined(DW_OUTPUT_CREATION_TEST)
  inject_bad_mode = 1;
  assert(dw_open_output(fifo, DW_OUTPUT_BINARY, DW_OUTPUT_REPLACE) == NULL);
  { struct stat st; assert(lstat(fifo, &st) < 0 && errno == ENOENT); }
  inject_bad_mode = 0;
  stream = dw_open_output(fifo, DW_OUTPUT_BINARY, DW_OUTPUT_REPLACE);
  assert(stream); check_private(stream); assert(fclose(stream) == 0);
  assert(remove(fifo) == 0);
  concurrent_destination = fifo;
  assert(dw_open_output(fifo, DW_OUTPUT_BINARY, DW_OUTPUT_REPLACE) == NULL);
  concurrent_destination = NULL;
  check_bytes(fifo, (const unsigned char *)"KEEP", 4);
  assert(remove(fifo) == 0);
#endif
  assert(remove(binary) == 0 && remove(text) == 0 && remove_directory(directory) == 0);
#if !__WIN32__ && defined(DW_OUTPUT_CREATION_TEST)
  assert(creation_checks == 6);
#endif
  puts("private output permissions, bytes, collision and object checks passed");
  return 0;
}
