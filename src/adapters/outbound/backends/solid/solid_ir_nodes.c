#include "adapters/outbound/backends/solid/solid_ir_internal.h"

void solid_ir_gen_ir_interpolation(StrBuf *sb, IrNode *node, int depth, GenCtx *ctx) {
  if (!node) return;
  if (node->value && node->n_kids == 0) {
    solid_ir_sb_indent(sb, depth);
    solid_ir_sb_append(sb, "{");
    solid_ir_solid_emit_expr(sb, node->value, ctx);
    solid_ir_sb_append(sb, "}\n");
    return;
  }
  if (node->n_kids > 0) {
    solid_ir_sb_indent(sb, depth);
    solid_ir_sb_append(sb, "{`");
    for (size_t i = 0; i < node->n_kids; i++) {
      IrNode *c = node->kids[i];
      if (c->kind == IR_TEXT && c->value) {
        for (const char *p = c->value; *p; p++) {
          if (*p == '`' || *p == '\\') {
            solid_ir_sb_append(sb, "\\");
            char ch[2] = {*p, 0};
            solid_ir_sb_append(sb, ch);
          } else if (*p == '$' && p[1] == '{') {
            solid_ir_sb_append(sb, "\\$");
          } else {
            char ch[2] = {*p, 0};
            solid_ir_sb_append(sb, ch);
          }
        }
      } else if (c->kind == IR_INTERP && c->value) {
        solid_ir_sb_appendf(sb, "${%s}", c->value);
      }
    }
    solid_ir_sb_append(sb, "`}\n");
  }
}

