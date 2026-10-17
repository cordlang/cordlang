#include "html_backend_internal.h"

int htmlb_is_style_attr(const char *name) {
  return strcmp(name, "variant") == 0 || strcmp(name, "size") == 0 ||
         strcmp(name, "color") == 0 || strcmp(name, "gap") == 0 ||
         strcmp(name, "cols") == 0 || strcmp(name, "p") == 0 ||
         strcmp(name, "px") == 0 || strcmp(name, "py") == 0 ||
         strcmp(name, "m") == 0 || strcmp(name, "mx") == 0 ||
         strcmp(name, "my") == 0 || strcmp(name, "w") == 0 ||
         strcmp(name, "h") == 0 || strcmp(name, "min-h") == 0 ||
         strcmp(name, "border") == 0 || strcmp(name, "flex-1") == 0 ||
         strcmp(name, "font-mono") == 0 || strcmp(name, "center") == 0 ||
         strcmp(name, "between") == 0 || strcmp(name, "bold") == 0 ||
         strcmp(name, "muted") == 0 || strcmp(name, "sticky") == 0 ||
         strcmp(name, "bg") == 0 || strcmp(name, "shadow") == 0 ||
         strcmp(name, "rounded") == 0 || strcmp(name, "max-w") == 0 ||
         strcmp(name, "overflow") == 0 || strcmp(name, "fit") == 0 ||
         strcmp(name, "aspect") == 0 || strcmp(name, "lines") == 0;
}

void htmlb_gen_element(StrBuf *sb, Node *node, int depth) {
  const char *tag = node->value ? node->value : "div";
  const char *html_tag = htmlb_html_tag_for(tag);
  const char *base_class = htmlb_tag_to_div_plus_class(tag);

  if (!html_tag) {
    htmlb_gen_children(sb, node, depth);
    return;
  }

  htmlb_sb_indent(sb, depth);

  int self_closing = irw_is_void_html(html_tag);

  htmlb_sb_appendf(sb, "<%s", html_tag);

  char classes[2048];
  htmlb_collect_classes(classes, sizeof(classes), node, base_class);
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
  for (size_t i = 0; i < node->children_len; i++) {
    Node *child = node->children[i];
    if (child->type == NODE_ATTR && child->value && !htmlb_is_style_attr(child->value)) {
      const char *attr_name = child->value;
      if (strcmp(attr_name, "to") == 0) attr_name = "href";
      if (strcmp(attr_name, "src") == 0 || strcmp(attr_name, "alt") == 0 ||
          strcmp(attr_name, "href") == 0 || strcmp(attr_name, "placeholder") == 0 ||
          strcmp(attr_name, "type") == 0 || strcmp(attr_name, "rows") == 0 ||
          strcmp(attr_name, "name") == 0 || strcmp(attr_name, "value") == 0 ||
          strcmp(attr_name, "id") == 0) {
        const char *aval = child->value2 ? child->value2 : "";
        if (strcmp(attr_name, "href") == 0 && !htmlb_href_is_safe(aval))
          aval = "#";
        htmlb_sb_appendf(sb, " %s=\"", attr_name);
        htmlb_html_escape_append(sb, aval);
        htmlb_sb_append(sb, "\"");
      }
    }
  }

  /* checkbox / radio type */
  if (strcmp(tag, "checkbox") == 0) htmlb_sb_append(sb, " type=\"checkbox\"");
  if (strcmp(tag, "radio") == 0) htmlb_sb_append(sb, " type=\"radio\"");
  if (strcmp(tag, "input") == 0) {
    int has_type = 0;
    for (size_t i = 0; i < node->children_len; i++) {
      Node *c = node->children[i];
      if (c->type == NODE_ATTR && c->value && strcmp(c->value, "type") == 0) has_type = 1;
      /* shorthand: input text / input email as first child text? handled as attrs elsewhere */
      if (c->type == NODE_BOOL_ATTR && c->value) {
        if (strcmp(c->value, "text") == 0 || strcmp(c->value, "email") == 0 ||
            strcmp(c->value, "password") == 0 || strcmp(c->value, "search") == 0 ||
            strcmp(c->value, "number") == 0) {
          if (!has_type) {
            htmlb_sb_appendf(sb, " type=\"%s\"", c->value);
            has_type = 1;
          }
        }
      }
    }
  }

  for (size_t i = 0; i < node->children_len; i++) {
    Node *child = node->children[i];
    if (child->type == NODE_BOOL_ATTR && child->value) {
      if (strcmp(child->value, "required") == 0 || strcmp(child->value, "disabled") == 0 ||
          strcmp(child->value, "readonly") == 0 || strcmp(child->value, "checked") == 0) {
        htmlb_sb_appendf(sb, " %s", child->value);
      }
    }
  }

  /* Events → lightweight preview handlers */
  for (size_t i = 0; i < node->children_len; i++) {
    Node *child = node->children[i];
    if (child->type == NODE_EVENT && child->value && child->value2) {
      const char *event = child->value;
      const char *handler = child->value2;
      /* only [A-Za-z][A-Za-z0-9_]* event names (onclick → click via parser) */
      if (!htmlb_is_safe_js_ident(event)) continue;
      /* escape single quotes in handler name for JS string */
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
  htmlb_gen_children(sb, node, depth + 1);
  htmlb_sb_indent(sb, depth);
  htmlb_sb_appendf(sb, "</%s>\n", html_tag);
}
