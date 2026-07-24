#include "adapters/outbound/backends/solid/solid_backend.h"
#include "adapters/outbound/backends/theme_css.h"
#include "adapters/outbound/html_escape.h"
#include "adapters/outbound/json/json_mini.h"
#include "application/ports/fs_port.h"
#include "domain/ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int write_path(const char *dir, const char *rel, const char *content) {
  char *path = fs_join(dir, rel);
  if (!path) return -1;
  int rc = fs_write_file(path, content);
  free(path);
  return rc;
}

typedef struct {
  char *out_root; /* dist/solid */
  int rc;
  int n_files;
  char files[128][256];
} ScaffoldCtx;

static int scaffold_write_src(const char *rel_from_src, const char *content,
                              void *userdata) {
  ScaffoldCtx *ctx = (ScaffoldCtx *)userdata;
  char rel[512];
  snprintf(rel, sizeof(rel), "src/%s", rel_from_src);
  int rc = write_path(ctx->out_root, rel, content);
  if (rc != 0) {
    ctx->rc = rc;
    return rc;
  }
  if (ctx->n_files < 128) {
    snprintf(ctx->files[ctx->n_files], sizeof(ctx->files[0]), "%s", rel);
    ctx->n_files++;
  }
  return 0;
}

/* Best-effort string from cordlang.json via json_mini. */
static int cfg_string(const char *json, const char *key, char *out, size_t outsz) {
  return json_object_copy_string(json, key, out, outsz);
}

static void load_html_meta(const char *project_dir, char *lang, size_t lang_sz,
                           char *title, size_t title_sz) {
  snprintf(lang, lang_sz, "en");
  snprintf(title, title_sz, "Cordlang App");
  if (!project_dir) return;
  char *cfg_path = fs_join(project_dir, "cordlang.json");
  if (!cfg_path) return;
  size_t len = 0;
  char *json = fs_read_file(cfg_path, &len);
  free(cfg_path);
  if (!json) return;
  char buf[256];
  if (cfg_string(json, "lang", buf, sizeof(buf)))
    snprintf(lang, lang_sz, "%s", buf);
  if (cfg_string(json, "title", buf, sizeof(buf)))
    snprintf(title, title_sz, "%s", buf);
  else if (cfg_string(json, "name", buf, sizeof(buf)))
    snprintf(title, title_sz, "%s", buf);
  free(json);
}

