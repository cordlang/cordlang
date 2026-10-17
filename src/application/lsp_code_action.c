#include "application/lsp_internal.h"

static int extract_quoted_attr(const char *msg, char *out, size_t out_sz) {
  if (!msg || !out || out_sz < 2) return 0;
  const char *a = strchr(msg, '\'');
  if (!a) return 0;
  a++;
  const char *b = strchr(a, '\'');
  if (!b || b <= a) return 0;
  size_t n = (size_t)(b - a);
  if (n + 1 > out_sz) n = out_sz - 1;
  memcpy(out, a, n);
  out[n] = '\0';
  return 1;
}

 void handle_code_action(const char *id, const char *msg) {
  char uri[1024];
  uri[0] = '\0';
  const char *td = strstr(msg, "\"textDocument\"");
  if (td) json_get_str(td, "uri", uri, sizeof(uri));
  LspDoc *d = doc_find(uri);
  if (!d) {
    respond_ok(id, "[]");
    return;
  }

  DiagList diags;
  diag_list_init(&diags);
  const char *label = d->path ? d->path : uri;
  if (d->text)
    check_service_run_source(label, d->text, strlen(d->text), &diags);
  else if (d->path && fs_exists(d->path))
    check_service_run(d->path, &diags);
  else {
    diag_list_free(&diags);
    respond_ok(id, "[]");
    return;
  }

  char *uri_esc = json_escape_dup(uri);
  size_t cap = 4096;
  size_t len = 0;
  char *out = malloc(cap);
  if (!out) {
    free(uri_esc);
    diag_list_free(&diags);
    respond_ok(id, "[]");
    return;
  }
  out[0] = '[';
  len = 1;
  int n_actions = 0;

  for (size_t i = 0; i < diags.len; i++) {
    Diagnostic *dg = &diags.items[i];
    if (!dg->code) continue;

    int line = dg->line > 0 ? dg->line - 1 : 0;
    int col = dg->col > 0 ? dg->col - 1 : 0;
    char item[2048];
    item[0] = '\0';

    if (strcmp(dg->code, "jsx-attr") == 0) {
      char attr[64];
      if (!extract_quoted_attr(dg->message, attr, sizeof(attr))) continue;
      const char *repl = cord_jsx_attr_replace(attr);
      if (!repl) continue;
      int end_col = col + (int)strlen(attr);
      char title[128];
      snprintf(title, sizeof(title), "Replace %s with %s", attr, repl);
      char *title_esc = json_escape_dup(title);
      char *repl_esc = json_escape_dup(repl);
      snprintf(item, sizeof(item),
               "%s{\"title\":\"%s\",\"kind\":\"quickfix\","
               "\"edit\":{\"changes\":{\"%s\":[{"
               "\"range\":{\"start\":{\"line\":%d,\"character\":%d},"
               "\"end\":{\"line\":%d,\"character\":%d}},"
               "\"newText\":\"%s\"}]}}}",
               n_actions ? "," : "", title_esc ? title_esc : "fix",
               uri_esc ? uri_esc : "", line, col, line, end_col,
               repl_esc ? repl_esc : "");
      free(title_esc);
      free(repl_esc);
    } else if (strcmp(dg->code, "bad-interp") == 0 && d->text) {
      /* Find first bare {ident} on the diagnostic line and insert # */
      int ln = 0;
      const char *p = d->text;
      while (*p && ln < line) {
        if (*p == '\n') ln++;
        p++;
      }
      if (ln != line) continue;
      const char *line_start = p;
      const char *hit = NULL;
      for (const char *s = line_start; *s && *s != '\n'; s++) {
        if (*s != '{') continue;
        if (s > d->text && s[-1] == '#') continue;
        if (s[1] && (isalpha((unsigned char)s[1]) || s[1] == '_')) {
          hit = s;
          break;
        }
      }
      if (!hit) continue;
      int hit_col = (int)(hit - line_start);
      char *title_esc = json_escape_dup("Fix interpolation: {x} → #{x}");
      snprintf(item, sizeof(item),
               "%s{\"title\":\"%s\",\"kind\":\"quickfix\","
               "\"edit\":{\"changes\":{\"%s\":[{"
               "\"range\":{\"start\":{\"line\":%d,\"character\":%d},"
               "\"end\":{\"line\":%d,\"character\":%d}},"
               "\"newText\":\"#{\"}]}}}",
               n_actions ? "," : "", title_esc ? title_esc : "fix",
               uri_esc ? uri_esc : "", line, hit_col, line, hit_col + 1);
      free(title_esc);
    } else {
      continue;
    }

    if (!item[0]) continue;
    size_t il = strlen(item);
    if (len + il + 2 >= cap) {
      while (len + il + 2 >= cap) cap *= 2;
      char *nbuf = realloc(out, cap);
      if (!nbuf) break;
      out = nbuf;
    }
    memcpy(out + len, item, il);
    len += il;
    out[len] = '\0';
    n_actions++;
  }

  if (len + 2 <= cap) {
    out[len++] = ']';
    out[len] = '\0';
  }
  respond_ok(id, out);
  free(out);
  free(uri_esc);
  diag_list_free(&diags);
}

