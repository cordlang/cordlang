#include "application/lsp_service.h"
#include "application/check_service.h"
#include "application/fmt_service.h"
#include "application/symbols_service.h"
#include "application/ports/compiler_port.h"
#include "application/ports/fs_port.h"
#include "domain/ast.h"
#include "domain/diag.h"
#include "domain/known_attrs.h"
#include "domain/version.h"
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

static void docs_free(void) {
  for (int i = 0; i < g_ndocs; i++) {
    free(g_docs[i].uri);
    free(g_docs[i].path);
    free(g_docs[i].text);
    memset(&g_docs[i], 0, sizeof(g_docs[i]));
  }
  g_ndocs = 0;
}

static void doc_ensure_text(LspDoc *d) {
  if (!d || d->text) return;
  if (d->path && fs_exists(d->path)) {
    size_t len = 0;
    d->text = fs_read_file(d->path, &len);
  }
}

/* Growable JSON string escape. *buf must be heap-allocated (or NULL). */
static void json_escape_append(char **buf, size_t *len, size_t *cap,
                               const char *s) {
  if (!s) return;
  if (!*buf) {
    *cap = 64;
    *len = 0;
    *buf = malloc(*cap);
    if (!*buf) return;
    (*buf)[0] = '\0';
  }
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
    else if ((unsigned char)*p < 0x20) {
      snprintf(tmp, sizeof(tmp), "\\u%04x", (unsigned char)*p);
      rep = tmp;
    } else {
      tmp[0] = *p;
      tmp[1] = 0;
      rep = tmp;
    }
    size_t rl = strlen(rep);
    if (*len + rl + 1 >= *cap) {
      size_t ncap = *cap ? *cap * 2 : 64;
      while (*len + rl + 1 >= ncap) ncap *= 2;
      char *n = realloc(*buf, ncap);
      if (!n) return;
      *buf = n;
      *cap = ncap;
    }
    memcpy(*buf + *len, rep, rl);
    *len += rl;
    (*buf)[*len] = '\0';
  }
}

static char *json_escape_dup(const char *s) {
  size_t len = 0, cap = 0;
  char *buf = NULL;
  json_escape_append(&buf, &len, &cap, s ? s : "");
  if (!buf) {
    buf = strdup("");
  }
  return buf;
}

static void lsp_send(const char *body) {
  if (!body) return;
  printf("Content-Length: %zu\r\n\r\n%s", strlen(body), body);
  fflush(stdout);
}