void solid_ir_gen_ir_node(StrBuf *sb, IrNode *node, int depth, GenCtx *ctx) {
  if (!node) return;
  switch (node->kind) {
    case IR_ELEMENT:
      solid_ir_gen_ir_element(sb, node, depth, ctx);
      break;
    case IR_SLOT:
      solid_ir_sb_indent(sb, depth);
      if (ctx && ctx->is_layout) {
        solid_ir_sb_append(sb, "<Outlet />\n");
        ctx->use_router = 1;
      } else {
        solid_ir_sb_append(sb, "{children}\n");
        if (ctx) ctx->use_children = 1;
      }
      break;
    case IR_HOOK:
      if (solid_ir_ir_hook_is(node, "suspense") || solid_ir_ir_hook_is(node, "loading")) {
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
        solid_ir_sb_indent(sb, depth);
        solid_ir_sb_append(sb, "<Suspense fallback={");
        if (fb_node) {
          solid_ir_sb_append(sb, "(\n");
          solid_ir_sb_indent(sb, depth + 1);
          solid_ir_sb_append(sb, "<>\n");
          for (size_t i = 0; i < fb_node->n_kids; i++)
            solid_ir_gen_ir_node(sb, fb_node->kids[i], depth + 2, ctx);
          solid_ir_sb_indent(sb, depth + 1);
          solid_ir_sb_append(sb, "</>\n");
          solid_ir_sb_indent(sb, depth);
          solid_ir_sb_append(sb, ")");
        } else if (fb_text) {
          solid_ir_sb_appendf(sb, "<div>%s</div>", fb_text);
        } else {
          solid_ir_sb_append(sb, "<div>Loading...</div>");
        }
        solid_ir_sb_append(sb, "}>\n");
        for (size_t i = 0; i < node->n_kids; i++) {
          IrNode *c = node->kids[i];
          if (c->kind == IR_ATTR) continue;
          if (c->kind == IR_ELEMENT && c->name &&
              strcmp(c->name, "__fallback__") == 0)
            continue;
          solid_ir_gen_ir_node(sb, c, depth + 1, ctx);
        }
        solid_ir_sb_indent(sb, depth);
        solid_ir_sb_append(sb, "</Suspense>\n");
      } else if (solid_ir_ir_hook_is(node, "empty")) {
        const char *cond = node->value ? node->value : "true";
        solid_ir_sb_indent(sb, depth);
        solid_ir_sb_appendf(sb, "{(%s) ? (\n", cond);
        solid_ir_sb_indent(sb, depth + 1);
        solid_ir_sb_append(sb, "<>\n");
        for (size_t i = 0; i < node->n_kids; i++)
          solid_ir_gen_ir_node(sb, node->kids[i], depth + 2, ctx);
        solid_ir_sb_indent(sb, depth + 1);
        solid_ir_sb_append(sb, "</>\n");
        solid_ir_sb_indent(sb, depth);
        solid_ir_sb_append(sb, ") : null}\n");
      } else if (solid_ir_ir_hook_is(node, "portal")) {
        if (ctx) ctx->use_portal = 1;
        solid_ir_sb_indent(sb, depth);
        solid_ir_sb_append(sb, "{createPortal(\n");
        solid_ir_sb_indent(sb, depth + 1);
        solid_ir_sb_append(sb, "<>\n");
        for (size_t i = 0; i < node->n_kids; i++)
          solid_ir_gen_ir_node(sb, node->kids[i], depth + 2, ctx);
        solid_ir_sb_indent(sb, depth + 1);
        solid_ir_sb_append(sb, "</>,\n");
        solid_ir_sb_indent(sb, depth + 1);
        solid_ir_sb_appendf(sb, "%s\n", node->value ? node->value : "document.body");
        solid_ir_sb_indent(sb, depth);
        solid_ir_sb_append(sb, ")}\n");
      } else if (solid_ir_ir_hook_is(node, "errorBoundary")) {
        if (ctx) ctx->use_error_boundary = 1;
        IrNode *fb_node = NULL;
        for (size_t i = 0; i < node->n_kids; i++) {
          IrNode *c = node->kids[i];
          if (c->kind == IR_ELEMENT && c->name &&
              strcmp(c->name, "__fallback__") == 0)
            fb_node = c;
        }
        solid_ir_sb_indent(sb, depth);
        if (fb_node) {
          solid_ir_sb_append(sb, "<ErrorBoundary fallback={(\n");
          solid_ir_sb_indent(sb, depth + 1);
          solid_ir_sb_append(sb, "<>\n");
          for (size_t i = 0; i < fb_node->n_kids; i++)
            solid_ir_gen_ir_node(sb, fb_node->kids[i], depth + 2, ctx);
          solid_ir_sb_indent(sb, depth + 1);
          solid_ir_sb_append(sb, "</>\n");
          solid_ir_sb_indent(sb, depth);
          solid_ir_sb_append(sb, ")}>\n");
        } else {
          solid_ir_sb_append(sb, "<ErrorBoundary>\n");
        }
        for (size_t i = 0; i < node->n_kids; i++) {
          IrNode *c = node->kids[i];
          if (c->kind == IR_ELEMENT && c->name &&
              strcmp(c->name, "__fallback__") == 0)
            continue;
          solid_ir_gen_ir_node(sb, c, depth + 1, ctx);
        }
        solid_ir_sb_indent(sb, depth);
        solid_ir_sb_append(sb, "</ErrorBoundary>\n");
      }
      break;
    case IR_INTERP:
      solid_ir_gen_ir_interpolation(sb, node, depth, ctx);
      break;
    case IR_TEXT:
      if (node->value && strcmp(node->value, "__else__") != 0) {
        solid_ir_sb_indent(sb, depth);
        if (interp_has(node->value)) {
          char *body = interp_to_js_template_body(node->value);
          solid_ir_sb_appendf(sb, "{`%s`}\n", body ? body : "");
          free(body);
        } else {
          char *plain = interp_plain_text(node->value);
          solid_ir_sb_append(sb, "{'");
          if (plain) {
            for (const char *p = plain; *p; p++) {
              if (*p == '\'' || *p == '\\') solid_ir_sb_append(sb, "\\");
              char ch[2] = {*p, 0};
              solid_ir_sb_append(sb, ch);
            }
          }
          solid_ir_sb_append(sb, "'}\n");
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
        solid_ir_gen_ir_children(sb, &fake, depth, ctx);
      }
      break;
    default:
      solid_ir_gen_ir_children(sb, node, depth, ctx);
      break;
  }
}
