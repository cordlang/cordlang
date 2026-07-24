#include "adapters/outbound/backends/theme_css.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── string buffer (local) ──────────────────────────────── */

typedef struct {
  char *buf;
  size_t len;
  size_t cap;
} Sb;

static void sb_init(Sb *sb) {
  sb->cap = 4096;
  sb->len = 0;
  sb->buf = calloc(sb->cap, 1);
}

static void sb_append(Sb *sb, const char *s) {
  if (!s || !sb->buf) return;
  size_t n = strlen(s);
  if (sb->len + n + 1 >= sb->cap) {
    while (sb->len + n + 1 >= sb->cap) sb->cap *= 2;
    sb->buf = realloc(sb->buf, sb->cap);
    if (!sb->buf) return;
  }
  memcpy(sb->buf + sb->len, s, n);
  sb->len += n;
  sb->buf[sb->len] = '\0';
}

static void sb_appendf(Sb *sb, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int n = vsnprintf(NULL, 0, fmt, args);
  va_end(args);
  if (n < 0 || !sb->buf) return;
  if (sb->len + (size_t)n + 1 >= sb->cap) {
    while (sb->len + (size_t)n + 1 >= sb->cap) sb->cap *= 2;
    sb->buf = realloc(sb->buf, sb->cap);
    if (!sb->buf) return;
  }
  va_start(args, fmt);
  vsnprintf(sb->buf + sb->len, sb->cap - sb->len, fmt, args);
  va_end(args);
  sb->len += (size_t)n;
  sb->buf[sb->len] = '\0';
}

static int looks_like_number(const char *s) {
  if (!s || !*s) return 0;
  const char *p = s;
  if (*p == '-' || *p == '+') p++;
  int digits = 0;
  while (*p) {
    if (isdigit((unsigned char)*p))
      digits++;
    else if (*p != '.')
      return 0;
    p++;
  }
  return digits > 0;
}

static int looks_like_color(const char *s) {
  if (!s || !*s) return 0;
  if (s[0] == '#') return 1;
  if (strncmp(s, "rgb", 3) == 0 || strncmp(s, "hsl", 3) == 0) return 1;
  /* bare hex without # (6 or 3 hex digits) */
  size_t n = strlen(s);
  if (n == 3 || n == 6 || n == 8) {
    int hex = 1;
    for (size_t i = 0; i < n; i++) {
      if (!isxdigit((unsigned char)s[i])) {
        hex = 0;
        break;
      }
    }
    if (hex) return 1;
  }
  return 0;
}

/* Keys that are never color tokens even if value is ambiguous */
static int is_non_color_key(const char *key) {
  static const char *keys[] = {
      "radius", "gap", "spacing", "space", "size", "font", "weight",
      "opacity", "z", "duration", "line-height", "letter-spacing",
      "border-width", "width", "height", "shadow", NULL};
  for (int i = 0; keys[i]; i++)
    if (strcmp(key, keys[i]) == 0) return 1;
  return 0;
}

/* Common color token keys used in Cord themes */
static int is_color_key_name(const char *key) {
  static const char *keys[] = {
      "primary", "secondary", "accent", "muted", "bg", "text", "surface",
      "danger",  "success",   "warning", "brand", "border", "foreground",
      "background", "error", "info", "link", "card", "ring", NULL};
  for (int i = 0; keys[i]; i++)
    if (strcmp(key, keys[i]) == 0) return 1;
  return 0;
}

static int entry_is_color(const char *key, const char *val) {
  if (is_non_color_key(key)) return 0;
  if (looks_like_color(val)) return 1;
  if (is_color_key_name(key)) return 1;
  return 0;
}

/* Tailwind / CSS named colors + palette scales — not theme tokens */
static int is_tailwind_color_class(const char *val) {
  if (!val || !*val) return 0;
  /* palette like blue-600, gray-500, red-100 */
  const char *dash = strchr(val, '-');
  if (dash && dash > val) {
    int all_digits = 1;
    for (const char *p = dash + 1; *p; p++) {
      if (!isdigit((unsigned char)*p)) {
        all_digits = 0;
        break;
      }
    }
    if (all_digits && dash[1]) return 1;
  }
  static const char *named[] = {
      "white", "black", "transparent", "current", "inherit", "red",
      "blue",  "green", "gray",        "slate",   "zinc",    "neutral",
      "stone", "orange", "amber",      "yellow",  "lime",    "emerald",
      "teal",  "cyan",   "sky",        "indigo",  "violet",  "purple",
      "fuchsia", "pink", "rose", NULL};
  for (int i = 0; named[i]; i++)
    if (strcmp(val, named[i]) == 0) return 1;
  return 0;
}

