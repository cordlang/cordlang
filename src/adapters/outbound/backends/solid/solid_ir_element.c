#include "adapters/outbound/backends/solid/solid_ir_internal.h"

void solid_ir_gen_ir_element(StrBuf *sb, IrNode *node, int depth, GenCtx *ctx) {
  const char *tag = node->name ? node->name : "div";

  if (solid_ir_gen_solid_component_element(sb, node, depth, ctx, tag)) return;

  if (solid_ir_gen_solid_special_element(sb, node, depth, ctx, tag)) return;

  const char *html_tag = solid_ir_html_tag_for(tag);
  const char *base_class = solid_ir_tag_to_div_plus_class(tag);

  if (!html_tag) {
    solid_ir_gen_ir_children(sb, node, depth, ctx);
    return;
  }

  int use_link = 0;
  if (strcmp(tag, "link") == 0) {
    if (ctx && ctx->use_router) {
      use_link = 1;
      html_tag = "Link";
    }
  }

  if (ctx) {
    if (strcmp(html_tag, "CordIcon") == 0) ctx->use_cord_icon = 1;
    if (strcmp(html_tag, "CordMotion") == 0) ctx->use_cord_motion = 1;
    if (strcmp(html_tag, "CordChart") == 0) ctx->use_cord_chart = 1;
  }

  solid_ir_sb_indent(sb, depth);
  int self_closing =
      (strcmp(html_tag, "img") == 0 || strcmp(html_tag, "input") == 0);
  solid_ir_sb_appendf(sb, "<%s", html_tag);

  char classes[2048];
  int skip_style_size = (strcmp(html_tag, "CordIcon") == 0 ||
                         strcmp(html_tag, "CordMotion") == 0 ||
                         strcmp(html_tag, "CordChart") == 0);
  if (!skip_style_size)
    solid_ir_collect_classes_ir(classes, sizeof(classes), node, base_class);
  else
    classes[0] = '\0';
  {
    char *cls = classes;
    while (*cls == ' ') cls++;
    if (*cls) solid_ir_sb_appendf(sb, " class=\"%s\"", cls);
  }

  solid_ir_gen_solid_dom_attributes(sb, node, tag, html_tag, use_link,
                           skip_style_size, ctx);

  if (strcmp(tag, "checkbox") == 0) solid_ir_sb_append(sb, " type=\"checkbox\"");
  if (strcmp(tag, "radio") == 0) solid_ir_sb_append(sb, " type=\"radio\"");
  if (strcmp(tag, "input") == 0) {
    int has_type = 0;
    for (size_t i = 0; i < node->n_kids; i++) {
      IrNode *c = node->kids[i];
      if (c->kind == IR_ATTR && c->name && strcmp(c->name, "type") == 0)
        has_type = 1;
      if (c->kind == IR_ATTR && c->name && solid_ir_attr_is_true(c->value)) {
        if (strcmp(c->name, "text") == 0 || strcmp(c->name, "email") == 0 ||
            strcmp(c->name, "password") == 0 ||
            strcmp(c->name, "search") == 0 || strcmp(c->name, "number") == 0) {
          if (!has_type) {
            solid_ir_sb_appendf(sb, " type=\"%s\"", c->name);
            has_type = 1;
          }
        }
      }
    }
  }

  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *child = node->kids[i];
    if (child->kind == IR_ATTR && child->name && solid_ir_attr_is_true(child->value)) {
      if (strcmp(child->name, "required") == 0 ||
          strcmp(child->name, "disabled") == 0 ||
          strcmp(child->name, "readonly") == 0 ||
          strcmp(child->name, "checked") == 0)
        solid_ir_sb_appendf(sb, " %s", child->name);
    }
  }

  if (ctx && ctx->action_pending[0]) {
    if ((strcmp(tag, "form") == 0 || strcmp(html_tag, "form") == 0) &&
        ctx->in_form_with_action) {
      int has_busy = 0;
      for (size_t i = 0; i < node->n_kids; i++) {
        IrNode *a = node->kids[i];
        if (a->kind == IR_ATTR && a->name &&
            (strcmp(a->name, "aria-busy") == 0 ||
             strcmp(a->name, "ariaBusy") == 0))
          has_busy = 1;
      }
      if (!has_busy)
        solid_ir_sb_appendf(sb, " aria-busy={%s}", ctx->action_pending);
    }
    if ((strcmp(tag, "btn") == 0 || strcmp(html_tag, "button") == 0) &&
        ctx->in_form_with_action) {
      int has_dis = 0;
      for (size_t i = 0; i < node->n_kids; i++) {
        IrNode *a = node->kids[i];
        if (a->kind == IR_ATTR && a->name && strcmp(a->name, "disabled") == 0)
          has_dis = 1;
      }
      if (!has_dis)
        solid_ir_sb_appendf(sb, " disabled={%s}", ctx->action_pending);
    }
  }

  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *child = node->kids[i];
    if (child->kind == IR_EVENT && child->name && child->value) {
      const char *event = child->name;
      const char *handler = child->value;
      char solid_event[64];
      solid_event[0] = 'o';
      solid_event[1] = 'n';
      solid_event[2] = (char)toupper((unsigned char)event[0]);
      strncpy(solid_event + 3, event + 1, sizeof(solid_event) - 4);
      solid_event[sizeof(solid_event) - 1] = '\0';
      if (strcmp(event, "submit") == 0) {
        if (solid_ir_needs_js_arrow(handler)) {
          solid_ir_sb_appendf(sb, " %s={(e) => { e.preventDefault(); ", solid_event);
          solid_ir_solid_emit_expr(sb, handler, ctx);
          solid_ir_sb_append(sb, "; }}");
        } else {
          solid_ir_sb_appendf(sb, " %s={(e) => { e.preventDefault(); ", solid_event);
          solid_ir_solid_emit_expr(sb, handler, ctx);
          solid_ir_sb_append(sb, "(e); }}");
        }
      } else if (solid_ir_needs_js_arrow(handler)) {
        solid_ir_sb_appendf(sb, " %s={() => ", solid_event);
        solid_ir_solid_emit_expr(sb, handler, ctx);
        solid_ir_sb_append(sb, "}");
      } else {
        solid_ir_sb_appendf(sb, " %s={", solid_event);
        solid_ir_solid_emit_expr(sb, handler, ctx);
        solid_ir_sb_append(sb, "}");
      }
    }
  }

  if (self_closing) {
    solid_ir_sb_append(sb, " />\n");
    return;
  }

  solid_ir_sb_append(sb, ">\n");

  /* Implicit children: bare identifiers (ATTR name=true) as content */
  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *child = node->kids[i];
    if (!child || child->kind != IR_ATTR || !child->name) continue;
    if (!solid_ir_attr_is_true(child->value)) continue;
    const char *v = child->name;
    if (solid_ir_is_style_bool_name(v) || solid_ir_is_style_attr_name(v) ||
        solid_ir_is_dom_a11y_or_data_attr(v) ||
        strcmp(v, "required") == 0 || strcmp(v, "disabled") == 0 ||
        strcmp(v, "readonly") == 0 || strcmp(v, "checked") == 0 ||
        strcmp(v, "text") == 0 || strcmp(v, "email") == 0 ||
        strcmp(v, "password") == 0 || strcmp(v, "search") == 0 ||
        strcmp(v, "number") == 0 || strcmp(v, "lazy") == 0 ||
        strcmp(v, "forwardRef") == 0 || strcmp(v, "__file__") == 0 ||
        strcmp(v, "style") == 0)
      continue;
    solid_ir_sb_indent(sb, depth + 1);
    solid_ir_sb_appendf(sb, "{%s}\n", v);
  }

  solid_ir_gen_ir_children(sb, node, depth + 1, ctx);
  if (ctx && (strcmp(tag, "form") == 0 || strcmp(html_tag, "form") == 0))
    ctx->in_form_with_action = 0;
  solid_ir_sb_indent(sb, depth);
  solid_ir_sb_appendf(sb, "</%s>\n", html_tag);
}

