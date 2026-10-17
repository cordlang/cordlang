#include "adapters/outbound/backends/react/react_ir_internal.h"

void react_ir_gen_ir_interpolation(StrBuf *sb, IrNode *node, int depth) {
  if (!node) return;
  if (node->value && node->n_kids == 0) {
    react_ir_sb_indent(sb, depth);
    react_ir_sb_appendf(sb, "{%s}\n", node->value);
    return;
  }
  if (node->n_kids > 0) {
    react_ir_sb_indent(sb, depth);
    react_ir_sb_append(sb, "{`");
    for (size_t i = 0; i < node->n_kids; i++) {
      IrNode *c = node->kids[i];
      if (c->kind == IR_TEXT && c->value) {
        for (const char *p = c->value; *p; p++) {
          if (*p == '`' || *p == '\\') {
            react_ir_sb_append(sb, "\\");
            char ch[2] = {*p, 0};
            react_ir_sb_append(sb, ch);
          } else if (*p == '$' && p[1] == '{') {
            react_ir_sb_append(sb, "\\$");
          } else {
            char ch[2] = {*p, 0};
            react_ir_sb_append(sb, ch);
          }
        }
      } else if (c->kind == IR_INTERP && c->value) {
        react_ir_sb_appendf(sb, "${%s}", c->value);
      }
    }
    react_ir_sb_append(sb, "`}\n");
  }
}

