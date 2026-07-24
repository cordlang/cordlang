#include "application/lsp_service.h"
#include "application/check_service.h"
#include "application/ports/compiler_port.h"
#include "application/ports/fs_port.h"
#include "domain/ast.h"
#include "domain/diag.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#define MAX_DOCS 64
#define MAX_MSG (4 * 1024 * 1024)

typedef struct {
  char *uri;
  char *path;
  char *text;
} LspDoc;

static LspDoc g_docs[MAX_DOCS];
static int g_ndocs = 0;
static int g_shutdown = 0;

static char *uri_to_path(const char *uri) {
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

static void path_to_uri(const char *path, char *out, size_t out_sz) {
  if (!path || !out || out_sz < 8) return;
#ifdef _WIN32
  snprintf(out, out_sz, "file:///%s", path);
  for (char *c = out; *c; c++)
    if (*c == '\\') *c = '/';
#else
  snprintf(out, out_sz, "file://%s", path);
#endif
}

static LspDoc *doc_find(const char *uri) {
  for (int i = 0; i < g_ndocs; i++) {
    if (g_docs[i].uri && uri && strcmp(g_docs[i].uri, uri) == 0)
      return &g_docs[i];
  }
  return NULL;
}

static LspDoc *doc_upsert(const char *uri, const char *text) {
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

static void json_escape_append(char **buf, size_t *len, size_t *cap,
                               const char *s) {
  if (!s) return;
  for (const char *p = s; *p; p++) {
    const char *rep = NULL;
    char tmp[8];
    if (*p == '"' || *p == '\\') {
      tmp[0] = '\\';
      tmp[1] = *p;
      tmp[2] = 0;
      rep = tmp;
    } else if (*p == '\n')
      rep = "\\n";
    else if (*p == '\r')
      rep = "\\r";
    else if (*p == '\t')
      rep = "\\t";
    else {
      tmp[0] = *p;
      tmp[1] = 0;
      rep = tmp;
    }
    size_t rl = strlen(rep);
    if (*len + rl + 1 >= *cap) {
      *cap *= 2;
      char *n = realloc(*buf, *cap);
      if (!n) return;
      *buf = n;
    }
    memcpy(*buf + *len, rep, rl);
    *len += rl;
    (*buf)[*len] = '\0';
  }
}

static void lsp_send(const char *body) {
  if (!body) return;
  printf("Content-Length: %zu\r\n\r\n%s", strlen(body), body);
  fflush(stdout);
}

/* Extract JSON string value for "key" near start of object (best-effort). */static int json_get_str(const char *json, const char *key, char *out,
                        size_t out_sz) {
  char pat[96];
  snprintf(pat, sizeof(pat), "\"%s\"", key);
  const char *p = strstr(json, pat);
  if (!p) return 0;
  p = strchr(p + strlen(pat), ':');
  if (!p) return 0;
  p++;
  while (*p && isspace((unsigned char)*p)) p++;
  if (*p != '"') return 0;
  p++;
  size_t i = 0;
  while (*p && *p != '"' && i + 1 < out_sz) {
    if (*p == '\\' && p[1]) {
      p++;
      out[i++] = *p++;
      continue;
    }
    out[i++] = *p++;
  }
  out[i] = '\0';
  return 1;
}

static int json_get_int(const char *json, const char *key, int *out) {
  char pat[96];
  snprintf(pat, sizeof(pat), "\"%s\"", key);
  const char *p = strstr(json, pat);
  if (!p) return 0;
  p = strchr(p + strlen(pat), ':');
  if (!p) return 0;
  p++;
  while (*p && isspace((unsigned char)*p)) p++;
  if (!isdigit((unsigned char)*p) && *p != '-') return 0;
  *out = (int)strtol(p, NULL, 10);
  return 1;
}

static const char *json_method(const char *msg) {
  static char method[128];
  if (!json_get_str(msg, "method", method, sizeof(method))) return NULL;
  return method;
}

static int json_id(const char *msg, char *out, size_t out_sz) {
  /* id may be number or string */
  const char *p = strstr(msg, "\"id\"");
  if (!p) return 0;
  p = strchr(p, ':');
  if (!p) return 0;
  p++;
  while (*p && isspace((unsigned char)*p)) p++;
  if (*p == '"') {
    return json_get_str(msg, "id", out, out_sz);
  }
  long v = strtol(p, NULL, 10);
  snprintf(out, out_sz, "%ld", v);
  return 1;
}

static void respond_ok(const char *id, const char *result_json) {
  char body[65536];
  if (id && id[0] && (id[0] == '"' || isdigit((unsigned char)id[0]) || id[0] == '-')) {
    /* id already may be bare number string */
  }
  int id_num = 1;
  for (const char *c = id; c && *c; c++)
    if (!isdigit((unsigned char)*c) && *c != '-') {
      id_num = 0;
      break;
    }
  if (id_num && id)
    snprintf(body, sizeof(body),
             "{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":%s}", id,
             result_json ? result_json : "null");
  else
    snprintf(body, sizeof(body),
             "{\"jsonrpc\":\"2.0\",\"id\":\"%s\",\"result\":%s}",
             id ? id : "0", result_json ? result_json : "null");
  lsp_send(body);
}

static void notify(const char *method, const char *params_json) {
  char body[65536];
  snprintf(body, sizeof(body),
           "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s}", method,
           params_json ? params_json : "null");
  lsp_send(body);
}

static int severity_of(DiagLevel lvl) {
  if (lvl == DIAG_ERROR) return 1;
  if (lvl == DIAG_WARN) return 2;
  return 3;
}

static void publish_diagnostics(const char *uri, const char *path) {
  if (!path || !fs_exists(path)) return;

  DiagList diags;
  diag_list_init(&diags);
  check_service_run(path, &diags);

  size_t cap = 8192;
  size_t len = 0;
  char *params = malloc(cap);
  if (!params) {
    diag_list_free(&diags);
    return;
  }
  {
    char head[1024];
    snprintf(head, sizeof(head),
             "{\"uri\":\"%s\",\"diagnostics\":[", uri ? uri : "");
    strcpy(params, head);
    len = strlen(params);
  }

  for (size_t i = 0; i < diags.len; i++) {
    Diagnostic *dg = &diags.items[i];
    int line = dg->line > 0 ? dg->line - 1 : 0;
    int col = dg->col > 0 ? dg->col - 1 : 0;
    char item[2048];
    char msg_esc[1024];
    msg_esc[0] = '\0';
    {
      size_t ml = 0, mc = sizeof(msg_esc);
      char *mp = msg_esc;
      json_escape_append(&mp, &ml, &mc, dg->message ? dg->message : "");
    }
    snprintf(item, sizeof(item),
             "%s{\"range\":{\"start\":{\"line\":%d,\"character\":%d},"
             "\"end\":{\"line\":%d,\"character\":%d}},\"severity\":%d,"
             "\"source\":\"cordlang\",\"message\":\"%s\"}",
             i ? "," : "", line, col, line, col + 1, severity_of(dg->level),
             msg_esc);
    size_t il = strlen(item);
    if (len + il + 4 >= cap) {
      cap *= 2;
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
    if (len + tl + 1 < cap) memcpy(params + len, tail, tl + 1);
  }
  notify("textDocument/publishDiagnostics", params);
  free(params);
  diag_list_free(&diags);
}

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

static void handle_document_symbol(const char *id, const char *msg) {
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
    char item[512];
    snprintf(item, sizeof(item),
             "%s{\"name\":\"%s\",\"kind\":5,\"range\":{\"start\":{\"line\":%d,"
             "\"character\":0},\"end\":{\"line\":%d,\"character\":0}},"
             "\"selectionRange\":{\"start\":{\"line\":%d,\"character\":0},"
             "\"end\":{\"line\":%d,\"character\":0}}}",
             first ? "" : ",", c->value, line, line, line, line);
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

static void ident_at(const char *text, int line, int character, char *out,
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
  int col = 0;
  while (*p && col < character && *p != '\n') {
    p++;
    col++;
  }
  const char *start = p;
  while (start > text && (isalnum((unsigned char)start[-1]) || start[-1] == '_'))
    start--;
  const char *end = p;
  while (*end && (isalnum((unsigned char)*end) || *end == '_')) end++;
  size_t n = (size_t)(end - start);
  if (n == 0 || n + 1 > out_sz) return;
  memcpy(out, start, n);
  out[n] = '\0';
}

static void handle_definition(const char *id, const char *msg) {
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
    char result[2048];
    snprintf(result, sizeof(result),
             "{\"uri\":\"%s\",\"range\":{\"start\":{\"line\":%d,\"character\":0},"
             "\"end\":{\"line\":%d,\"character\":0}}}",
             file_uri, fline, fline);
    respond_ok(id, result);
    compiler_result_free(&r);
    return;
  }
  compiler_result_free(&r);
  respond_ok(id, "null");
}

static void handle_message(const char *msg) {
  const char *method = json_method(msg);
  char id[64];
  int has_id = json_id(msg, id, sizeof(id));

  if (!method) return;

  if (strcmp(method, "initialize") == 0) {
    respond_ok(has_id ? id : "0",
               "{\"capabilities\":{"
               "\"textDocumentSync\":1,"
               "\"documentSymbolProvider\":true,"
               "\"definitionProvider\":true"
               "},\"serverInfo\":{\"name\":\"cordlang\",\"version\":\"0.1.0\"}}");
    return;
  }
  if (strcmp(method, "initialized") == 0) return;
  if (strcmp(method, "shutdown") == 0) {
    g_shutdown = 1;
    if (has_id) respond_ok(id, "null");
    return;
  }
  if (strcmp(method, "exit") == 0) {
    exit(g_shutdown ? 0 : 1);
  }
  if (strcmp(method, "textDocument/didOpen") == 0 ||
      strcmp(method, "textDocument/didChange") == 0 ||
      strcmp(method, "textDocument/didSave") == 0) {
    char uri[1024];
    uri[0] = '\0';
    const char *td = strstr(msg, "\"textDocument\"");
    if (td) json_get_str(td, "uri", uri, sizeof(uri));
    char *text = NULL;
    /* didOpen/didChange may include text */
    const char *tp = strstr(msg, "\"text\"");
    if (tp) {
      char buf[MAX_MSG];
      if (json_get_str(tp, "text", buf, sizeof(buf))) text = strdup(buf);
    }
    LspDoc *d = doc_upsert(uri, text);
    free(text);
    if (d) publish_diagnostics(uri, d->path);
    return;
  }
  if (strcmp(method, "textDocument/documentSymbol") == 0 && has_id) {
    handle_document_symbol(id, msg);
    return;
  }
  if (strcmp(method, "textDocument/definition") == 0 && has_id) {
    handle_definition(id, msg);
    return;
  }
  if (has_id) respond_ok(id, "null");
}

int lsp_service_run(void) {
  char *msg = malloc(MAX_MSG);
  if (!msg) return 1;

  for (;;) {
    char header[256];
    size_t content_len = 0;
    /* Read headers */
    for (;;) {
      if (!fgets(header, sizeof(header), stdin)) {
        free(msg);
        return 0;
      }
      if (header[0] == '\r' || header[0] == '\n') break;
      if (strncmp(header, "Content-Length:", 15) == 0)
        content_len = (size_t)strtoul(header + 15, NULL, 10);
    }
    if (content_len == 0 || content_len >= MAX_MSG) continue;
    size_t got = fread(msg, 1, content_len, stdin);
    if (got != content_len) {
      free(msg);
      return 1;
    }
    msg[content_len] = '\0';
    handle_message(msg);
  }
}
