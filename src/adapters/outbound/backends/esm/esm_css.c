/*
 * base.css for the native preview.
 *
 * The React scaffold resolves Cord layout attrs through Tailwind. The preview
 * has no npm, so it generates the equivalent rules itself: walk the IR, run the
 * SAME cord_collect_classes() the emitter uses, and emit a rule for every class
 * that actually appears. Coverage is exact by construction — a class can only
 * show up in the document if the emitter produced it, and the emitter and this
 * collector call the same function.
 *
 * Unknown classes (from `class=` in source, resolved by the project's own
 * public/site.css) are deliberately left alone.
 *
 * Cord's numeric scale is px/16 → rem, matching the spacing map the React
 * scaffold writes into tailwind.config.js (16 → 1rem, 240 → 15rem).
 */
#include "adapters/outbound/backends/esm/esm_backend.h"
#include "adapters/outbound/backends/cord_class.h"
#include "adapters/outbound/backends/theme_css.h"
#include "domain/ir.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── buffer ─────────────────────────────────────────────── */

typedef struct {
  char *buf;
  size_t len, cap;
} Cb;

static void cb_init(Cb *c) {
  c->cap = 32768;
  c->len = 0;
  c->buf = calloc(c->cap, 1);
  if (!c->buf) {
    fprintf(stderr, "fatal: out of memory (esm css)\n");
    exit(1);
  }
}

static void cb_add(Cb *c, const char *s) {
  if (!s) return;
  size_t n = strlen(s);
  if (c->len + n + 1 >= c->cap) {
    while (c->len + n + 1 >= c->cap) c->cap *= 2;
    char *nb = realloc(c->buf, c->cap);
    if (!nb) {
      fprintf(stderr, "fatal: out of memory (esm css)\n");
      exit(1);
    }
    c->buf = nb;
  }
  memcpy(c->buf + c->len, s, n);
  c->len += n;
  c->buf[c->len] = '\0';
}

static void cb_addf(Cb *c, const char *fmt, ...) {
  char tmp[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(tmp, sizeof(tmp), fmt, ap);
  va_end(ap);
  cb_add(c, tmp);
}

/* ── class set ──────────────────────────────────────────── */

#define CLS_MAX 3072
#define CLS_LEN 128

typedef struct {
  char names[CLS_MAX][CLS_LEN];
  int count;
} ClassSet;

static int cls_has(ClassSet *s, const char *n) {
  for (int i = 0; i < s->count; i++)
    if (strcmp(s->names[i], n) == 0) return 1;
  return 0;
}

static void cls_add(ClassSet *s, const char *n) {
  if (!n || !*n) return;
  if (strlen(n) >= CLS_LEN) return;
  if (s->count >= CLS_MAX) return;
  if (cls_has(s, n)) return;
  snprintf(s->names[s->count], CLS_LEN, "%s", n);
  s->count++;
}

static void cls_add_split(ClassSet *s, const char *blob) {
  if (!blob) return;
  const char *p = blob;
  while (*p) {
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) break;
    const char *start = p;
    while (*p && *p != ' ' && *p != '\t') p++;
    size_t n = (size_t)(p - start);
    if (n && n < CLS_LEN) {
      char buf[CLS_LEN];
      memcpy(buf, start, n);
      buf[n] = '\0';
      cls_add(s, buf);
    }
  }
}

/* Same traversal the emitter performs, so the class sets cannot diverge. */
static void collect_from_ir(IrNode *n, ClassSet *set) {
  if (!n) return;
  if (n->kind == IR_ELEMENT && n->name) {
    const char *base = cord_tag_base_class(n->name);
    char classes[2048];
    cord_collect_classes(classes, sizeof(classes), n, base);
    cls_add_split(set, classes);
  }
  for (size_t i = 0; i < n->n_kids; i++) collect_from_ir(n->kids[i], set);
}

/* ── value helpers ──────────────────────────────────────── */

static void fmt_rem(char *out, size_t n, double px) {
  char tmp[64];
  snprintf(tmp, sizeof(tmp), "%.4f", px / 16.0);
  /* trim trailing zeros / dot */
  size_t l = strlen(tmp);
  while (l > 0 && tmp[l - 1] == '0') tmp[--l] = '\0';
  if (l > 0 && tmp[l - 1] == '.') tmp[--l] = '\0';
  snprintf(out, n, "%srem", tmp[0] ? tmp : "0");
}

