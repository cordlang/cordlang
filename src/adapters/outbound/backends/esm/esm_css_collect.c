#include "adapters/outbound/backends/esm/esm_css_internal.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void cb_init(Cb *c) {
  c->cap = 32768;
  c->len = 0;
  c->buf = calloc(c->cap, 1);
  if (!c->buf) {
    fprintf(stderr, "fatal: out of memory (esm css)\n");
    exit(1);
  }
}

void cb_add(Cb *c, const char *s) {
  if (!s) return;
  size_t n = strlen(s);
  if (c->len + n + 1 >= c->cap) {
    while (c->len + n + 1 >= c->cap) c->cap *= 2;
    char *nb = realloc(c->buf, c->cap);
    if (!nb) {
      fprintf(stderr, "fatal: out of memory (esm css)\n");
      exit(1);
    }
    c->buf = nb;
  }
  memcpy(c->buf + c->len, s, n);
  c->len += n;
  c->buf[c->len] = '\0';
}

void cb_addf(Cb *c, const char *fmt, ...) {
  char tmp[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(tmp, sizeof(tmp), fmt, ap);
  va_end(ap);
  cb_add(c, tmp);
}

int cls_has(ClassSet *s, const char *n) {
  for (int i = 0; i < s->count; i++)
    if (strcmp(s->names[i], n) == 0) return 1;
  return 0;
}

void cls_add(ClassSet *s, const char *n) {
  if (!n || !*n) return;
  if (strlen(n) >= CLS_LEN) return;
  if (s->count >= CLS_MAX) return;
  if (cls_has(s, n)) return;
  snprintf(s->names[s->count], CLS_LEN, "%s", n);
  s->count++;
}

void cls_add_split(ClassSet *s, const char *blob) {
  if (!blob) return;
  const char *p = blob;
  while (*p) {
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) break;
    const char *start = p;
    while (*p && *p != ' ' && *p != '\t') p++;
    size_t n = (size_t)(p - start);
    if (n && n < CLS_LEN) {
      char buf[CLS_LEN];
      memcpy(buf, start, n);
      buf[n] = '\0';
      cls_add(s, buf);
    }
  }
}

void collect_from_ir(IrNode *n, ClassSet *set) {
  if (!n) return;
  if (n->kind == IR_ELEMENT && n->name) {
    const char *base = cord_tag_base_class(n->name);
    char classes[2048];
    cord_collect_classes(classes, sizeof(classes), n, base);
    cls_add_split(set, classes);
  }
  for (size_t i = 0; i < n->n_kids; i++) collect_from_ir(n->kids[i], set);
}
