#include "html_backend_internal.h"

void htmlb_sb_init(StrBuf *sb) {
  sb->cap = 65536;
  sb->len = 0;
  sb->buf = calloc(sb->cap, 1);
}

void htmlb_sb_oom(void) {
  fprintf(stderr, "fatal: out of memory (StrBuf)\n");
  exit(1);
}

void htmlb_sb_append(StrBuf *sb, const char *s) {
  if (!s) return;
  size_t slen = strlen(s);
  if (sb->len + slen + 1 >= sb->cap) {
    while (sb->len + slen + 1 >= sb->cap) {
      if (sb->cap > (size_t)-1 / 2) htmlb_sb_oom();
      sb->cap *= 2;
    }
    char *nbuf = realloc(sb->buf, sb->cap);
    if (!nbuf) htmlb_sb_oom();
    sb->buf = nbuf;
  }
  memcpy(sb->buf + sb->len, s, slen);
  sb->len += slen;
  sb->buf[sb->len] = '\0';
}

void htmlb_sb_appendf(StrBuf *sb, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int n = vsnprintf(NULL, 0, fmt, args);
  va_end(args);
  if (n < 0) return;

  if (sb->len + (size_t)n + 1 >= sb->cap) {
    while (sb->len + (size_t)n + 1 >= sb->cap) {
      if (sb->cap > (size_t)-1 / 2) htmlb_sb_oom();
      sb->cap *= 2;
    }
    char *nbuf = realloc(sb->buf, sb->cap);
    if (!nbuf) htmlb_sb_oom();
    sb->buf = nbuf;
  }

  va_start(args, fmt);
  vsnprintf(sb->buf + sb->len, sb->cap - sb->len, fmt, args);
  va_end(args);
  sb->len += (size_t)n;
  sb->buf[sb->len] = '\0';
}

void htmlb_sb_indent(StrBuf *sb, int depth) {
  for (int i = 0; i < depth; i++) htmlb_sb_append(sb, "  ");
}

void htmlb_html_escape_append(StrBuf *sb, const char *s) {
  if (!s) return;
  for (const char *p = s; *p; p++) {
    switch (*p) {
      case '&': htmlb_sb_append(sb, "&amp;"); break;
      case '<': htmlb_sb_append(sb, "&lt;"); break;
      case '>': htmlb_sb_append(sb, "&gt;"); break;
      case '"': htmlb_sb_append(sb, "&quot;"); break;
      case '\'': htmlb_sb_append(sb, "&#39;"); break;
      default: {
        char c[2] = {*p, 0};
        htmlb_sb_append(sb, c);
        break;
      }
    }
  }
}
