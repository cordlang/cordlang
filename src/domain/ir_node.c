#include "domain/ir.h"
#include "domain/expr.h"
#include <stdlib.h>
#include <string.h>
char *ir_xstrdup(const char *s) {
  if (!s) return NULL;
  return strdup(s);
}

IrNode *ir_node_create(IrKind kind, const char *name, const char *value,
                              const char *value2, int line, int col,
                              const char *file) {
  IrNode *n = calloc(1, sizeof(IrNode));
  if (!n) return NULL;
  n->kind = kind;
  n->name = ir_xstrdup(name);
  n->value = ir_xstrdup(value);
  n->value2 = ir_xstrdup(value2);
  n->line = line;
  n->col = col;
  n->file = ir_xstrdup(file);
  n->origin = NULL;
  n->cap = 4;
  n->kids = calloc(n->cap, sizeof(IrNode *));
  return n;
}

/* Attach weak AST origin (not owned). */
IrNode *ir_tag(IrNode *n, Node *ast) {
  if (n) n->origin = ast;
  return n;
}

void ir_add_kid(IrNode *parent, IrNode *child) {
  if (!parent || !child) {
    if (child) {
      /* free orphan if parent missing — handled by ir_node_free later paths */
    }
    return;
  }
  if (parent->n_kids >= parent->cap) {
    parent->cap *= 2;
    parent->kids =
        realloc(parent->kids, parent->cap * sizeof(IrNode *));
  }
  parent->kids[parent->n_kids++] = child;
}

static void ir_node_free(IrNode *n) {
  if (!n) return;
  for (size_t i = 0; i < n->n_kids; i++) ir_node_free(n->kids[i]);
  free(n->kids);
  free(n->name);
  free(n->value);
  free(n->value2);
  free(n->file);
  free(n);
}

void ir_free_node(IrNode *n) { ir_node_free(n); }

const char *ir_kind_name(IrKind k) {
  switch (k) {
    case IR_PROJECT:
      return "PROJECT";
    case IR_COMPONENT:
      return "COMPONENT";
    case IR_LAYOUT:
      return "LAYOUT";
    case IR_ROUTE:
      return "ROUTE";
    case IR_PROP:
      return "PROP";
    case IR_STATE:
      return "STATE";
    case IR_COMPUTED:
      return "COMPUTED";
    case IR_EFFECT:
      return "EFFECT";
    case IR_ELEMENT:
      return "ELEMENT";
    case IR_TEXT:
      return "TEXT";
    case IR_INTERP:
      return "INTERP";
    case IR_IF:
      return "IF";
    case IR_FOR:
      return "FOR";
    case IR_EVENT:
      return "EVENT";
    case IR_ATTR:
      return "ATTR";
    case IR_SLOT:
      return "SLOT";
    case IR_FETCH:
      return "FETCH";
    case IR_HOOK:
      return "HOOK";
    case IR_MODULE_USE:
      return "MODULE_USE";
    case IR_AWAIT:
      return "AWAIT";
    case IR_SNIPPET:
      return "SNIPPET";
    case IR_STORE:
      return "STORE";
    case IR_RENDER:
      return "RENDER";
    case IR_FOREIGN:
      return "FOREIGN";
    default:
      return "UNKNOWN";
  }
}

