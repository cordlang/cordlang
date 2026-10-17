#include "adapters/outbound/backends/svelte/svelte_backend_internal.h"

void svelte_collect_classes_ir(char *classes, size_t sz, const IrNode *node,
                               const char *base) {
  classes[0] = '\0';
  if (base) strncat(classes, base, sz - 1);
  if (!node) return;

  int has_between = 0;
  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *c = node->kids[i];
    if (!c || c->kind != IR_ATTR || !c->name) continue;
    if (strcmp(c->name, "between") == 0) has_between = 1;
  }

  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *c = node->kids[i];
    if (!c || c->kind != IR_ATTR || !c->name) continue;
    if (c->name[0] == '_' && c->name[1] == '_') continue;
    char vbuf[96];
    const char *k = c->name;
    const char *v = c->value ? c->value : "";

    /* Bool-like attrs (name is the flag / size token) */
    if (svelte_attr_is_true(c) || (!c->value || !c->value[0])) {
      if (strcmp(k, "between") == 0)
        strncat(classes,
                strstr(classes, "flex") ? " justify-between"
                                        : " flex justify-between",
                sz - strlen(classes) - 1);
      else if (strcmp(k, "center") == 0)
        strncat(classes,
                has_between ? " flex items-center"
                            : " flex items-center justify-center",
                sz - strlen(classes) - 1);
      else if (strcmp(k, "bold") == 0)
        strncat(classes, " font-bold", sz - strlen(classes) - 1);
      else if (strcmp(k, "muted") == 0)
        strncat(classes, " text-muted", sz - strlen(classes) - 1);
      else if (strcmp(k, "font-mono") == 0)
        strncat(classes, " font-mono", sz - strlen(classes) - 1);
      else if (strcmp(k, "flex-1") == 0)
        strncat(classes, " flex-1", sz - strlen(classes) - 1);
      else if (strcmp(k, "border") == 0)
        strncat(classes, " border border-gray-200", sz - strlen(classes) - 1);
      else if (strcmp(k, "sticky") == 0)
        strncat(classes, " sticky top-0", sz - strlen(classes) - 1);
      else if (strcmp(k, "primary") == 0)
        strncat(classes, " btn-primary", sz - strlen(classes) - 1);
      else if (strcmp(k, "outline") == 0)
        strncat(classes, " btn-outline", sz - strlen(classes) - 1);
      else if (strcmp(k, "ghost") == 0)
        strncat(classes, " btn-ghost", sz - strlen(classes) - 1);
      else if (strcmp(k, "wrap") == 0)
        strncat(classes, " flex-wrap", sz - strlen(classes) - 1);
      else if (strcmp(k, "xl") == 0)
        strncat(classes, " text-xl", sz - strlen(classes) - 1);
      else if (strcmp(k, "2xl") == 0)
        strncat(classes, " text-2xl", sz - strlen(classes) - 1);
      else if (strcmp(k, "3xl") == 0)
        strncat(classes, " text-3xl", sz - strlen(classes) - 1);
      else if (strcmp(k, "4xl") == 0)
        strncat(classes, " text-4xl", sz - strlen(classes) - 1);
      else if (strcmp(k, "lg") == 0)
        strncat(classes, " text-lg", sz - strlen(classes) - 1);
      else if (strcmp(k, "sm") == 0)
        strncat(classes, " text-sm", sz - strlen(classes) - 1);
      else if (strcmp(k, "xs") == 0)
        strncat(classes, " text-xs", sz - strlen(classes) - 1);
      else if (strchr(k, '-') != NULL || strcmp(k, "grow") == 0 ||
               strcmp(k, "shrink") == 0) {
        /* Bare Tailwind-like flags: h-screen, overflow-y-auto, w-full… */
        snprintf(vbuf, sizeof(vbuf), " %s", k);
        strncat(classes, vbuf, sz - strlen(classes) - 1);
      }
      /* valued style keys with empty/"true" already handled as flags above */
      if (strcmp(k, "variant") && strcmp(k, "size") && strcmp(k, "color") &&
          strcmp(k, "gap") && strcmp(k, "cols") && strcmp(k, "p") &&
          strcmp(k, "bg") && strcmp(k, "shadow") && strcmp(k, "rounded") &&
          strcmp(k, "max-w") && strcmp(k, "w") && strcmp(k, "h") &&
          strcmp(k, "min-h") && strcmp(k, "mx") && strcmp(k, "my") &&
          strcmp(k, "px") && strcmp(k, "py") && strcmp(k, "m") &&
          strcmp(k, "border") && strcmp(k, "flex") && strcmp(k, "opacity") &&
          strcmp(k, "overflow"))
        continue;
    }

    if (!c->value) continue;
    if (strcmp(k, "variant") == 0) {
      snprintf(vbuf, sizeof(vbuf), " btn-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "size") == 0) {
      snprintf(vbuf, sizeof(vbuf), " text-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "color") == 0) {
      if (theme_is_color_token(v)) {
        char tok[64];
        if (theme_token_name(v, tok, sizeof(tok))) {
          snprintf(vbuf, sizeof(vbuf), " text-[var(--color-%s)]", tok);
          strncat(classes, vbuf, sz - strlen(classes) - 1);
        }
      } else {
        snprintf(vbuf, sizeof(vbuf), " text-%s", v);
        strncat(classes, vbuf, sz - strlen(classes) - 1);
      }
    } else if (strcmp(k, "gap") == 0) {
      snprintf(vbuf, sizeof(vbuf), " gap-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "cols") == 0) {
      snprintf(vbuf, sizeof(vbuf), " grid-cols-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "p") == 0) {
      snprintf(vbuf, sizeof(vbuf), " p-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "px") == 0) {
      snprintf(vbuf, sizeof(vbuf), " px-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "py") == 0) {
      snprintf(vbuf, sizeof(vbuf), " py-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "m") == 0) {
      snprintf(vbuf, sizeof(vbuf), " m-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "mx") == 0) {
      snprintf(vbuf, sizeof(vbuf), " mx-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "my") == 0) {
      snprintf(vbuf, sizeof(vbuf), " my-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "w") == 0) {
      snprintf(vbuf, sizeof(vbuf), " w-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "h") == 0) {
      snprintf(vbuf, sizeof(vbuf), " h-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "min-h") == 0) {
      snprintf(vbuf, sizeof(vbuf), " min-h-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "flex") == 0) {
      snprintf(vbuf, sizeof(vbuf), " flex-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "opacity") == 0) {
      snprintf(vbuf, sizeof(vbuf), " opacity-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "overflow") == 0) {
      snprintf(vbuf, sizeof(vbuf), " overflow-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "border") == 0 && !svelte_attr_is_true(c)) {
      snprintf(vbuf, sizeof(vbuf), " border border-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "bg") == 0) {
      if (theme_is_color_token(v)) {
        char tok[64];
        if (theme_token_name(v, tok, sizeof(tok))) {
          snprintf(vbuf, sizeof(vbuf), " bg-[var(--color-%s)]", tok);
          strncat(classes, vbuf, sz - strlen(classes) - 1);
        }
      } else {
        snprintf(vbuf, sizeof(vbuf), " bg-%s", v);
        strncat(classes, vbuf, sz - strlen(classes) - 1);
      }
    } else if (strcmp(k, "shadow") == 0) {
      snprintf(vbuf, sizeof(vbuf), " shadow-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "rounded") == 0) {
      snprintf(vbuf, sizeof(vbuf), " rounded-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "max-w") == 0) {
      snprintf(vbuf, sizeof(vbuf), " max-w-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    }
  }
}
