#include "adapters/outbound/backends/vue/vue_ir_internal.h"

void vue_vue_emit_module_props_ir(VueUnit *u, IrNode *def,
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
  (void)need_children;
  if (has_props || params_decl) {
    PropsEmitCtx pctx;
    memset(&pctx, 0, sizeof(pctx));
    pctx.script = script;
    vue_sb_append(script, "const props = defineProps({\n");
    vue_foreach_prop(def, vue_props_emit_one, &pctx);
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
          vue_foreach_prop(def, vue_props_collect_name, &pctx);
          vue_sb_appendf(script, "  %s: {},\n", name);
          vue_props_collect_name(name, NULL, &pctx);
        }
      }
    }
    vue_sb_append(script, "});\n");
    /* expose prop names as locals for template ergonomics */
    for (int k = 0; k < pctx.n_prop_names; k++) {
      vue_sb_appendf(script, "const %s = props.%s;\n", pctx.prop_names[k],
                 pctx.prop_names[k]);
    }
  }
}
