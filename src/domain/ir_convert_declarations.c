#include "domain/ir_internal.h"
#include <stdlib.h>
#include <string.h>

IrNode *ir_convert_declaration(const Node *node, const char *file) {
  if (!node) return NULL;
  switch (node->type) {
    case NODE_PROPS_DECL: {
      /* Expand props block into individual IR_PROP kids under a synthetic parent
         â€” attach each prop as IR_PROP. Caller of ir_convert_children uses this
         by expanding inline when parent is component. Here return a bag of props
         as multiple nodes via a temporary: we create a dummy and let caller
         reparent. Simpler: emit first prop as this node and siblings via kids.
         Actually: props container has TEXT children (name=value default). */
      IrNode *bag =
          ir_node_create(IR_PROP, NULL, NULL, NULL, node->line, node->col, file);
      /* mark bag name as "__props__" so dump can flatten or show group */
      free(bag->name);
      bag->name = ir_xstrdup("__props__");
      for (size_t i = 0; i < node->children_len; i++) {
        Node *ch = node->children[i];
        if (!ch) continue;
        /* prop name in value, default in value2; optional type ATTR child */
        IrNode *pr = ir_tag(ir_node_create(IR_PROP, ch->value, ch->value2, NULL,
                                           ch->line, ch->col, file),
                            ch);
        for (size_t j = 0; j < ch->children_len; j++) {
          Node *a = ch->children[j];
          if (!a || a->type != NODE_ATTR || !a->value || !a->value2) continue;
          ir_add_kid(pr, ir_node_create(IR_ATTR, a->value, a->value2, NULL,
                                        a->line, a->col, file));
        }
        ir_add_kid(bag, pr);
      }
      return ir_tag(bag, (Node *)node);
    }

    case NODE_STATE_DECL: {
      /* Container (value==NULL) with STATE children, or leaf state. */
      if (!node->value && node->children_len > 0) {
        IrNode *bag =
            ir_node_create(IR_STATE, "__states__", NULL, NULL, node->line,
                           node->col, file);
        for (size_t i = 0; i < node->children_len; i++) {
          Node *ch = node->children[i];
          if (!ch) continue;
          if (ch->type == NODE_STATE_DECL) {
            IrNode *st = ir_tag(ir_node_create(IR_STATE, ch->value, ch->value2,
                                               NULL, ch->line, ch->col, file),
                                ch);
            ir_add_kid(bag, st);
          } else {
            IrNode *sub = ir_convert_node(ch, file);
            if (sub) ir_add_kid(bag, sub);
          }
        }
        return ir_tag(bag, (Node *)node);
      }
      return ir_tag(ir_node_create(IR_STATE, node->value, node->value2, NULL,
                                   node->line, node->col, file),
                    (Node *)node);
    }

    case NODE_COMPUTED_DECL: {
      char *expr = ir_maybe_normalize(node->value2);
      IrNode *c = ir_node_create(IR_COMPUTED, node->value, expr, NULL, node->line,
                                 node->col, file);
      free(expr);
      return ir_tag(c, (Node *)node);
    }

    case NODE_EFFECT_DECL: {
      char *body = ir_maybe_normalize(node->value2);
      IrNode *e = ir_node_create(IR_EFFECT, "effect", node->value /* deps */, body,
                                 node->line, node->col, file);
      free(body);
      ir_convert_children(node, e, file);
      return ir_tag(e, (Node *)node);
    }

    case NODE_LAYOUT_EFFECT: {
      char *body = ir_maybe_normalize(node->value2);
      IrNode *e =
          ir_node_create(IR_EFFECT, "layoutEffect", node->value, body, node->line,
                         node->col, file);
      free(body);
      ir_convert_children(node, e, file);
      return ir_tag(e, (Node *)node);
    }

    case NODE_INSERTION_EFFECT: {
      char *body = ir_maybe_normalize(node->value2);
      IrNode *e = ir_node_create(IR_EFFECT, "insertionEffect", node->value, body,
                                 node->line, node->col, file);
      free(body);
      ir_convert_children(node, e, file);
      return ir_tag(e, (Node *)node);
    }

    case NODE_STORE_DECL:
      return ir_tag(ir_node_create(IR_STORE, node->value, node->value2, NULL,
                                   node->line, node->col, file),
                    (Node *)node);
    case NODE_REF_DECL:
      return ir_make_hook("ref", node, file);
    case NODE_CONTEXT_DECL:
      return ir_make_hook("context", node, file);
    case NODE_CONTEXT_USE:
      return ir_make_hook("ctx", node, file);
    case NODE_REDUCER_DECL:
      return ir_make_hook("reducer", node, file);
    case NODE_PARAMS_DECL:
      return ir_make_hook("params", node, file);
    case NODE_NAVIGATE_DECL:
      return ir_make_hook("navigate", node, file);
    case NODE_CALLBACK_DECL:
      return ir_make_hook("callback", node, file);
    case NODE_ID_DECL:
      return ir_make_hook("id", node, file);
    case NODE_TRANSITION_DECL:
      return ir_make_hook("transition", node, file);
    case NODE_DEFERRED_DECL:
      return ir_make_hook("deferred", node, file);
    case NODE_ACTION_DECL:
      return ir_make_hook("action", node, file);
    case NODE_EFFECT_EVENT:
      return ir_make_hook("effectEvent", node, file);
    case NODE_EXTERNAL_STORE:
      return ir_make_hook("externalStore", node, file);
    case NODE_IMPERATIVE_HANDLE:
      return ir_make_hook("imperativeHandle", node, file);
    case NODE_LAZY_DECL:
      return ir_make_hook("lazy", node, file);
    case NODE_PORTAL:
      return ir_make_hook("portal", node, file);
    case NODE_ERROR_BOUNDARY:
      return ir_make_hook("errorBoundary", node, file);
    case NODE_SUSPENSE:
      return ir_make_hook("suspense", node, file);
    case NODE_LOADING:
      return ir_make_hook("loading", node, file);
    case NODE_EMPTY:
      return ir_make_hook("empty", node, file);
    case NODE_HEAD:
      return ir_make_hook("head", node, file);
    case NODE_THEME:
      return ir_make_hook("theme", node, file);

    default:
      return NULL;
  }
}

int ir_is_declaration_node(NodeType type) {
  switch (type) {
    case NODE_PROPS_DECL:
    case NODE_STATE_DECL:
    case NODE_COMPUTED_DECL:
    case NODE_EFFECT_DECL:
    case NODE_LAYOUT_EFFECT:
    case NODE_INSERTION_EFFECT:
    case NODE_STORE_DECL:
    case NODE_REF_DECL:
    case NODE_CONTEXT_DECL:
    case NODE_CONTEXT_USE:
    case NODE_REDUCER_DECL:
    case NODE_PARAMS_DECL:
    case NODE_NAVIGATE_DECL:
    case NODE_CALLBACK_DECL:
    case NODE_ID_DECL:
    case NODE_TRANSITION_DECL:
    case NODE_DEFERRED_DECL:
    case NODE_ACTION_DECL:
    case NODE_EFFECT_EVENT:
    case NODE_EXTERNAL_STORE:
    case NODE_IMPERATIVE_HANDLE:
    case NODE_LAZY_DECL:
    case NODE_PORTAL:
    case NODE_ERROR_BOUNDARY:
    case NODE_SUSPENSE:
    case NODE_LOADING:
    case NODE_EMPTY:
    case NODE_HEAD:
    case NODE_THEME:
      return 1;
    default:
      return 0;
  }
}
