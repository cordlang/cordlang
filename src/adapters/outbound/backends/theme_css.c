#include "adapters/outbound/backends/theme_css_internal.h"
#include "adapters/outbound/fonts/font_cache.h"
#include <stdlib.h>
#include <string.h>
/* ── AST path (legacy scaffold_from_ast) ────────────────── */

static void emit_theme_block(Sb *sb, Node *theme, int as_root) {
  const char *name = theme->value ? theme->value : "default";
  emit_theme_block_open(sb, name, as_root);

  for (size_t i = 0; i < theme->children_len; i++) {
    Node *e = theme->children[i];
    if (!e || e->type != NODE_ATTR || !e->value || !e->value2) continue;
    emit_css_var(sb, e->value, e->value2);
  }

  sb_append(sb, "}\n");
}

static void emit_utilities(Sb *sb, Node *theme) {
  int any = 0;
  for (size_t i = 0; i < theme->children_len; i++) {
    Node *e = theme->children[i];
    if (!e || e->type != NODE_ATTR || !e->value || !e->value2) continue;
    if (!entry_is_color(e->value, e->value2)) continue;
    if (!any) {
      sb_append(sb, "\n/* Utility classes for theme tokens */\n");
      any = 1;
    }
    const char *key = e->value;
    sb_appendf(sb, ".text-%s { color: var(--color-%s); }\n", key, key);
    sb_appendf(sb, ".bg-%s { background-color: var(--color-%s); }\n", key, key);
    sb_appendf(sb, ".border-%s { border-color: var(--color-%s); }\n", key, key);
  }

  /* radius utility if present */
  for (size_t i = 0; i < theme->children_len; i++) {
    Node *e = theme->children[i];
    if (!e || e->type != NODE_ATTR || !e->value) continue;
    if (strcmp(e->value, "radius") == 0) {
      sb_append(sb,
                "\n.rounded-theme { border-radius: var(--radius); }\n");
      break;
    }
  }
}

static ThemeCssOpts theme_css_defaults(const ThemeCssOpts *opts) {
  ThemeCssOpts o;
  o.font_url_prefix = "/fonts";
  o.resolve_fonts = 1;
  if (opts) {
    if (opts->font_url_prefix && *opts->font_url_prefix)
      o.font_url_prefix = opts->font_url_prefix;
    o.resolve_fonts = opts->resolve_fonts;
  }
  return o;
}

char *theme_css_generate_opts(Node *root, const ThemeCssOpts *opts) {
  ThemeCssOpts o = theme_css_defaults(opts);
  Sb sb;
  sb_init(&sb);
  if (!sb.buf) return strdup("/* theme.css: allocation failed */\n");

  emit_header(&sb);

  {
    char *faces = font_faces_css_from_ast(root, o.font_url_prefix, o.resolve_fonts);
    if (faces) {
      if (faces[0]) sb_append(&sb, faces);
      free(faces);
    }
  }

  if (!root) {
    sb_append(&sb, "/* No theme declared. */\n:root {}\n");
    return sb.buf;
  }

  int n_themes = 0;
  Node *first = NULL;
  for (size_t i = 0; i < root->children_len; i++) {
    Node *c = root->children[i];
    if (c && c->type == NODE_THEME) {
      if (!first) first = c;
      n_themes++;
    }
  }

  if (!first) {
    sb_append(&sb, "/* No theme declared in .cord sources. */\n:root {}\n");
    return sb.buf;
  }

  int idx = 0;
  for (size_t i = 0; i < root->children_len; i++) {
    Node *c = root->children[i];
    if (!c || c->type != NODE_THEME) continue;
    const char *name = c->value ? c->value : "default";
    sb_appendf(&sb, "/* theme \"%s\" */\n", name);
    emit_theme_block(&sb, c, idx == 0);
    if (idx == 0) {
      emit_utilities(&sb, c);
      emit_design_system_layer(
          &sb, theme_has_key_ast(c, "surface"),
          theme_has_key_ast(c, "surface-2") || theme_has_key_ast(c, "surfaceMuted"),
          theme_has_key_ast(c, "border"), theme_has_key_ast(c, "on-primary"),
          theme_has_key_ast(c, "font") || theme_has_key_ast(c, "font-sans"));
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

char *theme_css_generate(Node *root) {
  return theme_css_generate_opts(root, NULL);
}
