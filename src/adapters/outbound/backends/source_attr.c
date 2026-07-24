#include "adapters/outbound/backends/source_attr.h"
#include "application/ports/fs_port.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int path_exists_file(const char *path) {
  return path && fs_exists(path) && !fs_is_dir(path);
}

void cord_guess_source_path(const char *unit_name, const char *kind_dir,
                            char *out, size_t out_sz) {
  if (!out || out_sz == 0) return;
  out[0] = '\0';
  if (!unit_name || !*unit_name) {
    snprintf(out, out_sz, "src/app.cord");
    return;
  }

  const char *dir = kind_dir && *kind_dir ? kind_dir : "components";
  char candidates[8][256];
  int n = 0;

  if (strcmp(dir, "layouts") == 0 && strcmp(unit_name, "DefaultLayout") == 0) {
    snprintf(candidates[n++], sizeof(candidates[0]),
             "src/layouts/default.cord");
  }

  snprintf(candidates[n++], sizeof(candidates[0]), "src/%s/%s.cord", dir,
           unit_name);

  if (strcmp(dir, "layouts") == 0 && unit_name[0]) {
    char lower[96];
    snprintf(lower, sizeof(lower), "%s", unit_name);
    if (lower[0] >= 'A' && lower[0] <= 'Z')
      lower[0] = (char)(lower[0] - 'A' + 'a');
    if (strcmp(lower, "defaultlayout") != 0)
      snprintf(candidates[n++], sizeof(candidates[0]), "src/layouts/%s.cord",
               lower);
  }

  snprintf(candidates[n++], sizeof(candidates[0]), "%s/%s.cord", dir, unit_name);
  if (strcmp(dir, "layouts") == 0 && strcmp(unit_name, "DefaultLayout") == 0)
    snprintf(candidates[n++], sizeof(candidates[0]), "layouts/default.cord");

  for (int i = 0; i < n; i++) {
    if (path_exists_file(candidates[i])) {
      snprintf(out, out_sz, "%s", candidates[i]);
      return;
    }
  }

  snprintf(out, out_sz, "%s", candidates[0]);
}

static void path_to_posix(const char *src, char *dst, size_t dst_sz) {
  if (!dst || dst_sz == 0) return;
  size_t o = 0;
  if (src) {
    for (; src[o] && o + 1 < dst_sz; o++)
      dst[o] = (src[o] == '\\') ? '/' : src[o];
  }
  dst[o] = '\0';
}

static const char *VLQ_B64 =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

typedef struct {
  char *buf;
  size_t len, cap;
} MapBuf;

static void mb_init(MapBuf *m) {
  m->cap = 4096;
  m->len = 0;
  m->buf = malloc(m->cap);
  if (m->buf) m->buf[0] = '\0';
}

static int mb_grow(MapBuf *m, size_t need) {
  if (!m->buf) return -1;
  while (m->len + need + 1 >= m->cap) {
    size_t ncap = m->cap * 2;
    char *n = realloc(m->buf, ncap);
    if (!n) return -1;
    m->buf = n;
    m->cap = ncap;
  }
  return 0;
}

static void mb_putc(MapBuf *m, char c) {
  if (mb_grow(m, 1) != 0) return;
  m->buf[m->len++] = c;
  m->buf[m->len] = '\0';
}

static void mb_puts(MapBuf *m, const char *s) {
  if (!s) return;
  size_t n = strlen(s);
  if (mb_grow(m, n) != 0) return;
  memcpy(m->buf + m->len, s, n);
  m->len += n;
  m->buf[m->len] = '\0';
}

static void vlq_encode(MapBuf *m, int value) {
  unsigned int v =
      (unsigned int)((value < 0) ? (((unsigned int)(-value)) << 1) | 1u
                                 : ((unsigned int)value) << 1);
  do {
    unsigned int digit = v & 31u;
    v >>= 5;
    if (v) digit |= 32u;
    mb_putc(m, VLQ_B64[digit]);
  } while (v);
}

static int find_source_idx(char sources[][512], int *n_sources, int max,
                           const char *path) {
  char posix[512];
  path_to_posix(path, posix, sizeof(posix));
  for (int i = 0; i < *n_sources; i++) {
    if (strcmp(sources[i], posix) == 0) return i;
  }
  if (*n_sources >= max) return -1;
  snprintf(sources[*n_sources], sizeof(sources[0]), "%s", posix);
  return (*n_sources)++;
}

static int extract_source_marker(const char *line, char *out, size_t out_sz) {
  const char *p = strstr(line, "source=");
  if (!p) return 0;
  p += 7;
  size_t i = 0;
  while (*p && !isspace((unsigned char)*p) && *p != '*' && *p != '-' &&
         *p != '>' && i + 1 < out_sz) {
    out[i++] = *p++;
  }
  out[i] = '\0';
  return i > 0;
}

