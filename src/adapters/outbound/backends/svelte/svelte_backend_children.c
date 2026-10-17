#include "adapters/outbound/backends/svelte/svelte_backend_internal.h"

void svelte_emit_interp_expr(StrBuf *sb, const char *expr) {
  if (!expr) {
    svelte_sb_append(sb, "{}");
    return;
  }
  if (svelte_sv_is_store_expr(expr))
    svelte_sb_appendf(sb, "{$%s}", expr);
  else
    svelte_sb_appendf(sb, "{%s}", expr);
}

void svelte_gen_text_or_interp_ir(StrBuf *sb, IrNode *node, int depth) {
  if (node->kind == IR_INTERP && node->value && node->n_kids == 0) {
    svelte_sb_indent(sb, depth);
    svelte_emit_interp_expr(sb, node->value);
    svelte_sb_append(sb, "\n");
    return;
  }
  if (node->kind == IR_INTERP && node->n_kids > 0) {
    svelte_sb_indent(sb, depth);
    for (size_t i = 0; i < node->n_kids; i++) {
      IrNode *c = node->kids[i];
      if (c->kind == IR_TEXT && c->value) {
        char *plain = interp_plain_text(c->value);
        if (plain) svelte_sb_append(sb, plain);
        free(plain);
      } else if (c->kind == IR_INTERP && c->value)
        svelte_emit_interp_expr(sb, c->value);
    }
    svelte_sb_append(sb, "\n");
    return;
  }
  if (node->kind == IR_TEXT) {
    if (node->value && strcmp(node->value, "__else__") == 0) return;
    svelte_sb_indent(sb, depth);
    if (node->value && interp_has(node->value)) {
      const char *p = node->value;
      while (*p) {
        const char *hash = strstr(p, "#{");
        if (!hash) {
          char *plain = interp_plain_text(p);
          if (plain) svelte_sb_append(sb, plain);
          free(plain);
          break;
        }
        if (hash > node->value && hash[-1] == '\\') {
          /* literal \#{…}: emit text before \, then #{…} without '\' */
          if (hash - 1 > p) {
            size_t n = (size_t)((hash - 1) - p);
            char *tmp = malloc(n + 1);
            if (tmp) {
              memcpy(tmp, p, n);
              tmp[n] = '\0';
              svelte_sb_append(sb, tmp);
              free(tmp);
            }
          }
          const char *end = strchr(hash + 2, '}');
          if (!end) {
            svelte_sb_append(sb, hash);
            break;
          }
          size_t n = (size_t)(end - hash + 1);
          char *tmp = malloc(n + 1);
          if (tmp) {
            memcpy(tmp, hash, n);
            tmp[n] = '\0';
            svelte_sb_append(sb, tmp);
            free(tmp);
          }
          p = end + 1;
          continue;
        }
        if (hash > p) {
          char tmp[512];
          size_t n = (size_t)(hash - p);
          if (n >= sizeof(tmp)) n = sizeof(tmp) - 1;
          memcpy(tmp, p, n);
          tmp[n] = '\0';
          svelte_sb_append(sb, tmp);
        }
        const char *end = strchr(hash + 2, '}');
        if (!end) {
          svelte_sb_append(sb, hash);
          break;
        }
        char expr[256];
        size_t en = (size_t)(end - (hash + 2));
        if (en >= sizeof(expr)) en = sizeof(expr) - 1;
        memcpy(expr, hash + 2, en);
        expr[en] = '\0';
        svelte_emit_interp_expr(sb, expr);
        p = end + 1;
      }
      svelte_sb_append(sb, "\n");
    } else {
      char *plain = interp_plain_text(node->value);
      svelte_sb_append(sb, plain ? plain : "");
      free(plain);
      svelte_sb_append(sb, "\n");
    }
  }
}

int svelte_is_markup_child_kind(IrKind k) {
  return k == IR_ELEMENT || k == IR_TEXT || k == IR_FOR || k == IR_IF ||
         k == IR_INTERP || k == IR_SLOT || k == IR_AWAIT || k == IR_SNIPPET ||
         k == IR_RENDER;
}

