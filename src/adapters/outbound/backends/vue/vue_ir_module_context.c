#include "adapters/outbound/backends/vue/vue_ir_internal.h"

char *vue_gen_vue_module_ir(VueProject *proj, VueUnit *u) {
  IrNode *def = u->ir_def;
  if (!def) return strdup("<!-- empty -->\n");
  int is_layout = (u->kind == SK_LAYOUT);
  StrBuf script;
  vue_sb_init(&script);
  StrBuf markup;
  vue_sb_init(&markup);

  vue_vu_reset_stores();

  VuModuleNeeds needs = {0};
  ProvideInfo provides[16] = {0};
  int n_provides = 0;
  vue_vue_emit_module_imports_ir(proj, u, def, &script, &needs, provides,
                             &n_provides);
  vue_vue_emit_module_props_ir(u, def, is_layout, &script);
  vue_vue_emit_module_state_ir(def, &script, &needs, provides, n_provides);

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (vue_is_ir_script_decl(c)) continue;
    vue_gen_node_ir(&markup, c, 0, is_layout);
  }

  StrBuf out;
  vue_sb_init(&out);
  if (u->source_path[0])
    vue_sb_appendf(&out, "<!-- cordlang: source=%s -->\n", u->source_path);
  vue_sb_append(&out, "<script setup>\n");
  vue_sb_append(&out, script.buf ? script.buf : "");
  vue_sb_append(&out, "</script>\n\n");
  vue_sb_append(&out, "<template>\n");
  vue_sb_append(&out, markup.buf ? markup.buf : "");
  vue_sb_append(&out, "</template>\n");
  free(script.buf);
  free(markup.buf);
  return out.buf;
}

char *vue_gen_contexts_module_ir(VueProject *proj) {
  if (!proj || proj->n_contexts == 0) return NULL;
  StrBuf out;
  vue_sb_init(&out);
  vue_sb_append(&out, "/* Cordlang context keys — use with provide/inject */\n");
  for (int i = 0; i < proj->n_contexts; i++) {
    IrNode *c = proj->contexts[i];
    const char *name = c->value ? c->value : "AppContext";
    vue_sb_appendf(&out, "export const %s = Symbol('%s');\n", name, name);
  }
  return out.buf;
}

const char *vue_route_layout_attr_ir(IrNode *route) {
  return ir_attr(route, "layout");
}

VueUnit *vue_find_layout_for_route_ir(VueProject *proj, IrNode *route) {
  const char *attr = vue_route_layout_attr_ir(route);
  char export_name[96];

  if (attr) {
    vue_layout_name(attr, export_name, sizeof(export_name));
    for (int i = 0; i < proj->n_units; i++) {
      if (proj->units[i].kind != SK_LAYOUT) continue;
      if (strcmp(proj->units[i].name, export_name) == 0) return &proj->units[i];
      if (proj->units[i].ir_def && proj->units[i].ir_def->name &&
          strcmp(proj->units[i].ir_def->name, attr) == 0)
        return &proj->units[i];
    }
    return NULL;
  }

  VueUnit *first = NULL;
  for (int i = 0; i < proj->n_units; i++) {
    if (proj->units[i].kind != SK_LAYOUT) continue;
    if (!first) first = &proj->units[i];
    if (strcmp(proj->units[i].name, "DefaultLayout") == 0) return &proj->units[i];
    if (proj->units[i].ir_def && proj->units[i].ir_def->name &&
        strcmp(proj->units[i].ir_def->name, "default") == 0)
      return &proj->units[i];
  }
  return first;
}
