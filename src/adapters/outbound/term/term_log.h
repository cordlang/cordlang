#ifndef CORDLANG_TERM_LOG_H
#define CORDLANG_TERM_LOG_H

/* Tiny ANSI + UTF-8 terminal helpers for cordlang run / CLI. */

void term_init(void); /* enable VT + UTF-8 on Windows; safe to call often */

void term_banner_preview(const char *url, const char *entry_url, int watching);
void term_info(const char *fmt, ...);
void term_ok(const char *fmt, ...);
void term_warn(const char *fmt, ...);
void term_error(const char *fmt, ...);
void term_dim(const char *fmt, ...);

#endif