void solid_ir_gen_ir_children(StrBuf *sb, IrNode *node, int depth, GenCtx *ctx) {
  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *child = node->kids[i];
    if (!child) continue;
    if (child->kind == IR_FOR) {
      const char *var = child->name ? child->name : "item";
      const char *list = child->value ? child->value : "items";
      const char *key_expr = "idx";
      for (size_t j = 0; j < child->n_kids; j++) {
        IrNode *ch = child->kids[j];
        if (ch->kind == IR_ATTR && ch->name && strcmp(ch->name, "key") == 0 &&
            ch->value) {
          key_expr = ch->value;
          break;
        }
      }
      solid_ir_sb_indent(sb, depth);
      solid_ir_sb_appendf(sb, "{(%s || []).map((%s, idx) => (\n", list, var);
      solid_ir_sb_indent(sb, depth + 1);
      solid_ir_sb_appendf(sb, "<Fragment key={%s}>\n", key_expr);
      for (size_t j = 0; j < child->n_kids; j++) {
        IrNode *ch = child->kids[j];
        if (ch->kind == IR_ATTR || ch->kind == IR_EVENT) continue;
        solid_ir_gen_ir_node(sb, ch, depth + 2, ctx);
      }
      solid_ir_sb_indent(sb, depth + 1);
      solid_ir_sb_append(sb, "</Fragment>\n");
      solid_ir_sb_indent(sb, depth);
      solid_ir_sb_append(sb, "))}\n");
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
      solid_ir_sb_indent(sb, depth);
      if (has_else)
        solid_ir_sb_appendf(sb, "{%s ? (\n", cond);
      else
        solid_ir_sb_appendf(sb, "{%s && (\n", cond);
      solid_ir_sb_indent(sb, depth + 1);
      solid_ir_sb_append(sb, "<>\n");
      for (size_t j = 0; j < child->n_kids; j++)
        solid_ir_gen_ir_node(sb, child->kids[j], depth + 2, ctx);
      solid_ir_sb_indent(sb, depth + 1);
      solid_ir_sb_append(sb, "</>\n");
      if (has_else) {
        solid_ir_sb_indent(sb, depth);
        solid_ir_sb_append(sb, ") : (\n");
        solid_ir_sb_indent(sb, depth + 1);
        solid_ir_sb_append(sb, "<>\n");
        IrNode *else_node = node->kids[else_idx];
        for (size_t j = 0; j < else_node->n_kids; j++)
          solid_ir_gen_ir_node(sb, else_node->kids[j], depth + 2, ctx);
        solid_ir_sb_indent(sb, depth + 1);
        solid_ir_sb_append(sb, "</>\n");
        solid_ir_sb_indent(sb, depth);
        solid_ir_sb_append(sb, ")}\n");
        i = else_idx;
      } else {
        solid_ir_sb_indent(sb, depth);
        solid_ir_sb_append(sb, ")}\n");
      }
    } else if (child->kind == IR_SLOT) {
      solid_ir_sb_indent(sb, depth);
      if (ctx && ctx->is_layout) {
        solid_ir_sb_append(sb, "<Outlet />\n");
        if (ctx) ctx->use_router = 1;
      } else {
        solid_ir_sb_append(sb, "{children}\n");
        if (ctx) ctx->use_children = 1;
      }
    } else if (child->kind == IR_INTERP) {
      solid_ir_gen_ir_interpolation(sb, child, depth, ctx);
    } else if (child->kind == IR_TEXT) {
      if (child->value && strcmp(child->value, "__else__") == 0) continue;
      solid_ir_sb_indent(sb, depth);
      if (child->value && interp_has(child->value)) {
        char *body = interp_to_js_template_body(child->value);
        solid_ir_sb_appendf(sb, "{`%s`}\n", body ? body : "");
        free(body);
      } else {
        char *plain = interp_plain_text(child->value);
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
    } else if (child->kind != IR_ATTR && child->kind != IR_EVENT &&
               child->kind != IR_PROP && child->kind != IR_STATE &&
               child->kind != IR_COMPUTED && child->kind != IR_EFFECT &&
               child->kind != IR_COMPONENT && child->kind != IR_LAYOUT &&
               child->kind != IR_ROUTE && !solid_ir_is_decl_ir(child)) {
      solid_ir_gen_ir_node(sb, child, depth, ctx);
    }
  }
}
