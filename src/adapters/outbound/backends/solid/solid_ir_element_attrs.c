#include "adapters/outbound/backends/solid/solid_ir_internal.h"

int solid_ir_gen_solid_component_element(StrBuf *sb, IrNode *node, int depth,
                                       GenCtx *ctx, const char *tag) {
  if (!solid_ir_is_pascal_case(tag)) return 0;

    solid_ir_sb_indent(sb, depth);
    solid_ir_sb_appendf(sb, "<%s", tag);
    for (size_t i = 0; i < node->n_kids; i++) {
      IrNode *child = node->kids[i];
      if (!child) continue;
      if (child->kind == IR_ATTR && child->name) {
        if (solid_ir_is_style_attr_name(child->name) || solid_ir_is_style_bool_name(child->name))
          continue;
        if (strcmp(child->name, "__file__") == 0) continue;
        if (solid_ir_attr_is_true(child->value) && solid_ir_is_style_bool_name(child->name))
          continue;
        if (solid_ir_attr_is_true(child->value) &&
            (strcmp(child->name, "bold") == 0 ||
             strcmp(child->name, "muted") == 0 ||
             strcmp(child->name, "center") == 0 ||
             strcmp(child->name, "between") == 0 ||
             strcmp(child->name, "sticky") == 0 ||
             strcmp(child->name, "primary") == 0 ||
             strcmp(child->name, "outline") == 0 ||
             strcmp(child->name, "ghost") == 0))
          continue;
        if (solid_ir_attr_is_true(child->value) && !solid_ir_is_style_attr_name(child->name) &&
            strcmp(child->name, "required") != 0 &&
            strcmp(child->name, "disabled") != 0 &&
            strcmp(child->name, "readonly") != 0 &&
            strcmp(child->name, "checked") != 0) {
          /* boolean prop without value for true style-like already skipped */
          if (solid_ir_looks_like_bool(child->value) || solid_ir_attr_is_true(child->value)) {
            /* bare bool prop */
            if (child->value && strcmp(child->value, "true") == 0 &&
                !solid_ir_is_style_attr_name(child->name) &&
                !solid_ir_is_style_bool_name(child->name)) {
              /* pass as boolean attribute for component props that are bools */
            }
          }
        }
        if (solid_ir_attr_is_true(child->value) && !child->value[0]) {
          solid_ir_sb_appendf(sb, " %s", child->name);
        } else if (child->value && strcmp(child->value, "true") == 0 &&
                   !strchr(child->name, '=')) {
          /* only emit bare bool if it looks like a prop flag, not style */
          if (!solid_ir_is_style_attr_name(child->name) &&
              !solid_ir_is_style_bool_name(child->name))
            solid_ir_sb_appendf(sb, " %s", child->name);
        } else if (!solid_ir_attr_is_true(child->value) ||
                   (child->value && strcmp(child->value, "true") != 0)) {
          solid_ir_sb_appendf(sb, " %s=", child->name);
          solid_ir_emit_jsx_value(sb, child->value, 0);
        } else if (child->value && strcmp(child->value, "false") == 0) {
          solid_ir_sb_appendf(sb, " %s={false}", child->name);
        }
      } else if (child->kind == IR_EVENT && child->name && child->value) {
        char solid_event[64];
        solid_event[0] = 'o';
        solid_event[1] = 'n';
        solid_event[2] = (char)toupper((unsigned char)child->name[0]);
        strncpy(solid_event + 3, child->name + 1, sizeof(solid_event) - 4);
        solid_event[sizeof(solid_event) - 1] = '\0';
        if (solid_ir_needs_js_arrow(child->value)) {
          solid_ir_sb_appendf(sb, " %s={() => ", solid_event);
          solid_ir_solid_emit_expr(sb, child->value, ctx);
          solid_ir_sb_append(sb, "}");
        } else {
          solid_ir_sb_appendf(sb, " %s={", solid_event);
          solid_ir_solid_emit_expr(sb, child->value, ctx);
          solid_ir_sb_append(sb, "}");
        }
      }
    }
    int has_kids = 0;
    for (size_t i = 0; i < node->n_kids; i++) {
      IrKind t = node->kids[i]->kind;
      if (t == IR_ELEMENT || t == IR_TEXT || t == IR_FOR || t == IR_IF ||
          t == IR_SLOT || t == IR_INTERP)
        has_kids = 1;
    }
    if (!has_kids) {
      solid_ir_sb_append(sb, " />\n");
      return 1;
    }
    solid_ir_sb_append(sb, ">\n");
    solid_ir_gen_ir_children(sb, node, depth + 1, ctx);
    solid_ir_sb_indent(sb, depth);
    solid_ir_sb_appendf(sb, "</%s>\n", tag);
    return 1;
    return 1;
}

