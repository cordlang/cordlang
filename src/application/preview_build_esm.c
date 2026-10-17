#include "application/preview_internal.h"

/* ── build esm ──────────────────────────────────────────── */

static int ends_with_ci(const char *s, const char *suf) {
  size_t ls = strlen(s), lx = strlen(suf);
  if (ls < lx) return 0;
  for (size_t i = 0; i < lx; i++) {
    char a = s[ls - lx + i], b = suf[i];
    if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
    if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
    if (a != b) return 0;
  }
  return 1;
}

static void cord_to_js_rel(const char *rel_cord, char *out, size_t n) {
  snprintf(out, n, "%s", rel_cord);
  size_t l = strlen(out);
  if (l > 5 && ends_with_ci(out, ".cord")) {
    out[l - 5] = '\0';
    strncat(out, ".js", n - strlen(out) - 1);
  }
}

static void rewrite_cord_imports_to_js(char *js) {
  /* Rewrite '/path/Foo.cord' → '/path/Foo.js' inside the emit. */
  char *p = js;
  while (p && *p) {
    char *hit = strstr(p, ".cord'");
    if (!hit) hit = strstr(p, ".cord\"");
    if (!hit) break;
    hit[1] = 'j';
    hit[2] = 's';
    /* shift: ".cord'" is 6 chars, ".js'" is 4 — compact */
    memmove(hit + 3, hit + 5, strlen(hit + 5) + 1);
    p = hit + 3;
  }
}

typedef struct {
  PreviewCtx *ctx;
  const char *out_root;
  int errors;
  int written;
} BuildWalk;

static int write_text(const char *path, const char *body) {
  char *dir = fs_dirname(path);
  if (dir) {
    fs_mkdir_p(dir);
    free(dir);
  }
  return fs_write_file(path, body ? body : "");
}

static void build_one_cord(const char *abs, const char *rel, void *ud) {
  BuildWalk *bw = (BuildWalk *)ud;
  PreviewCtx *ctx = bw->ctx;

  CompileResult r = compiler_parse_file(abs);
  if (!r.ok || !r.ast) {
    fprintf(stderr, "build esm: skip %s (%s)\n", rel,
            r.error ? r.error : "parse error");
    compiler_result_free(&r);
    bw->errors++;
    return;
  }

  int is_entry = same_file(abs, ctx->entry_abs);
  char export_name[128];
  export_name[0] = '\0';
  if (!is_entry) {
    char *en = compiler_module_export_name(rel, NULL);
    if (en) {
      snprintf(export_name, sizeof(export_name), "%s", en);
      free(en);
    }
    compiler_wrap_module_body(r.ast->root, export_name, abs);
  }

  IrProgram *ir = ir_from_ast(r.ast->root, abs);
  if (!ir) {
    compiler_result_free(&r);
    bw->errors++;
    return;
  }

  EsmModuleCtx mod;
  memset(&mod, 0, sizeof(mod));
  mod.kind = is_entry ? ESM_MOD_ENTRY : ESM_MOD_COMPONENT;
  mod.export_name = export_name[0] ? export_name : NULL;
  mod.source_rel = rel;
  mod.abs_path = abs;
  mod.project_root = ctx->root_abs[0] ? ctx->root_abs : ctx->root;

  char *js = esm_generate_module(ir, &mod);
  ir_free(ir);
  compiler_result_free(&r);
  if (!js) {
    bw->errors++;
    return;
  }
  if (mod.truncated)
    fprintf(stderr, "build esm: import limit truncated in %s\n", rel);

  rewrite_cord_imports_to_js(js);

  char js_rel[512];
  cord_to_js_rel(rel, js_rel, sizeof(js_rel));
  char *out_path = fs_join(bw->out_root, js_rel);
  if (!out_path || write_text(out_path, js) != 0) {
    fprintf(stderr, "build esm: cannot write %s\n", js_rel);
    bw->errors++;
  } else {
    bw->written++;
  }
  free(out_path);
  free(js);
}

