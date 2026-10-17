#include "react_scaffold_internal.h"

int react_scaffold_write_vite_skeleton(const char *project_dir, const char *out) {
  char lang[32];
  char title[256];
  react_scaffold_load_html_meta(project_dir, lang, sizeof(lang), title, sizeof(title));
  char lang_e[64];
  char title_e[512];
  html_escape_to(lang_e, sizeof(lang_e), lang);
  html_escape_to(title_e, sizeof(title_e), title);

  const char *pkg =
      "{\n"
      "  \"name\": \"cordlang-react-app\",\n"
      "  \"private\": true,\n"
      "  \"version\": \"0.1.0\",\n"
      "  \"type\": \"module\",\n"
      "  \"scripts\": {\n"
      "    \"dev\": \"vite\",\n"
      "    \"build\": \"vite build\",\n"
      "    \"preview\": \"vite preview\"\n"
      "  },\n"
      "  \"dependencies\": {\n"
      "    \"react\": \"^19.0.0\",\n"
      "    \"react-dom\": \"^19.0.0\",\n"
      "    \"react-router-dom\": \"^6.28.0\"\n"
      "  },\n"
      "  \"devDependencies\": {\n"
      "    \"@vitejs/plugin-react\": \"^4.3.4\",\n"
      "    \"autoprefixer\": \"^10.4.20\",\n"
      "    \"postcss\": \"^8.4.49\",\n"
      "    \"tailwindcss\": \"^3.4.17\",\n"
      "    \"vite\": \"^6.0.3\"\n"
      "  }\n"
      "}\n";

  const char *vite =
      "import { defineConfig } from 'vite'\n"
      "import react from '@vitejs/plugin-react'\n"
      "\n"
      "export default defineConfig({\n"
      "  plugins: [react()],\n"
      "})\n";

  char html[4096];
  int has_site_css = 0, has_site_js = 0;
  int has_favicon = 0, has_logo_svg = 0, has_apple = 0, has_manifest = 0,
      has_og = 0;
  {
    char *css_p = fs_join(project_dir, "public/site.css");
    char *js_p = fs_join(project_dir, "public/site.js");
    char *ico_p = fs_join(project_dir, "public/favicon.ico");
    char *svg_p = fs_join(project_dir, "public/logo.svg");
    char *apple_p = fs_join(project_dir, "public/apple-touch-icon.png");
    char *man_p = fs_join(project_dir, "public/site.webmanifest");
    char *og_p = fs_join(project_dir, "public/og.png");
    if (css_p) {
      has_site_css = fs_exists(css_p);
      free(css_p);
    }
    if (js_p) {
      has_site_js = fs_exists(js_p);
      free(js_p);
    }
    if (ico_p) {
      has_favicon = fs_exists(ico_p);
      free(ico_p);
    }
    if (svg_p) {
      has_logo_svg = fs_exists(svg_p);
      free(svg_p);
    }
    if (apple_p) {
      has_apple = fs_exists(apple_p);
      free(apple_p);
    }
    if (man_p) {
      has_manifest = fs_exists(man_p);
      free(man_p);
    }
    if (og_p) {
      has_og = fs_exists(og_p);
      free(og_p);
    }
  }
  char head_extra[1536];
  {
    size_t o = 0;
    head_extra[0] = '\0';
#define HEAD_APPEND(s)                                                         \
  do {                                                                         \
    size_t _n = strlen(s);                                                     \
    if (o + _n + 1 < sizeof(head_extra)) {                                     \
      memcpy(head_extra + o, s, _n);                                           \
      o += _n;                                                                 \
      head_extra[o] = '\0';                                                    \
    }                                                                          \
  } while (0)
    if (has_site_css)
      HEAD_APPEND("    <link rel=\"stylesheet\" href=\"/site.css\" />\n");
    if (has_favicon)
      HEAD_APPEND("    <link rel=\"icon\" href=\"/favicon.ico\" sizes=\"any\" />\n");
    if (has_logo_svg)
      HEAD_APPEND(
          "    <link rel=\"icon\" href=\"/logo.svg\" type=\"image/svg+xml\" />\n");
    if (has_apple)
      HEAD_APPEND(
          "    <link rel=\"apple-touch-icon\" href=\"/apple-touch-icon.png\" />\n");
    if (has_manifest)
      HEAD_APPEND("    <link rel=\"manifest\" href=\"/site.webmanifest\" />\n");
    HEAD_APPEND("    <meta name=\"theme-color\" content=\"#0c0f12\" />\n");
    if (has_site_js) {
      HEAD_APPEND("    <script>\n");
      HEAD_APPEND("(function(){try{var t=localStorage.getItem(\"cord-docs-theme\");");
      HEAD_APPEND("if(t!==\"dark\"&&t!==\"light\")");
      HEAD_APPEND("t=window.matchMedia(\"(prefers-color-scheme: dark)\").matches?");
      HEAD_APPEND("\"dark\":\"light\";");
      HEAD_APPEND("document.documentElement.setAttribute(\"data-theme\",t);");
      HEAD_APPEND("document.documentElement.style.colorScheme=t;");
      HEAD_APPEND("}catch(e){}})();\n");
      HEAD_APPEND("    </script>\n");
    }
    if (has_og) {
      HEAD_APPEND("    <meta property=\"og:type\" content=\"website\" />\n");
      HEAD_APPEND("    <meta property=\"og:title\" content=\"");
      HEAD_APPEND(title_e);
      HEAD_APPEND("\" />\n");
      HEAD_APPEND("    <meta property=\"og:image\" content=\"/og.png\" />\n");
      HEAD_APPEND(
          "    <meta name=\"twitter:card\" content=\"summary_large_image\" />\n");
      HEAD_APPEND("    <meta name=\"twitter:image\" content=\"/og.png\" />\n");
    }
#undef HEAD_APPEND
  }
  snprintf(html, sizeof(html),
           "<!doctype html>\n"
           "<html lang=\"%s\">\n"
           "  <head>\n"
           "    <meta charset=\"UTF-8\" />\n"
           "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" />\n"
           "    <title>%s</title>\n"
           "%s"
           "  </head>\n"
           "  <body>\n"
           "    <div id=\"root\"></div>\n"
           "    <script type=\"module\" src=\"/src/main.jsx\"></script>\n"
           "%s"
           "  </body>\n"
           "</html>\n",
           lang_e, title_e, head_extra,
           has_site_js
               ? "    <script type=\"module\" src=\"/site.js\"></script>\n"
               : "");

  const char *main_jsx =
      "import React from 'react'\n"
      "import { createRoot } from 'react-dom/client'\n"
      "import App from './App.jsx'\n"
      "import ErrorBoundary from './ErrorBoundary.jsx'\n"
      "import './index.css'\n"
      "\n"
      "createRoot(document.getElementById('root')).render(\n"
      "  <React.StrictMode>\n"
      "    <ErrorBoundary>\n"
      "      <App />\n"
      "    </ErrorBoundary>\n"
      "  </React.StrictMode>\n"
      ")\n";

  const char *css = react_scaffold_index_css();

  /* Cord attrs like p=16 / gap=16 / max-w=640 map to class names p-16, etc.
   * Stock Tailwind treats p-16 as 4rem; Cord's HTML preview uses ~1rem.
   * Align React Tailwind with that Cord scale so demos aren't huge/broken. */
  const char *tailwind =
      "/** @type {import('tailwindcss').Config} */\n"
      "export default {\n"
      "  content: ['./index.html', './src/**/*.{js,jsx}'],\n"
      "  theme: {\n"
      "    extend: {\n"
      "      spacing: {\n"
      "        8: '0.5rem',\n"
      "        12: '0.75rem',\n"
      "        16: '1rem',\n"
      "        24: '1.5rem',\n"
      "        32: '2rem',\n"
      "        40: '2.5rem',\n"
      "        48: '3rem',\n"
      "        64: '4rem',\n"
      "        240: '15rem',\n"
      "      },\n"
      "      width: {\n"
      "        240: '15rem',\n"
      "      },\n"
      "      maxWidth: {\n"
      "        640: '40rem',\n"
      "        720: '45rem',\n"
      "        1280: '80rem',\n"
      "      },\n"
      "      borderRadius: {\n"
      "        8: '8px',\n"
      "        12: '12px',\n"
      "      },\n"
      "    },\n"
      "  },\n"
      "  plugins: [],\n"
      "}\n";

  const char *postcss =
      "export default {\n"
      "  plugins: {\n"
      "    tailwindcss: {},\n"
      "    autoprefixer: {},\n"
      "  },\n"
      "}\n";

  const char *gitignore =
      "node_modules\n"
      "dist\n"
      ".DS_Store\n"
      "*.local\n";

  int rc = 0;
  rc |= react_scaffold_write_path(out, "package.json", pkg);
  rc |= react_scaffold_write_path(out, "vite.config.js", vite);
  rc |= react_scaffold_write_path(out, "index.html", html);
  rc |= react_scaffold_write_path(out, "tailwind.config.js", tailwind);
  rc |= react_scaffold_write_path(out, "postcss.config.js", postcss);
  rc |= react_scaffold_write_path(out, ".gitignore", gitignore);
  rc |= react_scaffold_write_path(out, "src/main.jsx", main_jsx);
  rc |= react_scaffold_write_path(out, "src/index.css", css);
  /* Stub until scaffold_from_ast overwrites with real theme tokens */
  rc |= react_scaffold_write_path(out, "src/theme.css",
                   "/* No theme declared. */\n:root {}\n");

  /* Shared ErrorBoundary (class component — React requires this for EB) */
  const char *eb =
      "import { Component } from 'react';\n"
      "\n"
      "const shell = {\n"
      "  fontFamily: 'var(--font-sans, system-ui, sans-serif)',\n"
      "  color: 'var(--color-text, #eef2f0)',\n"
      "  background: 'var(--color-bg, #0c0f12)',\n"
      "  border: '1px solid var(--color-border, #2a323c)',\n"
      "  borderRadius: 12,\n"
      "  padding: '1.25rem 1.35rem',\n"
      "  maxWidth: '42rem',\n"
      "  margin: '1.5rem auto',\n"
      "};\n"
      "\n"
      "export default class ErrorBoundary extends Component {\n"
      "  constructor(props) {\n"
      "    super(props);\n"
      "    this.state = { hasError: false, error: null, componentStack: '' };\n"
      "  }\n"
      "  static getDerivedStateFromError(error) {\n"
      "    return { hasError: true, error };\n"
      "  }\n"
      "  componentDidCatch(error, info) {\n"
      "    const componentStack = info && info.componentStack ? info.componentStack : '';\n"
      "    this.setState({ componentStack });\n"
      "    if (typeof console !== 'undefined') console.error(error, info);\n"
      "  }\n"
      "  handleRetry = () => {\n"
      "    this.setState({ hasError: false, error: null, componentStack: '' });\n"
      "  };\n"
      "  render() {\n"
      "    if (this.state.hasError) {\n"
      "      if (this.props.fallback != null) return this.props.fallback;\n"
      "      const err = this.state.error;\n"
      "      const name = err && err.name ? err.name : 'Error';\n"
      "      const message = err && err.message ? err.message : 'Error desconocido';\n"
      "      const stack = err && err.stack ? err.stack : '';\n"
      "      const isDev = typeof import.meta !== 'undefined' && import.meta.env && import.meta.env.DEV;\n"
      "      return (\n"
      "        <div role=\"alert\" style={shell}>\n"
      "          <p style={{ margin: 0, fontFamily: 'var(--font-mono, ui-monospace, monospace)', fontSize: '0.72rem', letterSpacing: '0.12em', textTransform: 'uppercase', color: 'var(--signal-ink, var(--color-accent, #3dffb5))' }}>Error de render</p>\n"
      "          <h2 style={{ margin: '0.45rem 0 0.35rem', fontFamily: 'var(--font-display, var(--font-sans, system-ui))', fontSize: '1.25rem', letterSpacing: '-0.03em' }}>Algo falló al renderizar</h2>\n"
      "          <p style={{ margin: 0, color: 'var(--color-muted, #a8b0b8)', fontSize: '0.95rem', lineHeight: 1.5 }}><strong style={{ color: 'var(--color-text, #eef2f0)' }}>{name}</strong>: {message}</p>\n"
      "          {isDev && (stack || this.state.componentStack) ? (\n"
      "            <pre style={{ marginTop: '0.9rem', padding: '0.75rem', overflow: 'auto', maxHeight: '18rem', fontSize: '0.75rem', lineHeight: 1.45, background: 'var(--color-codebg, #080a0c)', color: 'var(--color-codefg, #d7e0dc)', borderRadius: 8, border: '1px solid var(--color-border, #2a323c)' }}>\n"
      "              {[stack, this.state.componentStack ? '\\n— componentStack —\\n' + this.state.componentStack : ''].filter(Boolean).join('\\n')}\n"
      "            </pre>\n"
      "          ) : null}\n"
      "          <button type=\"button\" onClick={this.handleRetry} style={{ marginTop: '1rem', appearance: 'none', border: 'none', borderRadius: 999, padding: '0.55rem 1.1rem', fontWeight: 600, cursor: 'pointer', background: 'var(--color-accent, #3dffb5)', color: 'var(--ink, #0c0f12)' }}>Reintentar</button>\n"
      "        </div>\n"
      "      );\n"
      "    }\n"
      "    return this.props.children;\n"
      "  }\n"
      "}\n";
  rc |= react_scaffold_write_path(out, "src/ErrorBoundary.jsx", eb);
  return rc;
}
