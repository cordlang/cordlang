#include "adapters/outbound/backends/next/next_backend.h"
#include "adapters/outbound/backends/react/react_backend.h"
#include "adapters/outbound/backends/theme_css.h"
#include "application/ports/fs_port.h"
#include "domain/ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Meta-backend: reuses React IR emit, wraps with a minimal Next.js App Router
 * entry (client). Not a full RSC claim — see docs/NEXT.md.
 */

typedef struct {
  char *out_root;
  int rc;
  int n_files;
  char files[128][256];
} ScaffoldCtx;

static int write_path(const char *dir, const char *rel, const char *content) {
  char *path = fs_join(dir, rel);
  if (!path) return -1;
  int rc = fs_write_file(path, content);
  free(path);
  return rc;
}

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

char *next_generate_from_ir(IrProgram *ir) {
  /* Compile output = React modules blob + Next wrapper marker */
  char *react = react_generate_from_ir(ir);
  if (!react) return strdup("// empty next/react emit\n");
  size_t n = strlen(react) + 128;
  char *out = malloc(n);
  if (!out) {
    free(react);
    return NULL;
  }
  snprintf(out, n,
           "/* Cordlang Next meta-backend (wraps React IR emit) */\n%s", react);
  free(react);
  return out;
}

char *next_generate(Node *root) {
  if (!root) return strdup("// empty\n");
  IrProgram *ir = ir_from_ast(root, NULL);
  if (!ir) return strdup("// empty\n");
  char *out = next_generate_from_ir(ir);
  ir_free(ir);
  return out ? out : strdup("// empty\n");
}

static int write_next_skeleton(const char *out) {
  const char *pkg =
      "{\n"
      "  \"name\": \"cordlang-next-app\",\n"
      "  \"private\": true,\n"
      "  \"version\": \"0.1.0\",\n"
      "  \"scripts\": {\n"
      "    \"dev\": \"next dev\",\n"
      "    \"build\": \"next build\",\n"
      "    \"start\": \"next start\"\n"
      "  },\n"
      "  \"dependencies\": {\n"
      "    \"next\": \"^15.1.0\",\n"
      "    \"react\": \"^19.0.0\",\n"
      "    \"react-dom\": \"^19.0.0\"\n"
      "  }\n"
      "}\n";

  const char *jsconfig =
      "{\n"
      "  \"compilerOptions\": {\n"
      "    \"baseUrl\": \".\",\n"
      "    \"paths\": { \"@/*\": [\"src/*\"] }\n"
      "  }\n"
      "}\n";

  const char *gitignore = "node_modules\n.next\nout\n.DS_Store\n*.local\n";

  const char *layout =
      "export const metadata = {\n"
      "  title: 'Cordlang Next App',\n"
      "}\n"
      "\n"
      "export default function RootLayout({ children }) {\n"
      "  return (\n"
      "    <html lang=\"en\">\n"
      "      <body style={{ margin: 0, fontFamily: 'system-ui, sans-serif' }}>\n"
      "        {children}\n"
      "      </body>\n"
      "    </html>\n"
      "  )\n"
      "}\n";

  const char *page =
      "'use client'\n"
      "\n"
      "import App from '../src/App.jsx'\n"
      "\n"
      "export default function Page() {\n"
      "  return <App />\n"
      "}\n";

  const char *nextcfg =
      "/** @type {import('next').NextConfig} */\n"
      "const nextConfig = {\n"
      "  reactStrictMode: true,\n"
      "  // Cordlang emits .jsx under src/; App Router page imports it as client.\n"
      "}\n"
      "export default nextConfig\n";

  int rc = 0;
  rc |= write_path(out, "package.json", pkg);
  rc |= write_path(out, "jsconfig.json", jsconfig);
  rc |= write_path(out, ".gitignore", gitignore);
  rc |= write_path(out, "next.config.mjs", nextcfg);
  rc |= write_path(out, "app/layout.jsx", layout);
  rc |= write_path(out, "app/page.jsx", page);
  return rc;
}

int next_scaffold_from_ir(const char *project_dir, IrProgram *ir) {
  if (!project_dir || !ir) return -1;
  char *out = fs_join(project_dir, "dist/next");
  if (!out) return -1;
  if (fs_mkdir_p(out) != 0) {
    free(out);
    return -1;
  }

  int rc = write_next_skeleton(out);
  if (rc != 0) {
    fprintf(stderr, "Error: failed writing Next skeleton\n");
    free(out);
    return rc;
  }

  {
    char *theme_css = theme_css_generate_from_ir(ir);
    if (theme_css) {
      rc |= write_path(out, "src/theme.css", theme_css);
      free(theme_css);
    }
  }

  ScaffoldCtx ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.out_root = out;

  rc = react_emit_modules_from_ir(ir, scaffold_write_src, &ctx);
  if (rc != 0 || ctx.rc != 0) {
    fprintf(stderr, "Error: failed writing React modules for Next\n");
    free(out);
    return -1;
  }

  printf("Next scaffold written to: dist/next/\n");
  printf("  app/layout.jsx, app/page.jsx (client → src/App.jsx)\n");
  printf("  + %d React module(s) under src/\n", ctx.n_files);
  free(out);
  return 0;
}

int next_scaffold_from_ast(const char *project_dir, Node *root) {
  if (!root) return -1;
  IrProgram *ir = ir_from_ast(root, NULL);
  if (!ir) return -1;
  int rc = next_scaffold_from_ir(project_dir, ir);
  ir_free(ir);
  return rc;
}

static const BackendPort next_port = {
    .name = "next",
    .extension = ".jsx",
    .needs_node_check = 1,
    .generate_from_ir = next_generate_from_ir,
    .scaffold_from_ir = next_scaffold_from_ir,
    .generate = next_generate,
    .scaffold = NULL,
    .scaffold_from_ast = next_scaffold_from_ast,
};

const BackendPort *next_backend_port(void) { return &next_port; }
