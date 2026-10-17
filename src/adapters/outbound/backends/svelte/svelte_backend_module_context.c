#include "adapters/outbound/backends/svelte/svelte_backend_internal.h"

char *svelte_gen_svelte_module_ir(SvelteProject *proj, SvelteUnit *u) {
  IrNode *def = u->ir_def;
  if (!def) return strdup("<!-- empty -->\n");
  int is_layout = (u->kind == SK_LAYOUT);
  StrBuf script;
  svelte_sb_init(&script);
  StrBuf markup;
  svelte_sb_init(&markup);

  svelte_sv_reset_stores();

  SvModuleNeeds needs = {0};
  ProvideInfo provides[16] = {0};
  int n_provides = 0;
  svelte_svelte_emit_module_imports_ir(proj, u, def, &script, &needs, provides,
                                &n_provides);
  svelte_svelte_emit_module_props_ir(u, def, is_layout, &script);
  svelte_svelte_emit_module_state_ir(def, &script, &needs, provides, n_provides);

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (svelte_is_ir_script_decl(c)) continue;
    svelte_gen_node_ir(&markup, c, 0, is_layout);
  }

  StrBuf out;
  svelte_sb_init(&out);
  if (u->source_path[0])
    svelte_sb_appendf(&out, "<!-- cordlang: source=%s -->\n", u->source_path);
  svelte_sb_append(&out, "<script>\n");
  svelte_sb_append(&out, script.buf ? script.buf : "");
  svelte_sb_append(&out, "</script>\n\n");
  svelte_sb_append(&out, markup.buf ? markup.buf : "");
  free(script.buf);
  free(markup.buf);
  return out.buf;
}

char *svelte_gen_contexts_module_ir(SvelteProject *proj) {
  if (!proj || proj->n_contexts == 0) return NULL;
  StrBuf out;
  svelte_sb_init(&out);
  svelte_sb_append(&out, "/* Cordlang context keys — use with setContext/getContext */\n");
  for (int i = 0; i < proj->n_contexts; i++) {
    IrNode *c = proj->contexts[i];
    const char *name = c->value ? c->value : "AppContext";
    svelte_sb_appendf(&out, "export const %s = Symbol('%s');\n", name, name);
  }
  return out.buf;
}

const char *svelte_route_layout_attr_ir(IrNode *route) {
  return ir_attr(route, "layout");
}

SvelteUnit *svelte_find_layout_for_route_ir(SvelteProject *proj, IrNode *route) {
  const char *attr = svelte_route_layout_attr_ir(route);
  char export_name[96];

  if (attr) {
    svelte_layout_name(attr, export_name, sizeof(export_name));
    for (int i = 0; i < proj->n_units; i++) {
      if (proj->units[i].kind != SK_LAYOUT) continue;
      if (strcmp(proj->units[i].name, export_name) == 0) return &proj->units[i];
      if (proj->units[i].ir_def && proj->units[i].ir_def->name &&
          strcmp(proj->units[i].ir_def->name, attr) == 0)
        return &proj->units[i];
    }
    return NULL;
  }

  SvelteUnit *first = NULL;
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