void svelte_gen_children_ir(StrBuf *sb, IrNode *node, int depth, int is_layout) {
  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *child = node->kids[i];
    if (!child) continue;
    if (child->kind == IR_FOR) {
      const char *var = child->name ? child->name : "item";
      const char *list = child->value ? child->value : "items";
      const char *key = ir_attr(child, "key");
      svelte_sb_indent(sb, depth);
      if (key)
        svelte_sb_appendf(sb, "{#each %s || [] as %s (%s)}\n", list, var, key);
      else
        svelte_sb_appendf(sb, "{#each %s || [] as %s}\n", list, var);
      for (size_t j = 0; j < child->n_kids; j++) {
        IrNode *ch = child->kids[j];
        if (ch->kind == IR_ATTR || ch->kind == IR_EVENT) continue;
        svelte_gen_node_ir(sb, ch, depth + 1, is_layout);
      }
      svelte_sb_indent(sb, depth);
      svelte_sb_append(sb, "{/each}\n");
    } else if (child->kind == IR_IF) {
      const char *cond = child->value ? child->value : "true";
      int has_else = 0;
      size_t else_idx = 0;
      for (size_t j = i + 1; j < node->n_kids; j++) {
        if (node->kids[j]->kind == IR_TEXT && node->kids[j]->value &&
            strcmp(node->kids[j]->value, "__else__") == 0) {
          has_else = 1;
          else_idx = j;
          break;
        }
      }
      svelte_sb_indent(sb, depth);
      svelte_sb_appendf(sb, "{#if %s}\n", cond);
      for (size_t j = 0; j < child->n_kids; j++)
        svelte_gen_node_ir(sb, child->kids[j], depth + 1, is_layout);
      if (has_else) {
        svelte_sb_indent(sb, depth);
        svelte_sb_append(sb, "{:else}\n");
        IrNode *en = node->kids[else_idx];
        for (size_t j = 0; j < en->n_kids; j++)
          svelte_gen_node_ir(sb, en->kids[j], depth + 1, is_layout);
        i = else_idx;
      }
      svelte_sb_indent(sb, depth);
      svelte_sb_append(sb, "{/if}\n");
    } else if (child->kind == IR_TEXT || child->kind == IR_INTERP) {
      svelte_gen_text_or_interp_ir(sb, child, depth);
    } else if (child->kind == IR_SLOT) {
      svelte_sb_indent(sb, depth);
      svelte_sb_append(sb, "{@render children?.()}\n");
    } else if (irw_hook_is(child, "empty")) {
      const char *cond = child->value ? child->value : "true";
      svelte_sb_indent(sb, depth);
      svelte_sb_appendf(sb, "{#if %s}\n", cond);
      for (size_t j = 0; j < child->n_kids; j++)
        svelte_gen_node_ir(sb, child->kids[j], depth + 1, is_layout);
      svelte_sb_indent(sb, depth);
      svelte_sb_append(sb, "{/if}\n");
    } else if (irw_hook_is(child, "loading") || irw_hook_is(child, "suspense")) {
      for (size_t j = 0; j < child->n_kids; j++) {
        IrNode *ch = child->kids[j];
        if (ch->kind == IR_ATTR) continue;
        if (ch->kind == IR_ELEMENT && ch->name &&
            strcmp(ch->name, "__fallback__") == 0)
          continue;
        svelte_gen_node_ir(sb, ch, depth, is_layout);
      }
    } else if (irw_hook_is(child, "head")) {
      if (child->value) {
        char title_e[512];
        html_escape_to(title_e, sizeof(title_e), child->value);
        svelte_sb_indent(sb, depth);
        svelte_sb_append(sb, "<svelte:head>\n");
        svelte_sb_indent(sb, depth + 1);
        svelte_sb_appendf(sb, "<title>%s</title>\n", title_e);
        svelte_sb_indent(sb, depth);
        svelte_sb_append(sb, "</svelte:head>\n");
      }
    } else if (child->kind == IR_AWAIT || child->kind == IR_SNIPPET ||
               child->kind == IR_RENDER || irw_hook_is(child, "portal")) {
      svelte_gen_node_ir(sb, child, depth, is_layout);
    } else if (!svelte_is_ir_script_decl(child) && child->kind != IR_COMPONENT &&
               child->kind != IR_LAYOUT && child->kind != IR_ROUTE) {
      svelte_gen_node_ir(sb, child, depth, is_layout);
    }
  }
}
