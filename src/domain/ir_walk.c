#include "domain/ir.h"
#include <string.h>
/* ── walk helpers ───────────────────────────────────────── */

IrNode *ir_find_child(const IrNode *n, IrKind kind) {
  if (!n) return NULL;
  for (size_t i = 0; i < n->n_kids; i++)
    if (n->kids[i] && n->kids[i]->kind == kind) return n->kids[i];
  return NULL;
}

IrNode *ir_find_hook(const IrNode *n, const char *hook_kind) {
  if (!n || !hook_kind) return NULL;
  for (size_t i = 0; i < n->n_kids; i++) {
    IrNode *k = n->kids[i];
    if (k && k->kind == IR_HOOK && k->name && strcmp(k->name, hook_kind) == 0)
      return k;
  }
  return NULL;
}

const char *ir_attr(const IrNode *n, const char *attr_name) {
  if (!n || !attr_name) return NULL;
  for (size_t i = 0; i < n->n_kids; i++) {
    IrNode *k = n->kids[i];
    if (k && k->kind == IR_ATTR && k->name && strcmp(k->name, attr_name) == 0)
      return k->value;
  }
  return NULL;
}

size_t ir_count_kind(const IrNode *n, IrKind kind) {
  size_t c = 0;
  if (!n) return 0;
  for (size_t i = 0; i < n->n_kids; i++)
    if (n->kids[i] && n->kids[i]->kind == kind) c++;
  return c;
}

Node *ir_origin(const IrNode *n) { return n ? n->origin : NULL; }

Node *ir_project_origin(const IrProgram *p) {
  if (!p || !p->root) return NULL;
  if (p->root->origin) return p->root->origin;
  /* Fallback: first kid origin */
  for (size_t i = 0; i < p->root->n_kids; i++) {
    if (p->root->kids[i] && p->root->kids[i]->origin)
      return p->root->kids[i]->origin;
  }
  return NULL;
}

