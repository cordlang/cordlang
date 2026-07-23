#include "adapters/outbound/backends/svelte/svelte_backend.h"
#include "adapters/outbound/backends/theme_css.h"
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

static int write_vite_skeleton(const char *out) {
  const char *pkg =
      "{\n"
      "  \"name\": \"cordlang-svelte-app\",\n"
      "  \"private\": true,\n"
      "  \"version\": \"0.1.0\",\n"
      "  \"type\": \"module\",\n"
      "  \"scripts\": {\n"
      "    \"dev\": \"vite\",\n"
      "    \"build\": \"vite build\",\n"
      "    \"preview\": \"vite preview\"\n"
      "  },\n"
      "  \"devDependencies\": {\n"
      "    \"@sveltejs/vite-plugin-svelte\": \"^5.0.3\",\n"
      "    \"autoprefixer\": \"^10.4.20\",\n"
      "    \"postcss\": \"^8.4.49\",\n"
      "    \"svelte\": \"^5.16.0\",\n"
      "    \"tailwindcss\": \"^3.4.17\",\n"
      "    \"vite\": \"^6.0.3\"\n"
      "  }\n"
      "}\n";

  const char *vite =
      "import { defineConfig } from 'vite'\n"
      "import { svelte } from '@sveltejs/vite-plugin-svelte'\n"
      "\n"
      "export default defineConfig({\n"
      "  plugins: [svelte()],\n"
      "})\n";

  const char *svelte_cfg =
      "import { vitePreprocess } from '@sveltejs/vite-plugin-svelte'\n"
      "\n"
      "export default {\n"
      "  preprocess: vitePreprocess(),\n"
      "  compilerOptions: {\n"
      "    runes: true,\n"
      "  },\n"
      "}\n";

  const char *html =
      "<!doctype html>\n"
      "<html lang=\"en\">\n"
      "  <head>\n"
      "    <meta charset=\"UTF-8\" />\n"
      "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" />\n"
      "    <title>Cordlang Svelte App</title>\n"
      "  </head>\n"
      "  <body>\n"
      "    <div id=\"app\"></div>\n"
      "    <script type=\"module\" src=\"/src/main.js\"></script>\n"
      "  </body>\n"
      "</html>\n";

  const char *main_js =
      "import { mount } from 'svelte'\n"
      "import './app.css'\n"
      "import App from './App.svelte'\n"
      "\n"
      "mount(App, { target: document.getElementById('app') })\n";

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
      "a {\n"
      "  color: var(--color-primary, #2563eb);\n"
      "}\n"
      "a:hover {\n"
      "  text-decoration: underline;\n"
      "}\n";

  const char *tailwind =
      "/** @type {import('tailwindcss').Config} */\n"
      "export default {\n"
      "  content: ['./index.html', './src/**/*.{js,svelte}'],\n"
      "  theme: { extend: {} },\n"
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
  rc |= write_path(out, "svelte.config.js", svelte_cfg);
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
  printf("\nSvelte app generated in: dist\\svelte\n");
  printf("  +-- package.json\n");
  printf("  +-- vite.config.js\n");
  printf("  +-- svelte.config.js\n");
  printf("  +-- index.html\n");
  printf("  +-- public\\            # static assets (/logo.png → public/logo.png)\n");
  printf("  \\-- src\\\n");
  printf("      +-- App.svelte       # router\n");
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
  printf("  cd dist\\svelte\n");
  printf("  npm install\n");
  printf("  npm run dev\n");
}

int svelte_scaffold_from_ast(const char *project_dir, Node *root) {
  char *out = fs_join(project_dir, "dist/svelte");
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

  int rc = write_vite_skeleton(out);
  if (rc != 0) {
    fprintf(stderr, "Error: failed writing Svelte skeleton\n");
    free(out);
    return rc;
  }

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

  rc = svelte_emit_modules(root, scaffold_write_src, &ctx);
  if (rc != 0 || ctx.rc != 0) {
    fprintf(stderr, "Error: failed writing Svelte modules\n");
    free(out);
    return -1;
  }

  print_scaffold_summary(&ctx);

  free(out);
  return 0;
}

int svelte_scaffold(const char *project_dir, const char *blob) {
  char *out = fs_join(project_dir, "dist/svelte");
  if (!out) return -1;
  if (fs_mkdir_p(out) != 0) {
    free(out);
    return -1;
  }
  int rc = write_vite_skeleton(out);
  rc |= write_path(out, "src/App.svelte",
                   blob ? blob : "<script></script>\n<p>Empty</p>\n");
  free(out);
  return rc;
}

int svelte_scaffold_from_ir(const char *project_dir, IrProgram *ir) {
  if (!ir || !ir->root) return -1;

  char *out = fs_join(project_dir, "dist/svelte");
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

  int rc = write_vite_skeleton(out);
  if (rc != 0) {
    fprintf(stderr, "Error: failed writing Svelte skeleton\n");
    free(out);
    return rc;
  }

  scaffold_public_assets(project_dir, out);

  /* theme.css: still AST-based when origin available; not used for body codegen */
  {
    Node *theme_root = ir_project_origin(ir);
    if (!theme_root && ir->root) theme_root = ir->root->origin;
    char *theme_css =
        theme_root ? theme_css_generate(theme_root)
                   : strdup("/* No theme declared. */\n:root {}\n");
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
  rc = svelte_emit_modules_from_ir(ir, scaffold_write_src, &ctx);
  if (rc != 0 || ctx.rc != 0) {
    fprintf(stderr, "Error: failed writing Svelte modules\n");
    free(out);
    return -1;
  }

  print_scaffold_summary(&ctx);
  free(out);
  return 0;
}

static const BackendPort svelte_port = {
    .name = "svelte",
    .extension = ".svelte",
    .generate_from_ir = svelte_generate_from_ir,
    .scaffold_from_ir = svelte_scaffold_from_ir,
    .generate = svelte_generate,
    .scaffold = svelte_scaffold,
    .scaffold_from_ast = svelte_scaffold_from_ast,
};

const BackendPort *svelte_backend_port(void) { return &svelte_port; }