static int write_vite_skeleton(const char *project_dir, const char *out) {
  char lang[32];
  char title[256];
  load_html_meta(project_dir, lang, sizeof(lang), title, sizeof(title));
  char lang_e[64];
  char title_e[512];
  html_escape_to(lang_e, sizeof(lang_e), lang);
  html_escape_to(title_e, sizeof(title_e), title);

  const char *pkg =
      "{\n"
      "  \"name\": \"cordlang-solid-app\",\n"
      "  \"private\": true,\n"
      "  \"version\": \"0.1.0\",\n"
      "  \"type\": \"module\",\n"
      "  \"scripts\": {\n"
      "    \"dev\": \"vite\",\n"
      "    \"build\": \"vite build\",\n"
      "    \"preview\": \"vite preview\"\n"
      "  },\n"
      "  \"dependencies\": {\n"
      "    \"solid-js\": \"^1.9.3\",\n"
      "    \"@solidjs/router\": \"^0.15.1\"\n"
      "  },\n"
      "  \"devDependencies\": {\n"
      "    \"vite-plugin-solid\": \"^2.10.2\",\n"
      "    \"autoprefixer\": \"^10.4.20\",\n"
      "    \"postcss\": \"^8.4.49\",\n"
      "    \"tailwindcss\": \"^3.4.17\",\n"
      "    \"vite\": \"^6.0.3\"\n"
      "  }\n"
      "}\n";

  const char *vite =
      "import { defineConfig } from 'vite'\n"
      "import solid from 'vite-plugin-solid'\n"
      "\n"
      "export default defineConfig({\n"
      "  plugins: [solid()],\n"
      "})\n";

  char html[1024];
  snprintf(html, sizeof(html),
           "<!doctype html>\n"
           "<html lang=\"%s\">\n"
           "  <head>\n"
           "    <meta charset=\"UTF-8\" />\n"
           "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" />\n"
           "    <title>%s</title>\n"
           "  </head>\n"
           "  <body>\n"
           "    <div id=\"root\"></div>\n"
           "    <script type=\"module\" src=\"/src/main.jsx\"></script>\n"
           "  </body>\n"
           "</html>\n",
           lang_e, title_e);

  const char *main_jsx =
      "/* @jsxImportSource solid-js */\n"
      "import { render } from 'solid-js/web'\n"
      "import App from './App.jsx'\n"
      "import './index.css'\n"
      "\n"
      "render(() => <App />, document.getElementById('root'))\n";

  const char *css =
      "/* Theme tokens from Cord `theme` blocks (see theme.css) */\n"
      "@import './theme.css';\n"
      "\n"
      "@tailwind base;\n"
      "@tailwind components;\n"
      "@tailwind utilities;\n"
      "\n"
      "@layer components {\n"
      "  .btn {\n"
      "    @apply inline-flex items-center justify-center rounded-lg px-4 py-2 font-medium transition;\n"
      "    border-radius: var(--radius, 0.5rem);\n"
      "  }\n"
      "  .btn-primary {\n"
      "    @apply text-white;\n"
      "    background-color: var(--color-primary, #2563eb);\n"
      "  }\n"
      "  .btn-primary:hover {\n"
      "    filter: brightness(0.92);\n"
      "  }\n"
      "  .btn-outline {\n"
      "    @apply border border-gray-300 bg-white text-gray-900 hover:bg-gray-50;\n"
      "  }\n"
      "  .btn-ghost {\n"
      "    @apply text-gray-700 hover:bg-gray-100;\n"
      "  }\n"
      "  .btn-secondary {\n"
      "    @apply bg-gray-800 text-white hover:bg-gray-900;\n"
      "  }\n"
      "  .card {\n"
      "    @apply bg-white shadow-md overflow-hidden;\n"
      "    border-radius: var(--radius, 0.75rem);\n"
      "  }\n"
      "}\n"
      "\n"
      "body {\n"
      "  @apply m-0 min-h-screen antialiased;\n"
      "  background-color: var(--color-bg, #fafaf9);\n"
      "  color: var(--color-text, #1c1917);\n"
      "  line-height: 1.6;\n"
      "  --ui-container: 80rem;\n"
      "  --ui-header-height: 4rem;\n"
      "}\n"
      "\n"
      ".text-muted {\n"
      "  color: var(--color-muted, #57534e);\n"
      "}\n"
      "\n"
      "/* Links: content uses primary; chrome stays quiet */\n"
      "a {\n"
      "  color: var(--color-primary, #0f766e);\n"
      "  text-decoration: none;\n"
      "}\n"
      "a:hover {\n"
      "  color: var(--color-accent, #0d9488);\n"
      "  text-decoration: underline;\n"
      "}\n"
      "header a,\n"
      "aside a,\n"
      "footer a {\n"
      "  color: var(--color-text, #1c1917);\n"
      "}\n"
      "header a:hover,\n"
      "aside a:hover,\n"
      "footer a:hover {\n"
      "  color: var(--color-primary, #0f766e);\n"
      "}\n"
      "aside a {\n"
      "  display: block;\n"
      "  padding: 0.35rem 0.5rem;\n"
      "  margin: 0 -0.5rem;\n"
      "  border-radius: 6px;\n"
      "  font-size: 0.925rem;\n"
      "  line-height: 1.35;\n"
      "}\n"
      "aside a:hover {\n"
      "  background-color: color-mix(in srgb, var(--color-primary, #0f766e) 10%, transparent);\n"
      "  text-decoration: none;\n"
      "}\n"
      "header.sticky {\n"
      "  min-height: var(--ui-header-height);\n"
      "  backdrop-filter: blur(8px);\n"
      "  background-color: color-mix(in srgb, var(--color-surface, #fff) 92%, transparent);\n"
      "  border-bottom: 1px solid var(--color-border, #e7e5e4);\n"
      "}\n"
      "header .max-w-1280,\n"
      "footer .max-w-1280,\n"
      ".max-w-1280 {\n"
      "  width: 100%;\n"
      "  max-width: var(--ui-container);\n"
      "  margin-left: auto;\n"
      "  margin-right: auto;\n"
      "}\n"
      "main {\n"
      "  min-width: 0;\n"
      "}\n"
      "main h1 {\n"
      "  line-height: 1.2;\n"
      "  letter-spacing: -0.02em;\n"
      "}\n"
      "main h2 {\n"
      "  line-height: 1.3;\n"
      "  margin-top: 0.25rem;\n"
      "}\n"
      "aside.sticky {\n"
      "  top: var(--ui-header-height);\n"
      "  align-self: flex-start;\n"
      "  max-height: calc(100vh - var(--ui-header-height));\n"
      "  overflow-y: auto;\n"
      "  border-right: 1px solid var(--color-border, #e7e5e4);\n"
      "}\n"
      ".bg-codebg,\n"
      ".bg-stone-900 {\n"
      "  color: var(--color-codefg, #e7e5e4);\n"
      "  background-color: var(--color-codebg, #1c1917);\n"
      "  box-shadow: inset 0 1px 0 rgba(255,255,255,0.06);\n"
      "}\n"
      ".bg-codebg .text-muted,\n"
      ".bg-stone-900 .text-muted {\n"
      "  color: #a8a29e;\n"
      "}\n"
      "footer {\n"
      "  border-top: 1px solid var(--color-border, #e7e5e4);\n"
      "}\n"
      "\n"
      "/* Stack sidebar shells on narrow viewports */\n"
      "@media (max-width: 768px) {\n"
      "  body .flex.flex-row:has(> aside) {\n"
      "    flex-direction: column;\n"
      "  }\n"
      "  body aside.w-240 {\n"
      "    width: 100%;\n"
      "    position: relative;\n"
      "    top: auto;\n"
      "    max-height: none;\n"
      "    min-height: 0;\n"
      "    border-right: none;\n"
      "    border-bottom: 1px solid var(--color-border, #e7e5e4);\n"
      "  }\n"
      "  body header.sticky {\n"
      "    z-index: 40;\n"
      "  }\n"
      "}\n";

  /* Cord attrs like p=16 / gap=16 / max-w=640 map to class names p-16, etc.
   * Stock Tailwind treats p-16 as 4rem; Cord's HTML preview uses ~1rem.
   * Align Solid Tailwind with that Cord scale so demos aren't huge/broken. */
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
  rc |= write_path(out, "package.json", pkg);
  rc |= write_path(out, "vite.config.js", vite);
  rc |= write_path(out, "index.html", html);
  rc |= write_path(out, "tailwind.config.js", tailwind);
  rc |= write_path(out, "postcss.config.js", postcss);
  rc |= write_path(out, ".gitignore", gitignore);
  rc |= write_path(out, "src/main.jsx", main_jsx);
  rc |= write_path(out, "src/index.css", css);
  /* Stub until scaffold_from_ast overwrites with real theme tokens */
  rc |= write_path(out, "src/theme.css",
                   "/* No theme declared. */\n:root {}\n");

  return rc;
}

