#include "domain/ir.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* ── dump ───────────────────────────────────────────────── */

typedef struct {
  char *data;
  size_t len;
  size_t cap;
} DumpBuf;

static void db_init(DumpBuf *b) {
  b->cap = 256;
  b->len = 0;
  b->data = malloc(b->cap);
  if (b->data) b->data[0] = '\0';
}

static void db_grow(DumpBuf *b, size_t need) {
  if (!b->data) return;
  if (b->len + need + 1 <= b->cap) return;
  while (b->len + need + 1 > b->cap) b->cap *= 2;
  char *nd = realloc(b->data, b->cap);
  if (nd) b->data = nd;
}

static void db_puts(DumpBuf *b, const char *s) {
  if (!s || !b->data) return;
  size_t n = strlen(s);
  db_grow(b, n);
  memcpy(b->data + b->len, s, n);
  b->len += n;
  b->data[b->len] = '\0';
}

static void db_printf(DumpBuf *b, const char *fmt, ...) {
  char tmp[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(tmp, sizeof(tmp), fmt, ap);
  va_end(ap);
  db_puts(b, tmp);
}

static void dump_node(DumpBuf *b, const IrNode *n, int depth) {
  if (!n) return;
  for (int i = 0; i < depth; i++) db_puts(b, "  ");

  db_puts(b, ir_kind_name(n->kind));

  switch (n->kind) {
    case IR_PROJECT:
      if (n->file) db_printf(b, " file=%s", n->file);
      break;
    case IR_COMPONENT:
    case IR_LAYOUT:
      if (n->name) db_printf(b, " %s", n->name);
      break;
    case IR_ROUTE:
      db_printf(b, " %s => %s", n->name ? n->name : "?",
                n->value ? n->value : "?");
      break;
    case IR_PROP:
      if (n->name && strcmp(n->name, "__props__") == 0)
        db_puts(b, "s");
      else {
        if (n->name) db_printf(b, " %s", n->name);
        if (n->value) db_printf(b, " = %s", n->value);
      }
      break;
    case IR_STATE:
      if (n->name && strcmp(n->name, "__states__") == 0)
        db_puts(b, "s");
      else {
        if (n->name) db_printf(b, " %s", n->name);
        if (n->value) db_printf(b, " = %s", n->value);
      }
      break;
    case IR_COMPUTED:
      if (n->name) db_printf(b, " %s", n->name);
      if (n->value) db_printf(b, " = %s", n->value);
      break;
    case IR_EFFECT:
      if (n->name) db_printf(b, " %s", n->name);
      if (n->value) db_printf(b, " deps=[%s]", n->value);
      if (n->value2) db_printf(b, " body=%s", n->value2);
      break;
    case IR_ELEMENT:
      if (n->name) db_printf(b, " <%s>", n->name);
      break;
    case IR_TEXT:
      db_printf(b, " \"%s\"", n->value ? n->value : "");
      break;
    case IR_INTERP:
      if (n->value)
        db_printf(b, " #{%s}", n->value);
      else
        db_puts(b, " (template)");
      break;
    case IR_IF:
      db_printf(b, " %s", n->value ? n->value : "?");
      break;
    case IR_FOR:
      db_printf(b, " %s in %s", n->name ? n->name : "?",
                n->value ? n->value : "?");
      break;
    case IR_EVENT:
      db_printf(b, " @%s = %s", n->name ? n->name : "?",
                n->value ? n->value : "?");
      break;
    case IR_ATTR:
      db_printf(b, " %s = %s", n->name ? n->name : "?",
                n->value ? n->value : "");
      break;
    case IR_SLOT:
      if (n->name) db_printf(b, " %s", n->name);
      break;
    case IR_FETCH:
      db_printf(b, " %s = %s", n->name ? n->name : "?",
                n->value ? n->value : "?");
      break;
    case IR_HOOK:
      db_printf(b, " %s", n->name ? n->name : "?");
      if (n->value) db_printf(b, " %s", n->value);
      if (n->value2) db_printf(b, " = %s", n->value2);
      break;
    case IR_MODULE_USE:
      db_printf(b, " %s", n->value ? n->value : "?");
      if (n->name) db_printf(b, " as %s", n->name);
      break;
    case IR_FOREIGN:
      db_printf(b, " %s", n->name ? n->name : "?");
      if (n->value) db_printf(b, " from %s", n->value);
      break;
    default:
      break;
  }

  if (n->line > 0) db_printf(b, "  @%d:%d", n->line, n->col);
  db_puts(b, "\n");

  for (size_t i = 0; i < n->n_kids; i++) dump_node(b, n->kids[i], depth + 1);
}

char *ir_dump(const IrProgram *p) {
  DumpBuf b;
  db_init(&b);
  if (!b.data) return NULL;
  if (!p || !p->root) {
    db_puts(&b, "(empty IR)\n");
    return b.data;
  }
  dump_node(&b, p->root, 0);
  return b.data;
}
