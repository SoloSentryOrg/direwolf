/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "direwolf.h"
#include "private_output.h"
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#if defined(__APPLE__)
#include <sys/acl.h>
#endif
#if __WIN32__
#include <io.h>
#else
#include <unistd.h>
#endif

#if !__WIN32__ && defined(DW_OUTPUT_CREATION_TEST)
/* Test observers run immediately after the real creation API returns. */
extern int dw_test_new_open(const char *, int, mode_t);
#define DW_NEW_OPEN dw_test_new_open
#if defined(__APPLE__)
extern int dw_test_new_openx(const char *, int, filesec_t);
#define DW_NEW_OPENX dw_test_new_openx
#endif
#else
#define DW_NEW_OPEN open
#if defined(__APPLE__)
#define DW_NEW_OPENX openx_np
#endif
#endif

#if __WIN32__
/* Explicit DACLs, rather than CRT mode bits, protect newly created Windows
 * files. These APIs retain the application's Windows XP baseline. */
static int open_windows_output(const char *path, int binary, int exclusive)
{
  union { TOKEN_USER user; unsigned char bytes[sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE]; } identity;
  union { DWORD alignment; ACL acl; unsigned char bytes[sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE) + SECURITY_MAX_SID_SIZE]; } access;
  HANDLE token = NULL, file;
  SECURITY_DESCRIPTOR descriptor;
  SECURITY_ATTRIBUTES attributes;
  BY_HANDLE_FILE_INFORMATION information;
  char absolute[MAX_PATH], volume[MAX_PATH];
  DWORD needed, serial, flags, length;
  int fd, saved, created = 0;
  /* NTFS streams share the base file descriptor; a per-stream creation DACL
   * cannot make an existing public file private. Allow only a drive colon. */
  {
    size_t i;
    for (i = 0; path[i]; ++i) {
      if (path[i] == ':' && !(i == 1 &&
          ((path[0] >= 'A' && path[0] <= 'Z') ||
           (path[0] >= 'a' && path[0] <= 'z')))) {
        errno = EINVAL;
        return -1;
      }
    }
  }
  if (!exclusive) {
    file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                       NULL, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (file != INVALID_HANDLE_VALUE) goto validate_file;
    if (GetLastError() != ERROR_FILE_NOT_FOUND && GetLastError() != ERROR_PATH_NOT_FOUND) {
      errno = EACCES;
      return -1;
    }
  }
  length = GetFullPathNameA(path, sizeof(absolute), absolute, NULL);
  if (length == 0 || length >= sizeof(absolute) ||
      !GetVolumePathNameA(absolute, volume, sizeof(volume)) ||
      !GetVolumeInformationA(volume, NULL, 0, &serial, NULL, &flags, NULL, 0) ||
      !(flags & FILE_PERSISTENT_ACLS)) {
    errno = EACCES;
    return -1;
  }
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
    errno = EACCES;
    return -1;
  }
  if (!GetTokenInformation(token, TokenUser, &identity, sizeof(identity), &needed)) {
    CloseHandle(token);
    errno = EACCES;
    return -1;
  }
  CloseHandle(token);
  if (!InitializeAcl(&access.acl, sizeof(access), ACL_REVISION) ||
      !AddAccessAllowedAce(&access.acl, ACL_REVISION, FILE_ALL_ACCESS, identity.user.User.Sid) ||
      !InitializeSecurityDescriptor(&descriptor, SECURITY_DESCRIPTOR_REVISION) ||
      !SetSecurityDescriptorDacl(&descriptor, TRUE, &access.acl, FALSE) ||
      !SetSecurityDescriptorControl(&descriptor, SE_DACL_PROTECTED, SE_DACL_PROTECTED)) {
    errno = EACCES;
    return -1;
  }
  attributes.nLength = sizeof(attributes);
  attributes.lpSecurityDescriptor = &descriptor;
  attributes.bInheritHandle = FALSE;
  file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                     &attributes, CREATE_NEW,
                     FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
  if (file == INVALID_HANDLE_VALUE) {
    DWORD error = GetLastError();
    errno = (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS) ? EEXIST : EACCES;
    return -1;
  }
  created = 1;
