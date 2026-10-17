#include "html_backend_internal.h"

static StyleMap htmlb_style_mappings[] = {
  {"bg", "bg-", 1},
  {"text", "text-", 1},
  {"p", "p-", 1},
  {"px", "px-", 1},
  {"py", "py-", 1},
  {"m", "m-", 1},
  {"mx", "mx-", 1},
  {"my", "my-", 1},
  {"gap", "gap-", 1},
  {"rounded", "rounded-", 1},
  {"shadow", "shadow-", 1},
  {"size", "text-", 1},
  {"w", "w-", 1},
  {"h", "h-", 1},
  {"max-w", "max-w-", 1},
  {"z", "z-", 1},
  {"op", "opacity-", 1},
  {"leading", "leading-", 1},
  {"tracking", "tracking-", 1},
  {NULL, NULL, 0},
};

void htmlb_gen_style_classes(StrBuf *sb, Node *style_map) {
  for (size_t i = 0; i < style_map->children_len; i++) {
    Node *entry = style_map->children[i];
    if (entry->type != NODE_STYLE_ENTRY) continue;

    const char *key = entry->value ? entry->value : "";
    const char *val = entry->value2 ? entry->value2 : "";

    int mapped = 0;
    for (int s = 0; htmlb_style_mappings[s].style_key; s++) {
      if (strcmp(key, htmlb_style_mappings[s].style_key) == 0) {
        htmlb_sb_append(sb, " ");
        htmlb_sb_append(sb, htmlb_style_mappings[s].tw_prefix);
        if (htmlb_style_mappings[s].is_value_appended) htmlb_sb_append(sb, val);
        mapped = 1;
        break;
      }
    }

    if (!mapped) {
      if (strcmp(key, "between") == 0) htmlb_sb_append(sb, " flex justify-between");
      else if (strcmp(key, "center") == 0) htmlb_sb_append(sb, " flex items-center justify-center");
      else if (strcmp(key, "around") == 0) htmlb_sb_append(sb, " justify-around");
      else if (strcmp(key, "evenly") == 0) htmlb_sb_append(sb, " justify-evenly");
      else if (strcmp(key, "sticky") == 0) htmlb_sb_append(sb, " sticky top-0");
      else if (strcmp(key, "bold") == 0) htmlb_sb_append(sb, " font-bold");
      else if (strcmp(key, "muted") == 0) htmlb_sb_append(sb, " text-muted");
      else if (strcmp(key, "overflow") == 0) {
        htmlb_sb_append(sb, " overflow-");
        htmlb_sb_append(sb, val);
      }
    }
  }
}

const char *htmlb_tag_to_div_plus_class(const char *tag) {
  if (strcmp(tag, "col") == 0) return "flex flex-col";
  if (strcmp(tag, "row") == 0) return "flex flex-row";
  if (strcmp(tag, "stack") == 0) return "flex flex-col";
  if (strcmp(tag, "sidebar") == 0) return "flex flex-col";
  if (strcmp(tag, "grid") == 0) return "grid";
  if (strcmp(tag, "page") == 0) return "min-h-screen";
  if (strcmp(tag, "btn") == 0) return "btn";
  if (strcmp(tag, "card") == 0) return "card";
  return NULL;
}

int htmlb_html_tag_is_forbidden(const char *tag) {
  static const char *bad[] = {
      "script", "iframe",   "object", "embed",  "applet", "frame",
      "frameset", "meta",   "base",   "link",   "style",  "html",
      "head",   "body",     "template", "foreignobject", "svg",
      "math",   "noscript", NULL};
  if (!tag) return 1;
  for (int i = 0; bad[i]; i++) {
    if (strcasecmp(tag, bad[i]) == 0) return 1;
  }
  return 0;
}

int htmlb_html_tag_is_safe_name(const char *tag) {
  if (!tag || !*tag) return 0;
  if (!isalpha((unsigned char)tag[0])) return 0;
  for (const char *p = tag + 1; *p; p++) {
    if (!(isalnum((unsigned char)*p) || *p == '-')) return 0;
  }
  return 1;
}