void react_ir_gen_ir_node(StrBuf *sb, IrNode *node, int depth, GenCtx *ctx) {
  if (!node) return;
  switch (node->kind) {
    case IR_ELEMENT:
      react_ir_gen_ir_element(sb, node, depth, ctx);
      break;
    case IR_SLOT:
      react_ir_sb_indent(sb, depth);
      if (ctx && ctx->is_layout) {
        react_ir_sb_append(sb, "<Outlet />\n");
        ctx->use_router = 1;
      } else {
        react_ir_sb_append(sb, "{children}\n");
        if (ctx) ctx->use_children = 1;
      }
      break;
    case IR_HOOK:
      if (react_ir_ir_hook_is(node, "suspense") || react_ir_ir_hook_is(node, "loading")) {
        if (ctx) ctx->use_suspense = 1;
        const char *fb_text = NULL;
        IrNode *fb_node = NULL;
        for (size_t i = 0; i < node->n_kids; i++) {
          IrNode *c = node->kids[i];
          if (c->kind == IR_ATTR && c->name && strcmp(c->name, "fallback") == 0)
            fb_text = c->value;
          if (c->kind == IR_ELEMENT && c->name &&
              strcmp(c->name, "__fallback__") == 0)
            fb_node = c;
        }
        react_ir_sb_indent(sb, depth);
        react_ir_sb_append(sb, "<Suspense fallback={");
        if (fb_node) {
          react_ir_sb_append(sb, "(\n");
          react_ir_sb_indent(sb, depth + 1);
          react_ir_sb_append(sb, "<>\n");
          for (size_t i = 0; i < fb_node->n_kids; i++)
            react_ir_gen_ir_node(sb, fb_node->kids[i], depth + 2, ctx);
          react_ir_sb_indent(sb, depth + 1);
          react_ir_sb_append(sb, "</>\n");
          react_ir_sb_indent(sb, depth);
          react_ir_sb_append(sb, ")");
        } else if (fb_text) {
          react_ir_sb_appendf(sb, "<div>%s</div>", fb_text);
        } else {
          react_ir_sb_append(sb, "<div>Loading...</div>");
        }
        react_ir_sb_append(sb, "}>\n");
        for (size_t i = 0; i < node->n_kids; i++) {
          IrNode *c = node->kids[i];
          if (c->kind == IR_ATTR) continue;
          if (c->kind == IR_ELEMENT && c->name &&
              strcmp(c->name, "__fallback__") == 0)
            continue;
          react_ir_gen_ir_node(sb, c, depth + 1, ctx);
        }
        react_ir_sb_indent(sb, depth);
        react_ir_sb_append(sb, "</Suspense>\n");
      } else if (react_ir_ir_hook_is(node, "empty")) {
        const char *cond = node->value ? node->value : "true";
        react_ir_sb_indent(sb, depth);
        react_ir_sb_appendf(sb, "{(%s) ? (\n", cond);
        react_ir_sb_indent(sb, depth + 1);
        react_ir_sb_append(sb, "<>\n");
        for (size_t i = 0; i < node->n_kids; i++)
          react_ir_gen_ir_node(sb, node->kids[i], depth + 2, ctx);
        react_ir_sb_indent(sb, depth + 1);
        react_ir_sb_append(sb, "</>\n");
        react_ir_sb_indent(sb, depth);
        react_ir_sb_append(sb, ") : null}\n");
      } else if (react_ir_ir_hook_is(node, "portal")) {
        if (ctx) ctx->use_portal = 1;
        react_ir_sb_indent(sb, depth);
        react_ir_sb_append(sb, "{createPortal(\n");
        react_ir_sb_indent(sb, depth + 1);
        react_ir_sb_append(sb, "<>\n");
        for (size_t i = 0; i < node->n_kids; i++)
          react_ir_gen_ir_node(sb, node->kids[i], depth + 2, ctx);
        react_ir_sb_indent(sb, depth + 1);
        react_ir_sb_append(sb, "</>,\n");
        react_ir_sb_indent(sb, depth + 1);
        react_ir_sb_appendf(sb, "%s\n", node->value ? node->value : "document.body");
        react_ir_sb_indent(sb, depth);
        react_ir_sb_append(sb, ")}\n");
      } else if (react_ir_ir_hook_is(node, "errorBoundary")) {
        if (ctx) ctx->use_error_boundary = 1;
        IrNode *fb_node = NULL;
        for (size_t i = 0; i < node->n_kids; i++) {
          IrNode *c = node->kids[i];
          if (c->kind == IR_ELEMENT && c->name &&
              strcmp(c->name, "__fallback__") == 0)
            fb_node = c;
        }
        react_ir_sb_indent(sb, depth);
        if (fb_node) {
          react_ir_sb_append(sb, "<ErrorBoundary fallback={(\n");
          react_ir_sb_indent(sb, depth + 1);
          react_ir_sb_append(sb, "<>\n");
          for (size_t i = 0; i < fb_node->n_kids; i++)
            react_ir_gen_ir_node(sb, fb_node->kids[i], depth + 2, ctx);
          react_ir_sb_indent(sb, depth + 1);
          react_ir_sb_append(sb, "</>\n");
          react_ir_sb_indent(sb, depth);
          react_ir_sb_append(sb, ")}>\n");
        } else {
          /* Default UI lives in ErrorBoundary.jsx (message + stack in DEV) */
          react_ir_sb_append(sb, "<ErrorBoundary>\n");
        }
        for (size_t i = 0; i < node->n_kids; i++) {
          IrNode *c = node->kids[i];
          if (c->kind == IR_ELEMENT && c->name &&
              strcmp(c->name, "__fallback__") == 0)
            continue;
          react_ir_gen_ir_node(sb, c, depth + 1, ctx);
        }
        react_ir_sb_indent(sb, depth);
        react_ir_sb_append(sb, "</ErrorBoundary>\n");
      }
      break;
    case IR_INTERP:
      react_ir_gen_ir_interpolation(sb, node, depth);
      break;
    case IR_TEXT:
      if (node->value && strcmp(node->value, "__else__") != 0) {
        react_ir_sb_indent(sb, depth);
        if (interp_has(node->value)) {
          char *body = interp_to_js_template_body(node->value);
          react_ir_sb_appendf(sb, "{`%s`}\n", body ? body : "");
          free(body);
        } else {
          char *plain = interp_plain_text(node->value);
          react_ir_sb_append(sb, "{'");
          if (plain) {
            for (const char *p = plain; *p; p++) {
              if (*p == '\'' || *p == '\\') react_ir_sb_append(sb, "\\");
              char ch[2] = {*p, 0};
              react_ir_sb_append(sb, ch);
            }
          }
          react_ir_sb_append(sb, "'}\n");
          free(plain);
        }
      }
      break;
    case IR_IF:
    case IR_FOR:
      /* handled as siblings in gen_ir_children; still emit if top-level */
      {
        IrNode fake;
        memset(&fake, 0, sizeof(fake));
        IrNode *kids[1] = {node};
        fake.kids = kids;
        fake.n_kids = 1;
        react_ir_gen_ir_children(sb, &fake, depth, ctx);
      }
      break;
    default:
      react_ir_gen_ir_children(sb, node, depth, ctx);
      break;
  }
}