static void print_tree(ScaffoldCtx *ctx) {
  printf("\nSolid app generated in: dist\\solid\n");
  printf("  +-- package.json\n");
  printf("  +-- vite.config.js\n");
  printf("  +-- index.html\n");
  printf("  +-- public\\            # static assets (/logo.png → public/logo.png)\n");
  printf("  \\-- src\\\n");
  printf("      +-- App.jsx          # router only\n");
  printf("      +-- main.jsx\n");
  printf("      +-- index.css\n");
  printf("      +-- theme.css        # CSS vars from theme blocks\n");
  printf("      +-- pages\\           # def *Page / route targets\n");
  printf("      +-- components\\      # reusable def components\n");
  printf("      \\-- layouts\\         # layout + slot\n");

  if (ctx->n_files > 0) {
    printf("\nGenerated modules:\n");
    for (int i = 0; i < ctx->n_files; i++) {
      printf("  - %s\n", ctx->files[i]);
    }
  }

  printf("\nNext steps:\n");
  printf("  cd dist\\solid\n");
  printf("  npm install\n");
  printf("  npm run dev\n");
}

/* Create dist public/ and copy project public/ if present (B6). */
static void scaffold_public_assets(const char *project_dir, const char *out) {
  char *pub_out = fs_join(out, "public");
  if (pub_out) {
    fs_mkdir_p(pub_out);
    write_path(out, "public/.gitkeep", "");
    write_path(out, "public/README.md",
               "# Static assets\n\n"
               "Files here are served at the site root by Vite.\n"
               "`public/logo.png` → `/logo.png`\n");
    free(pub_out);
  }
  char *pub_src = fs_join(project_dir, "public");
  if (pub_src) {
    if (fs_is_dir(pub_src)) {
      char *dst = fs_join(out, "public");
      if (dst) {
        fs_copy_tree(pub_src, dst);
        free(dst);
      }
    }
    free(pub_src);
  }
}

