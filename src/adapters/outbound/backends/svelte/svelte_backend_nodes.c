#include "adapters/outbound/backends/svelte/svelte_backend_internal.h"

int svelte_is_ir_script_decl(const IrNode *n) {
  if (!n) return 1;
  if (n->kind == IR_PROP || n->kind == IR_STATE || n->kind == IR_COMPUTED ||
      n->kind == IR_EFFECT || n->kind == IR_FETCH || n->kind == IR_STORE ||
      n->kind == IR_MODULE_USE || n->kind == IR_ATTR || n->kind == IR_EVENT)
    return 1;
  if (n->kind == IR_HOOK && n->name) {
    /* markup hooks */
    if (strcmp(n->name, "portal") == 0 || strcmp(n->name, "head") == 0 ||
        strcmp(n->name, "empty") == 0 || strcmp(n->name, "loading") == 0 ||
        strcmp(n->name, "suspense") == 0 ||
        strcmp(n->name, "errorBoundary") == 0)
      return 0;
    return 1;
  }
  return 0;
}

void svelte_gen_node_ir(StrBuf *sb, IrNode *node, int depth, int is_layout) {
  if (!node) return;
  switch (node->kind) {
    case IR_PROJECT:
      svelte_gen_children_ir(sb, node, depth, is_layout);
      break;
    case IR_ELEMENT:
      svelte_gen_element_ir(sb, node, depth, is_layout);
      break;
    case IR_SLOT:
      svelte_sb_indent(sb, depth);
      svelte_sb_append(sb, "{@render children?.()}\n");
      break;
    case IR_TEXT:
    case IR_INTERP:
      svelte_gen_text_or_interp_ir(sb, node, depth);
      break;
    case IR_HOOK:
      if (node->name && strcmp(node->name, "empty") == 0) {
        const char *cond = node->value ? node->value : "true";
        svelte_sb_indent(sb, depth);
        svelte_sb_appendf(sb, "{#if %s}\n", cond);
        for (size_t i = 0; i < node->n_kids; i++)
          svelte_gen_node_ir(sb, node->kids[i], depth + 1, is_layout);
        svelte_sb_indent(sb, depth);
        svelte_sb_append(sb, "{/if}\n");
      } else if (node->name && (strcmp(node->name, "loading") == 0 ||
                                strcmp(node->name, "suspense") == 0)) {
        for (size_t i = 0; i < node->n_kids; i++) {
          IrNode *ch = node->kids[i];
          if (ch->kind == IR_ATTR) continue;
          if (ch->kind == IR_ELEMENT && ch->name &&
              strcmp(ch->name, "__fallback__") == 0)
            continue;
          svelte_gen_node_ir(sb, ch, depth, is_layout);
        }
      } else if (node->name && strcmp(node->name, "head") == 0) {
        if (node->value) {
          char title_e[512];
          html_escape_to(title_e, sizeof(title_e), node->value);
          svelte_sb_indent(sb, depth);
          svelte_sb_append(sb, "<svelte:head>\n");
          svelte_sb_indent(sb, depth + 1);
          svelte_sb_appendf(sb, "<title>%s</title>\n", title_e);
          svelte_sb_indent(sb, depth);
          svelte_sb_append(sb, "</svelte:head>\n");
        }
      } else if (node->name && strcmp(node->name, "portal") == 0) {
        const char *target = node->value ? node->value : "document.body";
        svelte_sb_indent(sb, depth);
        svelte_sb_appendf(sb, "<div use:portal={%s}>\n", target);
        for (size_t i = 0; i < node->n_kids; i++) {
          IrNode *ch = node->kids[i];
          if (ch->kind == IR_ATTR || ch->kind == IR_EVENT) continue;
          svelte_gen_node_ir(sb, ch, depth + 1, is_layout);
        }
        svelte_sb_indent(sb, depth);
        svelte_sb_append(sb, "</div>\n");
      } else {
        svelte_gen_children_ir(sb, node, depth, is_layout);
      }
      break;
    case IR_AWAIT: {
      const char *prom = node->value ? node->value : "Promise.resolve(null)";
      const char *then_n = node->name ? node->name : "value";
      const char *catch_n = "error";
      IrNode *loading = NULL, *err_block = NULL;
      for (size_t i = 0; i < node->n_kids; i++) {
        IrNode *ch = node->kids[i];
        if (ch->kind == IR_ATTR && ch->name && strcmp(ch->name, "catch") == 0 &&
            ch->value)
          catch_n = ch->value;
        if (ch->kind == IR_ELEMENT && ch->name) {
          if (strcmp(ch->name, "__loading__") == 0) loading = ch;
          if (strcmp(ch->name, "__error__") == 0) err_block = ch;
        }
      }
      svelte_sb_indent(sb, depth);
      svelte_sb_appendf(sb, "{#await %s}\n", prom);
      if (loading) {
        for (size_t i = 0; i < loading->n_kids; i++)
          svelte_gen_node_ir(sb, loading->kids[i], depth + 1, is_layout);
      }
      svelte_sb_indent(sb, depth);
      svelte_sb_appendf(sb, "{:then %s}\n", then_n);
      for (size_t i = 0; i < node->n_kids; i++) {
        IrNode *ch = node->kids[i];
        if (ch->kind == IR_ATTR) continue;
        if (ch->kind == IR_ELEMENT && ch->name &&
            (strcmp(ch->name, "__loading__") == 0 ||
             strcmp(ch->name, "__error__") == 0))
          continue;
        svelte_gen_node_ir(sb, ch, depth + 1, is_layout);
      }
      if (err_block) {
        svelte_sb_indent(sb, depth);
        svelte_sb_appendf(sb, "{:catch %s}\n", catch_n);
        for (size_t i = 0; i < err_block->n_kids; i++)
          svelte_gen_node_ir(sb, err_block->kids[i], depth + 1, is_layout);
      }
      svelte_sb_indent(sb, depth);
      svelte_sb_append(sb, "{/await}\n");
      break;
    }
    case IR_SNIPPET: {
      const char *name = node->name ? node->name : "snippet";
      const char *params = node->value ? node->value : "";
      svelte_sb_indent(sb, depth);
      if (params[0])
        svelte_sb_appendf(sb, "{#snippet %s(%s)}\n", name, params);
      else
        svelte_sb_appendf(sb, "{#snippet %s()}\n", name);
      for (size_t i = 0; i < node->n_kids; i++) {
        IrNode *ch = node->kids[i];
        if (ch->kind == IR_ATTR || ch->kind == IR_EVENT) continue;
        svelte_gen_node_ir(sb, ch, depth + 1, is_layout);
      }
      svelte_sb_indent(sb, depth);
      svelte_sb_append(sb, "{/snippet}\n");
      break;
    }
    case IR_RENDER: {
      svelte_sb_indent(sb, depth);
      if (node->name && strchr(node->name, '(')) {
        svelte_sb_appendf(sb, "{@render %s}\n", node->name);
      } else if (node->name) {
        const char *arg = NULL;
        for (size_t i = 0; i < node->n_kids; i++) {
          IrNode *a = node->kids[i];
          if (a->kind == IR_ATTR && a->value) {
            arg = a->value;
            break;
          }
        }
        if (arg)
          svelte_sb_appendf(sb, "{@render %s(\"%s\")}\n", node->name, arg);
        else
          svelte_sb_appendf(sb, "{@render %s?.()}\n", node->name);
      }
      break;
    }
    default:
      svelte_gen_children_ir(sb, node, depth, is_layout);
      break;
  }
}