const char *htmlb_html_tag_for(const char *tag) {
  if (!tag) return "div";
  if (strcmp(tag, "col") == 0) return "div";
  if (strcmp(tag, "row") == 0) return "div";
  if (strcmp(tag, "stack") == 0) return "div";
  if (strcmp(tag, "page") == 0) return "div";
  if (strcmp(tag, "btn") == 0) return "button";
  if (strcmp(tag, "card") == 0) return "div";
  if (strcmp(tag, "grid") == 0) return "div";
  if (strcmp(tag, "group") == 0) return "div";
  if (strcmp(tag, "fragment") == 0) return NULL;
  if (strcmp(tag, "link") == 0) return "a";
  if (strcmp(tag, "img") == 0) return "img";
  if (strcmp(tag, "input") == 0) return "input";
  if (strcmp(tag, "textarea") == 0) return "textarea";
  if (strcmp(tag, "select") == 0) return "select";
  if (strcmp(tag, "checkbox") == 0) return "input";
  if (strcmp(tag, "radio") == 0) return "input";
  if (strcmp(tag, "icon") == 0) return "span"; /* stub; see capability note */
  if (strcmp(tag, "motion") == 0 || strcmp(tag, "Motion") == 0) return "div";
  if (strcmp(tag, "chart") == 0 || strcmp(tag, "Chart") == 0) return "div";
  if (strcmp(tag, "nav") == 0) return "nav";
  if (strcmp(tag, "header") == 0) return "header";
  if (strcmp(tag, "footer") == 0) return "footer";
  if (strcmp(tag, "main") == 0) return "main";
  if (strcmp(tag, "section") == 0) return "section";
  if (strcmp(tag, "sidebar") == 0) return "aside";
  if (strcmp(tag, "article") == 0) return "article";
  if (strcmp(tag, "aside") == 0) return "aside";
  if (strcmp(tag, "span") == 0) return "span";
  if (strcmp(tag, "hr") == 0) return "hr";
  if (strcmp(tag, "br") == 0) return "br";
  if (strcmp(tag, "h1") == 0) return "h1";
  if (strcmp(tag, "h2") == 0) return "h2";
  if (strcmp(tag, "h3") == 0) return "h3";
  if (strcmp(tag, "p") == 0) return "p";
  /* Unknown Cord tags: only emit if safe; never script/iframe/etc. */
  if (htmlb_html_tag_is_forbidden(tag) || !htmlb_html_tag_is_safe_name(tag)) return "div";
  return tag;
}

void htmlb_gen_style_classes_ir(StrBuf *sb, IrNode *style_map) {
  if (!style_map) return;
  for (size_t i = 0; i < style_map->n_kids; i++) {
    IrNode *entry = style_map->kids[i];
    if (!entry || entry->kind != IR_ATTR) continue;

    const char *key = entry->name ? entry->name : "";
    const char *val = entry->value ? entry->value : "";

    int mapped = 0;
    for (int s = 0; htmlb_style_mappings[s].style_key; s++) {
      if (strcmp(key, htmlb_style_mappings[s].style_key) == 0) {
        htmlb_sb_append(sb, " ");
        htmlb_sb_append(sb, htmlb_style_mappings[s].tw_prefix);
        if (htmlb_style_mappings[s].is_value_appended) htmlb_sb_append(sb, val);
        mapped = 1;
        break;
      }
    }

    if (!mapped) {
      if (strcmp(key, "between") == 0)
        htmlb_sb_append(sb, " flex justify-between");
      else if (strcmp(key, "center") == 0)
        htmlb_sb_append(sb, " flex items-center justify-center");
      else if (strcmp(key, "around") == 0)
        htmlb_sb_append(sb, " justify-around");
      else if (strcmp(key, "evenly") == 0)
        htmlb_sb_append(sb, " justify-evenly");
      else if (strcmp(key, "sticky") == 0)
        htmlb_sb_append(sb, " sticky top-0");
      else if (strcmp(key, "bold") == 0)
        htmlb_sb_append(sb, " font-bold");
      else if (strcmp(key, "muted") == 0)
        htmlb_sb_append(sb, " text-muted");
      else if (strcmp(key, "overflow") == 0) {
        htmlb_sb_append(sb, " overflow-");
        htmlb_sb_append(sb, val);
      }
    }
  }
}
