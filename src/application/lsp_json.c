#include "application/lsp_internal.h"

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

 char *json_escape_dup(const char *s) {
  size_t len = 0, cap = 0;
  char *buf = NULL;
  json_escape_append(&buf, &len, &cap, s ? s : "");
  if (!buf) {
    buf = strdup("");
  }
  return buf;
}

 void lsp_send(const char *body) {
  if (!body) return;
  /* Byte-oriented write: Windows text-mode stdout would turn \n into \r\n
   * and break LSP framing (Content-Length: …\r\n\r\n → \r\r\n\r\r\n). */
  char header[64];
  int hlen =
      snprintf(header, sizeof(header), "Content-Length: %zu\r\n\r\n", strlen(body));
  if (hlen > 0) fwrite(header, 1, (size_t)hlen, stdout);
  fwrite(body, 1, strlen(body), stdout);
  fflush(stdout);
}

/* Extract JSON string value for "key" near start of object (best-effort). */
 int json_get_str(const char *json, const char *key, char *out,
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
 char *json_get_str_dup(const char *json, const char *key) {
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

 int json_get_int(const char *json, const char *key, int *out) {
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

 int json_get_bool(const char *json, const char *key, int *out) {
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

 const char *json_method(const char *msg) {
  static char method[128];
  if (!json_get_str(msg, "method", method, sizeof(method))) return NULL;
  return method;
}

 int json_id(const char *msg, char *out, size_t out_sz) {
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

 void respond_err(const char *id, int code, const char *message) {
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

 void respond_ok(const char *id, const char *result_json) {
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

 void notify(const char *method, const char *params_json) {
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

