#include "adapters/outbound/process/process_spawn.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__EMSCRIPTEN__) || defined(CORDLANG_WASM)

/* Browser / WASI playground: no subprocesses (font curl, npm, …). Soft-fail. */
int process_run(const char *cwd, char *const argv[], int wait_child) {
  (void)cwd;
  (void)argv;
  (void)wait_child;
  return -1;
}

#else

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifdef _WIN32
static int append_win_arg(char *dst, size_t dst_sz, size_t *len, const char *arg) {
  if (!arg) arg = "";
  int need_quote = 0;
  for (const char *p = arg; *p; p++) {
    if (*p == ' ' || *p == '\t' || *p == '"') {
      need_quote = 1;
      break;
    }
  }

  size_t n = *len;
  if (need_quote) {
    if (n + 1 >= dst_sz) return -1;
    dst[n++] = '"';
  }

  for (const char *p = arg; *p; p++) {
    if (*p == '"') {
      if (n + 2 >= dst_sz) return -1;
      dst[n++] = '\\';
      dst[n++] = '"';
    } else {
      if (n + 1 >= dst_sz) return -1;
      dst[n++] = *p;
    }
  }

  if (need_quote) {
    if (n + 1 >= dst_sz) return -1;
    dst[n++] = '"';
  }
  *len = n;
  return 0;
}

static int build_win_cmdline(char *const argv[], char *out, size_t out_sz) {
  size_t n = 0;
  if (!argv || !argv[0]) return -1;
  for (int i = 0; argv[i]; i++) {
    if (i > 0) {
      if (n + 1 >= out_sz) return -1;
      out[n++] = ' ';
    }
    if (append_win_arg(out, out_sz, &n, argv[i]) != 0) return -1;
  }
  if (n >= out_sz) return -1;
  out[n] = '\0';
  return 0;
}

int process_run(const char *cwd, char *const argv[], int wait_child) {
  if (!argv || !argv[0]) return -1;

  char cmdline[4096];
  if (build_win_cmdline(argv, cmdline, sizeof(cmdline)) != 0) {
    fprintf(stderr, "Error: process_run: command line too long\n");
    return -1;
  }

  char app[MAX_PATH];
  char *app_ptr = NULL;
  DWORD found = SearchPathA(NULL, argv[0], ".exe", MAX_PATH, app, NULL);
  if (!found) found = SearchPathA(NULL, argv[0], ".cmd", MAX_PATH, app, NULL);
  if (!found) found = SearchPathA(NULL, argv[0], ".bat", MAX_PATH, app, NULL);
  if (found) app_ptr = app;

  STARTUPINFOA si;
  PROCESS_INFORMATION pi;
  memset(&si, 0, sizeof(si));
  memset(&pi, 0, sizeof(pi));
  si.cb = sizeof(si);

  DWORD flags = 0;
  if (!wait_child) flags |= DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP;

  /* .cmd/.bat need cmd.exe; keep argv quoted so paths cannot inject. */
  int is_script = 0;
  if (app_ptr) {
    size_t alen = strlen(app_ptr);
    if (alen >= 4) {
      const char *ext = app_ptr + alen - 4;
      if (_stricmp(ext, ".cmd") == 0 || _stricmp(ext, ".bat") == 0) is_script = 1;
    }
  }

  BOOL ok;
  if (is_script) {
    char via_cmd[4200];
    /* /s /c + quoted full command preserves per-arg quoting. */
    snprintf(via_cmd, sizeof(via_cmd), "cmd.exe /s /c \"%s\"", cmdline);
    ok = CreateProcessA(NULL, via_cmd, NULL, NULL, FALSE, flags, NULL,
                        cwd && cwd[0] ? cwd : NULL, &si, &pi);
  } else {
    ok = CreateProcessA(app_ptr, cmdline, NULL, NULL, FALSE, flags, NULL,
                        cwd && cwd[0] ? cwd : NULL, &si, &pi);
  }

  if (!ok) {
    fprintf(stderr, "Error: process_run: CreateProcess(%s) failed (%lu)\n",
            argv[0], (unsigned long)GetLastError());
    return -1;
  }

  CloseHandle(pi.hThread);
  if (!wait_child) {
    CloseHandle(pi.hProcess);
    return 0;
  }

  DWORD wait = WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD code = 1;
  if (wait == WAIT_OBJECT_0) GetExitCodeProcess(pi.hProcess, &code);
  CloseHandle(pi.hProcess);
  return (int)code;
}

#else /* POSIX */

int process_run(const char *cwd, char *const argv[], int wait_child) {
  if (!argv || !argv[0]) return -1;

  pid_t pid = fork();
  if (pid < 0) {
    fprintf(stderr, "Error: process_run: fork failed: %s\n", strerror(errno));
    return -1;
  }

  if (pid == 0) {
    if (cwd && cwd[0] && chdir(cwd) != 0) {
      fprintf(stderr, "Error: process_run: chdir(%s): %s\n", cwd,
              strerror(errno));
      _exit(127);
    }
    if (!wait_child) {
      setsid();
    }
    execvp(argv[0], argv);
    fprintf(stderr, "Error: process_run: execvp(%s): %s\n", argv[0],
            strerror(errno));
    _exit(127);
  }

  if (!wait_child) return 0;

  int status = 0;
  if (waitpid(pid, &status, 0) < 0) {
    fprintf(stderr, "Error: process_run: waitpid failed: %s\n",
            strerror(errno));
    return -1;
  }
  if (WIFEXITED(status)) return WEXITSTATUS(status);
  if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
  return 1;
}

#endif /* POSIX */

#endif /* !CORDLANG_WASM */
