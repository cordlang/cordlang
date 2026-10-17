#include "adapters/outbound/backends/preset_registry.h"
#include "adapters/outbound/json/json_mini.h"
#include "application/ports/fs_port.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PresetBackend preset_backend_from_name(const char *name) {
  if (!name) return PRESET_BACKEND_REACT;
  if (strcmp(name, "svelte") == 0) return PRESET_BACKEND_SVELTE;
  if (strcmp(name, "vue") == 0) return PRESET_BACKEND_VUE;
  if (strcmp(name, "solid") == 0) return PRESET_BACKEND_SOLID;
  return PRESET_BACKEND_REACT;
}

const char *preset_backend_name(PresetBackend b) {
  switch (b) {
    case PRESET_BACKEND_SVELTE: return "svelte";
    case PRESET_BACKEND_VUE: return "vue";
    case PRESET_BACKEND_SOLID: return "solid";
    default: return "react";
  }
}

int preset_is_known(const char *name) {
  return name && (strcmp(name, PRESET_TAILWIND) == 0 ||
                  strcmp(name, PRESET_ICONS) == 0 ||
                  strcmp(name, PRESET_MOTION) == 0 ||
                  strcmp(name, PRESET_CHARTS) == 0);
}

void preset_list_known(void) {
  printf("Known capabilities (presets):\n");
  printf("  tailwind  — utility CSS (default in SPA scaffolds)\n");
  printf("  icons     — icon name=… → lucide-* per backend\n");
  printf("  motion    — Motion / transition= → motion libs per backend\n");
  printf("  charts    — Chart type=… → chart libs per backend\n");
}

const char *preset_npm_pkg(const char *preset_id, PresetBackend backend) {
  if (!preset_id) return NULL;
  if (strcmp(preset_id, PRESET_TAILWIND) == 0) return NULL; /* already in scaffold */
  if (strcmp(preset_id, PRESET_ICONS) == 0) {
    switch (backend) {
      case PRESET_BACKEND_SVELTE: return "@lucide/svelte";
      case PRESET_BACKEND_VUE: return "lucide-vue-next";
      case PRESET_BACKEND_SOLID: return "lucide-solid";
      default: return "lucide-react";
    }
  }
  if (strcmp(preset_id, PRESET_MOTION) == 0) {
    switch (backend) {
      case PRESET_BACKEND_SVELTE: return NULL; /* native transitions */
      case PRESET_BACKEND_VUE: return NULL;    /* CSS bridge until wired */
      case PRESET_BACKEND_SOLID: return NULL;  /* CSS bridge until wired */
      default: return "framer-motion";
    }
  }
  if (strcmp(preset_id, PRESET_CHARTS) == 0) {
    switch (backend) {
      case PRESET_BACKEND_SVELTE: return NULL; /* SVG CordChart */
      case PRESET_BACKEND_VUE: return NULL;    /* thin pre/SVG */
      case PRESET_BACKEND_SOLID: return NULL;  /* thin pre/SVG */
      default: return "recharts";
    }
  }
  return NULL;
}

int preset_list_has(const char names[][PRESET_NAME_LEN], int n, const char *id) {
  if (!names || !id || n <= 0) return 0;
  for (int i = 0; i < n; i++) {
    if (strcmp(names[i], id) == 0) return 1;
  }
  return 0;
}

int preset_load_from_project(const char *project_dir, char names[][PRESET_NAME_LEN],
                             int max_names) {
  if (!project_dir || !names || max_names <= 0) return 0;
  char *cfg_path = fs_join(project_dir, "cordlang.json");
  if (!cfg_path) return 0;
  size_t len = 0;
  char *json = fs_read_file(cfg_path, &len);
  free(cfg_path);
  if (!json) return 0;

  int n = json_object_copy_string_array(json, "presets", (char *)names,
                                        PRESET_NAME_LEN, max_names);
  free(json);
  return n < 0 ? 0 : n;
}

int preset_css_imports(const char *project_dir, char *buf, size_t buf_sz) {
  if (!buf || buf_sz == 0) return 0;
  buf[0] = '\0';
  if (!project_dir) return 0;
  char *cfg_path = fs_join(project_dir, "cordlang.json");
  if (!cfg_path) return 0;
  size_t len = 0;
  char *json = fs_read_file(cfg_path, &len);
  free(cfg_path);
  if (!json) return 0;

  char paths[8][96];
  int nc = json_object_nested_string_array(json, "deps", "css", (char *)paths, 96,
                                           8);
  free(json);
  if (nc <= 0) return 0;
  size_t o = 0;
  for (int i = 0; i < nc; i++) {
    int n = snprintf(buf + o, buf_sz - o, "@import '%s';\n", paths[i]);
    if (n < 0 || (size_t)n >= buf_sz - o) break;
    o += (size_t)n;
  }
  (void)0;
  return (int)o;
}
