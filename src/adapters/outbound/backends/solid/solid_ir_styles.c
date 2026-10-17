#include "adapters/outbound/backends/solid/solid_ir_internal.h"

void solid_ir_solid_emit_expr(StrBuf *sb, const char *expr, GenCtx *ctx) {
  if (!expr) {
    solid_ir_sb_append(sb, "undefined");
    return;
  }
  if (!ctx || ctx->n_signals == 0) {
    solid_ir_sb_append(sb, expr);
    return;
  }
  const char *p = expr;
  while (*p) {
    if (isalpha((unsigned char)*p) || *p == '_' || *p == '$') {
      const char *start = p;
      while (isalnum((unsigned char)*p) || *p == '_' || *p == '$') p++;
      size_t n = (size_t)(p - start);
      char id[128];
      if (n >= sizeof(id)) n = sizeof(id) - 1;
      memcpy(id, start, n);
      id[n] = '\0';
      int is_sig = 0;
      for (int i = 0; i < ctx->n_signals; i++) {
        if (strcmp(ctx->signals[i], id) == 0) {
          is_sig = 1;
          break;
        }
      }
      if (is_sig && *p != '(')
        solid_ir_sb_appendf(sb, "%s()", id);
      else
        solid_ir_sb_append(sb, id);
    } else {
      char ch[2] = {*p, 0};
      solid_ir_sb_append(sb, ch);
      p++;
    }
  }
}

void solid_ir_append_style_entry_class(char *classes, size_t classes_sz,
                                     const char *key, const char *val) {
  char vbuf[96];
  if (!key) return;
  if (!val) val = "";
  if (strcmp(key, "between") == 0)
    strncat(classes, " justify-between", classes_sz - strlen(classes) - 1);
  else if (strcmp(key, "center") == 0)
    strncat(classes, " items-center justify-center",
            classes_sz - strlen(classes) - 1);
  else if (strcmp(key, "around") == 0)
    strncat(classes, " justify-around", classes_sz - strlen(classes) - 1);
  else if (strcmp(key, "evenly") == 0)
    strncat(classes, " justify-evenly", classes_sz - strlen(classes) - 1);
  else if (strcmp(key, "sticky") == 0)
    strncat(classes, " sticky top-0", classes_sz - strlen(classes) - 1);
  else if (strcmp(key, "bold") == 0)
    strncat(classes, " font-bold", classes_sz - strlen(classes) - 1);
  else if (strcmp(key, "muted") == 0)
    strncat(classes, " text-muted", classes_sz - strlen(classes) - 1);
  else if (strcmp(key, "overflow") == 0) {
    snprintf(vbuf, sizeof(vbuf), " overflow-%s", val);
    strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
  } else if (strcmp(key, "bg") == 0 || strcmp(key, "text") == 0 ||
             strcmp(key, "p") == 0 || strcmp(key, "px") == 0 ||
             strcmp(key, "py") == 0 || strcmp(key, "m") == 0 ||
             strcmp(key, "mx") == 0 || strcmp(key, "my") == 0 ||
             strcmp(key, "gap") == 0 || strcmp(key, "rounded") == 0 ||
             strcmp(key, "shadow") == 0 || strcmp(key, "size") == 0 ||
             strcmp(key, "w") == 0 || strcmp(key, "h") == 0 ||
             strcmp(key, "max-w") == 0 || strcmp(key, "z") == 0 ||
             strcmp(key, "op") == 0 || strcmp(key, "leading") == 0 ||
             strcmp(key, "tracking") == 0) {
    const char *prefix = key;
    if (strcmp(key, "size") == 0) prefix = "text";
    if (strcmp(key, "op") == 0) prefix = "opacity";
    snprintf(vbuf, sizeof(vbuf), " %s-%s", prefix, val);
    strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
  }
}

