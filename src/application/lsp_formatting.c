#include "application/lsp_internal.h"

 void handle_formatting(const char *id, const char *msg) {
  char uri[1024];
  uri[0] = '\0';
  const char *td = strstr(msg, "\"textDocument\"");
  if (td) json_get_str(td, "uri", uri, sizeof(uri));
  LspDoc *d = doc_find(uri);
  const char *text = d && d->text ? d->text : NULL;
  if (!text && d && d->path && fs_exists(d->path)) {
    size_t len = 0;
    char *src = fs_read_file(d->path, &len);
    if (src) {
      free(d->text);
      d->text = src;
      text = d->text;
    }
  }
  if (!text) {
    respond_ok(id, "null");
    return;
  }
  char *formatted = fmt_service_normalize(text, strlen(text));
  if (!formatted) {
    respond_ok(id, "null");
    return;
  }
  if (strcmp(formatted, text) == 0) {
    free(formatted);
    respond_ok(id, "[]");
    return;
  }
  /* Full-document replace */
  int end_line = 0, end_col = 0;
  for (const char *p = text; *p; p++) {
    if (*p == '\n') {
      end_line++;
      end_col = 0;
    } else {
      end_col++;
    }
  }
  char *esc = json_escape_dup(formatted);
  free(formatted);
  char *result = NULL;
  size_t need = (esc ? strlen(esc) : 0) + 256;
  result = malloc(need);
  if (!result) {
    free(esc);
    respond_ok(id, "null");
    return;
  }
  snprintf(result, need,
           "[{\"range\":{\"start\":{\"line\":0,\"character\":0},"
           "\"end\":{\"line\":%d,\"character\":%d}},\"newText\":\"%s\"}]",
           end_line, end_col, esc ? esc : "");
  free(esc);
  respond_ok(id, result);
  free(result);
}