validate_file:
  if (GetFileType(file) != FILE_TYPE_DISK || !GetFileInformationByHandle(file, &information) ||
      (created && information.dwVolumeSerialNumber != serial) ||
      (information.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) {
    CloseHandle(file);
    errno = EINVAL;
    return -1;
  }
  fd = _open_osfhandle((intptr_t)file, _O_WRONLY | _O_NOINHERIT | (binary ? _O_BINARY : _O_TEXT));
  if (fd < 0) {
    saved = errno;
    CloseHandle(file);
    errno = saved;
  }
  return fd;
}
#endif

#if defined(__APPLE__)
static int create_new_mac_output(const char *path, int flags)
{
  filesec_t security = filesec_init();
  acl_t empty;
  acl_flagset_t acl_flags;
  mode_t mode = S_IRUSR | S_IWUSR;
  int fd = -1, error;
  if (security == NULL) return -1;
  empty = acl_init(0);
  if (empty == NULL) {
    error = errno; filesec_free(security); errno = error;
    return -1;
  }
  if (acl_get_flagset_np(empty, &acl_flags) == 0 &&
      acl_add_flag_np(acl_flags, ACL_FLAG_NO_INHERIT) == 0 &&
      filesec_set_property(security, FILESEC_MODE, &mode) == 0 &&
      filesec_set_property(security, FILESEC_ACL, &empty) == 0) {
    /* ACL and mode are passed to the kernel with creation. No public interval
     * exists in which an inherited allow ACE can grant a retained read handle. */
    fd = DW_NEW_OPENX(path, flags, security);
  }
  error = errno;
  acl_free(empty);
  filesec_free(security);
  errno = error;
  return fd;
}

static int create_private_mac_directory(const char *path)
{
  filesec_t security = filesec_init();
  acl_t empty;
  acl_flagset_t acl_flags;
  mode_t mode = S_IRWXU;
  int fd = -1, error;
  if (security == NULL) return -1;
  empty = acl_init(0);
  if (empty == NULL) {
    error = errno; filesec_free(security); errno = error;
    return -1;
  }
  if (acl_get_flagset_np(empty, &acl_flags) == 0 &&
      acl_add_flag_np(acl_flags, ACL_FLAG_NO_INHERIT) == 0 &&
      filesec_set_property(security, FILESEC_MODE, &mode) == 0 &&
      filesec_set_property(security, FILESEC_ACL, &empty) == 0) {
    /* ACL and mode are passed to the kernel with creation. No public interval
     * exists in which an inherited allow ACE can grant a retained read handle. */
    fd = mkdirx_np(path, security);
  }
  error = errno;
  acl_free(empty);
  filesec_free(security);
  errno = error;
  return fd;
}

static int validate_new_mac_acl(int fd)
{
  acl_t actual;
  acl_entry_t entry;
  errno = 0;
  actual = acl_get_fd(fd);
  if (actual == NULL) return errno == ENOENT ? 0 : -1;
  if (acl_valid(actual) != 0 || acl_get_entry(actual, ACL_FIRST_ENTRY, &entry) == 0) {
    acl_free(actual); errno = EACCES;
    return -1;
  }
  acl_free(actual);
  return 0;
}
#endif

