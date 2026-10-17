#include "adapters/outbound/backends/esm/esm_ir_internal.h"

char *esm_generate_from_ir(IrProgram *ir) {
  if (!ir || !ir->root) return NULL;

  EsmModuleCtx mod;
  memset(&mod, 0, sizeof(mod));
  mod.kind = ESM_MOD_ENTRY;
  mod.source_rel = ir->entry_file;
  mod.abs_path = ir->entry_file;
  mod.project_root = NULL;

  EsmCtx c;
  memset(&c, 0, sizeof(c));
  c.mod = &mod;

  Sb out;
  esm_ir_sb_init(&out);
  /*
   * Basename only: `compile --backend esm` receives whatever path the caller
   * typed (relative or absolute), so echoing it whole would make the output
   * depend on the invocation and no snapshot could ever pin it. The dev server
   * has a real project-relative path and uses it (see esm_generate_module).
   */
  {
    char *base = ir->entry_file ? fs_basename(ir->entry_file) : NULL;
    esm_ir_sb_addf(&out, "/* cordlang: source=%s */\n", base ? base : "(inline)");
    free(base);
  }

  /* Flat `compile --backend esm`: still collect foreigns so stubs bind names. */
  esm_ir_collect_foreigns(&c, ir->root);
  esm_ir_gen_imports(&out, &c);

  esm_ir_gen_theme_export(&out, ir->root);

  for (size_t i = 0; i < ir->root->n_kids; i++) {
    IrNode *d = ir->root->kids[i];
    if (!d || (d->kind != IR_COMPONENT && d->kind != IR_LAYOUT)) continue;
    char en[96];
    if (d->kind == IR_LAYOUT)
      esm_ir_layout_export_name_of(d->name ? d->name : "default", en, sizeof(en));
    else
      snprintf(en, sizeof(en), "%s", d->name ? d->name : "Component");
    esm_ir_gen_component(&out, d, en, d->kind == IR_LAYOUT, &c);
  }

  /* Routes reference already-defined local components in this flat mode. */
  int n_routes = 0;
  for (size_t i = 0; i < ir->root->n_kids; i++)
    if (ir->root->kids[i] && ir->root->kids[i]->kind == IR_ROUTE) n_routes++;

  if (n_routes) {
    char first_layout[96];
    char default_layout[96];
    first_layout[0] = '\0';
    default_layout[0] = '\0';
    for (size_t i = 0; i < ir->root->n_kids; i++) {
      IrNode *d = ir->root->kids[i];
      if (!d || d->kind != IR_LAYOUT) continue;
      char en[96];
      esm_ir_layout_export_name_of(d->name ? d->name : "default", en, sizeof(en));
      if (!first_layout[0]) snprintf(first_layout, sizeof(first_layout), "%s", en);
      if (strcmp(en, "DefaultLayout") == 0)
        snprintf(default_layout, sizeof(default_layout), "%s", en);
    }
    esm_ir_sb_add(&out, "export const routes = [\n");
    for (size_t i = 0; i < ir->root->n_kids; i++) {
      IrNode *r = ir->root->kids[i];
      if (!r || r->kind != IR_ROUTE || !r->value) continue;
      const char *layout_attr = NULL;
      for (size_t j = 0; j < r->n_kids; j++) {
        IrNode *a = r->kids[j];
        if (a && a->kind == IR_ATTR && a->name &&
            strcmp(a->name, "layout") == 0 && a->value && a->value[0])
          layout_attr = a->value;
      }
      char layout[96];
      layout[0] = '\0';
      if (layout_attr)
        esm_ir_layout_export_name_of(layout_attr, layout, sizeof(layout));
      else if (default_layout[0])
        snprintf(layout, sizeof(layout), "%s", default_layout);
      else if (first_layout[0])
        snprintf(layout, sizeof(layout), "%s", first_layout);

      esm_ir_sb_add(&out, "  { path: ");
      esm_ir_emit_sq_string(&out, r->name ? r->name : "/");
      esm_ir_sb_addf(&out, ", component: %s", r->value);
      if (layout[0]) esm_ir_sb_addf(&out, ", layout: %s", layout);
      esm_ir_sb_add(&out, " },\n");
    }
    esm_ir_sb_add(&out, "];\n\n");
    esm_ir_sb_add(&out, "export default { __cord: 'app', routes: routes, theme: theme };\n");
  } else {
    const char *root_comp = NULL;
    static char buf[96];
    for (size_t i = 0; i < ir->root->n_kids; i++) {
      IrNode *d = ir->root->kids[i];
      if (!d || d->kind != IR_COMPONENT) continue;
      snprintf(buf, sizeof(buf), "%s", d->name ? d->name : "Component");
      root_comp = buf;
      break;
    }
    if (root_comp)
      esm_ir_sb_addf(&out, "export default %s;\n", root_comp);
    else
      esm_ir_sb_add(&out,
             "export default component('Empty', function () { return null; });\n");
  }

  return out.buf;
}

char *esm_generate(Node *root) {
  if (!root) return NULL;
  IrProgram *ir = ir_from_ast(root, NULL);
  if (!ir) return NULL;
  char *out = esm_generate_from_ir(ir);
  ir_free(ir);
  return out;
}

const BackendPort *esm_backend_port(void) { return &esm_port; }
