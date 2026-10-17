#ifndef CORDLANG_SOLID_SCAFFOLD_INTERNAL_H
#define CORDLANG_SOLID_SCAFFOLD_INTERNAL_H
#include "adapters/outbound/backends/solid/solid_backend.h"
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
int solid_scaffold_write_path(const char *dir, const char *rel, const char *content);
int solid_scaffold_scaffold_write_src(const char *rel_from_src, const char *content,
                              void *userdata);
int solid_scaffold_cfg_string(const char *json, const char *key, char *out, size_t outsz);
void solid_scaffold_load_html_meta(const char *project_dir, char *lang, size_t lang_sz,
                           char *title, size_t title_sz);
int solid_scaffold_write_vite_skeleton(const char *project_dir, const char *out);
void solid_scaffold_print_tree(ScaffoldCtx *ctx);
void solid_scaffold_scaffold_public_assets(const char *project_dir, const char *out);

#endif