static int all_digits(const char *s) {
  if (!s || !*s) return 0;
  for (const char *p = s; *p; p++)
    if (!isdigit((unsigned char)*p)) return 0;
  return 1;
}

/*
 * Size value for a utility suffix. Returns 0 when the suffix is not a size we
 * know how to express (caller then skips the rule).
 */
static int size_value(const char *v, char *out, size_t n) {
  if (!v || !*v) return 0;
  if (all_digits(v)) {
    fmt_rem(out, n, atof(v));
    return 1;
  }
  if (strcmp(v, "full") == 0) {
    snprintf(out, n, "100%%");
    return 1;
  }
  if (strcmp(v, "screen") == 0) {
    snprintf(out, n, "100vh");
    return 1;
  }
  if (strcmp(v, "auto") == 0) {
    snprintf(out, n, "auto");
    return 1;
  }
  if (strcmp(v, "px") == 0) {
    snprintf(out, n, "1px");
    return 1;
  }
  if (strcmp(v, "min") == 0) {
    snprintf(out, n, "min-content");
    return 1;
  }
  if (strcmp(v, "max") == 0) {
    snprintf(out, n, "max-content");
    return 1;
  }
  if (strcmp(v, "fit") == 0) {
    snprintf(out, n, "fit-content");
    return 1;
  }
  if (strcmp(v, "0") == 0) {
    snprintf(out, n, "0");
    return 1;
  }
  return 0;
}

/* CSS-escape a class name for use in a selector (bg-[var(--color-x)]). */
static void sel_escape(const char *cls, char *out, size_t n) {
  size_t o = 0;
  for (const char *p = cls; *p && o + 2 < n; p++) {
    if (*p == '[' || *p == ']' || *p == '(' || *p == ')' || *p == '.' ||
        *p == ':' || *p == '/' || *p == '%' || *p == '#' || *p == ',') {
      out[o++] = '\\';
    }
    out[o++] = *p;
  }
  out[o] = '\0';
}

static int starts(const char *s, const char *pfx, const char **rest) {
  size_t n = strlen(pfx);
  if (strncmp(s, pfx, n) != 0) return 0;
  *rest = s + n;
  return 1;
}

/*
 * Emit the declaration body for one utility class. Returns 0 when unknown, so
 * the caller can skip it (user classes from site.css land here).
 */
