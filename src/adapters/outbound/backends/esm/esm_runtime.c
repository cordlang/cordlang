/*
 * The client runtime, embedded in the executable and served at
 * /@cord/runtime.js. No npm, no bundler, no build step.
 *
 * Model: components are plain functions `(props, $) => vnode`. State lives on a
 * per-instance store keyed by declaration name, so re-running the function is
 * idempotent. DOM updates go through a keyed vnode diff.
 *
 * A component or fragment can occupy several sibling DOM nodes, so a node's DOM
 * range is DERIVED on demand (domsOf) instead of cached on the vnode. Caching it
 * is where hand-written runtimes usually rot: an ancestor keeps a stale node
 * list after a descendant swaps its subtree. Empty fragments and null-rendering
 * components keep a comment placeholder, so every vnode always owns >= 1 node
 * and insertion points stay computable.
 *
 * JS below intentionally avoids double quotes and backslashes so it survives as
 * a C string literal without escaping.
 */
#include "adapters/outbound/backends/esm/esm_backend.h"
#include "adapters/outbound/html_escape.h"
#include "domain/diag.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *esm_runtime_js_part_1(void);
const char *esm_runtime_js_part_2(void);
const char *esm_runtime_js_part_3(void);
const char *esm_runtime_js_part_4(void);

static char *g_runtime_js;
const char *esm_runtime_js(void) {
  if (g_runtime_js) return g_runtime_js;
  const char *parts[] = {esm_runtime_js_part_1(), esm_runtime_js_part_2(),
                         esm_runtime_js_part_3(), esm_runtime_js_part_4(),
                         NULL};
  size_t len = 0;
  for (size_t i = 0; parts[i]; i++) len += strlen(parts[i]);
  g_runtime_js = malloc(len + 1);
  if (!g_runtime_js) return "";
  char *out = g_runtime_js;
  for (size_t i = 0; parts[i]; i++) {
    size_t n = strlen(parts[i]);
    memcpy(out, parts[i], n);
    out += n;
  }
  *out = '\0';
  return g_runtime_js;
}

static char HMR_CLIENT_BUF[4096];

const char *esm_hmr_client_js(const char *entry_url) {
  const char *entry = entry_url && *entry_url ? entry_url : "/src/app.cord";
  char esc[512];
  size_t j = 0;
  esc[j++] = '\'';
  for (const char *p = entry; *p && j + 3 < sizeof(esc); p++) {
    if (*p == '\'' || *p == '\\') esc[j++] = '\\';
    esc[j++] = *p;
  }
  esc[j++] = '\'';
  esc[j] = '\0';
  snprintf(
      HMR_CLIENT_BUF, sizeof(HMR_CLIENT_BUF),
      "/* Cordlang soft update / reload: SSE from the compiler process. */\n"
      "(function () {\n"
      "  var ENTRY = %s;\n"
      "  var es = null;\n"
      "  var retry = 0;\n"
      "  var updating = 0;\n"
      "  function fullReload() { location.reload(); }\n"
      "  function softUpdate(url) {\n"
      "    if (updating) return;\n"
      "    updating = 1;\n"
      "    var bust = Date.now();\n"
      "    var entryUrl = ENTRY + (ENTRY.indexOf('?') >= 0 ? '&' : '?') + 't=' + bust;\n"
      "    import('/@cord/runtime.js').then(function (rt) {\n"
      "      return import(entryUrl).then(function (mod) {\n"
      "        var el = document.getElementById('app');\n"
      "        if (!el || !rt.remount) { fullReload(); return; }\n"
      "        /* Success: remount replaces UI and clears any error modal. */\n"
      "        rt.remount(mod.default !== undefined ? mod.default : mod, el);\n"
      "      });\n"
      "    }).catch(function () {\n"
      "      /* Compile error: keep last good page; modal already painted. */\n"
      "    }).then(function () { updating = 0; });\n"
      "  }\n"
      "  function connect() {\n"
      "    try { es = new EventSource('/@cord/hmr'); } catch (e) { return; }\n"
      "    es.onopen = function () { retry = 0; };\n"
      "    es.onmessage = function (ev) {\n"
      "      if (!ev || !ev.data) return;\n"
      "      if (ev.data === 'reload') { fullReload(); return; }\n"
      "      if (ev.data.indexOf('update:') === 0) softUpdate(ev.data.slice(7));\n"
      "    };\n"
      "    es.onerror = function () {\n"
      "      if (es) { es.close(); es = null; }\n"
      "      retry = Math.min(retry + 1, 10);\n"
      "      setTimeout(connect, 250 * retry);\n"
      "    };\n"
      "  }\n"
      "  connect();\n"
      "})();\n",
      esc);
  return HMR_CLIENT_BUF;
}