int solid_ir_gen_solid_special_element(StrBuf *sb, IrNode *node, int depth,
                                     GenCtx *ctx, const char *tag) {
  if (strcmp(tag, "provide") == 0) {
    const char *ctx_name = "Context";
    const char *val = NULL;
    for (size_t i = 0; i < node->n_kids; i++) {
      IrNode *ch = node->kids[i];
      if (ch->kind == IR_ATTR && ch->name) {
        if (strcmp(ch->name, "context") == 0 && ch->value) ctx_name = ch->value;
        if (strcmp(ch->name, "value") == 0) val = ch->value;
      }
    }
    solid_ir_sb_indent(sb, depth);
    solid_ir_sb_appendf(sb, "<%s.Provider", ctx_name);
    if (val) {
      solid_ir_sb_append(sb, " value=");
      solid_ir_emit_jsx_value(sb, val, 1);
    }
    solid_ir_sb_append(sb, ">\n");
    solid_ir_gen_ir_children(sb, node, depth + 1, ctx);
    solid_ir_sb_indent(sb, depth);
    solid_ir_sb_appendf(sb, "</%s.Provider>\n", ctx_name);
    if (ctx) ctx->use_context = 1;
    return 1;
  }

  if (strcmp(tag, "slot") == 0 || node->kind == IR_SLOT) {
    solid_ir_sb_indent(sb, depth);
    if (ctx && ctx->is_layout) {
      solid_ir_sb_append(sb, "<Outlet />\n");
      ctx->use_router = 1;
    } else {
      solid_ir_sb_append(sb, "{children}\n");
      if (ctx) ctx->use_children = 1;
    }
    return 1;
  }  return 0;
}