static int decls_for(const char *cls, char *out, size_t outn) {
  const char *v = NULL;
  char val[64];

  /* arbitrary theme-var values: bg-[var(--color-primary)] */
  if (starts(cls, "bg-[var(--", &v)) {
    char tok[64];
    snprintf(tok, sizeof(tok), "%s", v);
    char *close = strchr(tok, ')');
    if (close) *close = '\0';
    snprintf(out, outn, "background-color: var(--%s)", tok);
    return 1;
  }
  if (starts(cls, "text-[var(--", &v)) {
    char tok[64];
    snprintf(tok, sizeof(tok), "%s", v);
    char *close = strchr(tok, ')');
    if (close) *close = '\0';
    snprintf(out, outn, "color: var(--%s)", tok);
    return 1;
  }

  /* spacing */
  if (starts(cls, "gap-", &v) && size_value(v, val, sizeof(val))) {
    snprintf(out, outn, "gap: %s", val);
    return 1;
  }
  if (starts(cls, "px-", &v) && size_value(v, val, sizeof(val))) {
    snprintf(out, outn, "padding-left: %s; padding-right: %s", val, val);
    return 1;
  }
  if (starts(cls, "py-", &v) && size_value(v, val, sizeof(val))) {
    snprintf(out, outn, "padding-top: %s; padding-bottom: %s", val, val);
    return 1;
  }
  if (starts(cls, "pt-", &v) && size_value(v, val, sizeof(val))) {
    snprintf(out, outn, "padding-top: %s", val);
    return 1;
  }
  if (starts(cls, "pb-", &v) && size_value(v, val, sizeof(val))) {
    snprintf(out, outn, "padding-bottom: %s", val);
    return 1;
  }
  if (starts(cls, "p-", &v) && size_value(v, val, sizeof(val))) {
    snprintf(out, outn, "padding: %s", val);
    return 1;
  }
  if (starts(cls, "mx-", &v)) {
    if (strcmp(v, "auto") == 0) {
      snprintf(out, outn, "margin-left: auto; margin-right: auto");
      return 1;
    }
    if (size_value(v, val, sizeof(val))) {
      snprintf(out, outn, "margin-left: %s; margin-right: %s", val, val);
      return 1;
    }
    return 0;
  }
  if (starts(cls, "my-", &v) && size_value(v, val, sizeof(val))) {
    snprintf(out, outn, "margin-top: %s; margin-bottom: %s", val, val);
    return 1;
  }
  if (starts(cls, "mt-", &v) && size_value(v, val, sizeof(val))) {
    snprintf(out, outn, "margin-top: %s", val);
    return 1;
  }
  if (starts(cls, "mb-", &v) && size_value(v, val, sizeof(val))) {
    snprintf(out, outn, "margin-bottom: %s", val);
    return 1;
  }
  if (starts(cls, "m-", &v) && size_value(v, val, sizeof(val))) {
    snprintf(out, outn, "margin: %s", val);
    return 1;
  }

  /* sizing */
  if (starts(cls, "max-w-", &v) && size_value(v, val, sizeof(val))) {
    snprintf(out, outn, "max-width: %s", val);
    return 1;
  }
  if (starts(cls, "min-h-", &v)) {
    if (strcmp(v, "screen") == 0) {
      snprintf(out, outn, "min-height: 100vh");
      return 1;
    }
    if (size_value(v, val, sizeof(val))) {
      snprintf(out, outn, "min-height: %s", val);
      return 1;
    }
    return 0;
  }
  if (starts(cls, "w-", &v)) {
    if (strcmp(v, "screen") == 0) {
      snprintf(out, outn, "width: 100vw");
      return 1;
    }
    if (size_value(v, val, sizeof(val))) {
      snprintf(out, outn, "width: %s", val);
      return 1;
    }
    return 0;
  }
  if (starts(cls, "h-", &v)) {
    if (strcmp(v, "screen") == 0) {
      snprintf(out, outn, "height: 100vh");
      return 1;
    }
    if (size_value(v, val, sizeof(val))) {
      snprintf(out, outn, "height: %s", val);
      return 1;
    }
    return 0;
  }

  /* radius: rounded-12 (px) and the keyword scale */
  if (starts(cls, "rounded-", &v)) {
    if (all_digits(v)) {
      snprintf(out, outn, "border-radius: %spx", v);
      return 1;
    }
    if (strcmp(v, "full") == 0) {
      snprintf(out, outn, "border-radius: 9999px");
      return 1;
    }
    if (strcmp(v, "none") == 0) {
      snprintf(out, outn, "border-radius: 0");
      return 1;
    }
    if (strcmp(v, "sm") == 0) {
      snprintf(out, outn, "border-radius: 0.25rem");
      return 1;
    }
    if (strcmp(v, "md") == 0) {
      snprintf(out, outn, "border-radius: 0.5rem");
      return 1;
    }
    if (strcmp(v, "lg") == 0) {
      snprintf(out, outn, "border-radius: 0.75rem");
      return 1;
    }
    if (strcmp(v, "xl") == 0) {
      snprintf(out, outn, "border-radius: 1rem");
      return 1;
    }
    if (strcmp(v, "2xl") == 0) {
      snprintf(out, outn, "border-radius: 1.25rem");
      return 1;
    }
    return 0;
  }

  /* typography scale */
  if (starts(cls, "text-", &v)) {
    if (strcmp(v, "xs") == 0) {
      snprintf(out, outn, "font-size: 0.75rem; line-height: 1.4");
      return 1;
    }
    if (strcmp(v, "sm") == 0) {
      snprintf(out, outn, "font-size: 0.875rem; line-height: 1.5");
      return 1;
    }
    if (strcmp(v, "base") == 0) {
      snprintf(out, outn, "font-size: 1rem; line-height: 1.6");
      return 1;
    }
    if (strcmp(v, "lg") == 0) {
      snprintf(out, outn, "font-size: 1.125rem; line-height: 1.6");
      return 1;
    }
    if (strcmp(v, "xl") == 0) {
      snprintf(out, outn, "font-size: 1.25rem; line-height: 1.5");
      return 1;
    }
    if (strcmp(v, "2xl") == 0) {
      snprintf(out, outn, "font-size: 1.5rem; line-height: 1.35");
      return 1;
    }
    if (strcmp(v, "3xl") == 0) {
      snprintf(out, outn, "font-size: 1.875rem; line-height: 1.25");
      return 1;
    }
    if (strcmp(v, "4xl") == 0) {
      snprintf(out, outn, "font-size: 2.25rem; line-height: 1.15");
      return 1;
    }
    if (strcmp(v, "5xl") == 0) {
      snprintf(out, outn, "font-size: 3rem; line-height: 1.05");
      return 1;
    }
    if (strcmp(v, "left") == 0 || strcmp(v, "center") == 0 ||
        strcmp(v, "right") == 0) {
      snprintf(out, outn, "text-align: %s", v);
      return 1;
    }
    if (strcmp(v, "muted") == 0) {
      snprintf(out, outn, "color: var(--color-muted, #57534e)");
      return 1;
    }
    if (strcmp(v, "white") == 0) {
      snprintf(out, outn, "color: #fff");
      return 1;
    }
    if (strcmp(v, "black") == 0) {
      snprintf(out, outn, "color: #000");
      return 1;
    }
    /* theme token: text-primary → var(--color-primary) */
    if (theme_is_color_token(v)) {
      char tok[64];
      if (theme_token_name(v, tok, sizeof(tok))) {
        snprintf(out, outn, "color: var(--color-%s)", tok);
        return 1;
      }
    }
    return 0;
  }

  if (starts(cls, "bg-", &v)) {
    if (strcmp(v, "white") == 0) {
      snprintf(out, outn, "background-color: #fff");
      return 1;
    }
    if (strcmp(v, "black") == 0) {
      snprintf(out, outn, "background-color: #000");
      return 1;
    }
    if (strcmp(v, "transparent") == 0) {
      snprintf(out, outn, "background-color: transparent");
      return 1;
    }
    if (theme_is_color_token(v)) {
      char tok[64];
      if (theme_token_name(v, tok, sizeof(tok))) {
        snprintf(out, outn, "background-color: var(--color-%s)", tok);
        return 1;
      }
    }
    return 0;
  }

  if (starts(cls, "border-", &v)) {
    if (theme_is_color_token(v)) {
      char tok[64];
      if (theme_token_name(v, tok, sizeof(tok))) {
        snprintf(out, outn, "border-color: var(--color-%s)", tok);
        return 1;
      }
    }
    if (strcmp(v, "gray-200") == 0) {
      snprintf(out, outn, "border-color: #e5e7eb");
      return 1;
    }
    if (strcmp(v, "gray-300") == 0) {
      snprintf(out, outn, "border-color: #d1d5db");
      return 1;
    }
    return 0;
  }

  if (starts(cls, "grid-cols-", &v) && all_digits(v)) {
    snprintf(out, outn, "grid-template-columns: repeat(%s, minmax(0, 1fr))", v);
    return 1;
  }
  if (starts(cls, "z-", &v) && all_digits(v)) {
    snprintf(out, outn, "z-index: %s", v);
    return 1;
  }
  if (starts(cls, "opacity-", &v) && all_digits(v)) {
    snprintf(out, outn, "opacity: %.2f", atof(v) / 100.0);
    return 1;
  }
  if (starts(cls, "top-", &v) && size_value(v, val, sizeof(val))) {
    snprintf(out, outn, "top: %s", val);
    return 1;
  }
  if (starts(cls, "leading-", &v)) {
    if (all_digits(v)) {
      snprintf(out, outn, "line-height: %.3f", atof(v) / 100.0);
      return 1;
    }
    if (strcmp(v, "tight") == 0) {
      snprintf(out, outn, "line-height: 1.2");
      return 1;
    }
    if (strcmp(v, "snug") == 0) {
      snprintf(out, outn, "line-height: 1.35");
      return 1;
    }
    if (strcmp(v, "relaxed") == 0) {
      snprintf(out, outn, "line-height: 1.75");
      return 1;
    }
    return 0;
  }
  if (starts(cls, "tracking-", &v)) {
    if (strcmp(v, "tight") == 0) {
      snprintf(out, outn, "letter-spacing: -0.02em");
      return 1;
    }
    if (strcmp(v, "wide") == 0) {
      snprintf(out, outn, "letter-spacing: 0.04em");
      return 1;
    }
    if (strcmp(v, "wider") == 0) {
      snprintf(out, outn, "letter-spacing: 0.08em");
      return 1;
    }
    return 0;
  }
  if (starts(cls, "overflow-", &v)) {
    if (strcmp(v, "x-auto") == 0) {
      snprintf(out, outn, "overflow-x: auto");
      return 1;
    }
    if (strcmp(v, "y-auto") == 0) {
      snprintf(out, outn, "overflow-y: auto");
      return 1;
    }
    snprintf(out, outn, "overflow: %s", v);
    return 1;
  }
  if (starts(cls, "shadow-", &v)) {
    if (strcmp(v, "sm") == 0) {
      snprintf(out, outn, "box-shadow: 0 1px 2px rgba(0,0,0,0.06)");
      return 1;
    }
    if (strcmp(v, "md") == 0) {
      snprintf(out, outn, "box-shadow: 0 4px 10px rgba(0,0,0,0.08)");
      return 1;
    }
    if (strcmp(v, "lg") == 0) {
      snprintf(out, outn, "box-shadow: 0 10px 25px rgba(0,0,0,0.10)");
      return 1;
    }
    if (strcmp(v, "none") == 0) {
      snprintf(out, outn, "box-shadow: none");
      return 1;
    }
    return 0;
  }

  /* bare keywords */
  if (strcmp(cls, "flex") == 0) {
    snprintf(out, outn, "display: flex");
    return 1;
  }
  if (strcmp(cls, "grid") == 0) {
    snprintf(out, outn, "display: grid");
    return 1;
  }
  if (strcmp(cls, "flex-col") == 0) {
    snprintf(out, outn, "flex-direction: column");
    return 1;
  }
  if (strcmp(cls, "flex-row") == 0) {
    snprintf(out, outn, "flex-direction: row");
    return 1;
  }
  if (strcmp(cls, "flex-wrap") == 0) {
    snprintf(out, outn, "flex-wrap: wrap");
    return 1;
  }
  if (strcmp(cls, "flex-1") == 0) {
    snprintf(out, outn, "flex: 1 1 0%%");
    return 1;
  }
  if (strcmp(cls, "items-center") == 0) {
    snprintf(out, outn, "align-items: center");
    return 1;
  }
  if (strcmp(cls, "items-start") == 0) {
    snprintf(out, outn, "align-items: flex-start");
    return 1;
  }
  if (strcmp(cls, "items-end") == 0) {
    snprintf(out, outn, "align-items: flex-end");
    return 1;
  }
  if (strcmp(cls, "justify-center") == 0) {
    snprintf(out, outn, "justify-content: center");
    return 1;
  }
  if (strcmp(cls, "justify-between") == 0) {
    snprintf(out, outn, "justify-content: space-between");
    return 1;
  }
  if (strcmp(cls, "justify-around") == 0) {
    snprintf(out, outn, "justify-content: space-around");
    return 1;
  }
  if (strcmp(cls, "justify-evenly") == 0) {
    snprintf(out, outn, "justify-content: space-evenly");
    return 1;
  }
  if (strcmp(cls, "font-bold") == 0) {
    snprintf(out, outn, "font-weight: 700");
    return 1;
  }
  if (strcmp(cls, "font-medium") == 0) {
    snprintf(out, outn, "font-weight: 500");
    return 1;
  }
  if (strcmp(cls, "sticky") == 0) {
    snprintf(out, outn, "position: sticky");
    return 1;
  }
  if (strcmp(cls, "relative") == 0) {
    snprintf(out, outn, "position: relative");
    return 1;
  }
  if (strcmp(cls, "absolute") == 0) {
    snprintf(out, outn, "position: absolute");
    return 1;
  }
  if (strcmp(cls, "fixed") == 0) {
    snprintf(out, outn, "position: fixed");
    return 1;
  }
  if (strcmp(cls, "border") == 0) {
    snprintf(out, outn, "border-width: 1px; border-style: solid");
    return 1;
  }
  if (strcmp(cls, "rounded") == 0) {
    snprintf(out, outn, "border-radius: var(--radius, 0.5rem)");
    return 1;
  }
  if (strcmp(cls, "type-display") == 0) {
    snprintf(out, outn,
             "font-family: var(--font-display, var(--font-sans, system-ui)); "
             "font-size: clamp(2rem, 5vw, 3.25rem); line-height: 1.05; "
             "letter-spacing: -0.03em");
    return 1;
  }
  if (strcmp(cls, "type-title") == 0) {
    snprintf(out, outn,
             "font-family: var(--font-display, var(--font-sans, system-ui)); "
             "font-size: 1.5rem; line-height: 1.25; letter-spacing: -0.02em");
    return 1;
  }
  if (strcmp(cls, "type-body") == 0) {
    snprintf(out, outn, "font-size: 1rem; line-height: 1.65");
    return 1;
  }
  if (strcmp(cls, "type-caption") == 0) {
    snprintf(out, outn,
             "font-size: 0.75rem; letter-spacing: 0.1em; text-transform: uppercase");
    return 1;
  }
  if (strcmp(cls, "type-code") == 0) {
    snprintf(out, outn,
             "font-family: var(--font-mono, ui-monospace, monospace); "
             "font-size: 0.9rem");
    return 1;
  }
  return 0;
}

