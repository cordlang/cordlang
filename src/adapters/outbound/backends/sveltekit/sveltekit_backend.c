#include "adapters/outbound/backends/sveltekit/sveltekit_backend.h"
#include "adapters/outbound/backends/svelte/svelte_backend.h"
#include "adapters/outbound/backends/theme_css.h"
#include "application/ports/fs_port.h"
#include "domain/ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Meta-backend: reuses Svelte IR emit into src/lib/, wraps with SvelteKit
 * routes (+page.svelte). See docs/SVELTEKIT.md.
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

static int scaffold_write_lib(const char *rel_from_src, const char *content,
                              void *userdata) {
  ScaffoldCtx *ctx = (ScaffoldCtx *)userdata;
  char rel[512];
  snprintf(rel, sizeof(rel), "src/lib/%s", rel_from_src);
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

char *sveltekit_generate_from_ir(IrProgram *ir) {
  char *sv = svelte_generate_from_ir(ir);
  if (!sv) return strdup("<!-- empty sveltekit emit -->\n");
  size_t n = strlen(sv) + 96;
  char *out = malloc(n);
  if (!out) {
    free(sv);
    return NULL;
  }
  snprintf(out, n,
           "<!-- Cordlang SvelteKit meta-backend (wraps Svelte IR emit) -->\n%s",
           sv);
  free(sv);
  return out;
}

char *sveltekit_generate(Node *root) {
  if (!root) return strdup("<!-- empty -->\n");
  IrProgram *ir = ir_from_ast(root, NULL);
  if (!ir) return strdup("<!-- empty -->\n");
  char *out = sveltekit_generate_from_ir(ir);
  ir_free(ir);
  return out ? out : strdup("<!-- empty -->\n");
}

static int write_kit_skeleton(const char *out) {
  const char *pkg =
      "{\n"
      "  \"name\": \"cordlang-sveltekit-app\",\n"
      "  \"private\": true,\n"
      "  \"version\": \"0.1.0\",\n"
      "  \"type\": \"module\",\n"
      "  \"scripts\": {\n"
      "    \"dev\": \"vite dev\",\n"
      "    \"build\": \"vite build\",\n"
      "    \"preview\": \"vite preview\"\n"
      "  },\n"
      "  \"devDependencies\": {\n"
      "    \"@sveltejs/adapter-auto\": \"^3.3.1\",\n"
      "    \"@sveltejs/kit\": \"^2.15.0\",\n"
      "    \"@sveltejs/vite-plugin-svelte\": \"^5.0.3\",\n"
      "    \"svelte\": \"^5.16.0\",\n"
      "    \"vite\": \"^6.0.3\"\n"
      "  }\n"
      "}\n";

  const char *svelte_cfg =
      "import adapter from '@sveltejs/adapter-auto'\n"
      "\n"
      "/** @type {import('@sveltejs/kit').Config} */\n"
      "const config = {\n"
      "  kit: {\n"
      "    adapter: adapter(),\n"
      "  },\n"
      "}\n"
      "\n"
      "export default config\n";

  const char *vite =
      "import { sveltekit } from '@sveltejs/kit/vite'\n"
      "import { defineConfig } from 'vite'\n"
      "\n"
      "export default defineConfig({\n"
      "  plugins: [sveltekit()],\n"
      "}\n";

  const char *app_html =
      "<!DOCTYPE html>\n"
      "<html lang=\"en\">\n"
      "  <head>\n"
      "    <meta charset=\"utf-8\" />\n"
      "    <meta name=\"viewport\" content=\"width=device-width, "
      "initial-scale=1\" />\n"
      "    %sveltekit.head%\n"
      "  </head>\n"
      "  <body data-sveltekit-preload-data=\"hover\">\n"
      "    <div style=\"display: contents\">%sveltekit.body%</div>\n"
      "  </body>\n"
      "</html>\n";

  const char *layout =
      "<slot />\n";

  const char *page =
      "<script>\n"
      "  import App from '$lib/App.svelte'\n"
      "</script>\n"
      "\n"
      "<App />\n";

  const char *gitignore =
      "node_modules\n"
      ".svelte-kit\n"
      "build\n"
      ".DS_Store\n"
      "*.local\n";

  int rc = 0;
  rc |= write_path(out, "package.json", pkg);
  rc |= write_path(out, "svelte.config.js", svelte_cfg);
  rc |= write_path(out, "vite.config.js", vite);
  rc |= write_path(out, "src/app.html", app_html);
  rc |= write_path(out, "src/routes/+layout.svelte", layout);
  rc |= write_path(out, "src/routes/+page.svelte", page);
  rc |= write_path(out, ".gitignore", gitignore);
  return rc;
}

int sveltekit_scaffold_from_ir(const char *project_dir, IrProgram *ir) {
  if (!project_dir || !ir) return -1;
  char *out = fs_join(project_dir, "dist/sveltekit");
  if (!out) return -1;
  if (fs_mkdir_p(out) != 0) {
    free(out);
    return -1;
  }

  int rc = write_kit_skeleton(out);
  if (rc != 0) {
    fprintf(stderr, "Error: failed writing SvelteKit skeleton\n");
    free(out);
    return rc;
  }

  {
    char *theme_css = theme_css_generate_from_ir(ir);
    if (theme_css) {
      rc |= write_path(out, "src/lib/theme.css", theme_css);
      free(theme_css);
    }
  }

  ScaffoldCtx ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.out_root = out;

  rc = svelte_emit_modules_from_ir(ir, scaffold_write_lib, &ctx);
  if (rc != 0 || ctx.rc != 0) {
    fprintf(stderr, "Error: failed writing Svelte modules for SvelteKit\n");
    free(out);
    return -1;
  }

  printf("SvelteKit scaffold written to: dist/sveltekit/\n");
  printf("  src/routes/+page.svelte → $lib/App.svelte\n");
  printf("  + %d Svelte module(s) under src/lib/\n", ctx.n_files);
  free(out);
  return 0;
}

int sveltekit_scaffold_from_ast(const char *project_dir, Node *root) {
  if (!root) return -1;
  IrProgram *ir = ir_from_ast(root, NULL);
  if (!ir) return -1;
  int rc = sveltekit_scaffold_from_ir(project_dir, ir);
  ir_free(ir);
  return rc;
}

static const BackendPort sveltekit_port = {
    .name = "sveltekit",
    .extension = ".svelte",
    .needs_node_check = 1,
    .generate_from_ir = sveltekit_generate_from_ir,
    .scaffold_from_ir = sveltekit_scaffold_from_ir,
    .generate = sveltekit_generate,
    .scaffold = NULL,
    .scaffold_from_ast = sveltekit_scaffold_from_ast,
};

const BackendPort *sveltekit_backend_port(void) { return &sveltekit_port; }
