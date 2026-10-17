#include "adapters/outbound/backends/theme_css_internal.h"
#include "adapters/outbound/fonts/font_cache.h"
#include <stdlib.h>
#include <string.h>
/* ── IR path (scaffold_from_ir) ─────────────────────────── */

static int ir_is_theme_hook(const IrNode *n) {
  return n && n->kind == IR_HOOK && n->name && strcmp(n->name, "theme") == 0;
}

static void emit_theme_block_ir(Sb *sb, const IrNode *theme, int as_root) {
  const char *name = theme->value ? theme->value : "default";
  emit_theme_block_open(sb, name, as_root);

  for (size_t i = 0; i < theme->n_kids; i++) {
    IrNode *e = theme->kids[i];
    if (!e || e->kind != IR_ATTR || !e->name || !e->value) continue;
    emit_css_var(sb, e->name, e->value);
  }

  sb_append(sb, "}\n");
}

static void emit_utilities_ir(Sb *sb, const IrNode *theme) {
  int any = 0;
  for (size_t i = 0; i < theme->n_kids; i++) {
    IrNode *e = theme->kids[i];
    if (!e || e->kind != IR_ATTR || !e->name || !e->value) continue;
    if (!entry_is_color(e->name, e->value)) continue;
    if (!any) {
      sb_append(sb, "\n/* Utility classes for theme tokens */\n");
      any = 1;
    }
    const char *key = e->name;
    sb_appendf(sb, ".text-%s { color: var(--color-%s); }\n", key, key);
    sb_appendf(sb, ".bg-%s { background-color: var(--color-%s); }\n", key, key);
    sb_appendf(sb, ".border-%s { border-color: var(--color-%s); }\n", key, key);
  }

  for (size_t i = 0; i < theme->n_kids; i++) {
    IrNode *e = theme->kids[i];
    if (!e || e->kind != IR_ATTR || !e->name) continue;
    if (strcmp(e->name, "radius") == 0) {
      sb_append(sb,
                "\n.rounded-theme { border-radius: var(--radius); }\n");
      break;
    }
  }
}

char *theme_css_generate_from_ir_opts(const IrProgram *ir,
                                      const ThemeCssOpts *opts) {
  ThemeCssOpts o = theme_css_defaults(opts);
  Sb sb;
  sb_init(&sb);
  if (!sb.buf) return strdup("/* theme.css: allocation failed */\n");

  emit_header(&sb);

  {
    char *faces =
        font_faces_css_from_ir(ir, o.font_url_prefix, o.resolve_fonts);
    if (faces) {
      if (faces[0]) sb_append(&sb, faces);
      free(faces);
    }
  }

  if (!ir || !ir->root) {
    sb_append(&sb, "/* No theme declared. */\n:root {}\n");
    return sb.buf;
  }

  /* Match AST behavior: direct children of project root only. */
  const IrNode *root = ir->root;
  int n_themes = 0;
  const IrNode *first = NULL;
  for (size_t i = 0; i < root->n_kids; i++) {
    IrNode *c = root->kids[i];
    if (ir_is_theme_hook(c)) {
      if (!first) first = c;
      n_themes++;
    }
  }

  if (!first) {
    sb_append(&sb, "/* No theme declared in .cord sources. */\n:root {}\n");
    return sb.buf;
  }

  int idx = 0;
  for (size_t i = 0; i < root->n_kids; i++) {
    IrNode *c = root->kids[i];
    if (!ir_is_theme_hook(c)) continue;
    const char *name = c->value ? c->value : "default";
    sb_appendf(&sb, "/* theme \"%s\" */\n", name);
    emit_theme_block_ir(&sb, c, idx == 0);
    if (idx == 0) {
      emit_utilities_ir(&sb, c);
      emit_design_system_layer(
          &sb, theme_has_key_ir(c, "surface"),
          theme_has_key_ir(c, "surface-2") || theme_has_key_ir(c, "surfaceMuted"),
          theme_has_key_ir(c, "border"), theme_has_key_ir(c, "on-primary"),
          theme_has_key_ir(c, "font") || theme_has_key_ir(c, "font-sans"));
    }
    sb_append(&sb, "\n");
    idx++;
  }

  if (n_themes > 1) {
    sb_append(&sb,
              "/* Additional themes: add class \"theme-{name}\" on a root "
              "element to activate. */\n");
  }

  return sb.buf;
}

char *theme_css_generate_from_ir(const IrProgram *ir) {
  return theme_css_generate_from_ir_opts(ir, NULL);
}