/* ── static base layer ──────────────────────────────────── */

static const char *BASE_CSS =
    "/* Cordlang preview base — generated, no Tailwind */\n"
    "*, *::before, *::after { box-sizing: border-box; }\n"
    "html { -webkit-text-size-adjust: 100%; }\n"
    "body {\n"
    "  margin: 0;\n"
    "  min-height: 100vh;\n"
    "  -webkit-font-smoothing: antialiased;\n"
    "  background-color: var(--color-bg, #fafaf9);\n"
    "  color: var(--color-text, #1c1917);\n"
    "  line-height: 1.6;\n"
    "  font-family: var(--font-sans, system-ui, sans-serif);\n"
    "  --ui-container: 80rem;\n"
    "  --ui-header-height: 4rem;\n"
    "}\n"
    "img, svg, video { max-width: 100%; height: auto; }\n"
    "h1, h2, h3, h4 { margin: 0 0 0.5rem; font-weight: 600; }\n"
    "h1 { font-size: 2rem; line-height: 1.2; letter-spacing: -0.02em; }\n"
    "h2 { font-size: 1.5rem; line-height: 1.3; }\n"
    "h3 { font-size: 1.2rem; }\n"
    "p { margin: 0 0 0.75rem; }\n"
    "ul, ol { margin: 0 0 0.75rem; padding-left: 1.25rem; }\n"
    "pre { margin: 0; overflow-x: auto; }\n"
    "code, pre, .font-mono, .font-mono * {\n"
    "  font-family: var(--font-mono, ui-monospace, monospace);\n"
    "}\n"
    ".font-display {\n"
    "  font-family: var(--font-display, system-ui, sans-serif);\n"
    "}\n"
    ".text-muted { color: var(--color-muted, #57534e); }\n"
    "\n"
    "a { color: var(--color-primary, #0f766e); text-decoration: none; }\n"
    "a:hover { color: var(--color-accent, #0d9488); text-decoration: underline; }\n"
    "header a, aside a, footer a, nav a { color: var(--color-text, #1c1917); }\n"
    "header a:hover, aside a:hover, footer a:hover, nav a:hover {\n"
    "  color: var(--color-primary, #0f766e);\n"
    "}\n"
    "aside a {\n"
    "  display: block;\n"
    "  padding: 0.35rem 0.5rem;\n"
    "  margin: 0 -0.5rem;\n"
    "  border-radius: 6px;\n"
    "  font-size: 0.925rem;\n"
    "  line-height: 1.35;\n"
    "}\n"
    "aside a:hover {\n"
    "  background-color: color-mix(in srgb, var(--color-primary, #0f766e) 10%, transparent);\n"
    "  text-decoration: none;\n"
    "}\n"
    "a[aria-current='page'] {\n"
    "  color: var(--color-primary, #0f766e);\n"
    "  font-weight: 600;\n"
    "  background-color: color-mix(in srgb, var(--color-accent, #3DFFB5) 18%, transparent);\n"
    "  text-decoration: none;\n"
    "}\n"
    "\n"
    "button { font: inherit; }\n"
    ".btn {\n"
    "  display: inline-flex;\n"
    "  align-items: center;\n"
    "  justify-content: center;\n"
    "  gap: 0.5rem;\n"
    "  padding: 0.55rem 1rem;\n"
    "  font-weight: 500;\n"
    "  cursor: pointer;\n"
    "  border: 1px solid transparent;\n"
    "  background: transparent;\n"
    "  color: inherit;\n"
    "  border-radius: var(--radius, 0.5rem);\n"
    "  transition: filter 120ms ease, background-color 120ms ease;\n"
    "}\n"
    ".btn-primary {\n"
    "  background-color: var(--color-primary, #2563eb);\n"
    "  color: var(--color-on-primary, #fff);\n"
    "}\n"
    ".btn-primary:hover { filter: brightness(0.92); }\n"
    ".btn-outline {\n"
    "  border-color: var(--color-border, #d6d3d1);\n"
    "  background-color: var(--color-surface, #fff);\n"
    "}\n"
    ".btn-ghost:hover {\n"
    "  background-color: color-mix(in srgb, var(--color-text, #1c1917) 8%, transparent);\n"
    "}\n"
    ".btn-secondary {\n"
    "  background-color: var(--color-surface-2, #292524);\n"
    "  color: var(--color-text, #fff);\n"
    "}\n"
    ".btn:disabled { opacity: 0.55; cursor: not-allowed; }\n"
    ".card {\n"
    "  background-color: var(--color-surface, #fff);\n"
    "  border: 1px solid var(--color-border, #e7e5e4);\n"
    "  border-radius: var(--radius, 0.75rem);\n"
    "  overflow: hidden;\n"
    "}\n"
    ".cord-section { padding-block: 3rem; }\n"
    "\n"
    "input, textarea, select {\n"
    "  font: inherit;\n"
    "  color: inherit;\n"
    "  padding: 0.5rem 0.65rem;\n"
    "  border: 1px solid var(--color-border, #d6d3d1);\n"
    "  border-radius: var(--radius, 0.5rem);\n"
    "  background-color: var(--color-surface, #fff);\n"
    "}\n"
    "input[type='checkbox'], input[type='radio'] { padding: 0; }\n"
    "\n"
    "header.sticky {\n"
    "  top: 0;\n"
    "  z-index: 40;\n"
    "  min-height: var(--ui-header-height);\n"
    "  backdrop-filter: blur(8px);\n"
    "  background-color: color-mix(in srgb, var(--color-surface, #fff) 92%, transparent);\n"
    "  border-bottom: 1px solid var(--color-border, #e7e5e4);\n"
    "}\n"
    "aside.sticky {\n"
    "  top: var(--ui-header-height);\n"
    "  align-self: flex-start;\n"
    "  max-height: calc(100vh - var(--ui-header-height));\n"
    "  overflow-y: auto;\n"
    "  border-right: 1px solid var(--color-border, #e7e5e4);\n"
    "}\n"
    "main { min-width: 0; }\n"
    "footer { border-top: 1px solid var(--color-border, #e7e5e4); }\n"
    ".bg-codebg, .bg-stone-900 {\n"
    "  color: var(--color-codefg, #e7e5e4);\n"
    "  background-color: var(--color-codebg, #1c1917);\n"
    "  box-shadow: inset 0 1px 0 rgba(255,255,255,0.06);\n"
    "}\n"
    ".max-w-1280 { width: 100%; margin-left: auto; margin-right: auto; }\n"
    "\n"
    "@media (max-width: 768px) {\n"
    "  body .flex.flex-row:has(> aside) { flex-direction: column; }\n"
    "  body aside.sticky {\n"
    "    width: 100%;\n"
    "    position: relative;\n"
    "    top: auto;\n"
    "    max-height: none;\n"
    "    border-right: none;\n"
    "    border-bottom: 1px solid var(--color-border, #e7e5e4);\n"
    "  }\n"
    "}\n"
    "\n"
    "/* runtime surfaces */\n"
    ".cord-runtime-error {\n"
    "  margin: 1rem 0;\n"
    "  padding: 0.85rem 1rem;\n"
    "  border-radius: 8px;\n"
    "  border: 1px solid #f0a3a3;\n"
    "  background: #fff5f5;\n"
    "  color: #7f1d1d;\n"
    "  font-family: var(--font-mono, ui-monospace, monospace);\n"
    "  font-size: 0.85rem;\n"
    "}\n"
    ".cord-notfound { padding: 3rem 1.25rem; text-align: center; }\n"
    "#cord-overlay {\n"
    "  position: fixed;\n"
    "  inset: auto 1rem 1rem 1rem;\n"
    "  z-index: 99999;\n"
    "  max-height: 60vh;\n"
    "  overflow: auto;\n"
    "  padding: 1rem 1.1rem;\n"
    "  border-radius: 10px;\n"
    "  border: 1px solid #f0a3a3;\n"
    "  background: #1c1917;\n"
    "  color: #fecaca;\n"
    "  font-family: var(--font-mono, ui-monospace, monospace);\n"
    "  font-size: 0.8rem;\n"
    "  box-shadow: 0 12px 30px rgba(0,0,0,0.35);\n"
    "}\n"
    "#cord-overlay pre { white-space: pre-wrap; margin: 0.6rem 0; }\n"
    "#cord-overlay button {\n"
    "  border: none;\n"
    "  border-radius: 999px;\n"
    "  padding: 0.35rem 0.9rem;\n"
    "  cursor: pointer;\n"
    "  background: #fecaca;\n"
    "  color: #1c1917;\n"
    "  font-weight: 600;\n"
    "}\n"
    ".cord-icon { display: inline-flex; align-items: center; line-height: 1; }\n"
    ".cord-icon-svg { display: block; }\n"
    ".cord-motion-fade { animation: cord-fade-in 0.35s ease-out; }\n"
    "@keyframes cord-fade-in {\n"
    "  from { opacity: 0; transform: translateY(4px); }\n"
    "  to { opacity: 1; transform: none; }\n"
    "}\n";