int preview_service_build_esm(const char *project_dir) {
  const char *dir = project_dir && *project_dir ? project_dir : ".";
  backend_register_all();

  char *cfg = fs_join(dir, "cordlang.json");
  if (!cfg || !fs_exists(cfg)) {
    fprintf(stderr, "Error: not a Cordlang project (missing cordlang.json)\n");
    free(cfg);
    return 1;
  }
  free(cfg);

  PreviewCtx ctx;
  if (preview_ctx_init(&ctx, dir) != 0) return 1;

  {
    CompileResult probe = compiler_parse_project(ctx.entry_abs);
    if (!probe.ok || !probe.ast) {
      fprintf(stderr, "Error: %s\n",
              probe.error ? probe.error : "el proyecto no compila");
      compiler_result_free(&probe);
      preview_ctx_free(&ctx);
      return 1;
    }
    compiler_result_free(&probe);
  }

  char *out_root = fs_join(dir, "dist/esm");
  if (!out_root) {
    preview_ctx_free(&ctx);
    return 1;
  }
  fs_mkdir_p(out_root);

  /* Shell without HMR client; entry URL uses .js */
  char entry_js[512];
  {
    const char *e = ctx.entry_url;
    while (*e == '/') e++;
    cord_to_js_rel(e, entry_js, sizeof(entry_js));
  }
  char entry_url_js[520];
  snprintf(entry_url_js, sizeof(entry_url_js), "/%s", entry_js);

  char *html =
      esm_index_html(ctx.lang, ctx.title, entry_url_js, ctx.has_site_css,
                     ctx.has_site_js, ctx.has_favicon, ctx.has_logo_svg, 0);
  if (html) {
    char *hp = fs_join(out_root, "index.html");
    if (hp) {
      write_text(hp, html);
      free(hp);
    }
    free(html);
  }

  char *cord_dir = fs_join(out_root, "@cord");
  if (cord_dir) {
    fs_mkdir_p(cord_dir);
    free(cord_dir);
  }
  {
    char *p = fs_join(out_root, "@cord/runtime.js");
    if (p) {
      write_text(p, esm_runtime_js());
      free(p);
    }
  }

  {
    CompileResult r = compiler_parse_project(ctx.entry_abs);
    if (r.ok && r.ast) {
      IrProgram *ir = ir_from_ast(r.ast->root, ctx.entry_abs);
      if (ir) {
        ThemeCssOpts opts;
        memset(&opts, 0, sizeof(opts));
        opts.font_url_prefix = "/@cord/fonts";
        opts.resolve_fonts = 1;
        char *base = esm_base_css(ir);
        char *theme = theme_css_generate_from_ir_opts(ir, &opts);
        if (base) {
          char *p = fs_join(out_root, "@cord/base.css");
          if (p) {
            write_text(p, base);
            free(p);
          }
        }
        if (theme) {
          char *p = fs_join(out_root, "@cord/theme.css");
          if (p) {
            write_text(p, theme);
            free(p);
          }
        }
        if (theme && base) {
          size_t n = strlen(theme) + strlen(base) + 8;
          char *styles = malloc(n);
          if (styles) {
            snprintf(styles, n, "%s\n%s", theme, base);
            char *p = fs_join(out_root, "@cord/styles.css");
            if (p) {
              write_text(p, styles);
              free(p);
            }
            free(styles);
          }
        }
        free(base);
        free(theme);
        char *fonts_out = fs_join(out_root, "@cord/fonts");
        if (fonts_out) {
          font_cache_install_from_ir(ir, fonts_out);
          free(fonts_out);
        }
        ir_free(ir);
      }
    }
    compiler_result_free(&r);
  }

  /* Copy public/ to dist/esm root */
  {
    char *pub = fs_join(dir, "public");
    if (pub && fs_exists(pub) && fs_is_dir(pub)) {
      fs_copy_tree(pub, out_root);
    }
    free(pub);
  }

  BuildWalk bw;
  memset(&bw, 0, sizeof(bw));
  bw.ctx = &ctx;
  bw.out_root = out_root;
  fs_walk_cord(dir, build_one_cord, &bw);

  printf("build esm: wrote %d module(s) to %s\n", bw.written, out_root);
  if (bw.errors)
    fprintf(stderr, "build esm: %d file(s) failed\n", bw.errors);

  free(out_root);
  preview_ctx_free(&ctx);
  return bw.errors ? 1 : 0;
}