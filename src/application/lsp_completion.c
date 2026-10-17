#include "application/lsp_internal.h"

/* Line prefix before cursor (not including character at cursor). */
static void line_prefix_at(const char *text, int line, int character, char *out,
                           size_t out_sz) {
  out[0] = '\0';
  if (!text || !out || out_sz < 2) return;
  int ln = 0;
  const char *p = text;
  while (*p && ln < line) {
    if (*p == '\n') ln++;
    p++;
  }
  if (ln != line) return;
  size_t n = 0;
  int col = 0;
  while (*p && *p != '\n' && col < character && n + 1 < out_sz) {
    out[n++] = *p++;
    col++;
  }
  out[n] = '\0';
}

static void append_completion_item(char **out, size_t *len, size_t *cap,
                                   int *first, const char *label, int kind) {
  char item[256];
  snprintf(item, sizeof(item), "%s{\"label\":\"%s\",\"kind\":%d}",
           *first ? "" : ",", label, kind);
  *first = 0;
  size_t il = strlen(item);
  if (*len + il + 2 >= *cap) {
    *cap *= 2;
    char *n = realloc(*out, *cap);
    if (!n) return;
    *out = n;
  }
  memcpy(*out + *len, item, il);
  *len += il;
  (*out)[*len] = '\0';
}

/* Completion items from known_attrs / schema surface (context-aware). */
 void handle_completion(const char *id, const char *msg) {
  static const char *tags[] = {
      "col",     "row",     "stack",  "page",   "card",    "grid",
      "btn",     "button",  "link",   "input",  "textarea","form",
      "label",   "img",     "span",   "p",      "h1",      "h2",
      "h3",      "div",     "nav",    "header", "footer",  "main",
      "section", "slot",    "if",     "for",    "provide", "portal",
      NULL};
  static const char *attrs[] = {
      "gap",    "p",      "m",       "px",     "py",      "mx",
      "my",     "w",      "h",       "color",  "variant", "size",
      "bg",     "bold",   "muted",   "center", "to",      "href",
      "src",    "alt",    "bind",    "key",    "class",   "role",
      "aria-label", "disabled", "placeholder", "type", "value",
      "purpose", "importance",
      NULL};
  static const char *events[] = {
      "click", "change", "input", "submit", "keydown", "keyup", "focus",
      "blur",  NULL};
  static const char *kw[] = {"def", "state", "props", "computed", "route",
                             "layout", "theme", "use", "effect", "ref",
                             "ctx", "provide", "params", "fetch", "lazy",
                             NULL};

  char uri[1024];
  int line = 0, character = 0;
  uri[0] = '\0';
  const char *td = strstr(msg, "\"textDocument\"");
  if (td) json_get_str(td, "uri", uri, sizeof(uri));
  const char *pos = strstr(msg, "\"position\"");
  if (pos) {
    json_get_int(pos, "line", &line);
    json_get_int(pos, "character", &character);
  }
  LspDoc *d = doc_find(uri);
  char prefix[512];
  line_prefix_at(d ? d->text : NULL, line, character, prefix, sizeof(prefix));

  /* Mode: 0=tags+kw, 1=attrs, 2=events */
  int mode = 0;
  const char *at = strrchr(prefix, '@');
  if (at && !strchr(at, ' ') && !strchr(at, '=')) {
    mode = 2;
  } else {
    /* After a tag on the same line: "  col gap=" or "btn " with indent */
    const char *s = prefix;
    while (*s == ' ' || *s == '\t') s++;
    if (*s) {
      const char *sp = strchr(s, ' ');
      if (sp && sp[1]) {
        /* has content after first word → attrs */
        mode = 1;
      } else if (sp && !sp[1]) {
        mode = 1; /* trailing space after tag */
      }
    }
  }

  size_t cap = 16384, len = 0;
  char *out = malloc(cap);
  if (!out) {
    respond_ok(id, "[]");
    return;
  }
  strcpy(out, "[");
  len = 1;
  int first = 1;

  if (mode == 2) {
    for (int i = 0; events[i]; i++)
      append_completion_item(&out, &len, &cap, &first, events[i], 10);
  } else if (mode == 1) {
    for (int i = 0; attrs[i]; i++)
      append_completion_item(&out, &len, &cap, &first, attrs[i], 10);
  } else {
    for (int i = 0; kw[i]; i++)
      append_completion_item(&out, &len, &cap, &first, kw[i], 14);
    for (int i = 0; tags[i]; i++)
      append_completion_item(&out, &len, &cap, &first, tags[i], 14);
  }

  out[len++] = ']';
  out[len] = '\0';
  respond_ok(id, out);
  free(out);
}