static const char *BASE_CSS_CHART =
    ".cord-chart {\n"
    "  min-height: 8rem;\n"
    "  padding: 0.75rem;\n"
    "  border: 1px dashed var(--color-border, #d6d3d1);\n"
    "  border-radius: 8px;\n"
    "  color: var(--color-muted, #78716c);\n"
    "  font-size: 0.85rem;\n"
    "}\n"
    ".cord-chart-axes { display: flex; gap: 0.5rem; height: 100%; min-height: 6rem; }\n"
    ".cord-chart-y {\n"
    "  width: 2px;\n"
    "  background: var(--color-border, #d6d3d1);\n"
    "}\n"
    ".cord-chart-plot {\n"
    "  flex: 1;\n"
    "  display: flex;\n"
    "  align-items: flex-end;\n"
    "  border-bottom: 2px solid var(--color-border, #d6d3d1);\n"
    "}\n";

static const char *BASE_CSS_PORTAL =
    ".cord-portal { position: relative; z-index: 1000; }\n";

/* ── entry point ────────────────────────────────────────── */

static int class_set_has_prefix(ClassSet *set, const char *prefix) {
  if (!set || !prefix) return 0;
  size_t n = strlen(prefix);
  for (int i = 0; i < set->count; i++) {
    if (strncmp(set->names[i], prefix, n) == 0) return 1;
  }
  return 0;
}

