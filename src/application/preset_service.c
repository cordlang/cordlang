#include "application/preset_service.h"
#include "adapters/outbound/backends/preset_registry.h"
#include "application/ports/fs_port.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *find_project_dir(void) {
  char *cfg = fs_join(".", "cordlang.json");
  if (cfg && fs_exists(cfg)) {
    free(cfg);
    return strdup(".");
  }
  free(cfg);
  return NULL;
}

static int rewrite_presets_array(const char *cfg_path, const char *json,
                                 const char names[][PRESET_NAME_LEN], int n) {
  /* Rebuild cordlang.json with updated presets array (best-effort). */
  size_t cap = strlen(json) + (size_t)n * 40 + 128;
  char *out = malloc(cap);
  if (!out) return 1;

  const char *presets_key = strstr(json, "\"presets\"");
  if (!presets_key) {
    /* Insert before final } */
    const char *end = strrchr(json, '}');
    if (!end) {
      free(out);
      return 1;
    }
    size_t head = (size_t)(end - json);
    memcpy(out, json, head);
    size_t o = head;
    /* trim trailing whitespace in head */
    while (o > 0 && (out[o - 1] == ' ' || out[o - 1] == '\n' ||
                     out[o - 1] == '\r' || out[o - 1] == '\t'))
      o--;
    int need_comma = 1;
    if (o > 0 && out[o - 1] == '{') need_comma = 0;
    if (need_comma) out[o++] = ',';
    out[o++] = '\n';
    int w = snprintf(out + o, cap - o, "  \"presets\": [");
    if (w < 0) {
      free(out);
      return 1;
    }
    o += (size_t)w;
    for (int i = 0; i < n; i++) {
      w = snprintf(out + o, cap - o, "%s\"%s\"", i ? ", " : "", names[i]);
      if (w < 0) {
        free(out);
        return 1;
      }
      o += (size_t)w;
    }
    w = snprintf(out + o, cap - o, "]\n}\n");
    if (w < 0) {
      free(out);
      return 1;
    }
  } else {
    /* Replace existing presets array */
    const char *arr = strchr(presets_key, '[');
    if (!arr) {
      free(out);
      return 1;
    }
    const char *arr_end = strchr(arr, ']');
    if (!arr_end) {
      free(out);
      return 1;
    }
    size_t head = (size_t)(arr - json);
    memcpy(out, json, head);
    size_t o = head;
    out[o++] = '[';
    for (int i = 0; i < n; i++) {
      int w = snprintf(out + o, cap - o, "%s\"%s\"", i ? ", " : "", names[i]);
      if (w < 0) {
        free(out);
        return 1;
      }
      o += (size_t)w;
    }
    out[o++] = ']';
    strcpy(out + o, arr_end + 1);
  }

  int rc = fs_write_file(cfg_path, out);
  free(out);
  return rc;
}

int preset_service_run(int argc, char **argv) {
  if (argc < 1 || strcmp(argv[0], "list") == 0 ||
      (argc >= 1 && (strcmp(argv[0], "--help") == 0 || strcmp(argv[0], "-h") == 0))) {
    if (argc >= 1 && (strcmp(argv[0], "--help") == 0 || strcmp(argv[0], "-h") == 0)) {
      printf("Usage:\n");
      printf("  cordlang preset list\n");
      printf("  cordlang preset add <id> [id…]\n\n");
      printf("Capabilities are backend-agnostic; each scaffold resolves npm.\n");
      printf("See docs/LIBRARIES.md\n\n");
    }
    preset_list_known();
    char *dir = find_project_dir();
    if (dir) {
      char names[16][PRESET_NAME_LEN];
      int n = preset_load_from_project(dir, names, 16);
      printf("\nProject presets (%s):\n", dir);
      if (n <= 0)
        printf("  (none — add with: cordlang preset add icons motion)\n");
      else
        for (int i = 0; i < n; i++) printf("  - %s\n", names[i]);
      free(dir);
    }
    return 0;
  }

  if (strcmp(argv[0], "add") == 0) {
    if (argc < 2) {
      fprintf(stderr, "Error: preset add requires at least one id\n");
      return 1;
    }
    char *dir = find_project_dir();
    if (!dir) {
      fprintf(stderr, "Error: not a Cordlang project (missing cordlang.json)\n");
      return 1;
    }
    char names[16][PRESET_NAME_LEN];
    int n = preset_load_from_project(dir, names, 16);
    for (int i = 1; i < argc; i++) {
      if (!preset_is_known(argv[i])) {
        fprintf(stderr, "Error: unknown preset '%s'\n", argv[i]);
        preset_list_known();
        free(dir);
        return 1;
      }
      if (!preset_list_has(names, n, argv[i]) && n < 16) {
        strncpy(names[n], argv[i], PRESET_NAME_LEN - 1);
        names[n][PRESET_NAME_LEN - 1] = '\0';
        n++;
      }
    }
    char *cfg = fs_join(dir, "cordlang.json");
    size_t len = 0;
    char *json = fs_read_file(cfg, &len);
    if (!json) {
      fprintf(stderr, "Error: cannot read %s\n", cfg);
      free(cfg);
      free(dir);
      return 1;
    }
    int rc = rewrite_presets_array(cfg, json, names, n);
    free(json);
    if (rc == 0) {
      printf("Updated presets in %s:\n", cfg);
      for (int i = 0; i < n; i++) printf("  - %s\n", names[i]);
    } else {
      fprintf(stderr, "Error: failed to update cordlang.json\n");
    }
    free(cfg);
    free(dir);
    return rc;
  }

  fprintf(stderr, "Unknown preset subcommand: %s\n", argv[0]);
  fprintf(stderr, "Usage: cordlang preset list | add <id>…\n");
  return 1;
}
