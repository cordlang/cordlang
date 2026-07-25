#include "adapters/outbound/backends/vue/vue_backend.h"
#include "adapters/outbound/backends/preset_registry.h"
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
  char *out_root;
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

static int cfg_string(const char *json, const char *key, char *out, size_t outsz) {
  return json_object_copy_string(json, key, out, outsz);
}

static void load_html_meta(const char *project_dir, char *lang, size_t lang_sz,
                           char *title, size_t title_sz) {
  snprintf(lang, lang_sz, "en");
  snprintf(title, title_sz, "Cordlang Vue App");
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
      "  \"name\": \"cordlang-vue-app\",\n"
      "  \"private\": true,\n"
      "  \"version\": \"0.1.0\",\n"
      "  \"type\": \"module\",\n"
      "  \"scripts\": {\n"
      "    \"dev\": \"vite\",\n"
      "    \"build\": \"vite build\",\n"
      "    \"preview\": \"vite preview\"\n"
      "  },\n"
      "  \"dependencies\": {\n"
      "    \"vue\": \"^3.5.13\",\n"
      "    \"vue-router\": \"^4.5.0\"\n"
      "  },\n"
      "  \"devDependencies\": {\n"
      "    \"@vitejs/plugin-vue\": \"^5.2.1\",\n"
      "    \"autoprefixer\": \"^10.4.20\",\n"
      "    \"postcss\": \"^8.4.49\",\n"
      "    \"tailwindcss\": \"^3.4.17\",\n"
      "    \"vite\": \"^6.0.3\"\n"
      "  }\n"
      "}\n";

  const char *vite =
      "import { defineConfig } from 'vite'\n"
      "import vue from '@vitejs/plugin-vue'\n"
      "\n"
      "export default defineConfig({\n"
      "  plugins: [vue()],\n"
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
           "    <div id=\"app\"></div>\n"
           "    <script type=\"module\" src=\"/src/main.js\"></script>\n"
           "  </body>\n"
           "</html>\n",
           lang_e, title_e);

  const char *main_js =
      "import { createApp } from 'vue'\n"
      "import './app.css'\n"
      "import App from './App.vue'\n"
      "import { router } from './router.js'\n"
      "\n"
      "const app = createApp(App)\n"
      "if (router) app.use(router)\n"
      "app.mount('#app')\n";

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
      "  .card {\n"
      "    @apply bg-white shadow-md overflow-hidden;\n"
      "    border-radius: var(--radius, 0.75rem);\n"
      "  }\n"
      "}\n"
      "\n"
      "body {\n"
      "  @apply m-0 min-h-screen antialiased;\n"
      "  background-color: var(--color-bg, #f9fafb);\n"
      "  color: var(--color-text, #111827);\n"
      "}\n"
      "\n"
      ".text-muted {\n"
      "  color: var(--color-muted, #57534e);\n"
      "}\n"
      "\n"
      "a {\n"
      "  color: var(--color-primary, #2563eb);\n"
      "}\n"
      "a:hover {\n"
      "  text-decoration: underline;\n"
      "}\n";

  /* Same Cord spacing scale as react_scaffold (p=16 → 1rem, not Tailwind 4rem). */
  const char *tailwind =
      "/** @type {import('tailwindcss').Config} */\n"
      "export default {\n"
      "  content: ['./index.html', './src/**/*.{js,vue}'],\n"
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
  rc |= write_path(out, "src/main.js", main_js);
  rc |= write_path(out, "src/app.css", css);
  /* Stub until scaffold_from_ast overwrites with real theme tokens */
  rc |= write_path(out, "src/theme.css",
                   "/* No theme declared. */\n:root {}\n");
  return rc;
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

static void print_scaffold_summary(ScaffoldCtx *ctx) {
  printf("\nVue app generated in: dist\\vue\n");
  printf("  +-- package.json\n");
  printf("  +-- vite.config.js\n");
  printf("  +-- index.html\n");
  printf("  +-- public\\            # static assets (/logo.png → public/logo.png)\n");
  printf("  \\-- src\\\n");
  printf("      +-- App.vue       # router\n");
  printf("      +-- main.js\n");
  printf("      +-- app.css\n");
  printf("      +-- theme.css        # CSS vars from theme blocks\n");
  printf("      +-- pages\\\n");
  printf("      +-- components\\\n");
  printf("      \\-- layouts\\\n");
  if (ctx->n_files > 0) {
    printf("\nGenerated modules:\n");
    for (int i = 0; i < ctx->n_files; i++) printf("  - %s\n", ctx->files[i]);
  }
  printf("\nNext steps:\n");
  printf("  cd dist\\vue\n");
  printf("  npm install\n");
  printf("  npm run dev\n");
}

int vue_scaffold_from_ast(const char *project_dir, Node *root) {
  char *out = fs_join(project_dir, "dist/vue");
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
    fprintf(stderr, "Error: failed writing Vue skeleton\n");
    free(out);
    return rc;
  }
  {
    char *pj = fs_join(out, "package.json");
    if (pj) {
      preset_merge_package_json(pj, project_dir, PRESET_BACKEND_VUE);
      free(pj);
    }
  }
  preset_write_bridges(out, project_dir, PRESET_BACKEND_VUE);

  scaffold_public_assets(project_dir, out);

  {
    char *theme_css = theme_css_generate(root);
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

  rc = vue_emit_modules(root, scaffold_write_src, &ctx);
  if (rc != 0 || ctx.rc != 0) {
    fprintf(stderr, "Error: failed writing Vue modules\n");
    free(out);
    return -1;
  }

  print_scaffold_summary(&ctx);

  free(out);
  return 0;
}

int vue_scaffold(const char *project_dir, const char *blob) {
  char *out = fs_join(project_dir, "dist/vue");
  if (!out) return -1;
  if (fs_mkdir_p(out) != 0) {
    free(out);
    return -1;
  }
  int rc = write_vite_skeleton(project_dir, out);
  rc |= write_path(out, "src/App.vue",
                   blob ? blob : "<script></script>\n<p>Empty</p>\n");
  free(out);
  return rc;
}

int vue_scaffold_from_ir(const char *project_dir, IrProgram *ir) {
  if (!ir || !ir->root) return -1;

  char *out = fs_join(project_dir, "dist/vue");
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
    fprintf(stderr, "Error: failed writing Vue skeleton\n");
    free(out);
    return rc;
  }
  {
    char *pj = fs_join(out, "package.json");
    if (pj) {
      preset_merge_package_json(pj, project_dir, PRESET_BACKEND_VUE);
      free(pj);
    }
  }
  preset_write_bridges(out, project_dir, PRESET_BACKEND_VUE);

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

  /* Pure IR body emission (IR-2) */
  rc = vue_emit_modules_from_ir(ir, scaffold_write_src, &ctx);
  if (rc != 0 || ctx.rc != 0) {
    fprintf(stderr, "Error: failed writing Vue modules\n");
    free(out);
    return -1;
  }

  print_scaffold_summary(&ctx);
  free(out);
  return 0;
}

static const BackendPort vue_port = {
    .name = "vue",
    .extension = ".vue",
    .needs_node_check = 1,
    .generate_from_ir = vue_generate_from_ir,
    .scaffold_from_ir = vue_scaffold_from_ir,
    .generate = vue_generate,
    .scaffold = vue_scaffold,
    .scaffold_from_ast = vue_scaffold_from_ast,
};

const BackendPort *vue_backend_port(void) { return &vue_port; }
