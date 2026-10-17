#include "adapters/outbound/backends/preset_registry.h"
#include "application/ports/fs_port.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Insert "\"pkg\": \"^version\"" lines into dependencies (create section if needed). */
static int inject_deps(char **pkg_json, const char *const *pkgs, int npkgs) {
  if (!pkg_json || !*pkg_json || npkgs <= 0) return 0;
  char *json = *pkg_json;

  if (!strstr(json, "\"dependencies\"")) {
    const char *end = strrchr(json, '}');
    if (!end) return 1;
    size_t at = (size_t)(end - json);
    const char *section = "  \"dependencies\": {\n  }\n";
    size_t slen = strlen(section);
    size_t old_len = strlen(json);
    char *out = malloc(old_len + slen + 8);
    if (!out) return 1;
    /* Insert before final root } — after last property needs a preceding comma
       if the previous non-ws char isn't already a comma. */
    size_t trim = at;
    while (trim > 0 && isspace((unsigned char)json[trim - 1])) trim--;
    memcpy(out, json, trim);
    size_t o = trim;
    if (trim > 0 && json[trim - 1] != ',' && json[trim - 1] != '{') {
      out[o++] = ',';
      out[o++] = '\n';
    }
    memcpy(out + o, section, slen);
    o += slen;
    memcpy(out + o, json + at, old_len - at + 1);
    free(json);
    *pkg_json = out;
    json = out;
  }

  const char *deps = strstr(json, "\"dependencies\"");
  if (!deps) return 1;
  const char *brace = strchr(deps, '{');
  if (!brace) return 1;
  const char *p = brace + 1;
  int depth = 1;
  while (*p && depth > 0) {
    if (*p == '{') depth++;
    else if (*p == '}') depth--;
    if (depth > 0) p++;
  }
  if (depth != 0) return 1;
  size_t insert_at = (size_t)(p - json);

  size_t extra = 0;
  for (int i = 0; i < npkgs; i++) {
    if (!pkgs[i]) continue;
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\"", pkgs[i]);
    if (strstr(json, needle)) continue;
    extra += strlen(pkgs[i]) + 40;
  }
  if (extra == 0) return 0;

  size_t old_len = strlen(json);
  char *out = malloc(old_len + extra + 8);
  if (!out) return 1;
  memcpy(out, json, insert_at);
  size_t o = insert_at;
  int need_comma = 0;
  for (size_t i = insert_at; i > 0; i--) {
    char c = json[i - 1];
    if (isspace((unsigned char)c)) continue;
    if (c != '{' && c != ',') need_comma = 1;
    break;
  }
  for (int i = 0; i < npkgs; i++) {
    if (!pkgs[i]) continue;
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\"", pkgs[i]);
    if (strstr(json, needle)) continue;
    if (need_comma) {
      out[o++] = ',';
      out[o++] = '\n';
    }
    int n;
    if (strcmp(pkgs[i], "framer-motion") == 0)
      n = snprintf(out + o, old_len + extra + 8 - o,
                   "    \"%s\": \"^11.15.0\"", pkgs[i]);
    else if (strncmp(pkgs[i], "lucide-", 7) == 0)
      n = snprintf(out + o, old_len + extra + 8 - o,
                   "    \"%s\": \"^0.469.0\"", pkgs[i]);
    else if (strcmp(pkgs[i], "recharts") == 0)
      n = snprintf(out + o, old_len + extra + 8 - o,
                   "    \"%s\": \"^2.15.0\"", pkgs[i]);
    else if (strcmp(pkgs[i], "@vueuse/motion") == 0)
      n = snprintf(out + o, old_len + extra + 8 - o,
                   "    \"%s\": \"^2.2.0\"", pkgs[i]);
    else if (strcmp(pkgs[i], "@lucide/svelte") == 0)
      n = snprintf(out + o, old_len + extra + 8 - o,
                   "    \"%s\": \"^1.26.0\"", pkgs[i]);
    else
      n = snprintf(out + o, old_len + extra + 8 - o,
                   "    \"%s\": \"^1.0.0\"", pkgs[i]);
    if (n < 0) {
      free(out);
      return 1;
    }
    o += (size_t)n;
    need_comma = 1;
  }
  out[o++] = '\n';
  out[o++] = ' ';
  out[o++] = ' ';
  memcpy(out + o, p, strlen(p) + 1);
  free(json);
  *pkg_json = out;
  return 0;
}

int preset_merge_package_json(const char *package_json_path,
                              const char *project_dir, PresetBackend backend) {
  if (!package_json_path || !project_dir) return 1;
  char names[16][PRESET_NAME_LEN];
  int n = preset_load_from_project(project_dir, names, 16);
  if (n <= 0) return 0;

  const char *pkgs[16];
  int np = 0;
  for (int i = 0; i < n && np < 16; i++) {
    const char *pkg = preset_npm_pkg(names[i], backend);
    if (pkg) pkgs[np++] = pkg;
  }
  if (np == 0) return 0;

  size_t len = 0;
  char *json = fs_read_file(package_json_path, &len);
  if (!json) return 1;
  if (inject_deps(&json, pkgs, np) != 0) {
    free(json);
    return 1;
  }
  int rc = fs_write_file(package_json_path, json);
  free(json);
  return rc;
}