void solid_ir_collect_classes_ir(char *classes, size_t classes_sz, IrNode *node,
                               const char *base_class) {
  classes[0] = '\0';
  if (base_class) strncat(classes, base_class, classes_sz - 1);
  if (!node) return;

  int has_between = 0;
  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *child = node->kids[i];
    if (!child || child->kind != IR_ATTR || !child->name) continue;
    if (strcmp(child->name, "between") == 0) has_between = 1;
  }

  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *child = node->kids[i];
    if (!child || child->kind != IR_ATTR || !child->name) continue;
    if (strcmp(child->name, "style") == 0) {
      for (size_t j = 0; j < child->n_kids; j++) {
        IrNode *e = child->kids[j];
        if (e && e->kind == IR_ATTR && e->name)
          solid_ir_append_style_entry_class(classes, classes_sz, e->name,
                                   e->value ? e->value : "");
      }
    }
  }

  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *child = node->kids[i];
    char vbuf[96];
    if (!child || child->kind != IR_ATTR || !child->name) continue;
    if (strcmp(child->name, "style") == 0 || strcmp(child->name, "__file__") == 0)
      continue;
    const char *k = child->name;
    const char *v = child->value ? child->value : "";

    if (solid_ir_attr_is_true(v)) {
      if (strcmp(k, "between") == 0)
        strncat(classes,
                strstr(classes, "flex") ? " justify-between"
                                        : " flex justify-between",
                classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "center") == 0)
        strncat(classes,
                has_between ? " flex items-center"
                            : " flex items-center justify-center",
                classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "around") == 0)
        strncat(classes, " justify-around", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "evenly") == 0)
        strncat(classes, " justify-evenly", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "bold") == 0)
        strncat(classes, " font-bold", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "muted") == 0)
        strncat(classes, " text-muted", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "font-mono") == 0)
        strncat(classes, " font-mono", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "flex-1") == 0)
        strncat(classes, " flex-1", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "border") == 0)
        strncat(classes, " border border-gray-200",
                classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "sticky") == 0)
        strncat(classes, " sticky top-0", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "primary") == 0)
        strncat(classes, " btn-primary", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "outline") == 0)
        strncat(classes, " btn-outline", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "ghost") == 0)
        strncat(classes, " btn-ghost", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "secondary") == 0)
        strncat(classes, " btn-secondary", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "xs") == 0)
        strncat(classes, " text-xs", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "sm") == 0)
        strncat(classes, " text-sm", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "lg") == 0)
        strncat(classes, " text-lg", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "xl") == 0)
        strncat(classes, " text-xl", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "2xl") == 0)
        strncat(classes, " text-2xl", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "3xl") == 0)
        strncat(classes, " text-3xl", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "4xl") == 0)
        strncat(classes, " text-4xl", classes_sz - strlen(classes) - 1);
      continue;
    }

    if (strcmp(k, "variant") == 0) {
      snprintf(vbuf, sizeof(vbuf), " btn-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "size") == 0) {
      snprintf(vbuf, sizeof(vbuf), " text-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "color") == 0) {
      if (theme_is_color_token(v)) {
        char tok[64];
        if (theme_token_name(v, tok, sizeof(tok))) {
          snprintf(vbuf, sizeof(vbuf), " text-[var(--color-%s)]", tok);
          strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
        }
      } else {
        snprintf(vbuf, sizeof(vbuf), " text-%s", v);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      }
    } else if (strcmp(k, "gap") == 0) {
      snprintf(vbuf, sizeof(vbuf), " gap-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "cols") == 0) {
      snprintf(vbuf, sizeof(vbuf), " grid-cols-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "p") == 0) {
      snprintf(vbuf, sizeof(vbuf), " p-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "px") == 0) {
      snprintf(vbuf, sizeof(vbuf), " px-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "py") == 0) {
      snprintf(vbuf, sizeof(vbuf), " py-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "m") == 0) {
      snprintf(vbuf, sizeof(vbuf), " m-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "mx") == 0) {
      snprintf(vbuf, sizeof(vbuf), " mx-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "my") == 0) {
      snprintf(vbuf, sizeof(vbuf), " my-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "bg") == 0) {
      if (theme_is_color_token(v)) {
        char tok[64];
        if (theme_token_name(v, tok, sizeof(tok))) {
          snprintf(vbuf, sizeof(vbuf), " bg-[var(--color-%s)]", tok);
          strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
        }
      } else {
        snprintf(vbuf, sizeof(vbuf), " bg-%s", v);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      }
    } else if (strcmp(k, "shadow") == 0) {
      snprintf(vbuf, sizeof(vbuf), " shadow-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "rounded") == 0) {
      snprintf(vbuf, sizeof(vbuf), " rounded-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "max-w") == 0) {
      snprintf(vbuf, sizeof(vbuf), " max-w-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "w") == 0) {
      snprintf(vbuf, sizeof(vbuf), " w-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "h") == 0) {
      snprintf(vbuf, sizeof(vbuf), " h-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "min-h") == 0) {
      snprintf(vbuf, sizeof(vbuf), " min-h-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "border") == 0) {
      snprintf(vbuf, sizeof(vbuf), " border border-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "z") == 0) {
      snprintf(vbuf, sizeof(vbuf), " z-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    }
  }
}