FILE *dw_open_output(const char *path, dw_output_format_t format, dw_output_policy_t policy)
{
  int fd, saved;
  FILE *stream;
  if (path == NULL || (format != DW_OUTPUT_TEXT && format != DW_OUTPUT_BINARY) ||
      (policy != DW_OUTPUT_REPLACE && policy != DW_OUTPUT_NEW)) {
    errno = EINVAL;
    return NULL;
  }
#if __WIN32__
  fd = open_windows_output(path, format == DW_OUTPUT_BINARY, policy == DW_OUTPUT_NEW);
  if (fd < 0) return NULL;
  if (policy == DW_OUTPUT_REPLACE && _chsize(fd, 0) != 0) {
    saved = errno;
    _close(fd);
    errno = saved;
    return NULL;
  }
  stream = _fdopen(fd, format == DW_OUTPUT_BINARY ? "wb" : "w");
  if (stream == NULL) { saved = errno; _close(fd); errno = saved; }
#else
  struct stat status, directory_status;
  int created = 0, directory_fd = -1;
  char *stage = NULL, *child = NULL;
#ifndef O_NOFOLLOW
  errno = ENOTSUP;
  return NULL;
#else
  int flags = O_WRONLY | O_NOFOLLOW | O_NONBLOCK;
#ifdef O_CLOEXEC
  flags |= O_CLOEXEC;
#endif
  fd = -1;
  if (policy == DW_OUTPUT_REPLACE) {
    fd = open(path, flags);
    if (fd < 0 && errno != ENOENT) return NULL;
  }
  if (fd < 0) {
    const char *slash = strrchr(path, '/');
    size_t parent_length = slash ? (size_t)(slash - path + 1) : 0;
    size_t length = strlen(path);
    unsigned int attempt;
    if (length == 0 || length > 4096) { errno = EINVAL; return NULL; }
    stage = malloc(parent_length + 64);
    child = malloc(parent_length + 80);
    if (stage == NULL || child == NULL) goto failed;
    memcpy(stage, path, parent_length);
#if defined(__APPLE__)
    for (attempt = 0; attempt < 32; ++attempt) {
      snprintf(stage + parent_length, 64, ".dw-output-%08x%08x",
               arc4random(), arc4random());
      if (create_private_mac_directory(stage) == 0) break;
      if (errno != EEXIST) goto failed;
    }
    if (attempt == 32) { errno = EEXIST; goto failed; }
#else
    (void)attempt;
    strcpy(stage + parent_length, ".dw-output-XXXXXX");
    if (mkdtemp(stage) == NULL) goto failed;
#endif
    created = 1;
    directory_fd = open(stage, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    if (directory_fd < 0) goto failed;
    if (fstat(directory_fd, &directory_status) != 0) goto failed;
    if (!S_ISDIR(directory_status.st_mode) || directory_status.st_uid != geteuid() ||
        (directory_status.st_mode & 0077) != 0) { errno = EACCES; goto failed; }
#if defined(__APPLE__)
    if (validate_new_mac_acl(directory_fd) != 0) goto failed;
#endif
    snprintf(child, parent_length + 80, "%s/output", stage);
#if defined(__APPLE__)
    fd = create_new_mac_output(child, flags | O_CREAT | O_EXCL);
#else
    fd = DW_NEW_OPEN(child, flags | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
#endif
    if (fd < 0) goto failed;
  }
  if (fstat(fd, &status) != 0) goto failed;
  if (!S_ISREG(status.st_mode)) { errno = EINVAL; goto failed; }
  if (created && (status.st_uid != geteuid() || (status.st_mode & 0077) != 0)) {
    errno = EACCES; goto failed;
  }
#if defined(__APPLE__)
  if (created && validate_new_mac_acl(fd) != 0) goto failed;
#endif
  if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0 ||
      (!created && ftruncate(fd, 0) != 0)) goto failed;
  stream = fdopen(fd, format == DW_OUTPUT_BINARY ? "wb" : "w");
  if (stream == NULL) goto failed;
  if (created) {
    /* Publish only a validated descriptor. Never replace or remove a final
     * destination that appeared concurrently. The parent must be trusted. */
    if (linkat(directory_fd, "output", AT_FDCWD, path, 0) != 0) {
      saved = errno; fclose(stream); fd = -1; errno = saved; goto failed;
    }
    unlinkat(directory_fd, "output", 0);
    close(directory_fd);
    rmdir(stage);
  }
  free(child);
  free(stage);
  return stream;
failed:
  saved = errno;
  if (fd >= 0) close(fd);
  if (directory_fd >= 0) {
    unlinkat(directory_fd, "output", 0);
    close(directory_fd);
  }
  if (created) rmdir(stage);
  free(child);
  free(stage);
  errno = saved;
  return NULL;
#endif
#endif
  return stream;
}
