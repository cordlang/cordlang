#include "adapters/outbound/backends/svelte/svelte_backend_internal.h"

void svelte_svelte_emit_module_props_ir(SvelteUnit *u, IrNode *def,
                                        int is_layout, StrBuf *script) {
  IrNode *params_decl = ir_find_hook(def, "params");
  if (params_decl) u->use_params = 1;

  int has_props = 0;
  for (size_t i = 0; i < def->n_kids; i++) {
    if (def->kids[i]->kind == IR_PROP) {
      has_props = 1;
      break;
    }
  }

  int need_children = u->use_slot || is_layout;
  if (has_props || need_children || params_decl) {
    PropsEmitCtx pctx;
    memset(&pctx, 0, sizeof(pctx));
    pctx.script = script;
    svelte_sb_append(script, "  let { ");
    svelte_foreach_prop(def, svelte_props_emit_one, &pctx);
    /* remember prop names for params dup check */
    svelte_foreach_prop(def, svelte_props_collect_name, &pctx);

    if (params_decl && params_decl->value) {
      const char *p = params_decl->value;
      while (*p) {
        while (*p == ' ' || *p == ',') p++;
        if (!*p) break;
        char name[64];
        size_t ni = 0;
        while (*p && *p != ',' && *p != ' ' && ni + 1 < sizeof(name))
          name[ni++] = *p++;
        name[ni] = '\0';
        if (ni == 0) continue;
        int dup = 0;
        for (int k = 0; k < pctx.n_prop_names; k++) {
          if (strcmp(pctx.prop_names[k], name) == 0) dup = 1;
        }
        if (!dup) {
          if (pctx.n++) svelte_sb_append(script, ", ");
          svelte_sb_append(script, name);
        }
      }
    }
    if (need_children) {
      if (pctx.n++) svelte_sb_append(script, ", ");
      svelte_sb_append(script, "children");
    }
    svelte_sb_append(script, " } = $props();\n");
  }
}