char *esm_base_css(IrProgram *ir) {
  Cb out;
  cb_init(&out);
  cb_add(&out, BASE_CSS);

  ClassSet *set = NULL;
  if (ir && ir->root) {
    set = calloc(1, sizeof(ClassSet));
    if (set) collect_from_ir(ir->root, set);
  }

  if (set && class_set_has_prefix(set, "cord-chart"))
    cb_add(&out, BASE_CSS_CHART);
  if (set && class_set_has_prefix(set, "cord-portal"))
    cb_add(&out, BASE_CSS_PORTAL);

  if (!set) return out.buf;

  cb_add(&out, "\n/* utilities for the classes this project emits */\n");

  /* Responsive variants are grouped so the media query is emitted once. */
  const char *bps[] = {"sm", "md", "lg"};
  const char *bp_min[] = {"640px", "768px", "1024px"};

  for (int i = 0; i < set->count; i++) {
    const char *cls = set->names[i];
    if (strncmp(cls, "sm:", 3) == 0 || strncmp(cls, "md:", 3) == 0 ||
        strncmp(cls, "lg:", 3) == 0)
      continue;
    char decls[512];
    if (!decls_for(cls, decls, sizeof(decls))) continue;
    char sel[256];
    sel_escape(cls, sel, sizeof(sel));
    cb_addf(&out, ".%s { %s; }\n", sel, decls);
  }

  for (int b = 0; b < 3; b++) {
    int wrote_open = 0;
    char prefix[8];
    snprintf(prefix, sizeof(prefix), "%s:", bps[b]);
    for (int i = 0; i < set->count; i++) {
      const char *cls = set->names[i];
      if (strncmp(cls, prefix, strlen(prefix)) != 0) continue;
      char decls[512];
      if (!decls_for(cls + strlen(prefix), decls, sizeof(decls))) continue;
      if (!wrote_open) {
        cb_addf(&out, "\n@media (min-width: %s) {\n", bp_min[b]);
        wrote_open = 1;
      }
      char sel[256];
      sel_escape(cls, sel, sizeof(sel));
      cb_addf(&out, "  .%s { %s; }\n", sel, decls);
    }
    if (wrote_open) cb_add(&out, "}\n");
  }

  free(set);
  return out.buf;
}
