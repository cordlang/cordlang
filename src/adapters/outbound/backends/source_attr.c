#include "adapters/outbound/backends/source_attr.h"
#include "application/ports/fs_port.h"
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

  /* layouts/default.cord → unit DefaultLayout (prefer convention first) */
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

  /* Without src/ prefix */
  snprintf(candidates[n++], sizeof(candidates[0]), "%s/%s.cord", dir, unit_name);
  if (strcmp(dir, "layouts") == 0 && strcmp(unit_name, "DefaultLayout") == 0)
    snprintf(candidates[n++], sizeof(candidates[0]), "layouts/default.cord");

  for (int i = 0; i < n; i++) {
    if (path_exists_file(candidates[i])) {
      snprintf(out, out_sz, "%s", candidates[i]);
      return;
    }
  }

  /* Best-effort: first candidate (DefaultLayout → default.cord) */
  snprintf(out, out_sz, "%s", candidates[0]);
}

/* Copy path with \\ → / into dst (null-terminated). */
static void path_to_posix(const char *src, char *dst, size_t dst_sz) {
  if (!dst || dst_sz == 0) return;
  size_t o = 0;
  if (src) {
    for (; src[o] && o + 1 < dst_sz; o++)
      dst[o] = (src[o] == '\\') ? '/' : src[o];
  }
  dst[o] = '\0';
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

int cord_write_sourcemap_file(const char *generated_path,
                              const char *const *sources, int n_sources) {
  if (!generated_path) return -1;

  char *base = fs_basename(generated_path);
  const char *file_field = base ? base : "out";

  char *json = cord_sourcemap_stub(file_field, sources, n_sources);
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
