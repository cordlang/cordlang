#ifndef CORDLANG_SVELTE_SCAFFOLD_INTERNAL_H
#define CORDLANG_SVELTE_SCAFFOLD_INTERNAL_H
#include "adapters/outbound/backends/svelte/svelte_backend.h"
#include "adapters/outbound/backends/preset_registry.h"
#include "adapters/outbound/backends/theme_css.h"
#include "adapters/outbound/fonts/font_cache.h"
#include "adapters/outbound/html_escape.h"
#include "adapters/outbound/json/json_mini.h"
#include "application/ports/fs_port.h"
#include "domain/ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *out_root;
  int rc;
  int n_files;
  char files[128][256];
} ScaffoldCtx;
int svelte_scaffold_write_path(const char *dir, const char *rel, const char *content);
int svelte_scaffold_scaffold_write_src(const char *rel_from_src, const char *content,
                              void *userdata);
int svelte_scaffold_cfg_string(const char *json, const char *key, char *out, size_t outsz);
void svelte_scaffold_load_html_meta(const char *project_dir, char *lang, size_t lang_sz,
                           char *title, size_t title_sz);
int svelte_scaffold_write_vite_skeleton(const char *project_dir, const char *out);
void svelte_scaffold_scaffold_public_assets(const char *project_dir, const char *out);
void svelte_scaffold_print_scaffold_summary(ScaffoldCtx *ctx);

#endif
