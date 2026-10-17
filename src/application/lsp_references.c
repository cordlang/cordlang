#include "application/lsp_internal.h"

 const char *lsp_overlay(const char *abs, void *ud) {
  (void)ud;
  if (!abs) return NULL;
  char *na = fs_norm_path(abs);
  for (int i = 0; i < g_ndocs; i++) {
    if (!g_docs[i].path || !g_docs[i].text) continue;
    char *nb = fs_norm_path(g_docs[i].path);
    int eq = 0;
    if (na && nb) {
#ifdef _WIN32
      eq = _stricmp(na, nb) == 0;
#else
      eq = strcmp(na, nb) == 0;
#endif
    }
    free(nb);
    if (eq) {
      free(na);
      return g_docs[i].text;
    }
  }
  free(na);
  return NULL;
}

 int lsp_extra_paths(const char **out, int maxn) {
  int n = 0;
  for (int i = 0; i < g_ndocs && n < maxn; i++) {
    if (g_docs[i].path) out[n++] = g_docs[i].path;
  }
  return n;
}

 int loc_cmp(const void *a, const void *b) {
  const SymbolLoc *x = a, *y = b;
  const char *pa = x->path ? x->path : "";
  const char *pb = y->path ? y->path : "";
  int c = strcmp(pa, pb);
  if (c) return c;
  if (x->line != y->line) return x->line - y->line;
  return x->col - y->col;
}

 void abs_uri(const char *path, char *out, size_t n) {
  char *norm = path ? fs_norm_path(path) : NULL;
  path_to_uri(norm ? norm : (path ? path : ""), out, n);
  free(norm);
}

 int parse_pos_uri(const char *msg, char *uri, size_t uri_sz, int *line,
                         int *character) {
  uri[0] = '\0';
  *line = 0;
  *character = 0;
  const char *td = strstr(msg, "\"textDocument\"");
  if (td) json_get_str(td, "uri", uri, uri_sz);
  const char *pos = strstr(msg, "\"position\"");
  if (pos) {
    json_get_int(pos, "line", line);
    json_get_int(pos, "character", character);
  }
  return uri[0] != '\0';
}

 int ident_range(const char *text, int line, int character, char *name,
                       size_t name_sz, int *start_ch, int *end_ch) {
  ident_at(text, line, character, name, name_sz);
  if (!name[0] || !text) return 0;
  int ln = 0;
  const char *p = text;
  while (*p && ln < line) {
    if (*p == '\n') ln++;
    p++;
  }
  if (ln != line) return 0;
  int col = 0;
  while (*p && col < character && *p != '\n') {
    p++;
    col++;
  }
  const char *start = p;
  const char *ls = p;
  while (ls > text && ls[-1] != '\n') ls--;
  while (start > ls && (isalnum((unsigned char)start[-1]) || start[-1] == '_'))
    start--;
  const char *end = p;
  while (*end && (isalnum((unsigned char)*end) || *end == '_')) end++;
  *start_ch = (int)(start - ls);
  *end_ch = (int)(end - ls);
  return *end_ch > *start_ch;
}

 int cursor_component(const char *id, const char *msg, char *name,
                            size_t name_sz, LspDoc **out_doc, int err_if_none) {
  char uri[1024];
  int line = 0, character = 0;
  parse_pos_uri(msg, uri, sizeof(uri), &line, &character);
  LspDoc *d = doc_find(uri);
  if (d) doc_ensure_text(d);
  int sc = 0, ec = 0;
  ident_range(d ? d->text : NULL, line, character, name, name_sz, &sc, &ec);
  (void)sc;
  (void)ec;
  if (!name[0] || !d || !d->path ||
      !symbols_name_is_component(d->path, name, lsp_overlay, NULL)) {
    if (err_if_none)
      respond_err(id, -32602, "not a renamable component");
    return 0;
  }
  if (out_doc) *out_doc = d;
  return 1;
}

 void handle_references(const char *id, const char *msg) {
  char name[128];
  LspDoc *d = NULL;
  if (!cursor_component(id, msg, name, sizeof(name), &d, 0)) {
    respond_ok(id, "[]");
    return;
  }
  int include_decl = 1;
  const char *ctx = strstr(msg, "\"context\"");
  if (ctx) json_get_bool(ctx, "includeDeclaration", &include_decl);

  const char *extra[MAX_DOCS];
  int n_extra = lsp_extra_paths(extra, MAX_DOCS);
  SymbolLocList locs;
  symbols_collect_refs(d->path, name, include_decl, lsp_overlay, NULL, extra,
                       n_extra, &locs);
  if (locs.len > 1)
    qsort(locs.items, locs.len, sizeof(SymbolLoc), loc_cmp);

  size_t cap = 4096, len = 0;
  char *out = malloc(cap);
  if (!out) {
    symbol_loc_list_free(&locs);
    respond_ok(id, "[]");
    return;
  }
  strcpy(out, "[");
  len = 1;
  for (size_t i = 0; i < locs.len; i++) {
    SymbolLoc *it = &locs.items[i];
    char file_uri[1024];
    abs_uri(it->path, file_uri, sizeof(file_uri));
    char *uri_esc = json_escape_dup(file_uri);
    int sl = it->line > 0 ? it->line - 1 : 0;
    int sc = it->col > 0 ? it->col - 1 : 0;
    char item[1536];
    snprintf(item, sizeof(item),
             "%s{\"uri\":\"%s\",\"range\":{\"start\":{\"line\":%d,"
             "\"character\":%d},\"end\":{\"line\":%d,\"character\":%d}}}",
             i ? "," : "", uri_esc ? uri_esc : "", sl, sc, sl,
             sc + it->length);
    free(uri_esc);
    size_t il = strlen(item);
    if (len + il + 2 >= cap) {
      while (len + il + 2 >= cap) cap *= 2;
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
  symbol_loc_list_free(&locs);
}

