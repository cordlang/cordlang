#include "domain/ir_internal.h"
#include "domain/expr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *ir_maybe_normalize(const char *src) {
  if (!src) return NULL;
  char *normalized = expr_normalize(src);
  if (normalized) return normalized;
  size_t len = strlen(src) + 1;
  char *copy = malloc(len);
  if (copy) memcpy(copy, src, len);
  return copy;
}

IrNode *ir_make_hook(const char *hook_name, const Node *node,
                         const char *file) {
  IrNode *h =
      ir_node_create(IR_HOOK, hook_name, node->value, node->value2, node->line,
                     node->col, file);
  ir_convert_children(node, h, file);
  return ir_tag(h, (Node *)node);
}

IrNode *ir_convert_node(const Node *node, const char *file) {
  if (!node) return NULL;
  if (ir_is_declaration_node(node->type))
    return ir_convert_declaration(node, file);

  switch (node->type) {
    case NODE_ROOT: {
      IrNode *p =
          ir_node_create(IR_PROJECT, NULL, NULL, NULL, node->line, node->col,
                         file);
      ir_convert_children(node, p, file);
      return ir_tag(p, (Node *)node);
    }

    case NODE_COMPONENT_DEF: {
      int is_layout =
          node->value2 && strcmp(node->value2, "__layout__") == 0;
      IrNode *c = ir_node_create(is_layout ? IR_LAYOUT : IR_COMPONENT,
                                 node->value, NULL, is_layout ? NULL : node->value2,
                                 node->line, node->col, file);
      ir_convert_children(node, c, file);
      return ir_tag(c, (Node *)node);
    }

    case NODE_ROUTE: {
      /* name=path, value=target component/path */
      IrNode *r = ir_node_create(IR_ROUTE, node->value, node->value2, NULL,
                                 node->line, node->col, file);
      ir_convert_children(node, r, file);
      return ir_tag(r, (Node *)node);
    }

    case NODE_ELEMENT: {
      IrNode *el = ir_node_create(IR_ELEMENT, node->value, NULL, NULL, node->line,
                                  node->col, file);
      ir_convert_children(node, el, file);
      return ir_tag(el, (Node *)node);
    }

    case NODE_FOREIGN: {
      IrNode *f = ir_node_create(IR_FOREIGN, node->value, node->value2, NULL,
                                 node->line, node->col, file);
      ir_convert_children(node, f, file);
      return ir_tag(f, (Node *)node);
    }

    case NODE_IF: {
      char *cond = ir_maybe_normalize(node->value);
      IrNode *n =
          ir_node_create(IR_IF, NULL, cond, NULL, node->line, node->col, file);
      free(cond);
      ir_convert_children(node, n, file);
      return ir_tag(n, (Node *)node);
    }

    case NODE_FOR: {
      char *list = ir_maybe_normalize(node->value2);
      IrNode *n = ir_node_create(IR_FOR, node->value, list, NULL, node->line,
                                 node->col, file);
      free(list);
      ir_convert_children(node, n, file);
      return ir_tag(n, (Node *)node);
    }

    case NODE_TEXT: {
      IrNode *t = ir_node_create(IR_TEXT, NULL, node->value, node->value2,
                                 node->line, node->col, file);
      /* else marker carries else-branch body as children */
      if (node->value && strcmp(node->value, "__else__") == 0)
        ir_convert_children(node, t, file);
      return ir_tag(t, (Node *)node);
    }

    case NODE_STRING:
      return ir_tag(ir_node_create(IR_TEXT, NULL, node->value, NULL, node->line,
                                   node->col, file),
                    (Node *)node);

    case NODE_INTERPOLATION: {
      char *expr = node->value ? ir_maybe_normalize(node->value) : NULL;
      IrNode *n = ir_node_create(IR_INTERP, NULL, expr, NULL, node->line, node->col,
                                 file);
      free(expr);
      ir_convert_children(node, n, file);
      return ir_tag(n, (Node *)node);
    }

    case NODE_SLOT: {
      IrNode *n =
          ir_node_create(IR_SLOT, node->value, node->value2, NULL, node->line,
                         node->col, file);
      ir_convert_children(node, n, file);
      return ir_tag(n, (Node *)node);
    }

    case NODE_EVENT: {
      char *handler = ir_maybe_normalize(node->value2);
      IrNode *n = ir_node_create(IR_EVENT, node->value, handler, NULL, node->line,
                                 node->col, file);
      free(handler);
      return ir_tag(n, (Node *)node);
    }

    case NODE_ATTR:
      return ir_tag(ir_node_create(IR_ATTR, node->value, node->value2, NULL,
                                   node->line, node->col, file),
                    (Node *)node);

    case NODE_BOOL_ATTR:
      return ir_tag(ir_node_create(IR_ATTR, node->value, "true", NULL, node->line,
                                   node->col, file),
                    (Node *)node);

    case NODE_STYLE_MAP: {
      IrNode *n =
          ir_node_create(IR_ATTR, "style", NULL, NULL, node->line, node->col, file);
      ir_convert_children(node, n, file);
      return ir_tag(n, (Node *)node);
    }

    case NODE_STYLE_ENTRY:
      return ir_tag(ir_node_create(IR_ATTR, node->value, node->value2, NULL,
                                   node->line, node->col, file),
                    (Node *)node);

    case NODE_FETCH_DECL: {
      IrNode *n = ir_node_create(IR_FETCH, node->value, node->value2, NULL,
                                 node->line, node->col, file);
      ir_convert_children(node, n, file);
      return ir_tag(n, (Node *)node);
    }

    case NODE_USE:
      return ir_tag(ir_node_create(IR_MODULE_USE, node->value2 /* alias */,
                                   node->value /* path */, NULL, node->line,
                                   node->col, file),
                    (Node *)node);

    case NODE_AWAIT: {
      IrNode *n = ir_node_create(IR_AWAIT, node->value2 /* then name */,
                                 node->value /* promise */, NULL, node->line,
                                 node->col, file);
      ir_convert_children(node, n, file);
      return ir_tag(n, (Node *)node);
    }
    case NODE_SNIPPET: {
      IrNode *n = ir_node_create(IR_SNIPPET, node->value, node->value2, NULL,
                                 node->line, node->col, file);
      ir_convert_children(node, n, file);
      return ir_tag(n, (Node *)node);
    }
    case NODE_RENDER:
      return ir_tag(ir_node_create(IR_RENDER, node->value, node->value2, NULL,
                                   node->line, node->col, file),
                    (Node *)node);

    /* Hooks â†’ IR_HOOK with kind in name */
    default: {
      char kindbuf[32];
      snprintf(kindbuf, sizeof(kindbuf), "ast_%d", (int)node->type);
      return ir_make_hook(kindbuf, node, file);
    }
  }
}

void ir_convert_children(const Node *node, IrNode *parent,
                             const char *file) {
  if (!node || !parent) return;
  for (size_t i = 0; i < node->children_len; i++) {
    IrNode *kid = ir_convert_node(node->children[i], file);
    if (kid) ir_add_kid(parent, kid);
  }
}
