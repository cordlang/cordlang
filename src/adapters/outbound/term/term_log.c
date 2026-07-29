#include "adapters/outbound/term/term_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#ifndef CP_UTF8
#define CP_UTF8 65001
#endif
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#include <locale.h>
#endif

static int term_color_ok = -1;
static int term_utf8_ok = 0;

void term_init(void) {
  if (term_color_ok >= 0) return;
  term_color_ok = 0;
  term_utf8_ok = 0;

#ifdef _WIN32
  /* UTF-8 console so banners / arrows / box lines render in PowerShell. */
  if (SetConsoleOutputCP(CP_UTF8)) term_utf8_ok = 1;
  SetConsoleCP(CP_UTF8);

  HANDLE outs[2];
  outs[0] = GetStdHandle(STD_OUTPUT_HANDLE);
  outs[1] = GetStdHandle(STD_ERROR_HANDLE);
  for (int i = 0; i < 2; i++) {
    HANDLE h = outs[i];
    if (h == INVALID_HANDLE_VALUE || h == NULL) continue;
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode)) {
      mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
      SetConsoleMode(h, mode);
    }
  }
  if (_isatty(_fileno(stderr)) || _isatty(_fileno(stdout))) term_color_ok = 1;
#else
  setlocale(LC_ALL, "");
  setlocale(LC_CTYPE, "C.UTF-8");
  if (isatty(fileno(stderr)) || isatty(fileno(stdout))) {
    term_color_ok = 1;
    term_utf8_ok = 1;
  }
#endif

  if (getenv("NO_COLOR") && getenv("NO_COLOR")[0]) term_color_ok = 0;
  if (getenv("CORDLANG_NO_COLOR") && getenv("CORDLANG_NO_COLOR")[0])
    term_color_ok = 0;
  if (getenv("CORDLANG_ASCII") && getenv("CORDLANG_ASCII")[0]) term_utf8_ok = 0;
}

static const char *c_reset(void) { return term_color_ok ? "\033[0m" : ""; }
static const char *c_bold(void) { return term_color_ok ? "\033[1m" : ""; }
static const char *c_dim(void) { return term_color_ok ? "\033[2m" : ""; }
static const char *c_cyan(void) { return term_color_ok ? "\033[36m" : ""; }
static const char *c_green(void) { return term_color_ok ? "\033[32m" : ""; }
static const char *c_yellow(void) { return term_color_ok ? "\033[33m" : ""; }
static const char *c_red(void) { return term_color_ok ? "\033[31m" : ""; }
static const char *c_magenta(void) { return term_color_ok ? "\033[35m" : ""; }

static void vprint_tag(FILE *f, const char *color, const char *tag,
                       const char *fmt, va_list ap) {
  term_init();
  fprintf(f, "%s%s%s%s ", color, c_bold(), tag, c_reset());
  vfprintf(f, fmt, ap);
  fputc('\n', f);
  fflush(f);
}

void term_info(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vprint_tag(stderr, c_cyan(), "info", fmt, ap);
  va_end(ap);
}

void term_ok(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vprint_tag(stderr, c_green(), "ok  ", fmt, ap);
  va_end(ap);
}

void term_warn(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vprint_tag(stderr, c_yellow(), "warn", fmt, ap);
  va_end(ap);
}

void term_error(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vprint_tag(stderr, c_red(), "err ", fmt, ap);
  va_end(ap);
}

void term_dim(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  term_init();
  fprintf(stderr, "%s", c_dim());
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "%s\n", c_reset());
  fflush(stderr);
  va_end(ap);
}

void term_banner_preview(const char *url, const char *entry_url, int watching) {
  term_init();
  const char *e = entry_url && *entry_url ? entry_url : "/src/app.cord";
  const char *dot = term_utf8_ok ? "\xC2\xB7" : "|";          /* · */
  const char *arrow = term_utf8_ok ? "\xE2\x86\x92" : "->";    /* → */
  const char *rule =
      term_utf8_ok
          ? "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
            "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
            "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
            "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
            "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
            "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
            "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
            "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
          : "----------------------------------------";

  fprintf(stderr, "\n");
  fprintf(stderr, "  %s%sCordlang%s %spreview%s  %s  ESM nativo (sin Node)\n",
          c_bold(), c_magenta(), c_reset(), c_cyan(), c_reset(), dot);
  fprintf(stderr, "  %s%s%s\n", c_dim(), rule, c_reset());
  fprintf(stderr, "  %sLocal%s   %s%s%s%s\n", c_dim(), c_reset(), c_bold(),
          c_green(), url ? url : "http://127.0.0.1:4173", c_reset());
  fprintf(stderr, "  %sEntry%s   %s\n", c_dim(), c_reset(), e);
  fprintf(stderr, "  %sServe%s   .cord %s ES modules (JIT)\n", c_dim(),
          c_reset(), arrow);
  if (watching)
    fprintf(stderr,
            "  %sWatch%s   .cord soft-update %s entry/public full reload\n",
            c_dim(), c_reset(), dot);
  fprintf(stderr, "  %sStop%s    Ctrl+C\n", c_dim(), c_reset());
  fprintf(stderr, "\n");
  fflush(stderr);
}