/* Extract JSON string value for "key" near start of object (best-effort). */
static int json_get_str(const char *json, const char *key, char *out,
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

/* Heap version — avoids large stack buffers for document text. */
static char *json_get_str_dup(const char *json, const char *key) {
  char pat[96];
  snprintf(pat, sizeof(pat), "\"%s\"", key);
  const char *p = strstr(json, pat);
  if (!p) return NULL;
  p = strchr(p + strlen(pat), ':');
  if (!p) return NULL;
  p++;
  while (*p && isspace((unsigned char)*p)) p++;
  if (*p != '"') return NULL;
  p++;
  size_t cap = 256, len = 0;
  char *out = malloc(cap);
  if (!out) return NULL;
  while (*p && *p != '"') {
    char ch;
    if (*p == '\\' && p[1]) {
      p++;
      switch (*p) {
        case 'n': ch = '\n'; break;
        case 'r': ch = '\r'; break;
        case 't': ch = '\t'; break;
        case '"': ch = '"'; break;
        case '\\': ch = '\\'; break;
        default: ch = *p; break;
      }
      p++;
    } else {
      ch = *p++;
    }
    if (len + 2 >= cap) {
      cap *= 2;
      char *n = realloc(out, cap);
      if (!n) {
        free(out);
        return NULL;
      }
      out = n;
    }
    out[len++] = ch;
  }
  out[len] = '\0';
  return out;
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

static int json_get_bool(const char *json, const char *key, int *out) {
  char pat[96];
  snprintf(pat, sizeof(pat), "\"%s\"", key);
  const char *p = strstr(json, pat);
  if (!p) return 0;
  p = strchr(p + strlen(pat), ':');
  if (!p) return 0;
  p++;
  while (*p && isspace((unsigned char)*p)) p++;
  if (strncmp(p, "true", 4) == 0) {
    *out = 1;
    return 1;
  }
  if (strncmp(p, "false", 5) == 0) {
    *out = 0;
    return 1;
  }
  return 0;
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

static void respond_err(const char *id, int code, const char *message) {
  char *msg_esc = json_escape_dup(message ? message : "error");
  int id_num = 1;
  for (const char *c = id; c && *c; c++)
    if (!isdigit((unsigned char)*c) && *c != '-') {
      id_num = 0;
      break;
    }
  char body[1536];
  if (id_num && id)
    snprintf(body, sizeof(body),
             "{\"jsonrpc\":\"2.0\",\"id\":%s,\"error\":{\"code\":%d,"
             "\"message\":\"%s\"}}",
             id, code, msg_esc ? msg_esc : "error");
  else
    snprintf(body, sizeof(body),
             "{\"jsonrpc\":\"2.0\",\"id\":\"%s\",\"error\":{\"code\":%d,"
             "\"message\":\"%s\"}}",
             id ? id : "0", code, msg_esc ? msg_esc : "error");
  free(msg_esc);
  lsp_send(body);
}

static void respond_ok(const char *id, const char *result_json) {
  int id_num = 1;
  for (const char *c = id; c && *c; c++)
    if (!isdigit((unsigned char)*c) && *c != '-') {
      id_num = 0;
      break;
    }

  const char *res = result_json ? result_json : "null";
  char *body = NULL;
  if (id_num && id) {
    size_t need = strlen(id) + strlen(res) + 64;
    body = malloc(need);
    if (!body) return;
    snprintf(body, need, "{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":%s}", id,
             res);
  } else {
    char *id_esc = json_escape_dup(id ? id : "0");
    size_t need = (id_esc ? strlen(id_esc) : 1) + strlen(res) + 64;
    body = malloc(need);
    if (!body) {
      free(id_esc);
      return;
    }
    snprintf(body, need, "{\"jsonrpc\":\"2.0\",\"id\":\"%s\",\"result\":%s}",
             id_esc ? id_esc : "0", res);
    free(id_esc);
  }
  lsp_send(body);
  free(body);
}

static void notify(const char *method, const char *params_json) {
  char *meth_esc = json_escape_dup(method ? method : "");
  size_t plen = params_json ? strlen(params_json) : 4;
  size_t mlen = meth_esc ? strlen(meth_esc) : 0;
  size_t need = mlen + plen + 64;
  char *body = malloc(need);
  if (!body) {
    free(meth_esc);
    return;
  }
  snprintf(body, need, "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s}",
           meth_esc ? meth_esc : "", params_json ? params_json : "null");
  lsp_send(body);
  free(body);
  free(meth_esc);
}

static int severity_of(DiagLevel lvl) {
  if (lvl == DIAG_ERROR) return 1;
  if (lvl == DIAG_WARN) return 2;
  return 3;
}

static void publish_diagnostics(const char *uri, LspDoc *d) {
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

static const char *lsp_overlay(const char *abs, void *ud) {
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

static int lsp_extra_paths(const char **out, int maxn) {
  int n = 0;
  for (int i = 0; i < g_ndocs && n < maxn; i++) {
    if (g_docs[i].path) out[n++] = g_docs[i].path;
  }
  return n;
}

static int loc_cmp(const void *a, const void *b) {
  const SymbolLoc *x = a, *y = b;
  const char *pa = x->path ? x->path : "";
  const char *pb = y->path ? y->path : "";
  int c = strcmp(pa, pb);
  if (c) return c;
  if (x->line != y->line) return x->line - y->line;
  return x->col - y->col;
}

static void abs_uri(const char *path, char *out, size_t n) {
  char *norm = path ? fs_norm_path(path) : NULL;
  path_to_uri(norm ? norm : (path ? path : ""), out, n);
  free(norm);
}

static int parse_pos_uri(const char *msg, char *uri, size_t uri_sz, int *line,
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

static int ident_range(const char *text, int line, int character, char *name,
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

static int cursor_component(const char *id, const char *msg, char *name,
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

static void handle_references(const char *id, const char *msg) {
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

static void handle_prepare_rename(const char *id, const char *msg) {
  char uri[1024];
  int line = 0, character = 0;
  parse_pos_uri(msg, uri, sizeof(uri), &line, &character);
  LspDoc *d = doc_find(uri);
  if (d) doc_ensure_text(d);
  char name[128];
  int sc = 0, ec = 0;
  if (!ident_range(d ? d->text : NULL, line, character, name, sizeof(name), &sc,
                   &ec) ||
      !d || !d->path ||
      !symbols_name_is_component(d->path, name, lsp_overlay, NULL)) {
    respond_err(id, -32602, "not a renamable component");
    return;
  }
  char *ph = json_escape_dup(name);
  char result[512];
  snprintf(result, sizeof(result),
           "{\"range\":{\"start\":{\"line\":%d,\"character\":%d},"
           "\"end\":{\"line\":%d,\"character\":%d}},\"placeholder\":\"%s\"}",
           line, sc, line, ec, ph ? ph : "");
  free(ph);
  respond_ok(id, result);
}

static char *renamed_file_path(const char *old_path, const char *new_name) {
  if (!old_path || !new_name) return NULL;
  char *dir = fs_dirname(old_path);
  if (!dir) return NULL;
  char fname[256];
  snprintf(fname, sizeof(fname), "%s.cord", new_name);
  char *joined = fs_join(dir, fname);
  free(dir);
  return joined;
}

static int basename_stem_eq(const char *path, const char *name) {
  char *base = fs_basename(path);
  if (!base) return 0;
  size_t n = strlen(base);
  if (n > 5 && strcmp(base + n - 5, ".cord") == 0) base[n - 5] = '\0';
  int eq = name && strcmp(base, name) == 0;
  free(base);
  return eq;
}

static void handle_rename(const char *id, const char *msg) {
  char name[128];
  LspDoc *d = NULL;
  if (!cursor_component(id, msg, name, sizeof(name), &d, 1)) return;

  char new_name[128];
  new_name[0] = '\0';
  json_get_str(msg, "newName", new_name, sizeof(new_name));
  char err[256];
  int rc = symbols_rename_check(d->path, name, new_name, lsp_overlay, NULL, err,
                                sizeof(err));
  if (rc != 0) {
    respond_err(id, -32602, err[0] ? err : "invalid component name");
    return;
  }

  const char *extra[MAX_DOCS];
  int n_extra = lsp_extra_paths(extra, MAX_DOCS);
  SymbolLocList locs;
  symbols_collect_refs(d->path, name, 1, lsp_overlay, NULL, extra, n_extra,
                       &locs);
  if (locs.len > 1)
    qsort(locs.items, locs.len, sizeof(SymbolLoc), loc_cmp);

  int rename_file = locs.decl_path && basename_stem_eq(locs.decl_path, name) &&
                    strcmp(name, new_name) != 0;

  char *new_text_esc = json_escape_dup(new_name);
  size_t cap = 4096, len = 0;
  char *out = malloc(cap);
  if (!out) {
    free(new_text_esc);
    symbol_loc_list_free(&locs);
    respond_err(id, -32603, "out of memory");
    return;
  }

  if (rename_file) {
    strcpy(out, "{\"documentChanges\":[");
    len = strlen(out);
  } else {
    strcpy(out, "{\"changes\":{");
    len = strlen(out);
  }

  /* Group consecutive locs by path. */
  size_t i = 0;
  int file_n = 0;
  while (i < locs.len) {
    const char *path = locs.items[i].path;
    size_t j = i;
    while (j < locs.len && locs.items[j].path &&
           strcmp(locs.items[j].path, path) == 0)
      j++;

    char file_uri[1024];
    abs_uri(path, file_uri, sizeof(file_uri));
    char *uri_esc = json_escape_dup(file_uri);

    size_t chunk_cap = 2048, chunk_len = 0;
    char *chunk = malloc(chunk_cap);
    if (!chunk) {
      free(uri_esc);
      break;
    }
    chunk[0] = '\0';
    int edit_n = 0;
    for (size_t k = i; k < j; k++) {
      SymbolLoc *it = &locs.items[k];
      if (it->is_path && !rename_file) continue;
      int sl = it->line > 0 ? it->line - 1 : 0;
      int sc = it->col > 0 ? it->col - 1 : 0;
      char item[512];
      snprintf(item, sizeof(item),
               "%s{\"range\":{\"start\":{\"line\":%d,\"character\":%d},"
               "\"end\":{\"line\":%d,\"character\":%d}},\"newText\":\"%s\"}",
               edit_n ? "," : "", sl, sc, sl, sc + it->length,
               new_text_esc ? new_text_esc : "");
      size_t il = strlen(item);
      if (chunk_len + il + 1 >= chunk_cap) {
        chunk_cap = (chunk_len + il + 1) * 2;
        char *nc = realloc(chunk, chunk_cap);
        if (!nc) break;
        chunk = nc;
      }
      memcpy(chunk + chunk_len, item, il);
      chunk_len += il;
      chunk[chunk_len] = '\0';
      edit_n++;
    }

    if (edit_n > 0) {
      char header[1536];
      if (rename_file)
        snprintf(header, sizeof(header),
                 "%s{\"textDocument\":{\"uri\":\"%s\",\"version\":null},"
                 "\"edits\":[",
                 file_n ? "," : "", uri_esc ? uri_esc : "");
      else
        snprintf(header, sizeof(header), "%s\"%s\":[", file_n ? "," : "",
                 uri_esc ? uri_esc : "");
      size_t need = strlen(header) + chunk_len + 8;
      if (len + need >= cap) {
        while (len + need >= cap) cap *= 2;
        char *nbuf = realloc(out, cap);
        if (!nbuf) {
          free(uri_esc);
          free(chunk);
          break;
        }
        out = nbuf;
      }
      memcpy(out + len, header, strlen(header));
      len += strlen(header);
      memcpy(out + len, chunk, chunk_len);
      len += chunk_len;
      out[len++] = ']';
      if (rename_file) out[len++] = '}';
      out[len] = '\0';
      file_n++;
    }
    free(uri_esc);
    free(chunk);
    i = j;
  }

  if (rename_file && locs.decl_path) {
    char old_uri[1024], new_uri[1024];
    abs_uri(locs.decl_path, old_uri, sizeof(old_uri));
    char *np = renamed_file_path(locs.decl_path, new_name);
    if (np) abs_uri(np, new_uri, sizeof(new_uri));
    else new_uri[0] = '\0';
    free(np);
    char *old_esc = json_escape_dup(old_uri);
    char *new_esc = json_escape_dup(new_uri);
    char item[2048];
    snprintf(item, sizeof(item),
             "%s{\"kind\":\"rename\",\"oldUri\":\"%s\",\"newUri\":\"%s\"}",
             file_n ? "," : "", old_esc ? old_esc : "",
             new_esc ? new_esc : "");
    free(old_esc);
    free(new_esc);
    size_t il = strlen(item);
    if (len + il + 4 >= cap) {
      cap = len + il + 4;
      char *nbuf = realloc(out, cap);
      if (nbuf) out = nbuf;
    }
    if (len + il + 4 <= cap) {
      memcpy(out + len, item, il);
      len += il;
      out[len] = '\0';
    }
  }

  const char *tail = rename_file ? "]}" : "}}";
  size_t tl = strlen(tail);
  if (len + tl + 1 >= cap) {
    cap = len + tl + 1;
    char *nbuf = realloc(out, cap);
    if (nbuf) out = nbuf;
  }
  if (len + tl + 1 <= cap) memcpy(out + len, tail, tl + 1);

  respond_ok(id, out);
  free(out);
  free(new_text_esc);
  symbol_loc_list_free(&locs);
}

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
static void handle_completion(const char *id, const char *msg) {
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

static void handle_formatting(const char *id, const char *msg) {
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

static void handle_hover(const char *id, const char *msg) {
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
  if (!name[0]) {
    respond_ok(id, "null");
    return;
  }

  char md[1024];
  md[0] = '\0';
  if (cord_is_builtin_tag(name)) {
    snprintf(md, sizeof(md),
             "**`%s`** — built-in Cordlang tag\\n\\nLayout/control tag. Attrs: "
             "style (`gap`, `p`, …) + DOM (`class`, `to`, `bind`, …). See "
             "`docs/schema/attrs.json`.",
             name);
  } else if (cord_is_style_attr(name)) {
    snprintf(md, sizeof(md),
             "**`%s`** — style attribute\\n\\nMaps to utility classes / CSS. "
             "Schema v1.0.",
             name);
  } else if (cord_is_dom_attr(name) || strncmp(name, "aria-", 5) == 0 ||
             strncmp(name, "data-", 5) == 0) {
    snprintf(md, sizeof(md),
             "**`%s`** — DOM / Cord attribute\\n\\nKnown by `cordlang check`.",
             name);
  } else if (strcmp(name, "state") == 0) {
    snprintf(md, sizeof(md),
             "**`state`** — local reactive state\\n\\n`state count=0` → "
             "`setCount(...)`. Not `useState`.");
  } else if (strcmp(name, "props") == 0) {
    snprintf(md, sizeof(md),
             "**`props`** — component props\\n\\n`props title: string = \"\"`. "
             "Types: string|number|boolean|any.");
  } else if (strcmp(name, "computed") == 0 || strcmp(name, "def") == 0 ||
             strcmp(name, "route") == 0 || strcmp(name, "layout") == 0 ||
             strcmp(name, "theme") == 0 || strcmp(name, "fetch") == 0) {
    snprintf(md, sizeof(md),
             "**`%s`** — Cordlang keyword\\n\\nSee `docs/SPEC.md` (v1.0).",
             name);
  } else if (d && (d->text || d->path)) {
    /* Try prop type from component under cursor via buffer or disk */
    CompileResult r = {0};
    if (d->text)
      r = compiler_parse_source(d->text, strlen(d->text));
    else if (d->path)
      r = compiler_parse_project(d->path);
    if (r.ok && r.ast && r.ast->root) {
      Node *root = r.ast->root;
      for (size_t i = 0; i < root->children_len; i++) {
        Node *c = root->children[i];
        if (!c || c->type != NODE_COMPONENT_DEF) continue;
        for (size_t j = 0; j < c->children_len; j++) {
          Node *props = c->children[j];
          if (!props || props->type != NODE_PROPS_DECL) continue;
          for (size_t pi = 0; pi < props->children_len; pi++) {
            Node *ch = props->children[pi];
            if (!ch || !ch->value) continue;
            if (strcmp(ch->value, name) != 0) continue;
            const char *ty = "any";
            for (size_t k = 0; k < ch->children_len; k++) {
              Node *a = ch->children[k];
              if (a && a->type == NODE_ATTR && a->value &&
                  strcmp(a->value, "type") == 0 && a->value2)
                ty = a->value2;
            }
            snprintf(md, sizeof(md),
                     "**props `%s`**: `%s`\\n\\nValidated by `cordlang check`.",
                     name, ty);
            break;
          }
          if (md[0]) break;
        }
      }
    }
    compiler_result_free(&r);
  }
  if (!md[0]) {
    snprintf(md, sizeof(md), "`%s`", name);
  }
  char *esc = json_escape_dup(md);
  char result[2048];
  snprintf(result, sizeof(result),
           "{\"contents\":{\"kind\":\"markdown\",\"value\":\"%s\"}}",
           esc ? esc : "");
  free(esc);
  respond_ok(id, result);
}

/* Extract 'Name' from messages like: JSX attribute 'className' is not Cordlang */
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

static void handle_code_action(const char *id, const char *msg) {
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

static void handle_message(const char *msg) {
  const char *method = json_method(msg);
  char id[64];
  int has_id = json_id(msg, id, sizeof(id));

  if (!method) return;

  if (strcmp(method, "initialize") == 0) {
    char caps[512];
    snprintf(caps, sizeof(caps),
             "{\"capabilities\":{"
             "\"textDocumentSync\":1,"
             "\"documentSymbolProvider\":true,"
             "\"definitionProvider\":true,"
             "\"referencesProvider\":true,"
             "\"renameProvider\":{\"prepareProvider\":true},"
             "\"completionProvider\":{\"triggerCharacters\":[\" \",\"=\",\"@\"]},"
             "\"hoverProvider\":true,"
             "\"codeActionProvider\":true,"
             "\"documentFormattingProvider\":true"
             "},\"serverInfo\":{\"name\":\"cordlang\",\"version\":\"%s\"}}",
             CORDLANG_VERSION);
    respond_ok(has_id ? id : "0", caps);
    return;
  }
  if (strcmp(method, "initialized") == 0) return;
  if (strcmp(method, "shutdown") == 0) {
    g_shutdown = 1;
    docs_free();
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
    /* didOpen/didChange may include text — parse on heap (never 4MB stack). */
    const char *tp = strstr(msg, "\"text\"");
    if (tp) text = json_get_str_dup(tp, "text");
    LspDoc *d = doc_upsert(uri, text);
    free(text);
    if (d) publish_diagnostics(uri, d);
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
  if (strcmp(method, "textDocument/references") == 0 && has_id) {
    handle_references(id, msg);
    return;
  }
  if (strcmp(method, "textDocument/prepareRename") == 0 && has_id) {
    handle_prepare_rename(id, msg);
    return;
  }
  if (strcmp(method, "textDocument/rename") == 0 && has_id) {
    handle_rename(id, msg);
    return;
  }
  if (strcmp(method, "textDocument/completion") == 0 && has_id) {
    handle_completion(id, msg);
    return;
  }
  if (strcmp(method, "textDocument/hover") == 0 && has_id) {
    handle_hover(id, msg);
    return;
  }
  if (strcmp(method, "textDocument/codeAction") == 0 && has_id) {
    handle_code_action(id, msg);
    return;
  }
  if (strcmp(method, "textDocument/formatting") == 0 && has_id) {
    handle_formatting(id, msg);
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
