#include "solid_scaffold_internal.h"

int solid_scaffold_write_path(const char *dir, const char *rel, const char *content) {
  char *path = fs_join(dir, rel);
  if (!path) return -1;
  int rc = fs_write_file(path, content);
  free(path);
  return rc;
}

int solid_scaffold_scaffold_write_src(const char *rel_from_src, const char *content,
                              void *userdata) {
  ScaffoldCtx *ctx = (ScaffoldCtx *)userdata;
  char rel[512];
  snprintf(rel, sizeof(rel), "src/%s", rel_from_src);
  int rc = solid_scaffold_write_path(ctx->out_root, rel, content);
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

int solid_scaffold_cfg_string(const char *json, const char *key, char *out, size_t outsz) {
  return json_object_copy_string(json, key, out, outsz);
}

void solid_scaffold_load_html_meta(const char *project_dir, char *lang, size_t lang_sz,
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
  if (solid_scaffold_cfg_string(json, "lang", buf, sizeof(buf)))
    snprintf(lang, lang_sz, "%s", buf);
  if (solid_scaffold_cfg_string(json, "title", buf, sizeof(buf)))
    snprintf(title, title_sz, "%s", buf);
  else if (solid_scaffold_cfg_string(json, "name", buf, sizeof(buf)))
    snprintf(title, title_sz, "%s", buf);
  free(json);
}

void solid_scaffold_print_tree(ScaffoldCtx *ctx) {
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

void solid_scaffold_scaffold_public_assets(const char *project_dir, const char *out) {
  char *pub_out = fs_join(out, "public");
  if (pub_out) {
    fs_mkdir_p(pub_out);
    solid_scaffold_write_path(out, "public/.gitkeep", "");
    solid_scaffold_write_path(out, "public/README.md",
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
