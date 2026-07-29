#include "adapters/outbound/fonts/font_cache.h"
#include "application/ports/fs_port.h"
#include "adapters/outbound/process/process_spawn.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FONT_UA                                                                \
  "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "              \
  "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"

static char *strip_quotes_dup(const char *s) {
  if (!s) return NULL;
  size_t n = strlen(s);
  if (n >= 2 && ((s[0] == '"' && s[n - 1] == '"') ||
                 (s[0] == '\'' && s[n - 1] == '\''))) {
    char *o = malloc(n - 1);
    if (!o) return NULL;
    memcpy(o, s + 1, n - 2);
    o[n - 2] = '\0';
    return o;
  }
  return strdup(s);
}

char *font_cache_dir(void) {
  char base[1024];
  base[0] = '\0';
#ifdef _WIN32
  const char *home = getenv("USERPROFILE");
  if (!home || !*home) home = getenv("HOME");
  if (home && *home)
    snprintf(base, sizeof(base), "%s\\.cordlang\\cache\\fonts", home);
  else
    snprintf(base, sizeof(base), ".cordlang\\cache\\fonts");
#else
  const char *home = getenv("HOME");
  if (home && *home)
    snprintf(base, sizeof(base), "%s/.cordlang/cache/fonts", home);
  else
    snprintf(base, sizeof(base), ".cordlang/cache/fonts");
#endif
  fs_mkdir_p(base);
  return strdup(base);
}

void font_cache_slug(const char *family, char *buf, size_t n) {
  if (!buf || n == 0) return;
  size_t j = 0;
  int prev_dash = 1;
  for (const char *p = family ? family : ""; *p && j + 1 < n; p++) {
    unsigned char c = (unsigned char)*p;
    if (isalnum(c)) {
      buf[j++] = (char)tolower(c);
      prev_dash = 0;
    } else if (!prev_dash) {
      buf[j++] = '-';
      prev_dash = 1;
    }
  }
  while (j > 0 && buf[j - 1] == '-') j--;
  buf[j] = '\0';
  if (j == 0) {
    snprintf(buf, n, "font");
  }
}

char *font_cache_path_if_exists(const char *slug_weight_woff2) {
  if (!slug_weight_woff2 || !*slug_weight_woff2) return NULL;
  char *dir = font_cache_dir();
  if (!dir) return NULL;
  char *path = fs_join(dir, slug_weight_woff2);
  free(dir);
  if (!path) return NULL;
  if (!fs_exists(path) || fs_is_dir(path)) {
    free(path);
    return NULL;
  }
  return path;
}

static int file_nonempty(const char *path) {
  size_t len = 0;
  char *b = fs_read_file(path, &len);
  if (!b) return 0;
  free(b);
  return len > 64;
}

static void url_encode_family(const char *family, char *out, size_t n) {
  size_t j = 0;
  for (const char *p = family; *p && j + 4 < n; p++) {
    unsigned char c = (unsigned char)*p;
    if (c == ' ') {
      out[j++] = '+';
    } else if (isalnum(c) || c == '-' || c == '.') {
      out[j++] = (char)c;
    } else {
      int w = snprintf(out + j, n - j, "%%%02X", c);
      if (w > 0) j += (size_t)w;
    }
  }
  out[j] = '\0';
}

static int extract_woff2_url(const char *css, char *url, size_t n) {
  if (!css || !url || n < 16) return -1;
  const char *p = css;
  while ((p = strstr(p, "url(")) != NULL) {
    p += 4;
    while (*p == ' ' || *p == '"' || *p == '\'') p++;
    const char *end = p;
    while (*end && *end != ')' && *end != '"' && *end != '\'' && *end != ' ')
      end++;
    size_t len = (size_t)(end - p);
    if (len > 12 && len + 1 < n && strstr(p, "woff2") != NULL) {
      memcpy(url, p, len);
      url[len] = '\0';
      return 0;
    }
    if (len > 12 && len + 1 < n && strncmp(p, "http", 4) == 0) {
      /* Prefer any gstatic url if woff2 token not in path fragment yet */
      memcpy(url, p, len);
      url[len] = '\0';
      if (strstr(url, "gstatic") || strstr(url, "fonts")) return 0;
    }
  }
  return -1;
}

static int curl_download(const char *url, const char *out_path,
                         const char *user_agent) {
  if (!url || !out_path) return -1;
#ifdef _WIN32
  char *argv[] = {"curl.exe", "-fsSL", "-A", (char *)user_agent, "-o",
                  (char *)out_path, (char *)url, NULL};
#else
  char *argv[] = {"curl", "-fsSL", "-A", (char *)user_agent, "-o",
                  (char *)out_path, (char *)url, NULL};
#endif
  int rc = process_run(NULL, argv, 1);
  if (rc != 0) {
    /* Retry without .exe on Windows / bare curl */
    char *argv2[] = {"curl", "-fsSL", "-A", (char *)user_agent, "-o",
                     (char *)out_path, (char *)url, NULL};
    rc = process_run(NULL, argv2, 1);
  }
  return rc == 0 && file_nonempty(out_path) ? 0 : -1;
}

