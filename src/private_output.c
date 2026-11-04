/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "direwolf.h"
#include "private_output.h"
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#if defined(__APPLE__)
#include <sys/acl.h>
#endif
#if __WIN32__
#include <io.h>
#else
#include <unistd.h>
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
static int restrict_new_mac_acl(int fd)
{
  acl_t empty = acl_init(0), actual;
  acl_entry_t entry;
  acl_flagset_t flags;
  int error;
  if (empty == NULL) return -1;
  if (acl_get_flagset_np(empty, &flags) != 0 ||
      acl_add_flag_np(flags, ACL_FLAG_NO_INHERIT) != 0 || acl_set_fd(fd, empty) != 0) {
    error = errno; acl_free(empty); errno = error;
    return -1;
  }
  acl_free(empty);
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
  struct stat status;
  int created = 1;
#ifndef O_NOFOLLOW
  /* Do not silently fall back to following links on unsupported platforms. */
  errno = ENOTSUP;
  return NULL;
#else
  int flags = O_WRONLY | O_CREAT | O_NOFOLLOW | O_NONBLOCK;
#ifdef O_CLOEXEC
  flags |= O_CLOEXEC;
#endif
  fd = open(path, flags | O_EXCL, S_IRUSR | S_IWUSR);
  if (fd < 0 && errno == EEXIST && policy == DW_OUTPUT_REPLACE) {
    created = 0;
    fd = open(path, flags & ~O_CREAT);
  }
  if (fd < 0) return NULL;
  if (fstat(fd, &status) != 0) {
    saved = errno; close(fd); errno = saved;
    return NULL;
  }
  if (!S_ISREG(status.st_mode)) {
    close(fd); errno = EINVAL;
    return NULL;
  }
  if (created && (status.st_uid != geteuid() || (status.st_mode & 0077) != 0)) {
    close(fd); errno = EACCES;
    return NULL;
  }
#if defined(__APPLE__)
  if (created && restrict_new_mac_acl(fd) != 0) {
    saved = errno; close(fd); errno = saved;
    return NULL;
  }
#endif
  if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0 ||
      (policy == DW_OUTPUT_REPLACE && ftruncate(fd, 0) != 0)) {
    saved = errno; close(fd); errno = saved;
    return NULL;
  }
  stream = fdopen(fd, format == DW_OUTPUT_BINARY ? "wb" : "w");
  if (stream == NULL) { saved = errno; close(fd); errno = saved; }
#endif
#endif
  return stream;
}
