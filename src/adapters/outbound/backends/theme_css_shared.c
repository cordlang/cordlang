#include "adapters/outbound/backends/theme_css_internal.h"
#include <string.h>

/* Design-system layer: type scale, elevation, motion, semantic fallbacks. */
int theme_has_key_ast(Node *theme, const char *key) {
  if (!theme || !key) return 0;
  for (size_t i = 0; i < theme->children_len; i++) {
    Node *e = theme->children[i];
    if (e && e->type == NODE_ATTR && e->value && strcmp(e->value, key) == 0)
      return 1;
  }
  return 0;
}

int theme_has_key_ir(const IrNode *theme, const char *key) {
  if (!theme || !key) return 0;
  for (size_t i = 0; i < theme->n_kids; i++) {
    IrNode *e = theme->kids[i];
    if (e && e->kind == IR_ATTR && e->name && strcmp(e->name, key) == 0)
      return 1;
  }
  return 0;
}

void emit_design_system_layer(Sb *sb, int has_surface, int has_surface2,
                                    int has_border, int has_on_primary,
                                    int has_font) {
  sb_append(sb, "\n/* Cord design system defaults (DESIGN.md) */\n");
  sb_append(sb, ":root {\n");
  if (!has_surface)
    sb_append(sb, "  --color-surface: var(--color-bg, #fafaf9);\n");
  if (!has_surface2)
    sb_append(sb,
              "  --color-surface-2: color-mix(in srgb, var(--color-surface, "
              "#fff) 92%, var(--color-text, #111) 8%);\n");
  if (!has_border)
    sb_append(sb,
              "  --color-border: color-mix(in srgb, var(--color-text, #111) "
              "12%, transparent);\n");
  if (!has_on_primary)
    sb_append(sb, "  --color-on-primary: #ffffff;\n");
  sb_append(sb,
            "  --type-display-size: 2.75rem;\n"
            "  --type-display-leading: 1.15;\n"
            "  --type-display-weight: 700;\n"
            "  --type-title-size: 1.5rem;\n"
            "  --type-title-leading: 1.25;\n"
            "  --type-title-weight: 650;\n"
            "  --type-body-size: 1rem;\n"
            "  --type-body-leading: 1.55;\n"
            "  --type-body-weight: 400;\n"
            "  --type-caption-size: 0.875rem;\n"
            "  --type-caption-leading: 1.4;\n"
            "  --type-caption-weight: 400;\n"
            "  --type-code-size: 0.875rem;\n"
            "  --type-code-leading: 1.5;\n"
            "  --type-code-weight: 400;\n"
            "  --elevate-0: none;\n"
            "  --elevate-1: 0 1px 2px color-mix(in srgb, #000 6%, transparent);\n"
            "  --elevate-2: 0 4px 12px color-mix(in srgb, #000 8%, transparent);\n"
            "  --elevate-3: 0 12px 28px color-mix(in srgb, #000 12%, transparent);\n"
            "  --elevate-4: 0 24px 48px color-mix(in srgb, #000 16%, transparent);\n"
            "  --duration-fast: 150ms;\n"
            "  --duration-normal: 280ms;\n"
            "  --duration-slow: 480ms;\n"
            "  --ease-standard: cubic-bezier(0.22, 1, 0.36, 1);\n"
            "  --density-compact-gap: 0.5rem;\n"
            "  --density-comfortable-gap: 1rem;\n"
            "  --density-spacious-gap: 1.5rem;\n"
            "}\n");
  if (has_font)
    sb_append(sb, "body { font-family: var(--font-sans, system-ui, sans-serif); }\n");
  sb_append(sb,
            ".type-display { font-size: var(--type-display-size); "
            "line-height: var(--type-display-leading); "
            "font-weight: var(--type-display-weight); letter-spacing: -0.02em; }\n"
            ".type-title { font-size: var(--type-title-size); "
            "line-height: var(--type-title-leading); "
            "font-weight: var(--type-title-weight); letter-spacing: -0.01em; }\n"
            ".type-body { font-size: var(--type-body-size); "
            "line-height: var(--type-body-leading); "
            "font-weight: var(--type-body-weight); }\n"
            ".type-caption { font-size: var(--type-caption-size); "
            "line-height: var(--type-caption-leading); "
            "font-weight: var(--type-caption-weight); "
            "color: var(--color-muted, inherit); }\n"
            ".type-code { font-family: ui-monospace, monospace; "
            "font-size: var(--type-code-size); "
            "line-height: var(--type-code-leading); }\n"
            ".elevate-0 { box-shadow: var(--elevate-0); }\n"
            ".elevate-1 { box-shadow: var(--elevate-1); }\n"
            ".elevate-2 { box-shadow: var(--elevate-2); }\n"
            ".elevate-3 { box-shadow: var(--elevate-3); }\n"
            ".elevate-4 { box-shadow: var(--elevate-4); }\n"
            ".density-compact { gap: var(--density-compact-gap); }\n"
            ".density-comfortable { gap: var(--density-comfortable-gap); }\n"
            ".density-spacious { gap: var(--density-spacious-gap); }\n"
            ".cord-section { width: 100%; max-width: 72rem; margin-left: auto; "
            "margin-right: auto; padding-left: 1.5rem; padding-right: 1.5rem; }\n");
}

void emit_theme_block_open(Sb *sb, const char *name, int as_root) {
  if (as_root) {
    sb_append(sb, ":root {\n");
  } else {
    sb_appendf(sb, ".theme-%s {\n", name ? name : "default");
  }
}

void emit_header(Sb *sb) {
  sb_append(sb,
            "/* Generated by Cordlang from `theme` declarations.\n"
            " * Do not edit — regenerate with `cordlang run react|svelte`.\n"
            " *\n"
            " * Tailwind arbitrary values:\n"
            " *   text-[var(--color-primary)]  bg-[var(--color-bg)]\n"
            " *   rounded-[var(--radius)]\n"
            " * Cord attrs color=primary / color=$primary map to these vars.\n"
            " */\n\n");
}

