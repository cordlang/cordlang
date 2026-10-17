#include "adapters/outbound/backends/vue/vue_ir_internal.h"

void vue_vue_emit_module_imports_ir(VueProject *proj, VueUnit *u,
                                      IrNode *def, StrBuf *script,
                                      VuModuleNeeds *needs,
                                      ProvideInfo *provides, int *n_provides) {
  memset(needs, 0, sizeof(*needs));
  vue_scan_module_needs_ir(def, needs);

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (c->kind == IR_STORE && c->name) vue_vu_add_store(c->name);
  }

  vue_collect_provides_ir(def, provides, n_provides, 16);

  int need_set_ctx = (*n_provides > 0);
  int need_get_ctx = 0;
  for (size_t i = 0; i < def->n_kids; i++) {
    if (irw_hook_is(def->kids[i], "ctx")) need_get_ctx = 1;
  }
  if (need_set_ctx || need_get_ctx) u->use_context = 1;

  {
    int need_ref = 0, need_computed = 0, need_watch = 0, need_provide = need_set_ctx,
        need_inject = need_get_ctx;
    for (size_t _i = 0; _i < def->n_kids; _i++) {
      IrNode *_c = def->kids[_i];
      if (_c->kind == IR_STATE || irw_hook_is(_c, "ref") || _c->kind == IR_FETCH ||
          irw_hook_is(_c, "action"))
        need_ref = 1;
      if (_c->kind == IR_COMPUTED) need_computed = 1;
      if (_c->kind == IR_EFFECT) need_watch = 1;
      if (_c->kind == IR_FETCH) need_watch = 1;
    }
    char binds_tmp[32][64];
    int n_bt = 0;
    vue_vu_collect_binds_ir(def, binds_tmp, &n_bt, 32);
    if (n_bt > 0) need_ref = 1;
    if (need_ref || need_computed || need_watch || need_provide || need_inject) {
      vue_sb_append(script, "import { ");
      int first = 1;
      if (need_ref) { vue_sb_append(script, "ref"); first = 0; }
      if (need_computed) {
        if (!first) vue_sb_append(script, ", ");
        vue_sb_append(script, "computed");
        first = 0;
      }
      if (need_watch) {
        if (!first) vue_sb_append(script, ", ");
        vue_sb_append(script, "watchEffect");
        first = 0;
      }
      if (need_provide) {
        if (!first) vue_sb_append(script, ", ");
        vue_sb_append(script, "provide");
        first = 0;
      }
      if (need_inject) {
        if (!first) vue_sb_append(script, ", ");
        vue_sb_append(script, "inject");
      }
      vue_sb_append(script, " } from 'vue';\n");
    }
  }
  (void)needs;

  if (proj->n_contexts > 0 && (need_set_ctx || need_get_ctx || u->use_context)) {
    vue_sb_append(script, "  import { ");
    for (int i = 0; i < proj->n_contexts; i++) {
      if (i) vue_sb_append(script, ", ");
      vue_sb_append(script,
                proj->contexts[i]->value ? proj->contexts[i]->value : "Ctx");
    }
    vue_sb_append(script, " } from '../contexts.js';\n");
  }

  for (int i = 0; i < u->n_used; i++) {
    VueUnit *dep = vue_find_unit(proj, u->used[i]);
    if (dep) {
      if (strcmp(u->dir, dep->dir) == 0)
        vue_sb_appendf(script, "  import %s from './%s.vue';\n", dep->name,
                   dep->name);
      else
        vue_sb_appendf(script, "  import %s from '../%s/%s.vue';\n", dep->name,
                   dep->dir, dep->name);
      continue;
    }
    for (int f = 0; f < proj->n_foreigns; f++) {
      IrNode *fn = proj->foreigns[f];
      if (!fn || !fn->name || strcmp(fn->name, u->used[i]) != 0) continue;
      const char *mod = fn->value;
      for (size_t k = 0; k < fn->n_kids; k++) {
        IrNode *a = fn->kids[k];
        if (a && a->kind == IR_ATTR && a->name && a->value &&
            strcmp(a->name, "vue") == 0)
          mod = a->value;
      }
      if (mod && mod[0])
        vue_sb_appendf(script, "  import %s from '%s';\n", fn->name, mod);
      break;
    }
  }
  if (u->use_cord_icon)
    vue_sb_append(script, "  import CordIcon from '../CordIcon.vue';\n");
  if (u->use_cord_motion)
    vue_sb_append(script, "  import CordMotion from '../CordMotion.vue';\n");
  if (u->use_cord_chart)
    vue_sb_append(script, "  import CordChart from '../CordChart.vue';\n");
}
