#include "adapters/outbound/backends/react/react_backend.h"
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
  char *out_root; /* dist/react */
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

  const char *html =
      "<!doctype html>\n"
      "<html lang=\"en\">\n"
      "  <head>\n"
      "    <meta charset=\"UTF-8\" />\n"
      "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" />\n"
      "    <title>Cordlang App</title>\n"
      "  </head>\n"
      "  <body>\n"
      "    <div id=\"root\"></div>\n"
      "    <script type=\"module\" src=\"/src/main.jsx\"></script>\n"
      "  </body>\n"
      "</html>\n";

  const char *main_jsx =
      "import React from 'react'\n"
      "import { createRoot } from 'react-dom/client'\n"
      "import App from './App.jsx'\n"
      "import './index.css'\n"
      "\n"
      "createRoot(document.getElementById('root')).render(\n"
      "  <React.StrictMode>\n"
      "    <App />\n"
      "  </React.StrictMode>\n"
      ")\n";

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

  /* Shared ErrorBoundary (class component — React requires this for EB) */
  const char *eb =
      "import { Component } from 'react';\n"
      "\n"
      "export default class ErrorBoundary extends Component {\n"
      "  constructor(props) {\n"
      "    super(props);\n"
      "    this.state = { hasError: false };\n"
      "  }\n"
      "  static getDerivedStateFromError() {\n"
      "    return { hasError: true };\n"
      "  }\n"
      "  componentDidCatch(error, info) {\n"
      "    if (typeof console !== 'undefined') console.error(error, info);\n"
      "  }\n"
      "  render() {\n"
      "    if (this.state.hasError) {\n"
      "      return this.props.fallback ?? <div>Something went wrong.</div>;\n"
      "    }\n"
      "    return this.props.children;\n"
      "  }\n"
      "}\n";
  rc |= write_path(out, "src/ErrorBoundary.jsx", eb);
  return rc;
}

static void print_tree(ScaffoldCtx *ctx) {
  printf("\nReact app generated in: dist\\react\n");
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
  printf("  cd dist\\react\n");
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

int react_scaffold_from_ast(const char *project_dir, Node *root) {
  char *out = fs_join(project_dir, "dist/react");
  if (!out) return -1;

  if (fs_mkdir_p(out) != 0) {
    free(out);
    return -1;
  }

  /* ensure subdirs exist */
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
    fprintf(stderr, "Error: failed writing Vite skeleton\n");
    free(out);
    return rc;
  }

  scaffold_public_assets(project_dir, out);

  /* theme.css from NODE_THEME (first → :root) */
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

  rc = react_emit_modules(root, scaffold_write_src, &ctx);
  if (rc != 0 || ctx.rc != 0) {
    fprintf(stderr, "Error: failed writing React modules\n");
    free(out);
    return -1;
  }

  print_tree(&ctx);
  free(out);
  return 0;
}

/* Legacy: single blob → App.jsx only (used if someone calls scaffold directly) */
int react_scaffold(const char *project_dir, const char *app_jsx) {
  char *out = fs_join(project_dir, "dist/react");
  if (!out) return -1;
  if (fs_mkdir_p(out) != 0) {
    free(out);
    return -1;
  }
  int rc = write_vite_skeleton(out);
  rc |= write_path(out, "src/App.jsx",
                   app_jsx ? app_jsx
                           : "export default function App(){return null}\n");
  free(out);
  return rc;
}

int react_scaffold_from_ir(const char *project_dir, IrProgram *ir) {
  if (!ir || !ir->root) return -1;

  char *out = fs_join(project_dir, "dist/react");
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
    fprintf(stderr, "Error: failed writing Vite skeleton\n");
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

  rc = react_emit_modules_from_ir(ir, scaffold_write_src, &ctx);
  if (rc != 0 || ctx.rc != 0) {
    fprintf(stderr, "Error: failed writing React modules\n");
    free(out);
    return -1;
  }

  print_tree(&ctx);
  free(out);
  return 0;
}

static const BackendPort react_port = {
    .name = "react",
    .extension = ".jsx",
    .generate_from_ir = react_generate_from_ir,
    .scaffold_from_ir = react_scaffold_from_ir,
    .generate = react_generate,
    .scaffold = react_scaffold,
    .scaffold_from_ast = react_scaffold_from_ast,
};

const BackendPort *react_backend_port(void) { return &react_port; }
