#include "adapters/outbound/backends/vue/vue_ir_internal.h"

void vue_gen_element_ir(StrBuf *sb, IrNode *node, int depth, int is_layout) {
  const char *tag = node->name ? node->name : "div";

  if (strcmp(tag, "slot") == 0 || node->kind == IR_SLOT) {
    vue_sb_indent(sb, depth);
    vue_sb_append(sb, "<slot />\n");
    return;
  }

  if (strcmp(tag, "provide") == 0) {
    vue_gen_children_ir(sb, node, depth, is_layout);
    return;
  }

  {
    const char *bridge = irw_cord_bridge_name(tag);
    if (bridge) {
      vue_sb_indent(sb, depth);
      vue_sb_appendf(sb, "<%s", bridge);
      int has_kids = 0;
      for (size_t i = 0; i < node->n_kids; i++) {
        IrNode *c = node->kids[i];
        if (c->kind == IR_ATTR && c->name &&
            !(c->name[0] == '_' && c->name[1] == '_')) {
          if (vue_is_style_attr(c->name) && strcmp(c->name, "size") != 0)
            continue;
          if (c->value && (irw_looks_number(c->value) || irw_looks_bool(c->value) ||
              vue_looks_like_js_expr(c->value))) {
            vue_sb_appendf(sb, " :%s=", c->name);
            vue_emit_vue_prop_value(sb, c->value);
          } else if (vue_attr_is_true(c) && strcmp(c->name, "fade") == 0) {
            vue_sb_append(sb, " :fade=\"true\"");
          } else {
            vue_sb_appendf(sb, " %s=", c->name);
            vue_emit_vue_prop_value(sb, c->value);
          }
        } else if (vue_is_markup_child_kind(c->kind))
          has_kids = 1;
      }
      if (!has_kids) {
        vue_sb_append(sb, " />\n");
        return;
      }
      vue_sb_append(sb, ">\n");
      vue_gen_children_ir(sb, node, depth + 1, is_layout);
      vue_sb_indent(sb, depth);
      vue_sb_appendf(sb, "</%s>\n", bridge);
      return;
    }
  }

  if (irw_is_pascal(tag)) {
    vue_sb_indent(sb, depth);
    vue_sb_appendf(sb, "<%s", tag);
    int has_kids = 0;
    for (size_t i = 0; i < node->n_kids; i++) {
      IrNode *c = node->kids[i];
      if (c->kind == IR_ATTR && c->name && !vue_is_style_attr(c->name) &&
          !(c->name[0] == '_' && c->name[1] == '_')) {
        if (c->value && (irw_looks_number(c->value) || irw_looks_bool(c->value) ||
            vue_looks_like_js_expr(c->value))) {
          vue_sb_appendf(sb, " :%s=", c->name);
          vue_emit_vue_prop_value(sb, c->value);
        } else {
          vue_sb_appendf(sb, " %s=", c->name);
          vue_emit_vue_prop_value(sb, c->value);
        }
      } else if (c->kind == IR_EVENT && c->name && c->value) {
        int needs_arrow = strchr(c->value, '(') || strchr(c->value, '+') ||
                          strchr(c->value, '-') || strchr(c->value, ' ');
        if (needs_arrow)
          vue_sb_appendf(sb, " @%s=\"() => %s\"", c->name, c->value);
        else
          vue_sb_appendf(sb, " @%s=\"%s\"", c->name, c->value);
      } else if (vue_is_markup_child_kind(c->kind))
        has_kids = 1;
    }
    if (!has_kids) {
      vue_sb_append(sb, " />\n");
      return;
    }
    vue_sb_append(sb, ">\n");
    vue_gen_children_ir(sb, node, depth + 1, is_layout);
    vue_sb_indent(sb, depth);
    vue_sb_appendf(sb, "</%s>\n", tag);
    return;
  }

  const char *html = irw_html_tag(tag);
  if (!html) {
    vue_gen_children_ir(sb, node, depth, is_layout);
    return;
  }
  if (strcmp(tag, "link") == 0) html = "router-link";

  int self_close = (strcmp(html, "img") == 0 || strcmp(html, "input") == 0);
  vue_sb_indent(sb, depth);
  vue_sb_appendf(sb, "<%s", html);

  char classes[2048];
  vue_collect_classes_ir(classes, sizeof(classes), node, irw_base_class(tag));
  {
    char *cls = classes;
    while (*cls == ' ') cls++;
    if (*cls) {
      char *esc = js_escape_dq_dup(cls);
      vue_sb_appendf(sb, " class=\"%s\"", esc ? esc : "");
      free(esc);
    }
  }

  if (strcmp(tag, "link") == 0) {
    const char *to = "/";
    for (size_t i = 0; i < node->n_kids; i++) {
      IrNode *c = node->kids[i];
      if (c->kind == IR_ATTR && c->name &&
          (strcmp(c->name, "to") == 0 || strcmp(c->name, "href") == 0) &&
          c->value)
        to = c->value;
    }
    if (!url_href_is_safe(to)) to = "/";
    char *esc = js_escape_dq_dup(to);
    vue_sb_appendf(sb, " :to=\"%s\"", esc ? esc : "/");
    free(esc);
  }

  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *c = node->kids[i];
    if (c->kind != IR_ATTR || !c->name) continue;
    if (c->name[0] == '_' && c->name[1] == '_') continue;
    if (vue_is_style_attr(c->name)) continue;
    if (strcmp(c->name, "to") == 0 || strcmp(c->name, "href") == 0) continue;
    /* bool class flags already in class= */
    if (vue_attr_is_true(c) &&
        (strcmp(c->name, "between") == 0 || strcmp(c->name, "center") == 0 ||
         strcmp(c->name, "bold") == 0 || strcmp(c->name, "muted") == 0 ||
         strcmp(c->name, "sticky") == 0 || strcmp(c->name, "primary") == 0 ||
         strcmp(c->name, "outline") == 0 || strcmp(c->name, "ghost") == 0 ||
         strcmp(c->name, "font-mono") == 0 || strcmp(c->name, "flex-1") == 0 ||
         strcmp(c->name, "border") == 0 ||
         strcmp(c->name, "xl") == 0 || strcmp(c->name, "2xl") == 0 ||
         strcmp(c->name, "4xl") == 0 || strcmp(c->name, "lg") == 0 ||
         strcmp(c->name, "sm") == 0 || strcmp(c->name, "3xl") == 0 ||
         strcmp(c->name, "xs") == 0 || strcmp(c->name, "required") == 0 ||
         strcmp(c->name, "disabled") == 0 || strcmp(c->name, "text") == 0 ||
         strcmp(c->name, "email") == 0 || strcmp(c->name, "password") == 0 ||
         strcmp(c->name, "search") == 0))
      continue;
    if (strcmp(c->name, "bind") == 0 && c->value) {
      vue_sb_appendf(sb, " v-model=\"%s\"", c->value);
      {
        int has_name = 0;
        for (size_t j = 0; j < node->n_kids; j++) {
          IrNode *a = node->kids[j];
          if (a->kind == IR_ATTR && a->name && strcmp(a->name, "name") == 0)
            has_name = 1;
        }
        if (!has_name && !strchr(c->value, '.'))
          vue_sb_appendf(sb, " name=\"%s\"", c->value);
      }
      continue;
    }
    if (strcmp(c->name, "ref") == 0 && c->value) {
      vue_sb_appendf(sb, " ref=\"%s\"", c->value);
      continue;
    }
    if (strcmp(c->name, "action") == 0 && c->value) {
      vue_sb_appendf(sb, " onsubmit={%s}", c->value);
      continue;
    }
    if (vue_is_vue_directive_attr(c->name) && c->value) {
      vue_emit_vue_directive(sb, c->name, c->value);
      continue;
    }
    if (strcmp(c->name, "src") == 0 || strcmp(c->name, "alt") == 0 ||
        strcmp(c->name, "placeholder") == 0 || strcmp(c->name, "type") == 0 ||
        strcmp(c->name, "name") == 0 || strcmp(c->name, "id") == 0 ||
        strcmp(c->name, "rows") == 0) {
      if (c->value && vue_looks_like_js_expr(c->value) && strchr(c->value, '.'))
        vue_sb_appendf(sb, " %s={%s}", c->name, c->value);
      else {
        char *esc = js_escape_dq_dup(c->value ? c->value : "");
        vue_sb_appendf(sb, " %s=\"%s\"", c->name, esc ? esc : "");
        free(esc);
      }
    }
  }

  if (strcmp(tag, "checkbox") == 0) vue_sb_append(sb, " type=\"checkbox\"");
  if (strcmp(tag, "input") == 0) {
    int has_type = 0;
    for (size_t i = 0; i < node->n_kids; i++) {
      IrNode *c = node->kids[i];
      if (c->kind == IR_ATTR && c->name && strcmp(c->name, "type") == 0)
        has_type = 1;
      if (c->kind == IR_ATTR && c->name && vue_attr_is_true(c)) {
        if (strcmp(c->name, "text") == 0 || strcmp(c->name, "email") == 0 ||
            strcmp(c->name, "password") == 0 ||
            strcmp(c->name, "search") == 0) {
          if (!has_type) {
            vue_sb_appendf(sb, " type=\"%s\"", c->name);
            has_type = 1;
          }
        }
      }
    }
  }

  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *c = node->kids[i];
    if (c->kind == IR_EVENT && c->name && c->value) {
      int needs_arrow = strchr(c->value, '(') || strchr(c->value, '+') ||
                        strchr(c->value, '-') || strchr(c->value, ' ');
      if (strcmp(c->name, "submit") == 0) {
        if (needs_arrow)
          vue_sb_appendf(sb, " @submit.prevent=\"() => { %s; }\"", c->value);
        else
          vue_sb_appendf(sb, " @submit.prevent=\"%s\"", c->value);
      } else if (needs_arrow) {
        vue_sb_appendf(sb, " @%s=\"() => %s\"", c->name, c->value);
      } else {
        vue_sb_appendf(sb, " @%s=\"%s\"", c->name, c->value);
      }
    }
  }

  if (self_close) {
    vue_sb_append(sb, " />\n");
    return;
  }
  vue_sb_append(sb, ">\n");

  /* implicit expr children from unknown bool attrs (legacy parity) */
  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *c = node->kids[i];
    if (c->kind == IR_ATTR && c->name && vue_attr_is_true(c)) {
      const char *v = c->name;
      if (strcmp(v, "between") && strcmp(v, "center") && strcmp(v, "bold") &&
          strcmp(v, "muted") && strcmp(v, "sticky") && strcmp(v, "primary") &&
          strcmp(v, "outline") && strcmp(v, "ghost") && strcmp(v, "required") &&
          strcmp(v, "disabled") && strcmp(v, "text") && strcmp(v, "email") &&
          strcmp(v, "search") && strcmp(v, "password") && strcmp(v, "xl") &&
          strcmp(v, "2xl") && strcmp(v, "3xl") && strcmp(v, "4xl") &&
          strcmp(v, "lg") && strcmp(v, "sm") && strcmp(v, "xs") &&
          !(v[0] == '_' && v[1] == '_')) {
        vue_sb_indent(sb, depth + 1);
        vue_sb_appendf(sb, "{{ %s }}\n", v);
      }
    }
  }

  vue_gen_children_ir(sb, node, depth + 1, is_layout);
  vue_sb_indent(sb, depth);
  vue_sb_appendf(sb, "</%s>\n", html);
}
