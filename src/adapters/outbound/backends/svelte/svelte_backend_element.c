#include "adapters/outbound/backends/svelte/svelte_backend_internal.h"

void svelte_gen_element_ir(StrBuf *sb, IrNode *node, int depth, int is_layout) {
  const char *tag = node->name ? node->name : "div";

  if (strcmp(tag, "slot") == 0 || node->kind == IR_SLOT) {
    svelte_sb_indent(sb, depth);
    svelte_sb_append(sb, "{@render children?.()}\n");
    return;
  }

  if (strcmp(tag, "provide") == 0) {
    svelte_gen_children_ir(sb, node, depth, is_layout);
    return;
  }

  /* Capability bridges (icon / motion / chart) */
  {
    const char *bridge = irw_cord_bridge_name(tag);
    if (bridge) {
      svelte_sb_indent(sb, depth);
      svelte_sb_appendf(sb, "<%s", bridge);
      int has_kids = 0;
      for (size_t i = 0; i < node->n_kids; i++) {
        IrNode *c = node->kids[i];
        if (c->kind == IR_ATTR && c->name &&
            !(c->name[0] == '_' && c->name[1] == '_')) {
          /* size is a prop for CordIcon, not a Tailwind class */
          if (svelte_is_style_attr(c->name) && strcmp(c->name, "size") != 0 &&
              strcmp(c->name, "fade") != 0)
            continue;
          if (svelte_attr_is_true(c) &&
              (strcmp(c->name, "fade") == 0 || strcmp(c->name, "center") == 0 ||
               strcmp(c->name, "bold") == 0 || strcmp(c->name, "muted") == 0)) {
            if (strcmp(c->name, "fade") == 0)
              svelte_sb_append(sb, " fade={true}");
            continue;
          }
          /* Icon/chart string props: quote identifiers as literals */
          if ((strcmp(c->name, "name") == 0 || strcmp(c->name, "type") == 0) &&
              c->value && !irw_looks_number(c->value) && !irw_looks_bool(c->value) &&
              strchr(c->value, '.') == NULL) {
            svelte_sb_appendf(sb, " %s=\"%s\"", c->name, c->value);
            continue;
          }
          svelte_sb_appendf(sb, " %s=", c->name);
          svelte_emit_svelte_prop_value(sb, c->value);
        } else if (svelte_is_markup_child_kind(c->kind))
          has_kids = 1;
      }
      if (!has_kids) {
        svelte_sb_append(sb, " />\n");
        return;
      }
      svelte_sb_append(sb, ">\n");
      svelte_gen_children_ir(sb, node, depth + 1, is_layout);
      svelte_sb_indent(sb, depth);
      svelte_sb_appendf(sb, "</%s>\n", bridge);
      return;
    }
  }

  if (irw_is_pascal(tag)) {
    svelte_sb_indent(sb, depth);
    svelte_sb_appendf(sb, "<%s", tag);
    int has_kids = 0;
    for (size_t i = 0; i < node->n_kids; i++) {
      IrNode *c = node->kids[i];
      if (c->kind == IR_ATTR && c->name && !svelte_is_style_attr(c->name) &&
          !(c->name[0] == '_' && c->name[1] == '_')) {
        svelte_sb_appendf(sb, " %s=", c->name);
        svelte_emit_svelte_prop_value(sb, c->value);
      } else if (c->kind == IR_EVENT && c->name && c->value) {
        int needs_arrow = strchr(c->value, '(') || strchr(c->value, '+') ||
                          strchr(c->value, '-') || strchr(c->value, ' ');
        if (needs_arrow)
          svelte_sb_appendf(sb, " on%s={() => %s}", c->name, c->value);
        else
          svelte_sb_appendf(sb, " on%s={%s}", c->name, c->value);
      } else if (svelte_is_markup_child_kind(c->kind))
        has_kids = 1;
    }
    if (!has_kids) {
      svelte_sb_append(sb, " />\n");
      return;
    }
    svelte_sb_append(sb, ">\n");
    svelte_gen_children_ir(sb, node, depth + 1, is_layout);
    svelte_sb_indent(sb, depth);
    svelte_sb_appendf(sb, "</%s>\n", tag);
    return;
  }

  const char *html = irw_html_tag(tag);
  if (!html) {
    svelte_gen_children_ir(sb, node, depth, is_layout);
    return;
  }

  int self_close = (strcmp(html, "img") == 0 || strcmp(html, "input") == 0);
  svelte_sb_indent(sb, depth);
  svelte_sb_appendf(sb, "<%s", html);

  char classes[2048];
  svelte_collect_classes_ir(classes, sizeof(classes), node, irw_base_class(tag));
  {
    char *cls = classes;
    while (*cls == ' ') cls++;
    if (*cls) {
      char *esc = js_escape_dq_dup(cls);
      svelte_sb_appendf(sb, " class=\"%s\"", esc ? esc : "");
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
    if (to[0] == '/')
      svelte_sb_appendf(sb, " href=\"#%s\"", esc ? esc : "/");
    else
      svelte_sb_appendf(sb, " href=\"#/%s\"", esc ? esc : "/");
    free(esc);
  }

  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *c = node->kids[i];
    if (c->kind != IR_ATTR || !c->name) continue;
    if (c->name[0] == '_' && c->name[1] == '_') continue;
    if (svelte_is_style_attr(c->name)) continue;
    if (strcmp(c->name, "to") == 0 || strcmp(c->name, "href") == 0) continue;
    /* bool class flags already in class= */
    if (svelte_attr_is_true(c) &&
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
      svelte_sb_appendf(sb, " bind:value={%s}", c->value);
      {
        int has_name = 0;
        for (size_t j = 0; j < node->n_kids; j++) {
          IrNode *a = node->kids[j];
          if (a->kind == IR_ATTR && a->name && strcmp(a->name, "name") == 0)
            has_name = 1;
        }
        if (!has_name && !strchr(c->value, '.'))
          svelte_sb_appendf(sb, " name=\"%s\"", c->value);
      }
      continue;
    }
    if (strcmp(c->name, "ref") == 0 && c->value) {
      svelte_sb_appendf(sb, " bind:this={%s}", c->value);
      continue;
    }
    if (strcmp(c->name, "action") == 0 && c->value) {
      svelte_sb_appendf(sb, " onsubmit={%s}", c->value);
      continue;
    }
    if (svelte_is_svelte_directive_attr(c->name) && c->value) {
      svelte_emit_svelte_directive(sb, c->name, c->value);
      continue;
    }
    if (strcmp(c->name, "src") == 0 || strcmp(c->name, "alt") == 0 ||
        strcmp(c->name, "placeholder") == 0 || strcmp(c->name, "type") == 0 ||
        strcmp(c->name, "name") == 0 || strcmp(c->name, "id") == 0 ||
        strcmp(c->name, "rows") == 0) {
      if (c->value && svelte_looks_like_js_expr(c->value) && strchr(c->value, '.'))
        svelte_sb_appendf(sb, " %s={%s}", c->name, c->value);
      else {
        char *esc = js_escape_dq_dup(c->value ? c->value : "");
        svelte_sb_appendf(sb, " %s=\"%s\"", c->name, esc ? esc : "");
        free(esc);
      }
    }
  }

  if (strcmp(tag, "checkbox") == 0) svelte_sb_append(sb, " type=\"checkbox\"");
  if (strcmp(tag, "input") == 0) {
    int has_type = 0;
    for (size_t i = 0; i < node->n_kids; i++) {
      IrNode *c = node->kids[i];
      if (c->kind == IR_ATTR && c->name && strcmp(c->name, "type") == 0)
        has_type = 1;
      if (c->kind == IR_ATTR && c->name && svelte_attr_is_true(c)) {
        if (strcmp(c->name, "text") == 0 || strcmp(c->name, "email") == 0 ||
            strcmp(c->name, "password") == 0 ||
            strcmp(c->name, "search") == 0) {
          if (!has_type) {
            svelte_sb_appendf(sb, " type=\"%s\"", c->name);
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
          svelte_sb_appendf(sb, " onsubmit={(e) => { e.preventDefault(); %s; }}",
                     c->value);
        else
          svelte_sb_appendf(sb, " onsubmit={(e) => { e.preventDefault(); %s(e); }}",
                     c->value);
      } else if (needs_arrow) {
        svelte_sb_appendf(sb, " on%s={() => %s}", c->name, c->value);
      } else {
        svelte_sb_appendf(sb, " on%s={%s}", c->name, c->value);
      }
    }
  }

  if (self_close) {
    svelte_sb_append(sb, " />\n");
    return;
  }
  svelte_sb_append(sb, ">\n");

  /* implicit expr children from unknown bool attrs (legacy parity) */
  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *c = node->kids[i];
    if (c->kind == IR_ATTR && c->name && svelte_attr_is_true(c)) {
      const char *v = c->name;
      if (strcmp(v, "between") && strcmp(v, "center") && strcmp(v, "bold") &&
          strcmp(v, "muted") && strcmp(v, "sticky") && strcmp(v, "primary") &&
          strcmp(v, "outline") && strcmp(v, "ghost") && strcmp(v, "required") &&
          strcmp(v, "disabled") && strcmp(v, "text") && strcmp(v, "email") &&
          strcmp(v, "search") && strcmp(v, "password") && strcmp(v, "xl") &&
          strcmp(v, "2xl") && strcmp(v, "3xl") && strcmp(v, "4xl") &&
          strcmp(v, "lg") && strcmp(v, "sm") && strcmp(v, "xs") &&
          !(v[0] == '_' && v[1] == '_')) {
        svelte_sb_indent(sb, depth + 1);
        svelte_sb_appendf(sb, "{%s}\n", v);
      }
    }
  }

  svelte_gen_children_ir(sb, node, depth + 1, is_layout);
  svelte_sb_indent(sb, depth);
  svelte_sb_appendf(sb, "</%s>\n", html);
}