/*
 * Vite-style shell: no inline bootstrap. The entry .cord is loaded as
 *   <script type="module" src="/src/app.cord"></script>
 * and self-mounts into #app (see esm_generate_module). /@cord/client is the
 * reload client (same role as /@vite/client).
 */
char *esm_index_html(const char *lang, const char *title, const char *entry_url,
                     int has_site_css, int has_site_js, int has_favicon,
                     int has_logo_svg, int with_hmr) {
  char lang_e[64];
  char title_e[512];
  html_escape_to(lang_e, sizeof(lang_e), lang && *lang ? lang : "en");
  html_escape_to(title_e, sizeof(title_e),
                 title && *title ? title : "Cordlang App");

  const char *entry = entry_url && *entry_url ? entry_url : "/src/app.cord";

  size_t cap = 4096 + strlen(entry);
  char *out = malloc(cap);
  if (!out) return NULL;

  snprintf(
      out, cap,
      "<!doctype html>\n"
      "<html lang=\"%s\">\n"
      "  <head>\n"
      "    <meta charset=\"UTF-8\" />\n"
      "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" />\n"
      "%s"
      "%s"
      "    <title>%s</title>\n"
      "    <link rel=\"modulepreload\" href=\"/@cord/runtime.js\" />\n"
      "    <link rel=\"stylesheet\" href=\"/@cord/styles.css\" />\n"
      "%s"
      "%s"
      "  </head>\n"
      "  <body>\n"
      "    <div id=\"app\"></div>\n"
      "    <script type=\"module\" src=\"%s\"></script>\n"
      "%s"
      "%s"
      "  </body>\n"
      "</html>\n",
      lang_e,
      has_favicon ? "    <link rel=\"icon\" href=\"/favicon.ico\" sizes=\"any\" />\n"
                  : "",
      has_logo_svg
          ? "    <link rel=\"icon\" href=\"/logo.svg\" type=\"image/svg+xml\" />\n"
          : "",
      title_e,
      has_site_css ? "    <link rel=\"stylesheet\" href=\"/site.css\" />\n" : "",
      has_site_js
          ? "    <script>\n"
            "(function(){try{var t=localStorage.getItem(\"cord-docs-theme\");"
            "if(t!==\"dark\"&&t!==\"light\")"
            "t=window.matchMedia(\"(prefers-color-scheme: dark)\").matches?"
            "\"dark\":\"light\";"
            "document.documentElement.setAttribute(\"data-theme\",t);"
            "document.documentElement.style.colorScheme=t;"
            "}catch(e){}})();\n"
            "    </script>\n"
          : "",
      entry,
      has_site_js ? "    <script type=\"module\" src=\"/site.js\"></script>\n" : "",
      with_hmr
          ? "    <script>\n"
            "    (function(){function load(){import('/@cord/client').catch(function(){})}"
            "if('requestIdleCallback' in window)requestIdleCallback(load,{timeout:2000});"
            "else setTimeout(load,1);})();\n"
            "    </script>\n"
          : "");
  return out;
}