char *cord_sourcemap_stub(const char *generated_file,
                          const char *const *sources, int n_sources) {
  size_t cap = 256;
  for (int i = 0; i < n_sources; i++) {
    if (sources[i]) cap += strlen(sources[i]) + 8;
  }
  if (generated_file) cap += strlen(generated_file) + 8;

  char *buf = malloc(cap);
  if (!buf) return NULL;

  char file_posix[512];
  path_to_posix(generated_file ? generated_file : "out", file_posix,
                sizeof(file_posix));

  size_t o = 0;
  int n = snprintf(buf + o, cap - o,
                   "{\n"
                   "  \"version\": 3,\n"
                   "  \"file\": \"%s\",\n"
                   "  \"sources\": [",
                   file_posix);
  if (n < 0) {
    free(buf);
    return NULL;
  }
  o += (size_t)n;

  int first = 1;
  for (int i = 0; i < n_sources; i++) {
    if (!sources[i] || !*sources[i]) continue;
    char sp[512];
    path_to_posix(sources[i], sp, sizeof(sp));
    n = snprintf(buf + o, cap - o, "%s\"%s\"", first ? "" : ", ", sp);
    first = 0;
    if (n < 0 || (size_t)n >= cap - o) break;
    o += (size_t)n;
  }

  n = snprintf(buf + o, cap - o,
               "],\n"
               "  \"mappings\": \"\"\n"
               "}\n");
  if (n > 0) o += (size_t)n;
  (void)o;
  return buf;
}

char *cord_sourcemap_build(const char *generated_file, const char *content) {
  if (!content) return cord_sourcemap_stub(generated_file, NULL, 0);

  char sources[64][512];
  int n_sources = 0;
  int cur_src = -1;
  int approx_line = 0;
  int prev_gen_col = 0, prev_src = 0, prev_oline = 0, prev_ocol = 0;

  MapBuf map;
  mb_init(&map);
  if (!map.buf) return NULL;

  int first_line = 1;
  const char *p = content;
  for (;;) {
    const char *eol = strchr(p, '\n');
    size_t llen = eol ? (size_t)(eol - p) : strlen(p);
    char line[2048];
    if (llen >= sizeof(line)) llen = sizeof(line) - 1;
    memcpy(line, p, llen);
    line[llen] = '\0';

    if (!first_line) mb_putc(&map, ';');
    first_line = 0;

    char src_path[512];
    if (extract_source_marker(line, src_path, sizeof(src_path))) {
      cur_src = find_source_idx(sources, &n_sources, 64, src_path);
      approx_line = 0;
      prev_gen_col = prev_src = prev_oline = prev_ocol = 0;
    } else if (cur_src >= 0 && line[0] &&
               strstr(line, "cordlang: source=") == NULL) {
      vlq_encode(&map, 0 - prev_gen_col);
      vlq_encode(&map, cur_src - prev_src);
      vlq_encode(&map, approx_line - prev_oline);
      vlq_encode(&map, 0 - prev_ocol);
      prev_gen_col = 0;
      prev_src = cur_src;
      prev_oline = approx_line;
      prev_ocol = 0;
      approx_line++;
    }

    if (!eol) break;
    p = eol + 1;
  }

  char file_posix[512];
  path_to_posix(generated_file ? generated_file : "out", file_posix,
                sizeof(file_posix));

  MapBuf json;
  mb_init(&json);
  if (!json.buf) {
    free(map.buf);
    return NULL;
  }
  mb_puts(&json, "{\n  \"version\": 3,\n  \"file\": \"");
  mb_puts(&json, file_posix);
  mb_puts(&json, "\",\n  \"sources\": [");
  for (int i = 0; i < n_sources; i++) {
    if (i) mb_puts(&json, ", ");
    mb_putc(&json, '"');
    mb_puts(&json, sources[i]);
    mb_putc(&json, '"');
  }
  mb_puts(&json, "],\n  \"mappings\": \"");
  mb_puts(&json, map.buf ? map.buf : "");
  mb_puts(&json, "\"\n}\n");

  free(map.buf);
  return json.buf;
}

int cord_write_sourcemap_file(const char *generated_path,
                              const char *const *sources, int n_sources) {
  if (!generated_path) return -1;

  char *base = fs_basename(generated_path);
  const char *file_field = base ? base : "out";

  size_t glen = 0;
  char *content = fs_read_file(generated_path, &glen);
  char *json = NULL;
  if (content) {
    json = cord_sourcemap_build(file_field, content);
    free(content);
  }
  if (!json) json = cord_sourcemap_stub(file_field, sources, n_sources);
  free(base);
  if (!json) return -1;

  size_t len = strlen(generated_path);
  char *map_path = malloc(len + 5);
  if (!map_path) {
    free(json);
    return -1;
  }
  memcpy(map_path, generated_path, len);
  memcpy(map_path + len, ".map", 5);

  int rc = fs_write_file(map_path, json);
  if (rc == 0) printf("Wrote %s\n", map_path);
  free(map_path);
  free(json);
  return rc;
}
