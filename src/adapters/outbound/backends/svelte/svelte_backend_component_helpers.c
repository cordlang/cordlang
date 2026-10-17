#include "adapters/outbound/backends/svelte/svelte_backend_internal.h"

void svelte_sv_collect_binds_ir(IrNode *n, char names[][64], int *count,
                                int max) {
  if (!n || *count >= max) return;
  if (n->kind == IR_ATTR && n->name && strcmp(n->name, "bind") == 0 &&
      n->value && !strchr(n->value, '.')) {
    int found = 0;
    for (int i = 0; i < *count; i++) {
      if (strcmp(names[i], n->value) == 0) {
        found = 1;
        break;
      }
    }
    if (!found && *count < max) {
      snprintf(names[*count], sizeof(names[0]), "%s", n->value);
      (*count)++;
    }
  }
  for (size_t i = 0; i < n->n_kids; i++)
    svelte_sv_collect_binds_ir(n->kids[i], names, count, max);
}

int svelte_sv_state_exists_ir(IrNode *def, const char *name) {
  if (!def || !name) return 0;
  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (c->kind == IR_STATE && c->name && strcmp(c->name, "__states__") != 0 &&
        strcmp(c->name, name) == 0)
      return 1;
    if (c->kind == IR_STATE && c->name && strcmp(c->name, "__states__") == 0) {
      for (size_t j = 0; j < c->n_kids; j++) {
        if (c->kids[j]->name && strcmp(c->kids[j]->name, name) == 0) return 1;
      }
    }
    if (irw_hook_is(c, "action") && c->value && strcmp(c->value, name) == 0)
      return 1;
    if (c->kind == IR_FETCH && c->name && strcmp(c->name, name) == 0) return 1;
  }
  return 0;
}

void svelte_foreach_state(IrNode *def,
                          void (*fn)(const char *name, const char *init,
                                     void *ud),
                          void *ud) {
  if (!def) return;
  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (c->kind != IR_STATE) continue;
    if (c->name && strcmp(c->name, "__states__") == 0) {
      for (size_t j = 0; j < c->n_kids; j++) {
        IrNode *st = c->kids[j];
        if (st->kind == IR_STATE && st->name) fn(st->name, st->value, ud);
      }
    } else if (c->name) {
      fn(c->name, c->value, ud);
    }
  }
}

void svelte_foreach_prop(IrNode *def,
                         void (*fn)(const char *name, const char *deflt,
                                    void *ud),
                         void *ud) {
  if (!def) return;
  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (c->kind != IR_PROP) continue;
    if (c->name && strcmp(c->name, "__props__") == 0) {
      for (size_t j = 0; j < c->n_kids; j++) {
        IrNode *pr = c->kids[j];
        if (pr->kind == IR_PROP && pr->name) fn(pr->name, pr->value, ud);
      }
    } else if (c->name && strcmp(c->name, "__props__") != 0) {
      fn(c->name, c->value, ud);
    }
  }
}

void svelte_props_collect_name(const char *name, const char *deflt, void *ud) {
  (void)deflt;
  PropsEmitCtx *ctx = ud;
  if (!name || ctx->n_prop_names >= 64) return;
  snprintf(ctx->prop_names[ctx->n_prop_names], sizeof(ctx->prop_names[0]), "%s",
           name);
  ctx->n_prop_names++;
}

void svelte_props_emit_one(const char *name, const char *deflt, void *ud) {
  PropsEmitCtx *ctx = ud;
  if (!name) return;
  if (ctx->n++) svelte_sb_append(ctx->script, ", ");
  svelte_sb_append(ctx->script, name);
  if (deflt) {
    svelte_sb_append(ctx->script, " = ");
    svelte_emit_js_lit(ctx->script, deflt);
  }
}

void svelte_state_emit_one(const char *name, const char *init, void *ud) {
  StateEmitCtx *ctx = ud;
  if (!name) return;
  svelte_sb_appendf(ctx->script, "  let %s = $state(", name);
  svelte_emit_js_lit(ctx->script, init ? init : "null");
  svelte_sb_append(ctx->script, ");\n");
}

void svelte_state_emit_setter(const char *name, const char *init, void *ud) {
  (void)init;
  StateEmitCtx *ctx = ud;
  if (!name || !*name) return;
  char setter[128];
  snprintf(setter, sizeof(setter), "set%c%s",
           (char)toupper((unsigned char)name[0]), name + 1);
  svelte_sb_appendf(ctx->script,
             "  function %s(v) { %s = typeof v === 'function' ? v(%s) : v; "
             "}\n",
             setter, name, name);
}
