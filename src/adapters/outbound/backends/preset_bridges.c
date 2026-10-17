#include "adapters/outbound/backends/preset_registry.h"
#include "application/ports/fs_port.h"
#include <stdlib.h>
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
          "export default function CordMotion({ fade, y, children, ...rest }) {\n"
          "  const initial = fade || y ? { opacity: fade ? 0 : 1, y: y ? 12 : 0 } : undefined;\n"
          "  const animate = fade || y ? { opacity: 1, y: 0 } : undefined;\n"
          "  return (\n"
          "    <motion.div initial={initial} animate={animate} "
          "transition={{ duration: 0.35 }} {...rest}>\n"
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
          "<!-- Cord capability: icons → @lucide/svelte -->\n"
          "<script>\n"
          "  import * as icons from '@lucide/svelte';\n"
          "  let { name = 'circle', size = 20 } = $props();\n"
          "  let key = $derived(String(name).split(/[-_\\s]+/).map(p => "
          "p.charAt(0).toUpperCase()+p.slice(1)).join(''));\n"
          "  let Comp = $derived(icons[key] || icons.Circle);\n"
          "</script>\n"
          "{#if Comp}\n"
          "  <Comp {size} />\n"
          "{/if}\n";
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
          "  import { fade as fadeTrans, fly } from 'svelte/transition';\n"
          "  let { fade = true, y = false, children } = $props();\n"
          "</script>\n"
          "{#if y}\n"
          "  <div in:fly={{ y: 12, duration: 280 }}>{@render children?.()}</div>\n"
          "{:else if fade}\n"
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
          "<!-- Cord capability: charts (SVG bar MVP) -->\n"
          "<script>\n"
          "  let { type = 'bar', data = [] } = $props();\n"
          "  let rows = $derived(Array.isArray(data) ? data : []);\n"
          "  let max = $derived(Math.max(1, ...rows.map(r => Number(r?.value) || "
          "0)));\n"
          "</script>\n"
          "<div class=\"cord-chart\" data-type={type} "
          "style=\"display:flex;align-items:flex-end;gap:6px;height:160px\">\n"
          "  {#each rows as row}\n"
          "    <div title={String(row?.name ?? '')} "
          "style=\"flex:1;background:var(--color-primary,#2563eb);"
          "height:{(Number(row?.value)||0)/max*100}%;min-height:4px;"
          "border-radius:4px 4px 0 0\"></div>\n"
          "  {/each}\n"
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
          "<template>\n"
          "  <component :is=\"Comp\" :size=\"props.size\" />\n"
          "</template>\n";
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
          "<template>\n"
          "  <div :class=\"{ 'cord-fade': fade }\"><slot /></div>\n"
          "</template>\n"
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
          "<!-- Cord capability: charts (SVG bar MVP) -->\n"
          "<script setup>\n"
          "import { computed } from 'vue';\n"
          "const props = defineProps({ type: { type: String, default: 'bar' }, "
          "data: { type: Array, default: () => [] } });\n"
          "const rows = computed(() => Array.isArray(props.data) ? props.data : "
          "[]);\n"
          "const max = computed(() => Math.max(1, ...rows.value.map(r => "
          "Number(r?.value) || 0)));\n"
          "</script>\n"
          "<template>\n"
          "  <div class=\"cord-chart\" :data-type=\"type\" "
          "style=\"display:flex;align-items:flex-end;gap:6px;height:160px\">\n"
          "    <div v-for=\"(row, i) in rows\" :key=\"i\" "
          ":title=\"String(row?.name ?? '')\" "
          ":style=\"{ flex: 1, background: 'var(--color-primary,#2563eb)', "
          "height: ((Number(row?.value)||0)/max*100)+'%', minHeight: '4px', "
          "borderRadius: '4px 4px 0 0' }\" />\n"
          "  </div>\n"
          "</template>\n";
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
          "/* Cord capability: charts (SVG bar MVP) */\n"
          "export default function CordChart(props) {\n"
          "  const rows = Array.isArray(props.data) ? props.data : [];\n"
          "  const max = Math.max(1, ...rows.map(r => Number(r?.value) || 0));\n"
          "  return (\n"
          "    <div class=\"cord-chart\" data-type={props.type || 'bar'} "
          "style={{ display: 'flex', alignItems: 'flex-end', gap: 6, height: "
          "160 }}>\n"
          "      {rows.map((row, i) => (\n"
          "        <div key={i} title={String(row?.name ?? '')} style={{\n"
          "          flex: 1, background: 'var(--color-primary, #2563eb)',\n"
          "          height: `${((Number(row?.value)||0)/max)*100}%`,\n"
          "          minHeight: 4, borderRadius: '4px 4px 0 0'\n"
          "        }} />\n"
          "      ))}\n"
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
