// SPDX-License-Identifier: GPL-2.0-or-later
// Launch the explicitly configured SPEECH_STDIN runner with bounded stdin.

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include "direwolf.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "ax25_pad.h"
#include "speech_runner.h"

#ifndef SPEECH_WAIT_MS
#define SPEECH_WAIT_MS 30000
#endif

int speech_runner_text_valid (const unsigned char *text, size_t len)
{
  size_t i;

  if (text == NULL || len == 0 || len > AX25_MAX_INFO_LEN) return 0;
  for (i = 0; i < len; ++i) {
    if (text[i] == 0 || (text[i] < 32 && text[i] != '\t' &&
                         text[i] != '\n' && text[i] != '\r') ||
        text[i] == 127) return 0;
  }
  return 1;
}

#if __WIN32__

#include <windows.h>

#ifndef EXTENDED_STARTUPINFO_PRESENT
#define EXTENDED_STARTUPINFO_PRESENT 0x00080000
#endif
#ifndef PROC_THREAD_ATTRIBUTE_HANDLE_LIST
#define PROC_THREAD_ATTRIBUTE_HANDLE_LIST 0x00020002
#endif

/* Load Vista-only entry points at feature use, preserving XP core startup. */
typedef BOOL (WINAPI *speech_init_attributes_fn)(void *, DWORD, DWORD, SIZE_T *);
typedef BOOL (WINAPI *speech_update_attributes_fn)(void *, DWORD, DWORD_PTR,
                                                     void *, SIZE_T, void *, SIZE_T *);
typedef VOID (WINAPI *speech_delete_attributes_fn)(void *);

typedef struct speech_startup_info_ex_s {
  STARTUPINFOA startup;
  void *attributes;
} speech_startup_info_ex_t;

static int speech_vista_api (speech_init_attributes_fn *init,
                             speech_update_attributes_fn *update,
                             speech_delete_attributes_fn *del)
{
  HMODULE kernel = GetModuleHandleA("kernel32.dll");
  if (kernel == NULL) return 0;
  *init = (speech_init_attributes_fn)GetProcAddress(kernel, "InitializeProcThreadAttributeList");
  *update = (speech_update_attributes_fn)GetProcAddress(kernel, "UpdateProcThreadAttribute");
  *del = (speech_delete_attributes_fn)GetProcAddress(kernel, "DeleteProcThreadAttributeList");
  return *init != NULL && *update != NULL && *del != NULL;
}

static int speech_windows_path (const char *program)
{
  const unsigned char *p = (const unsigned char *)program;
  size_t n;
  if (p == NULL) return 0;
  n = strlen(program);
  if (n < 7 || n >= 80 || !isalpha(p[0]) || p[1] != ':' ||
      (p[2] != '\\' && p[2] != '/')) return 0;
  for (; *p; ++p) {
    if (!isalnum(*p) && *p != ' ' && *p != '.' && *p != '_' &&
        *p != '-' && *p != ':' && *p != '\\' && *p != '/') return 0;
  }
  return _stricmp(program + n - 4, ".exe") == 0 ||
         _stricmp(program + n - 4, ".bat") == 0;
}

int speech_runner_validate (const char *program)
{
  DWORD attrs;
  speech_init_attributes_fn init;
  speech_update_attributes_fn update;
  speech_delete_attributes_fn del;
  if (!speech_windows_path(program) || !speech_vista_api(&init, &update, &del)) return 0;
  attrs = GetFileAttributesA(program);
  return attrs != INVALID_FILE_ATTRIBUTES &&
         !(attrs & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT));
}