int font_cache_ensure(const char *family, int weight, char *out_path,
                      size_t out_sz) {
  if (!family || !*family || !out_path || out_sz < 8) return -1;
  if (weight < 100 || weight > 900) weight = 400;

  char *clean = strip_quotes_dup(family);
  if (!clean) return -1;

  char slug[128];
  font_cache_slug(clean, slug, sizeof(slug));

  char fname[160];
  snprintf(fname, sizeof(fname), "%s-%d.woff2", slug, weight);

  char *dir = font_cache_dir();
  if (!dir) {
    free(clean);
    return -1;
  }
  char *path = fs_join(dir, fname);
  if (!path) {
    free(dir);
    free(clean);
    return -1;
  }

  if (file_nonempty(path)) {
    snprintf(out_path, out_sz, "%s", path);
    free(path);
    free(dir);
    free(clean);
    return 0;
  }

  /* Fetch CSS face list from Google Fonts CSS API, then the WOFF2. */
  char enc[256];
  url_encode_family(clean, enc, sizeof(enc));
  char css_url[512];
  snprintf(css_url, sizeof(css_url),
           "https://fonts.googleapis.com/css2?family=%s:wght@%d&display=swap",
           enc, weight);

  char *css_tmp = fs_join(dir, "_fetch.css");
  if (!css_tmp) {
    free(path);
    free(dir);
    free(clean);
    return -1;
  }

  if (curl_download(css_url, css_tmp, FONT_UA) != 0) {
    fprintf(stderr,
            "cordlang font: could not fetch CSS for \"%s\" %d (need curl + "
            "network); using system fonts\n",
            clean, weight);
    free(css_tmp);
    free(path);
    free(dir);
    free(clean);
    return -1;
  }

  size_t css_len = 0;
  char *css = fs_read_file(css_tmp, &css_len);
  remove(css_tmp);
  free(css_tmp);
  if (!css) {
    free(path);
    free(dir);
    free(clean);
    return -1;
  }

  char woff_url[1024];
  if (extract_woff2_url(css, woff_url, sizeof(woff_url)) != 0) {
    fprintf(stderr, "cordlang font: no woff2 URL in CSS for \"%s\" %d\n", clean,
            weight);
    free(css);
    free(path);
    free(dir);
    free(clean);
    return -1;
  }
  free(css);

  if (curl_download(woff_url, path, FONT_UA) != 0) {
    fprintf(stderr, "cordlang font: download failed for \"%s\" %d\n", clean,
            weight);
    free(path);
    free(dir);
    free(clean);
    return -1;
  }

  snprintf(out_path, out_sz, "%s", path);
  free(path);
  free(dir);
  free(clean);
  return 0;
}

typedef struct {
  char family[128];
  char role[16]; /* sans | mono | display */
  int weights[4];
  int n_weights;
} FontReq;

static void font_req_set_weights(FontReq *r) {
  if (strcmp(r->role, "mono") == 0) {
    r->weights[0] = 400;
    r->n_weights = 1;
  } else if (strcmp(r->role, "display") == 0) {
    r->weights[0] = 700;
    r->n_weights = 1;
  } else {
    r->weights[0] = 400;
    r->weights[1] = 600;
    r->n_weights = 2;
  }
}

static int add_font_req(FontReq *reqs, int *n, int max, const char *key,
                        const char *val) {
  if (!key || !val || !*val || *n >= max) return 0;
  const char *role = NULL;
  if (strcmp(key, "font") == 0 || strcmp(key, "font-sans") == 0)
    role = "sans";
  else if (strcmp(key, "font-mono") == 0)
    role = "mono";
  else if (strcmp(key, "font-display") == 0)
    role = "display";
  else
    return 0;

  char *clean = strip_quotes_dup(val);
  if (!clean || !*clean) {
    free(clean);
    return 0;
  }
  /* Skip obvious system stacks — no download. */
  if (strcmp(clean, "system-ui") == 0 || strcmp(clean, "sans-serif") == 0 ||
      strcmp(clean, "monospace") == 0 || strcmp(clean, "ui-monospace") == 0 ||
      strcmp(clean, "serif") == 0) {
    free(clean);
    return 0;
  }

  FontReq *r = &reqs[(*n)++];
  memset(r, 0, sizeof(*r));
  snprintf(r->family, sizeof(r->family), "%s", clean);
  snprintf(r->role, sizeof(r->role), "%s", role);
  font_req_set_weights(r);
  free(clean);
  return 1;
}

static int collect_from_ir(const IrProgram *ir, FontReq *reqs, int max) {
  int n = 0;
  if (!ir || !ir->root) return 0;
  for (size_t i = 0; i < ir->root->n_kids; i++) {
    IrNode *c = ir->root->kids[i];
    if (!c || c->kind != IR_HOOK || !c->name || strcmp(c->name, "theme") != 0)
      continue;
    for (size_t j = 0; j < c->n_kids; j++) {
      IrNode *e = c->kids[j];
      if (!e || e->kind != IR_ATTR || !e->name || !e->value) continue;
      add_font_req(reqs, &n, max, e->name, e->value);
    }
    break; /* first theme only for faces */
  }
  return n;
}