const char *theme_token_name(const char *val, char *buf, size_t bufsz) {
  if (!val || !*val || !buf || bufsz == 0) return NULL;
  const char *p = val;
  if (*p == '$') p++;
  if (!*p) return NULL;
  size_t n = 0;
  while (p[n] && n + 1 < bufsz) {
    buf[n] = p[n];
    n++;
  }
  buf[n] = '\0';
  return buf;
}

int theme_is_color_token(const char *val) {
  if (!val || !*val) return 0;
  /* explicit $token */
  if (val[0] == '$' && val[1]) return 1;
  /* hex / rgb / hsl are raw colors, not tokens */
  if (looks_like_color(val)) return 0;
  if (is_tailwind_color_class(val)) return 0;
  /* simple identifier: primary, muted, brand… */
  if (!(isalpha((unsigned char)val[0]) || val[0] == '_')) return 0;
  for (const char *p = val; *p; p++) {
    if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '-')) return 0;
  }
  /* multi-segment like blue-600 already handled; remaining dashed names
     that aren't palette scales still count as theme tokens */
  return 1;
}

/* ── shared CSS var / utility emit (key, val pairs) ─────── */

/* Custom-property name: leading letter/underscore, then alnum/_/- */
static int css_prop_key_ok(const char *key) {
  if (!key || !*key) return 0;
  if (!(isalpha((unsigned char)key[0]) || key[0] == '_')) return 0;
  for (const char *p = key + 1; *p; p++) {
    if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '-')) return 0;
  }
  return 1;
}

/* Reject CSS breakout: no ; { } or controls in values we interpolate raw. */
static int css_prop_val_ok(const char *val) {
  if (!val || !*val) return 0;
  for (const char *p = val; *p; p++) {
    unsigned char c = (unsigned char)*p;
    if (c < 0x20) return 0;
    if (c == ';' || c == '{' || c == '}' || c == '"' || c == '\'') return 0;
  }
  return 1;
}

static void emit_css_var(Sb *sb, const char *key, const char *val) {
  if (!key || !val) return;
  if (!css_prop_key_ok(key) || !css_prop_val_ok(val)) {
    sb_appendf(sb, "  /* skipped unsafe theme entry: %s */\n",
               css_prop_key_ok(key) ? key : "?");
    return;
  }
  if (entry_is_color(key, val)) {
    /* ensure # prefix for bare hex */
    if (looks_like_color(val) && val[0] != '#' &&
        strncmp(val, "rgb", 3) != 0 && strncmp(val, "hsl", 3) != 0) {
      sb_appendf(sb, "  --color-%s: #%s;\n", key, val);
    } else {
      sb_appendf(sb, "  --color-%s: %s;\n", key, val);
    }
  } else if (looks_like_number(val)) {
    sb_appendf(sb, "  --%s: %spx;\n", key, val);
  } else {
    sb_appendf(sb, "  --%s: %s;\n", key, val);
  }
}

static void emit_theme_block_open(Sb *sb, const char *name, int as_root) {
  if (as_root) {
    sb_append(sb, ":root {\n");
  } else {
    sb_appendf(sb, ".theme-%s {\n", name ? name : "default");
  }
}

static void emit_header(Sb *sb) {
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

char *theme_css_generate(Node *root) {
  Sb sb;
  sb_init(&sb);
  if (!sb.buf) return strdup("/* theme.css: allocation failed */\n");

  emit_header(&sb);

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
    if (idx == 0) emit_utilities(&sb, c);
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

char *theme_css_generate_from_ir(const IrProgram *ir) {
  Sb sb;
  sb_init(&sb);
  if (!sb.buf) return strdup("/* theme.css: allocation failed */\n");

  emit_header(&sb);

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
    if (idx == 0) emit_utilities_ir(&sb, c);
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
