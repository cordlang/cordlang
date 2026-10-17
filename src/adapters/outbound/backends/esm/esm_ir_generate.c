#include "adapters/outbound/backends/esm/esm_ir_internal.h"

char *esm_generate_module(IrProgram *ir, EsmModuleCtx *mod) {
  if (!ir || !ir->root || !mod) return NULL;
  mod->truncated = 0;

  EsmCtx c;
  memset(&c, 0, sizeof(c));
  c.mod = mod;
  /* Only the project entry script self-mounts; component modules are imports. */
  c.self_mount = (mod->kind == ESM_MOD_ENTRY);

  IrNode *root = ir->root;

  /* Body is generated first so imports can be collected, then prepended. */
  Sb body;
  esm_ir_sb_init(&body);

  if (mod->kind == ESM_MOD_ENTRY) {
    esm_ir_gen_theme_export(&body, root);
    esm_ir_gen_routes(&body, root, &c);
  } else {
    for (size_t i = 0; i < root->n_kids; i++) {
      IrNode *u = root->kids[i];
      if (!u || u->kind != IR_MODULE_USE) continue;
      esm_ir_import_add(&c, u->value, u->name);
    }
  }

  /* foreign Chart from "pkg" — no npm in preview; stub before components use it. */
  esm_ir_collect_foreigns(&c, root);

  /* Component / layout definitions declared in this file. */
  const char *primary = NULL;
  for (size_t i = 0; i < root->n_kids; i++) {
    IrNode *d = root->kids[i];
    if (!d || (d->kind != IR_COMPONENT && d->kind != IR_LAYOUT)) continue;
    const char *name = d->name ? d->name : "Component";
    int is_layout = (d->kind == IR_LAYOUT);
    char export_name[96];
    if (is_layout)
      esm_ir_layout_export_name_of(name, export_name, sizeof(export_name));
    else
      snprintf(export_name, sizeof(export_name), "%s", name);
    esm_ir_gen_component(&body, d, export_name, is_layout, &c);
    if (mod->export_name && strcmp(export_name, mod->export_name) == 0)
      primary = mod->export_name;
    if (!primary) primary = NULL;
  }

  /* Pick the default export. */
  char default_name[96];
  default_name[0] = '\0';
  if (mod->kind != ESM_MOD_ENTRY) {
    if (mod->export_name) {
      for (size_t i = 0; i < root->n_kids; i++) {
        IrNode *d = root->kids[i];
        if (!d || (d->kind != IR_COMPONENT && d->kind != IR_LAYOUT)) continue;
        char en[96];
        if (d->kind == IR_LAYOUT)
          esm_ir_layout_export_name_of(d->name ? d->name : "default", en, sizeof(en));
        else
          snprintf(en, sizeof(en), "%s", d->name ? d->name : "Component");
        if (strcmp(en, mod->export_name) == 0) {
          snprintf(default_name, sizeof(default_name), "%s", en);
          break;
        }
      }
    }
    if (!default_name[0]) {
      /* Fall back to the first definition in the file. */
      for (size_t i = 0; i < root->n_kids; i++) {
        IrNode *d = root->kids[i];
        if (!d || (d->kind != IR_COMPONENT && d->kind != IR_LAYOUT)) continue;
        if (d->kind == IR_LAYOUT)
          esm_ir_layout_export_name_of(d->name ? d->name : "default", default_name,
                                sizeof(default_name));
        else
          snprintf(default_name, sizeof(default_name), "%s",
                   d->name ? d->name : "Component");
        break;
      }
    }
  }

  Sb out;
  esm_ir_sb_init(&out);
  /* Normalize separators: the header must not differ between Windows and POSIX. */
  {
    char *src = mod->source_rel ? fs_norm_path(mod->source_rel) : NULL;
    esm_ir_sb_addf(&out, "/* cordlang: source=%s */\n", src ? src : "(inline)");
    free(src);
  }
  esm_ir_gen_imports(&out, &c);
  esm_ir_sb_add(&out, body.buf ? body.buf : "");
  free(body.buf);

  if (mod->kind == ESM_MOD_ENTRY) {
    /*
     * Vite-style: the shell is just
     *   <script type="module" src="/src/app.cord"></script>
     * so the entry boots itself when a #app host exists. `compile --backend esm`
     * uses esm_generate_from_ir (flat, self_mount=0) and does not boot.
     */
    if (c.self_mount) {
      esm_ir_sb_add(&out,
             "const __cord_root = { __cord: 'app', routes: routes, theme: theme");
      if (default_name[0]) esm_ir_sb_addf(&out, ", component: %s", default_name);
      esm_ir_sb_add(&out, " };\n");
      esm_ir_sb_add(&out, "export default __cord_root;\n");
      esm_ir_sb_add(&out,
             "if (typeof document !== 'undefined') {\n"
             "  const __el = document.getElementById('app');\n"
             "  if (__el) mount(__cord_root, __el);\n"
             "}\n");
    } else {
      esm_ir_sb_add(&out,
             "export default { __cord: 'app', routes: routes, theme: theme");
      if (default_name[0]) esm_ir_sb_addf(&out, ", component: %s", default_name);
      esm_ir_sb_add(&out, " };\n");
    }
  } else if (default_name[0]) {
    esm_ir_sb_addf(&out, "export default %s;\n", default_name);
  } else {
    esm_ir_sb_add(&out,
           "export default component('Empty', function () { return null; });\n");
  }

  if (c.truncated) {
    mod->truncated = 1;
    esm_ir_sb_add(&out, "/* cordlang: import limit reached, some modules omitted */\n");
  }

  return out.buf;
}






