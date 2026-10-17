#include "html_backend_internal.h"

void htmlb_gen_ir_element(StrBuf *sb, IrNode *node, int depth) {
  const char *tag = node->name ? node->name : "div";
  const char *html_tag = htmlb_html_tag_for(tag);
  const char *base_class = htmlb_tag_to_div_plus_class(tag);

  if (!html_tag) {
    htmlb_gen_ir_children(sb, node, depth);
    return;
  }

  htmlb_sb_indent(sb, depth);

  int self_closing = irw_is_void_html(html_tag);

  htmlb_sb_appendf(sb, "<%s", html_tag);

  char classes[2048];
  htmlb_collect_classes_ir(classes, sizeof(classes), node, base_class);
  {
    char *cls = classes;
    while (*cls == ' ') cls++;
    if (*cls) {
      htmlb_sb_append(sb, " class=\"");
      htmlb_html_escape_append(sb, cls);
      htmlb_sb_append(sb, "\"");
    }
  }

  /* Regular HTML attributes */
  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *child = node->kids[i];
    if (child && child->kind == IR_ATTR && child->name &&
        !htmlb_is_style_attr(child->name) && !htmlb_ir_attr_is_true(child->value) &&
        strcmp(child->name, "style") != 0 &&
        strcmp(child->name, "__file__") != 0 &&
        strcmp(child->name, "bind") != 0) {
      const char *attr_name = child->name;
      if (strcmp(attr_name, "to") == 0) attr_name = "href";
      if (strcmp(attr_name, "src") == 0 || strcmp(attr_name, "alt") == 0 ||
          strcmp(attr_name, "href") == 0 ||
          strcmp(attr_name, "placeholder") == 0 ||
          strcmp(attr_name, "type") == 0 || strcmp(attr_name, "rows") == 0 ||
          strcmp(attr_name, "name") == 0 || strcmp(attr_name, "value") == 0 ||
          strcmp(attr_name, "id") == 0) {
        htmlb_sb_appendf(sb, " %s=\"", attr_name);
        htmlb_html_escape_append(sb, child->value ? child->value : "");
        htmlb_sb_append(sb, "\"");
      }
    }
  }

  /* Two-way bind on form controls (preview): value + oninput → setField */
  {
    const char *bind_field = NULL;
    for (size_t i = 0; i < node->n_kids; i++) {
      IrNode *c = node->kids[i];
      if (c && c->kind == IR_ATTR && c->name && strcmp(c->name, "bind") == 0 &&
          c->value && htmlb_is_safe_js_ident(c->value)) {
        bind_field = c->value;
        break;
      }
    }
    if (bind_field &&
        (strcmp(html_tag, "input") == 0 || strcmp(html_tag, "textarea") == 0 ||
         strcmp(html_tag, "select") == 0)) {
      const char *init = htmlb_state_init_for(bind_field);
      /* strip quotes from string inits for value attr */
      htmlb_sb_append(sb, " value=\"");
      if (init && init[0] == '"') {
        size_t n = strlen(init);
        if (n >= 2)
          for (size_t k = 1; k + 1 < n; k++) {
            char c[2] = {init[k], 0};
            if (init[k] == '"')
              htmlb_sb_append(sb, "&quot;");
            else
              htmlb_sb_append(sb, c);
          }
      } else {
        htmlb_html_escape_append(sb, init ? init : "");
      }
      htmlb_sb_append(sb, "\"");
      htmlb_sb_append(sb, " data-bind-value=\"");
      htmlb_html_escape_append(sb, bind_field);
      htmlb_sb_append(sb, "\"");
      htmlb_sb_appendf(sb, " oninput=\"clBindInput(this, '%s')\"", bind_field);
    }
  }

  if (strcmp(tag, "checkbox") == 0) htmlb_sb_append(sb, " type=\"checkbox\"");
  if (strcmp(tag, "radio") == 0) htmlb_sb_append(sb, " type=\"radio\"");
  if (strcmp(tag, "input") == 0) {
    int has_type = 0;
    for (size_t i = 0; i < node->n_kids; i++) {
      IrNode *c = node->kids[i];
      if (c && c->kind == IR_ATTR && c->name && strcmp(c->name, "type") == 0)
        has_type = 1;
      if (c && c->kind == IR_ATTR && c->name && htmlb_ir_attr_is_true(c->value)) {
        if (strcmp(c->name, "text") == 0 || strcmp(c->name, "email") == 0 ||
            strcmp(c->name, "password") == 0 ||
            strcmp(c->name, "search") == 0 || strcmp(c->name, "number") == 0) {
          if (!has_type) {
            htmlb_sb_appendf(sb, " type=\"%s\"", c->name);
            has_type = 1;
          }
        }
      }
    }
  }

  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *child = node->kids[i];
    if (child && child->kind == IR_ATTR && child->name &&
        htmlb_ir_attr_is_true(child->value)) {
      if (strcmp(child->name, "required") == 0 ||
          strcmp(child->name, "disabled") == 0 ||
          strcmp(child->name, "readonly") == 0 ||
          strcmp(child->name, "checked") == 0) {
        htmlb_sb_appendf(sb, " %s", child->name);
      }
    }
  }

  /* Events → clPreviewHandler (setCount etc.) */
  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *child = node->kids[i];
    if (child && child->kind == IR_EVENT && child->name && child->value) {
      const char *event = child->name;
      const char *handler = child->value;
      if (!htmlb_is_safe_js_ident(event)) continue;
      htmlb_sb_appendf(sb, " on%s=\"clPreviewHandler('", event);
      for (const char *p = handler; *p; p++) {
        if (*p == '\'' || *p == '\\') htmlb_sb_append(sb, "\\");
        char c[2] = {*p, 0};
        if (*p != '\n' && *p != '\r') htmlb_sb_append(sb, c);
      }
      htmlb_sb_append(sb, "', event)\"");
    }
  }

  if (self_closing) {
    htmlb_sb_append(sb, " />\n");
    return;
  }

  htmlb_sb_append(sb, ">\n");
  htmlb_gen_ir_children(sb, node, depth + 1);
  htmlb_sb_indent(sb, depth);
  htmlb_sb_appendf(sb, "</%s>\n", html_tag);
}
