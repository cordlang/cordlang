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
      case PRESET_BACKEND_VUE: return "@vueuse/motion";
      case PRESET_BACKEND_SOLID: return "@solid-primitives/transition-group";
      default: return "framer-motion";
    }
  }
  if (strcmp(preset_id, PRESET_CHARTS) == 0) {
    switch (backend) {
      case PRESET_BACKEND_SVELTE: return NULL; /* thin CordChart bridge */
      case PRESET_BACKEND_VUE: return "vue-chartjs";
      case PRESET_BACKEND_SOLID: return "solid-chart.js";
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

int preset_write_bridges(const char *out_dir, const char *project_dir,
                         PresetBackend backend) {
  if (!out_dir || !project_dir) return 1;
  char names[16][PRESET_NAME_LEN];
  int n = preset_load_from_project(project_dir, names, 16);
  if (n <= 0) return 0;

  int want_icons = preset_list_has(names, n, PRESET_ICONS);
  int want_motion = preset_list_has(names, n, PRESET_MOTION);
  int want_charts = preset_list_has(names, n, PRESET_CHARTS);

  if (backend == PRESET_BACKEND_REACT) {
    if (want_icons) {
      const char *icon =
          "/* Cord capability: icons → lucide-react */\n"
          "import * as Lucide from 'lucide-react';\n"
          "function toPascal(s){\n"
          "  if(!s) return 'Circle';\n"
          "  return String(s).split(/[-_\\s]+/).map(p => "
          "p.charAt(0).toUpperCase()+p.slice(1)).join('');\n"
          "}\n"
          "export default function CordIcon({ name, size=20, ...rest }) {\n"
          "  const key = toPascal(name);\n"
          "  const Comp = Lucide[key] || Lucide[name] || Lucide.Circle;\n"
          "  return <Comp size={size} {...rest} />;\n"
          "}\n";
      char *path = fs_join(out_dir, "src/CordIcon.jsx");
      if (path) {
        fs_write_file(path, icon);
        free(path);
      }
    }
    if (want_motion) {
      const char *motion =
          "/* Cord capability: motion → framer-motion */\n"
          "import { motion } from 'framer-motion';\n"
          "export default function CordMotion({ fade, children, ...rest }) {\n"
          "  const initial = fade ? { opacity: 0 } : undefined;\n"
          "  const animate = fade ? { opacity: 1 } : undefined;\n"
          "  return (\n"
          "    <motion.div initial={initial} animate={animate} {...rest}>\n"
          "      {children}\n"
          "    </motion.div>\n"
          "  );\n"
          "}\n";
      char *path = fs_join(out_dir, "src/CordMotion.jsx");
      if (path) {
        fs_write_file(path, motion);
        free(path);
      }
    }
    if (want_charts) {
      const char *chart =
          "/* Cord capability: charts → recharts (MVP bar) */\n"
          "import { ResponsiveContainer, BarChart, Bar, XAxis, YAxis, "
          "Tooltip } from 'recharts';\n"
          "export default function CordChart({ type='bar', data=[], "
          "dataKey='value', nameKey='name', ...rest }) {\n"
          "  const rows = Array.isArray(data) ? data : [];\n"
          "  return (\n"
          "    <div style={{ width: '100%', height: 280 }} {...rest}>\n"
          "      <ResponsiveContainer>\n"
          "        <BarChart data={rows}>\n"
          "          <XAxis dataKey={nameKey} />\n"
          "          <YAxis />\n"
          "          <Tooltip />\n"
          "          <Bar dataKey={dataKey} fill=\"#2563eb\" />\n"
          "        </BarChart>\n"
          "      </ResponsiveContainer>\n"
          "    </div>\n"
          "  );\n"
          "}\n";
      char *path = fs_join(out_dir, "src/CordChart.jsx");
      if (path) {
        fs_write_file(path, chart);
        free(path);
      }
    }
  } else if (backend == PRESET_BACKEND_SVELTE) {
    if (want_icons) {
      const char *icon =
          "<!-- Cord capability: icons (lucide-svelte peer; thin MVP bridge) -->\n"
          "<script>\n"
          "  let { name = 'circle', size = 20 } = $props();\n"
          "</script>\n"
          "<span\n"
          "  class=\"cord-icon\"\n"
          "  data-name={name}\n"
          "  style=\"display:inline-flex;width:{size}px;height:{size}px;align-items:center;justify-content:center\"\n"
          "  aria-hidden=\"true\"\n"
          ">◇</span>\n";
      char *path = fs_join(out_dir, "src/CordIcon.svelte");
      if (path) {
        fs_write_file(path, icon);
        free(path);
      }
    }
    if (want_motion) {
      const char *motion =
          "<!-- Cord capability: motion → svelte/transition -->\n"
          "<script>\n"
          "  import { fade as fadeTrans } from 'svelte/transition';\n"
          "  let { fade = true, children } = $props();\n"
          "</script>\n"
          "{#if fade}\n"
          "  <div transition:fadeTrans>{@render children?.()}</div>\n"
          "{:else}\n"
          "  <div>{@render children?.()}</div>\n"
          "{/if}\n";
      char *path = fs_join(out_dir, "src/CordMotion.svelte");
      if (path) {
        fs_write_file(path, motion);
        free(path);
      }
    }
    if (want_charts) {
      const char *chart =
          "<!-- Cord capability: charts (thin MVP) -->\n"
          "<script>\n"
          "  let { type = 'bar', data = [] } = $props();\n"
          "</script>\n"
          "<div class=\"cord-chart\" data-type={type}>\n"
          "  <pre>{JSON.stringify(data)}</pre>\n"
          "</div>\n";
      char *path = fs_join(out_dir, "src/CordChart.svelte");
      if (path) {
        fs_write_file(path, chart);
        free(path);
      }
    }
  } else if (backend == PRESET_BACKEND_VUE) {
    if (want_icons) {
      const char *icon =
          "<!-- Cord capability: icons → lucide-vue-next -->\n"
          "<script setup>\n"
          "import { computed } from 'vue';\n"
          "import * as Lucide from 'lucide-vue-next';\n"
          "const props = defineProps({ name: String, size: { type: [Number, "
          "String], default: 20 } });\n"
          "const Comp = computed(() => {\n"
          "  const key = String(props.name || 'circle').split(/[-_\\s]+/).map(p "
          "=> p.charAt(0).toUpperCase()+p.slice(1)).join('');\n"
          "  return Lucide[key] || Lucide.Circle;\n"
          "});\n"
          "</script>\n"
          "<component :is=\"Comp\" :size=\"props.size\" />\n";
      char *path = fs_join(out_dir, "src/CordIcon.vue");
      if (path) {
        fs_write_file(path, icon);
        free(path);
      }
    }
    if (want_motion) {
      const char *motion =
          "<!-- Cord capability: motion (CSS) -->\n"
          "<script setup>\n"
          "defineProps({ fade: { type: Boolean, default: true } });\n"
          "</script>\n"
          "<div :class=\"{ 'cord-fade': fade }\"><slot /></div>\n"
          "<style scoped>\n"
          ".cord-fade { animation: cordFade 0.4s ease; }\n"
          "@keyframes cordFade { from { opacity: 0; } to { opacity: 1; } }\n"
          "</style>\n";
      char *path = fs_join(out_dir, "src/CordMotion.vue");
      if (path) {
        fs_write_file(path, motion);
        free(path);
      }
    }
    if (want_charts) {
      const char *chart =
          "<!-- Cord capability: charts (thin MVP) -->\n"
          "<script setup>\n"
          "defineProps({ type: { type: String, default: 'bar' }, data: { type: "
          "Array, default: () => [] } });\n"
          "</script>\n"
          "<div class=\"cord-chart\" :data-type=\"type\"><pre>{{ data "
          "}}</pre></div>\n";
      char *path = fs_join(out_dir, "src/CordChart.vue");
      if (path) {
        fs_write_file(path, chart);
        free(path);
      }
    }
  } else if (backend == PRESET_BACKEND_SOLID) {
    if (want_icons) {
      const char *icon =
          "/* Cord capability: icons → lucide-solid */\n"
          "import * as Lucide from 'lucide-solid';\n"
          "function toPascal(s){\n"
          "  if(!s) return 'Circle';\n"
          "  return String(s).split(/[-_\\s]+/).map(p => "
          "p.charAt(0).toUpperCase()+p.slice(1)).join('');\n"
          "}\n"
          "export default function CordIcon(props) {\n"
          "  const key = toPascal(props.name);\n"
          "  const Comp = Lucide[key] || Lucide.Circle;\n"
          "  return <Comp size={props.size || 20} />;\n"
          "}\n";
      char *path = fs_join(out_dir, "src/CordIcon.jsx");
      if (path) {
        fs_write_file(path, icon);
        free(path);
      }
    }
    if (want_motion) {
      const char *motion =
          "/* Cord capability: motion (CSS) */\n"
          "export default function CordMotion(props) {\n"
          "  const cls = props.fade ? 'cord-fade' : undefined;\n"
          "  return <div class={cls}>{props.children}</div>;\n"
          "}\n";
      char *path = fs_join(out_dir, "src/CordMotion.jsx");
      if (path) {
        fs_write_file(path, motion);
        free(path);
      }
    }
    if (want_charts) {
      const char *chart =
          "/* Cord capability: charts (thin MVP) */\n"
          "export default function CordChart(props) {\n"
          "  return (\n"
          "    <div class=\"cord-chart\" data-type={props.type || 'bar'}>\n"
          "      <pre>{JSON.stringify(props.data || [])}</pre>\n"
          "    </div>\n"
          "  );\n"
          "}\n";
      char *path = fs_join(out_dir, "src/CordChart.jsx");
      if (path) {
        fs_write_file(path, chart);
        free(path);
      }
    }
  }
  return 0;
}