int speech_runner_run (const char *program, int channel,
                       const unsigned char *text, size_t text_len)
{
  SECURITY_ATTRIBUTES security = {sizeof(security), NULL, TRUE};
  speech_init_attributes_fn init;
  speech_update_attributes_fn update;
  speech_delete_attributes_fn del;
  speech_startup_info_ex_t si;
  PROCESS_INFORMATION pi;
  HANDLE pipe_read = NULL, pipe_write = NULL, null_out = INVALID_HANDLE_VALUE;
  HANDLE job = NULL, allowed[2];
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits;
  char app[MAX_PATH], command[2 * MAX_PATH + 40];
  DWORD written, wait_result, exit_code = 1;
  SIZE_T attribute_size = 0;
  int is_batch, command_len, started = 0, initialized = 0, result = -1;

  if (!speech_runner_validate(program) || channel < 0 || channel >= MAX_RADIO_CHANS ||
      !speech_runner_text_valid(text, text_len) ||
      !speech_vista_api(&init, &update, &del)) return -1;

  is_batch = _stricmp(program + strlen(program) - 4, ".bat") == 0;
  if (is_batch) {
    UINT n = GetSystemDirectoryA(app, sizeof(app));
    if (n == 0 || n > sizeof(app) - 9) return -1;
    memcpy(app + n, "\\cmd.exe", 9);
    command_len = snprintf(command, sizeof(command),
                           "\"%s\" /D /S /C \"\"%s\" %d\"", app, program, channel);
  } else {
    size_t n = strlen(program);
    if (n >= sizeof(app)) return -1;
    memcpy(app, program, n + 1);
    command_len = snprintf(command, sizeof(command), "\"%s\" %d", app, channel);
  }
  if (command_len < 0 || (size_t)command_len >= sizeof(command)) return -1;

  memset(&si, 0, sizeof(si));
  memset(&pi, 0, sizeof(pi));
  memset(&limits, 0, sizeof(limits));
  si.startup.cb = sizeof(si);
  si.startup.dwFlags = STARTF_USESTDHANDLES;
  if (!CreatePipe(&pipe_read, &pipe_write, &security, 4096)) goto done;
  if (!SetHandleInformation(pipe_write, HANDLE_FLAG_INHERIT, 0)) goto done;
  if (!WriteFile(pipe_write, text, (DWORD)text_len, &written, NULL) ||
      written != (DWORD)text_len) goto done;
  CloseHandle(pipe_write);
  pipe_write = NULL;
  null_out = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                         &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (null_out == INVALID_HANDLE_VALUE) goto done;
  si.startup.hStdInput = pipe_read;
  si.startup.hStdOutput = null_out;
  si.startup.hStdError = null_out;
  allowed[0] = pipe_read;
  allowed[1] = null_out;
  /* The first call obtains the required buffer size and deliberately fails. */
  init(NULL, 1, 0, &attribute_size);
  if (attribute_size == 0 || attribute_size > 65536) goto done;
  si.attributes = HeapAlloc(GetProcessHeap(), 0, attribute_size);
  if (si.attributes == NULL) goto done;
  if (!init(si.attributes, 1, 0, &attribute_size)) goto done;
  initialized = 1;
  if (!update(si.attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
              allowed, sizeof(allowed), NULL, NULL)) goto done;

  job = CreateJobObjectA(NULL, NULL);
  if (job == NULL) goto done;
  limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation,
                                &limits, sizeof(limits))) goto done;
  if (!CreateProcessA(app, command, NULL, NULL, TRUE,
                      EXTENDED_STARTUPINFO_PRESENT | CREATE_SUSPENDED | CREATE_NO_WINDOW,
                      NULL, NULL, &si.startup, &pi)) goto done;
  started = 1;
  if (!AssignProcessToJobObject(job, pi.hProcess)) goto done;
  if (ResumeThread(pi.hThread) == (DWORD)-1) goto done;
  CloseHandle(pipe_read);
  pipe_read = NULL;
  wait_result = WaitForSingleObject(pi.hProcess, SPEECH_WAIT_MS);
  if (wait_result == WAIT_OBJECT_0 && GetExitCodeProcess(pi.hProcess, &exit_code) &&
      exit_code == 0) result = 0;

done:
  if (started) {
    if (result != 0) {
      TerminateJobObject(job, 1);
      /* Assignment can fail while the child is still suspended. */
      TerminateProcess(pi.hProcess, 1);
      WaitForSingleObject(pi.hProcess, 5000);
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
  }
  if (job != NULL) CloseHandle(job);
  if (initialized) del(si.attributes);
  if (si.attributes != NULL) HeapFree(GetProcessHeap(), 0, si.attributes);
  if (pipe_read != NULL) CloseHandle(pipe_read);
  if (pipe_write != NULL) CloseHandle(pipe_write);
  if (null_out != INVALID_HANDLE_VALUE) CloseHandle(null_out);
  return result;
}

#else

#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#if defined(__GLIBC__)
#if __GLIBC_PREREQ(2, 34)
#define SPEECH_HAVE_CLOSEFROM 1
#endif
#endif

#if !defined(SPEECH_FORCE_FORK) && \
    (defined(POSIX_SPAWN_CLOEXEC_DEFAULT) || defined(SPEECH_HAVE_CLOSEFROM))
#define SPEECH_SAFE_SPAWN 1
#endif

