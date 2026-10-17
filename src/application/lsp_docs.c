#include "application/lsp_internal.h"

LspDoc g_docs[MAX_DOCS];
int g_ndocs = 0;

 char *uri_to_path(const char *uri) {
  if (!uri) return NULL;
  if (strncmp(uri, "file://", 7) != 0) return strdup(uri);
  const char *p = uri + 7;
  /* file:///abs or file://localhost/abs */
  if (strncmp(p, "localhost", 9) == 0) p += 9;
  if (*p == '/') {
#ifdef _WIN32
    if (p[1] && p[2] == ':') p++; /* /C:/... */
#endif
    return strdup(p);
  }
  return strdup(p);
}

 void path_to_uri(const char *path, char *out, size_t out_sz) {
  if (!path || !out || out_sz < 8) return;
#ifdef _WIN32
  snprintf(out, out_sz, "file:///%s", path);
  for (char *c = out; *c; c++)
    if (*c == '\\') *c = '/';
#else
  snprintf(out, out_sz, "file://%s", path);
#endif
}

 LspDoc *doc_find(const char *uri) {
  for (int i = 0; i < g_ndocs; i++) {
    if (g_docs[i].uri && uri && strcmp(g_docs[i].uri, uri) == 0)
      return &g_docs[i];
  }
  return NULL;
}

 LspDoc *doc_upsert(const char *uri, const char *text) {
  LspDoc *d = doc_find(uri);
  if (!d) {
    if (g_ndocs >= MAX_DOCS) return NULL;
    d = &g_docs[g_ndocs++];
    memset(d, 0, sizeof(*d));
    d->uri = strdup(uri);
    d->path = uri_to_path(uri);
  }
  free(d->text);
  d->text = text ? strdup(text) : NULL;
  return d;
}

 void docs_free(void) {
  for (int i = 0; i < g_ndocs; i++) {
    free(g_docs[i].uri);
    free(g_docs[i].path);
    free(g_docs[i].text);
    memset(&g_docs[i], 0, sizeof(g_docs[i]));
  }
  g_ndocs = 0;
}

 void doc_ensure_text(LspDoc *d) {
  if (!d || d->text) return;
  if (d->path && fs_exists(d->path)) {
    size_t len = 0;
    d->text = fs_read_file(d->path, &len);
  }
}

