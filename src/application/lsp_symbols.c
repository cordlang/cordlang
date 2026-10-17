#include "application/lsp_internal.h"

static const char *component_file(Node *def) {
  if (!def) return NULL;
  for (size_t i = 0; i < def->children_len; i++) {
    Node *c = def->children[i];
    if (c && c->type == NODE_ATTR && c->value &&
        strcmp(c->value, "__file__") == 0 && c->value2)
      return c->value2;
  }
  return NULL;
}

 void handle_document_symbol(const char *id, const char *msg) {
  char uri[1024];
  if (!json_get_str(msg, "uri", uri, sizeof(uri))) {
    /* nested textDocument.uri */
    const char *td = strstr(msg, "\"textDocument\"");
    if (td) json_get_str(td, "uri", uri, sizeof(uri));
  }
  LspDoc *d = doc_find(uri);
  const char *path = d && d->path ? d->path : NULL;
  if (!path || !fs_exists(path)) {
    respond_ok(id, "[]");
    return;
  }

  CompileResult r = compiler_parse_project(path);
  if (!r.ok || !r.ast || !r.ast->root) {
    compiler_result_free(&r);
    respond_ok(id, "[]");
    return;
  }

  size_t cap = 8192, len = 0;
  char *out = malloc(cap);
  if (!out) {
    compiler_result_free(&r);
    respond_ok(id, "[]");
    return;
  }
  strcpy(out, "[");
  len = 1;
  int first = 1;
  Node *root = r.ast->root;
  for (size_t i = 0; i < root->children_len; i++) {
    Node *c = root->children[i];
    if (!c || c->type != NODE_COMPONENT_DEF || !c->value) continue;
    int line = c->line > 0 ? c->line - 1 : 0;
    char *name_esc = json_escape_dup(c->value);
    char item[1024];
    snprintf(item, sizeof(item),
             "%s{\"name\":\"%s\",\"kind\":5,\"range\":{\"start\":{\"line\":%d,"
             "\"character\":0},\"end\":{\"line\":%d,\"character\":0}},"
             "\"selectionRange\":{\"start\":{\"line\":%d,\"character\":0},"
             "\"end\":{\"line\":%d,\"character\":0}}}",
             first ? "" : ",", name_esc ? name_esc : "", line, line, line,
             line);
    free(name_esc);
    first = 0;
    size_t il = strlen(item);
    if (len + il + 2 >= cap) {
      cap *= 2;
      char *n = realloc(out, cap);
      if (!n) break;
      out = n;
    }
    memcpy(out + len, item, il);
    len += il;
    out[len] = '\0';
  }
  out[len++] = ']';
  out[len] = '\0';
  respond_ok(id, out);
  free(out);
  compiler_result_free(&r);
  (void)component_file;
}

 void ident_at(const char *text, int line, int character, char *out,
                     size_t out_sz) {
  out[0] = '\0';
  if (!text) return;
  int ln = 0;
  const char *p = text;
  while (*p && ln < line) {
    if (*p == '\n') ln++;
    p++;
  }
  if (ln != line) return;
  const char *line_start = p;
  if (*p == '#' && p[1] != '{') return;
  {
    const char *s = p;
    while (*s == ' ' || *s == '\t') s++;
    if (s[0] == '#' && s[1] != '{') return;
    if (s[0] == '/' && s[1] == '/') return;
  }
  int col = 0;
  int in_str = 0;
  const char *cursor = p;
  while (*cursor && col < character && *cursor != '\n') {
    if (!in_str && *cursor == '"')
      in_str = 1;
    else if (in_str) {
      if (*cursor == '\\' && cursor[1] && cursor[1] != '\n') {
        cursor++;
        col++;
      } else if (*cursor == '"')
        in_str = 0;
    }
    cursor++;
    col++;
  }
  if (in_str) return;
  const char *start = cursor;
  while (start > line_start &&
         (isalnum((unsigned char)start[-1]) || start[-1] == '_'))
    start--;
  const char *end = cursor;
  while (*end && (isalnum((unsigned char)*end) || *end == '_')) end++;
  size_t n = (size_t)(end - start);
  if (n == 0 || n + 1 > out_sz) return;
  memcpy(out, start, n);
  out[n] = '\0';
}

 void handle_definition(const char *id, const char *msg) {
  char uri[1024];
  int line = 0, character = 0;
  const char *td = strstr(msg, "\"textDocument\"");
  if (td) json_get_str(td, "uri", uri, sizeof(uri));
  const char *pos = strstr(msg, "\"position\"");
  if (pos) {
    json_get_int(pos, "line", &line);
    json_get_int(pos, "character", &character);
  }
  LspDoc *d = doc_find(uri);
  char name[128];
  ident_at(d ? d->text : NULL, line, character, name, sizeof(name));
  if (!name[0] || !d || !d->path) {
    respond_ok(id, "null");
    return;
  }

  CompileResult r = compiler_parse_project(d->path);
  if (!r.ok || !r.ast || !r.ast->root) {
    compiler_result_free(&r);
    respond_ok(id, "null");
    return;
  }
  Node *root = r.ast->root;
  for (size_t i = 0; i < root->children_len; i++) {
    Node *c = root->children[i];
    if (!c || c->type != NODE_COMPONENT_DEF || !c->value) continue;
    if (strcmp(c->value, name) != 0) continue;
    const char *file = component_file(c);
    const char *fp = file ? file : d->path;
    int fline = c->line > 0 ? c->line - 1 : 0;
    char file_uri[1024];
    char abs[1024];
    if (fp[0] == '/' || (fp[0] && fp[1] == ':'))
      snprintf(abs, sizeof(abs), "%s", fp);
    else {
      /* relative to project dir of open file */
      char *dir = strdup(d->path);
      char *slash = strrchr(dir, '/');
#ifdef _WIN32
      if (!slash) slash = strrchr(dir, '\\');
#endif
      if (slash) {
        *slash = '\0';
        snprintf(abs, sizeof(abs), "%s/%s", dir, fp);
      } else
        snprintf(abs, sizeof(abs), "%s", fp);
      free(dir);
    }
    path_to_uri(abs, file_uri, sizeof(file_uri));
    char *uri_esc = json_escape_dup(file_uri);
    char result[2048];
    snprintf(result, sizeof(result),
             "{\"uri\":\"%s\",\"range\":{\"start\":{\"line\":%d,\"character\":0},"
             "\"end\":{\"line\":%d,\"character\":0}}}",
             uri_esc ? uri_esc : "", fline, fline);
    free(uri_esc);
    respond_ok(id, result);
    compiler_result_free(&r);
    return;
  }
  compiler_result_free(&r);
  respond_ok(id, "null");
}