static long long speech_now_ms (void)
{
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return -1;
  return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int speech_runner_validate (const char *program)
{
  struct stat st;
  if (program == NULL || program[0] != '/' || strlen(program) >= 4096) return 0;
  return lstat(program, &st) == 0 && S_ISREG(st.st_mode) && access(program, X_OK) == 0;
}

int speech_runner_run (const char *program, int channel,
                       const unsigned char *text, size_t text_len)
{
  int pipefd[2] = {-1, -1};
  int status, flags, result = -1;
  pid_t pid = -1;
  ssize_t written;
  long long deadline, now;
  char channel_arg[16];
  char *argv[3];
  char *envp[] = {"PATH=/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin", "LANG=C.UTF-8", NULL};
#if defined(SPEECH_SAFE_SPAWN)
  int spawned = 0;
  posix_spawn_file_actions_t actions;
  posix_spawnattr_t attr;
#endif

  if (!speech_runner_validate(program) || channel < 0 || channel >= MAX_RADIO_CHANS ||
      !speech_runner_text_valid(text, text_len)) return -1;
  if (pipe(pipefd) != 0) return -1;
  flags = fcntl(pipefd[1], F_GETFL, 0);
  if (flags < 0 || fcntl(pipefd[1], F_SETFL, flags | O_NONBLOCK) < 0) goto done;
  written = write(pipefd[1], text, text_len);
  if (written < 0 || (size_t)written != text_len) goto done;
  close(pipefd[1]);
  pipefd[1] = -1;

  snprintf(channel_arg, sizeof(channel_arg), "%d", channel);
  argv[0] = (char *)program;
  argv[1] = channel_arg;
  argv[2] = NULL;
#if defined(SPEECH_SAFE_SPAWN)
  if (posix_spawn_file_actions_init(&actions) != 0) goto done;
  if (posix_spawnattr_init(&attr) != 0) {
    posix_spawn_file_actions_destroy(&actions);
    goto done;
  }
  if (posix_spawn_file_actions_adddup2(&actions, pipefd[0], STDIN_FILENO) != 0 ||
      (pipefd[0] != STDIN_FILENO &&
       posix_spawn_file_actions_addclose(&actions, pipefd[0]) != 0) ||
      posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0) != 0 ||
      posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0) != 0) goto destroy;
#if defined(SPEECH_HAVE_CLOSEFROM)
  if (posix_spawn_file_actions_addclosefrom_np(&actions, 3) != 0) goto destroy;
#endif
  short spawn_flags = POSIX_SPAWN_SETPGROUP;
#if defined(POSIX_SPAWN_CLOEXEC_DEFAULT)
  spawn_flags |= POSIX_SPAWN_CLOEXEC_DEFAULT;
#endif
  if (posix_spawnattr_setflags(&attr, spawn_flags) != 0 ||
      posix_spawnattr_setpgroup(&attr, 0) != 0) goto destroy;
  if (posix_spawn(&pid, program, &actions, &attr, argv, envp) != 0) goto destroy;
  spawned = 1;

destroy:
  posix_spawnattr_destroy(&attr);
  posix_spawn_file_actions_destroy(&actions);
  close(pipefd[0]);
  pipefd[0] = -1;
  if (!spawned) goto done;
#else
  /* Other POSIX systems use an async-safe child path with every unrelated
   * descriptor closed before exec. Determine the bound before forking. */
  long max_fd = sysconf(_SC_OPEN_MAX);
  if (max_fd < 3 || max_fd > 1048576) goto done;
  pid = fork();
  if (pid < 0) goto done;
  if (pid == 0) {
    int null_fd, fd;
    if (setpgid(0, 0) != 0 || dup2(pipefd[0], STDIN_FILENO) < 0) _exit(127);
    null_fd = open("/dev/null", O_WRONLY);
    if (null_fd < 0 || dup2(null_fd, STDOUT_FILENO) < 0 ||
        dup2(null_fd, STDERR_FILENO) < 0) _exit(127);
    for (fd = 3; fd < max_fd; ++fd) close(fd);
    execve(program, argv, envp);
    _exit(127);
  }
  close(pipefd[0]);
  pipefd[0] = -1;
#endif
  now = speech_now_ms();
  if (now < 0) goto terminate;
  deadline = now + SPEECH_WAIT_MS;
  for (;;) {
    pid_t waited = waitpid(pid, &status, WNOHANG);
    if (waited == pid) {
      pid = -1;
      if (WIFEXITED(status) && WEXITSTATUS(status) == 0) result = 0;
      break;
    }
    if (waited < 0 && errno != EINTR) {
      if (errno == ECHILD) pid = -1;
      break;
    }
    now = speech_now_ms();
    if (now < 0 || now >= deadline) break;
    struct timespec pause = {0, 10000000};
    nanosleep(&pause, NULL);
  }

terminate:
  if (pid > 0) {
    kill(-pid, SIGKILL);
    kill(pid, SIGKILL);
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
  }
done:
  if (pipefd[0] >= 0) close(pipefd[0]);
  if (pipefd[1] >= 0) close(pipefd[1]);
  return result;
}

#endif