static int collect_from_ast(Node *root, FontReq *reqs, int max) {
  int n = 0;
  if (!root) return 0;
  for (size_t i = 0; i < root->children_len; i++) {
    Node *c = root->children[i];
    if (!c || c->type != NODE_THEME) continue;
    for (size_t j = 0; j < c->children_len; j++) {
      Node *e = c->children[j];
      if (!e || e->type != NODE_ATTR || !e->value || !e->value2) continue;
      add_font_req(reqs, &n, max, e->value, e->value2);
    }
    break;
  }
  return n;
}

static char *emit_faces(FontReq *reqs, int n, const char *url_prefix,
                        int resolve) {
  const char *prefix =
      url_prefix && *url_prefix ? url_prefix : "/fonts";
  size_t cap = 1024 + (size_t)n * 512;
  char *out = malloc(cap);
  if (!out) return strdup("/* font faces: oom */\n");
  size_t len = 0;
  out[0] = '\0';

  if (n == 0) return out;

  len += (size_t)snprintf(out + len, cap - len, "/* Cord native font faces */\n");

  for (int i = 0; i < n; i++) {
    FontReq *r = &reqs[i];
    for (int w = 0; w < r->n_weights; w++) {
      int weight = r->weights[w];
      char slug[128];
      font_cache_slug(r->family, slug, sizeof(slug));
      char fname[160];
      snprintf(fname, sizeof(fname), "%s-%d.woff2", slug, weight);

      if (resolve) {
        char abs[1024];
        if (font_cache_ensure(r->family, weight, abs, sizeof(abs)) != 0)
          continue;
      } else {
        char *existing = font_cache_path_if_exists(fname);
        if (!existing) continue;
        free(existing);
      }

      int need = snprintf(NULL, 0,
                          "@font-face {\n"
                          "  font-family: \"%s\";\n"
                          "  font-style: normal;\n"
                          "  font-weight: %d;\n"
                          "  font-display: swap;\n"
                          "  src: url(\"%s/%s\") format(\"woff2\");\n"
                          "}\n",
                          r->family, weight, prefix, fname);
      if (need < 0) continue;
      if (len + (size_t)need + 1 > cap) {
        cap = len + (size_t)need + 1024;
        char *nbuf = realloc(out, cap);
        if (!nbuf) break;
        out = nbuf;
      }
      len += (size_t)snprintf(out + len, cap - len,
                              "@font-face {\n"
                              "  font-family: \"%s\";\n"
                              "  font-style: normal;\n"
                              "  font-weight: %d;\n"
                              "  font-display: swap;\n"
                              "  src: url(\"%s/%s\") format(\"woff2\");\n"
                              "}\n",
                              r->family, weight, prefix, fname);
    }
  }
  if (len > 0 && out[len - 1] != '\n') {
    if (len + 2 < cap) {
      out[len++] = '\n';
      out[len] = '\0';
    }
  }
  return out;
}

char *font_faces_css_from_ir(const IrProgram *ir, const char *url_prefix,
                             int resolve) {
  FontReq reqs[8];
  int n = collect_from_ir(ir, reqs, 8);
  return emit_faces(reqs, n, url_prefix, resolve);
}

char *font_faces_css_from_ast(Node *root, const char *url_prefix, int resolve) {
  FontReq reqs[8];
  int n = collect_from_ast(root, reqs, 8);
  return emit_faces(reqs, n, url_prefix, resolve);
}

static int install_reqs(FontReq *reqs, int n, const char *dest_dir) {
  if (!dest_dir || !*dest_dir) return -1;
  if (n == 0) return 0;
  fs_mkdir_p(dest_dir);
  for (int i = 0; i < n; i++) {
    FontReq *r = &reqs[i];
    for (int w = 0; w < r->n_weights; w++) {
      char abs[1024];
      if (font_cache_ensure(r->family, r->weights[w], abs, sizeof(abs)) != 0)
        continue;
      char slug[128];
      font_cache_slug(r->family, slug, sizeof(slug));
      char fname[160];
      snprintf(fname, sizeof(fname), "%s-%d.woff2", slug, r->weights[w]);
      char *dst = fs_join(dest_dir, fname);
      if (!dst) continue;
      if (fs_copy_file(abs, dst) != 0)
        fprintf(stderr, "cordlang font: copy failed %s → %s\n", abs, dst);
      free(dst);
    }
  }
  return 0;
}

int font_cache_install_from_ir(const IrProgram *ir, const char *dest_dir) {
  FontReq reqs[8];
  int n = collect_from_ir(ir, reqs, 8);
  return install_reqs(reqs, n, dest_dir);
}

int font_cache_install_from_ast(Node *root, const char *dest_dir) {
  FontReq reqs[8];
  int n = collect_from_ast(root, reqs, 8);
  return install_reqs(reqs, n, dest_dir);
}
