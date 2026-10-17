#ifndef CORDLANG_THEME_CSS_INTERNAL_H
#define CORDLANG_THEME_CSS_INTERNAL_H

#include "adapters/outbound/backends/theme_css.h"

typedef struct {
  char *buf;
  size_t len;
  size_t cap;
} Sb;

void sb_init(Sb *sb);
void sb_append(Sb *sb, const char *s);
void sb_appendf(Sb *sb, const char *fmt, ...);
int entry_is_color(const char *key, const char *val);
void emit_css_var(Sb *sb, const char *key, const char *val);
int theme_has_key_ast(Node *theme, const char *key);
int theme_has_key_ir(const IrNode *theme, const char *key);
void emit_design_system_layer(Sb *sb, int has_surface, int has_surface2,
                              int has_border, int has_on_primary, int has_font);
void emit_theme_block_open(Sb *sb, const char *name, int as_root);
void emit_header(Sb *sb);
ThemeCssOpts theme_css_defaults(const ThemeCssOpts *opts);

#endif