int solid_scaffold_from_ast(const char *project_dir, Node *root) {
  if (!root) return -1;
  IrProgram *ir = ir_from_ast(root, NULL);
  if (!ir) return -1;
  int rc = solid_scaffold_from_ir(project_dir, ir);
  ir_free(ir);
  return rc;
}

/* Legacy: single blob → App.jsx only (used if someone calls scaffold directly) */
int solid_scaffold(const char *project_dir, const char *app_jsx) {
  char *out = fs_join(project_dir, "dist/solid");
  if (!out) return -1;
  if (fs_mkdir_p(out) != 0) {
    free(out);
    return -1;
  }
  int rc = write_vite_skeleton(project_dir, out);
  rc |= write_path(out, "src/App.jsx",
                   app_jsx ? app_jsx
                           : "export default function App(){return null}\n");
  free(out);
  return rc;
}

int solid_scaffold_from_ir(const char *project_dir, IrProgram *ir) {
  if (!ir || !ir->root) return -1;

  char *out = fs_join(project_dir, "dist/solid");
  if (!out) return -1;

  if (fs_mkdir_p(out) != 0) {
    free(out);
    return -1;
  }

  char *p1 = fs_join(out, "src/pages");
  char *p2 = fs_join(out, "src/components");
  char *p3 = fs_join(out, "src/layouts");
  if (p1) {
    fs_mkdir_p(p1);
    free(p1);
  }
  if (p2) {
    fs_mkdir_p(p2);
    free(p2);
  }
  if (p3) {
    fs_mkdir_p(p3);
    free(p3);
  }

  int rc = write_vite_skeleton(project_dir, out);
  if (rc != 0) {
    fprintf(stderr, "Error: failed writing Solid Vite skeleton\n");
    free(out);
    return rc;
  }

  scaffold_public_assets(project_dir, out);

  /* theme.css from IR hooks only (no AST origin) */
  {
    char *theme_css = theme_css_generate_from_ir(ir);
    if (theme_css) {
      rc |= write_path(out, "src/theme.css", theme_css);
      free(theme_css);
    }
    if (rc != 0) {
      fprintf(stderr, "Error: failed writing theme.css\n");
      free(out);
      return rc;
    }
  }

  ScaffoldCtx ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.out_root = out;

  rc = solid_emit_modules_from_ir(ir, scaffold_write_src, &ctx);
  if (rc != 0 || ctx.rc != 0) {
    fprintf(stderr, "Error: failed writing Solid modules\n");
    free(out);
    return -1;
  }

  print_tree(&ctx);
  free(out);
  return 0;
}

static const BackendPort solid_port = {
    .name = "solid",
    .extension = ".jsx",
    .needs_node_check = 1,
    .generate_from_ir = solid_generate_from_ir,
    .scaffold_from_ir = solid_scaffold_from_ir,
    .generate = solid_generate,
    .scaffold = solid_scaffold,
    .scaffold_from_ast = solid_scaffold_from_ast,
};

const BackendPort *solid_backend_port(void) { return &solid_port; }