void solid_ir_gen_solid_dom_attributes(StrBuf *sb, IrNode *node, const char *tag,
                                     const char *html_tag, int use_link,
                                     int skip_style_size, GenCtx *ctx) {
  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *child = node->kids[i];
    if (!child || child->kind != IR_ATTR || !child->name) continue;
    if (solid_ir_is_style_attr_name(child->name) &&
        !(skip_style_size &&
          (strcmp(child->name, "size") == 0 || strcmp(child->name, "fade") == 0)))
      continue;
    if (solid_ir_is_style_bool_name(child->name) && solid_ir_attr_is_true(child->value)) continue;
    if (strcmp(child->name, "__file__") == 0) continue;
    if (strcmp(child->name, "style") == 0) continue;

    /* skip bare bool style-ish and input type bools handled later */
    if (solid_ir_attr_is_true(child->value) &&
        (strcmp(child->name, "text") == 0 || strcmp(child->name, "email") == 0 ||
         strcmp(child->name, "password") == 0 ||
         strcmp(child->name, "search") == 0 ||
         strcmp(child->name, "number") == 0 ||
         strcmp(child->name, "required") == 0 ||
         strcmp(child->name, "disabled") == 0 ||
         strcmp(child->name, "readonly") == 0 ||
         strcmp(child->name, "checked") == 0))
      continue;

    /* bare identifier content (bool true non-style) skipped for attrs */
    if (solid_ir_attr_is_true(child->value) && !solid_ir_is_style_attr_name(child->name) &&
        !solid_ir_is_style_bool_name(child->name) &&
        !solid_ir_is_dom_a11y_or_data_attr(child->name) &&
        strcmp(child->name, "lazy") != 0 &&
        strcmp(child->name, "forwardRef") != 0 &&
        strcmp(child->name, "action") != 0 &&
        strcmp(child->name, "bind") != 0 && strcmp(child->name, "ref") != 0 &&
        strcmp(child->name, "to") != 0 && strcmp(child->name, "href") != 0 &&
        strcmp(child->name, "src") != 0 && strcmp(child->name, "alt") != 0 &&
        strcmp(child->name, "type") != 0 &&
        strcmp(child->name, "placeholder") != 0 &&
        strcmp(child->name, "name") != 0 && strcmp(child->name, "value") != 0 &&
        strcmp(child->name, "id") != 0 && strcmp(child->name, "key") != 0 &&
        strcmp(child->name, "rows") != 0 &&
        strcmp(child->name, "layout") != 0) {
      /* treat as content later, not attribute */
      continue;
    }

    if (use_link &&
        (strcmp(child->name, "to") == 0 || strcmp(child->name, "href") == 0)) {
      solid_ir_sb_append(sb, " to=");
      solid_ir_emit_jsx_value(sb, child->value ? child->value : "/", 0);
      continue;
    }
    if (strcmp(child->name, "to") == 0) {
      const char *href = child->value ? child->value : "/";
      if (!url_href_is_safe(href)) href = "#";
      char *esc = js_escape_dq_dup(href);
      solid_ir_sb_appendf(sb, " href=\"%s\"", esc ? esc : "#");
      free(esc);
      continue;
    }
    if (strcmp(child->name, "action") == 0 && child->value) {
      solid_ir_sb_appendf(sb, " action={%s}", child->value);
      if (ctx && ctx->action_pending[0] &&
          (strcmp(tag, "form") == 0 || strcmp(html_tag, "form") == 0)) {
        if (ctx->action_handler[0] &&
            strcmp(child->value, ctx->action_handler) == 0)
          ctx->in_form_with_action = 1;
        else {
          size_t al = strlen(child->value);
          if (al > 6 && strcmp(child->value + al - 6, "Action") == 0)
            ctx->in_form_with_action = 1;
        }
      }
    } else if (strcmp(child->name, "src") == 0 ||
               strcmp(child->name, "alt") == 0 ||
               strcmp(child->name, "href") == 0 ||
               strcmp(child->name, "placeholder") == 0 ||
               strcmp(child->name, "type") == 0 ||
               strcmp(child->name, "rows") == 0 ||
               strcmp(child->name, "name") == 0 ||
               strcmp(child->name, "value") == 0 ||
               strcmp(child->name, "id") == 0 ||
               strcmp(child->name, "key") == 0) {
      const char *an = child->name;
      if (child->value && interp_has(child->value)) {
        char *body = interp_to_js_template_body(child->value);
        solid_ir_sb_appendf(sb, " %s={`%s`}", an, body ? body : "");
        free(body);
      } else if (child->value && solid_ir_looks_like_js_expr(child->value) &&
                 strchr(child->value, '.')) {
        solid_ir_sb_appendf(sb, " %s={%s}", an, child->value);
      } else if (child->value && solid_ir_looks_like_number(child->value)) {
        solid_ir_sb_appendf(sb, " %s={%s}", an, child->value);
      } else {
        solid_ir_sb_appendf(sb, " %s=\"%s\"", an, child->value ? child->value : "");
      }
    } else if (strcmp(child->name, "bind") == 0 && child->value) {
      const char *bn = child->value;
      char setter[128];
      int has_name = 0;
      for (size_t j = 0; j < node->n_kids; j++) {
        IrNode *a = node->kids[j];
        if (a->kind == IR_ATTR && a->name && strcmp(a->name, "name") == 0)
          has_name = 1;
      }
      if (strchr(bn, '.')) {
        solid_ir_sb_appendf(sb, " value={%s}", bn);
        solid_ir_sb_appendf(sb, " onChange={(e) => { /* bind %s */ }}", bn);
      } else {
        snprintf(setter, sizeof(setter), "set%c%s",
                 (char)toupper((unsigned char)bn[0]), bn + 1);
        solid_ir_sb_appendf(sb, " value={%s}", bn);
        solid_ir_sb_appendf(sb, " onChange={(e) => %s(e.target.value)}", setter);
        if (!has_name) solid_ir_sb_appendf(sb, " name=\"%s\"", bn);
      }
    } else if (strcmp(child->name, "ref") == 0 && child->value) {
      solid_ir_sb_appendf(sb, " ref={%s}", child->value);
    } else if (solid_ir_is_dom_a11y_or_data_attr(child->name)) {
      char *esc = js_escape_dq_dup(child->value ? child->value : "");
      solid_ir_sb_appendf(sb, " %s=\"%s\"", child->name, esc ? esc : "");
      free(esc);
    } else {
      solid_ir_sb_appendf(sb, " %s=", child->name);
      solid_ir_emit_jsx_value(sb, child->value, 0);
    }
  }
}
