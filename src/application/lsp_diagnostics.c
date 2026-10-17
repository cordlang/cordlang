#include "application/lsp_internal.h"

static int severity_of(DiagLevel lvl) {
  if (lvl == DIAG_ERROR) return 1;
  if (lvl == DIAG_WARN) return 2;
  return 3;
}

 void publish_diagnostics(const char *uri, LspDoc *d) {
  if (!d) return;

  DiagList diags;
  diag_list_init(&diags);

  const char *label = d->path ? d->path : (uri ? uri : "<buffer>");
  if (d->text) {
    check_service_run_source(label, d->text, strlen(d->text), &diags);
  } else if (d->path && fs_exists(d->path)) {
    check_service_run(d->path, &diags);
  } else {
    diag_list_free(&diags);
    return;
  }

  size_t cap = 8192;
  size_t len = 0;
  char *params = malloc(cap);
  if (!params) {
    diag_list_free(&diags);
    return;
  }
  {
    char *uri_esc = json_escape_dup(uri ? uri : "");
    int n = snprintf(params, cap, "{\"uri\":\"%s\",\"diagnostics\":[",
                     uri_esc ? uri_esc : "");
    free(uri_esc);
    if (n < 0) {
      free(params);
      diag_list_free(&diags);
      return;
    }
    len = (size_t)n;
  }

  for (size_t i = 0; i < diags.len; i++) {
    Diagnostic *dg = &diags.items[i];
    int line = dg->line > 0 ? dg->line - 1 : 0;
    int col = dg->col > 0 ? dg->col - 1 : 0;
    char *msg_esc = json_escape_dup(dg->message ? dg->message : "");
    char *code_esc =
        dg->code && dg->code[0] ? json_escape_dup(dg->code) : NULL;
    char *hint_esc =
        dg->hint && dg->hint[0] ? json_escape_dup(dg->hint) : NULL;
    char item[8192];
    if (code_esc && hint_esc) {
      snprintf(item, sizeof(item),
               "%s{\"range\":{\"start\":{\"line\":%d,\"character\":%d},"
               "\"end\":{\"line\":%d,\"character\":%d}},\"severity\":%d,"
               "\"source\":\"cordlang\",\"code\":\"%s\",\"message\":\"%s\","
               "\"data\":{\"hint\":\"%s\",\"code\":\"%s\"}}",
               i ? "," : "", line, col, line, col + 1, severity_of(dg->level),
               code_esc, msg_esc ? msg_esc : "", hint_esc, code_esc);
    } else if (code_esc) {
      snprintf(item, sizeof(item),
               "%s{\"range\":{\"start\":{\"line\":%d,\"character\":%d},"
               "\"end\":{\"line\":%d,\"character\":%d}},\"severity\":%d,"
               "\"source\":\"cordlang\",\"code\":\"%s\",\"message\":\"%s\"}",
               i ? "," : "", line, col, line, col + 1, severity_of(dg->level),
               code_esc, msg_esc ? msg_esc : "");
    } else if (hint_esc) {
      snprintf(item, sizeof(item),
               "%s{\"range\":{\"start\":{\"line\":%d,\"character\":%d},"
               "\"end\":{\"line\":%d,\"character\":%d}},\"severity\":%d,"
               "\"source\":\"cordlang\",\"message\":\"%s — %s\"}",
               i ? "," : "", line, col, line, col + 1, severity_of(dg->level),
               msg_esc ? msg_esc : "", hint_esc);
    } else {
      snprintf(item, sizeof(item),
               "%s{\"range\":{\"start\":{\"line\":%d,\"character\":%d},"
               "\"end\":{\"line\":%d,\"character\":%d}},\"severity\":%d,"
               "\"source\":\"cordlang\",\"message\":\"%s\"}",
               i ? "," : "", line, col, line, col + 1, severity_of(dg->level),
               msg_esc ? msg_esc : "");
    }
    free(msg_esc);
    free(code_esc);
    free(hint_esc);
    size_t il = strlen(item);
    if (len + il + 4 >= cap) {
      while (len + il + 4 >= cap) cap *= 2;
      char *n = realloc(params, cap);
      if (!n) break;
      params = n;
    }
    memcpy(params + len, item, il);
    len += il;
    params[len] = '\0';
  }
  {
    const char *tail = "]}";
    size_t tl = strlen(tail);
    if (len + tl + 1 >= cap) {
      cap = len + tl + 1;
      char *n = realloc(params, cap);
      if (n) params = n;
    }
    if (len + tl + 1 <= cap) memcpy(params + len, tail, tl + 1);
  }
  notify("textDocument/publishDiagnostics", params);
  free(params);
  diag_list_free(&diags);
}

