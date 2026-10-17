#include "adapters/outbound/backends/svelte/svelte_backend_internal.h"

void svelte_svelte_emit_module_imports_ir(SvelteProject *proj, SvelteUnit *u,
                                         IrNode *def, StrBuf *script,
                                         SvModuleNeeds *needs,
                                         ProvideInfo *provides, int *n_provides) {
  memset(needs, 0, sizeof(*needs));
  svelte_scan_module_needs_ir(def, needs);

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (c->kind == IR_STORE && c->name) svelte_sv_add_store(c->name);
  }

  svelte_collect_provides_ir(def, provides, n_provides, 16);

  int need_set_ctx = (*n_provides > 0);
  int need_get_ctx = 0;
  for (size_t i = 0; i < def->n_kids; i++) {
    if (irw_hook_is(def->kids[i], "ctx")) need_get_ctx = 1;
  }
  if (need_set_ctx || need_get_ctx) u->use_context = 1;

  if (need_set_ctx && need_get_ctx)
    svelte_sb_append(script, "  import { setContext, getContext } from 'svelte';\n");
  else if (need_set_ctx)
    svelte_sb_append(script, "  import { setContext } from 'svelte';\n");
  else if (need_get_ctx)
    svelte_sb_append(script, "  import { getContext } from 'svelte';\n");

  if (needs->need_writable)
    svelte_sb_append(script, "  import { writable } from 'svelte/store';\n");

  if (needs->n_transition > 0) {
    svelte_sb_append(script, "  import { ");
    for (int i = 0; i < needs->n_transition; i++) {
      if (i) svelte_sb_append(script, ", ");
      svelte_sb_append(script, needs->transition_fns[i]);
    }
    svelte_sb_append(script, " } from 'svelte/transition';\n");
  }
  if (needs->n_animate > 0) {
    svelte_sb_append(script, "  import { ");
    for (int i = 0; i < needs->n_animate; i++) {
      if (i) svelte_sb_append(script, ", ");
      svelte_sb_append(script, needs->animate_fns[i]);
    }
    svelte_sb_append(script, " } from 'svelte/animate';\n");
  }

  if (proj->n_contexts > 0 && (need_set_ctx || need_get_ctx || u->use_context)) {
    svelte_sb_append(script, "  import { ");
    for (int i = 0; i < proj->n_contexts; i++) {
      if (i) svelte_sb_append(script, ", ");
      svelte_sb_append(script,
                proj->contexts[i]->value ? proj->contexts[i]->value : "Ctx");
    }
    svelte_sb_append(script, " } from '../contexts.js';\n");
  }

  for (int i = 0; i < u->n_used; i++) {
    SvelteUnit *dep = svelte_find_unit(proj, u->used[i]);
    if (dep) {
      if (strcmp(u->dir, dep->dir) == 0)
        svelte_sb_appendf(script, "  import %s from './%s.svelte';\n", dep->name,
                   dep->name);
      else
        svelte_sb_appendf(script, "  import %s from '../%s/%s.svelte';\n", dep->name,
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
            strcmp(a->name, "svelte") == 0)
          mod = a->value;
      }
      if (mod && mod[0])
        svelte_sb_appendf(script, "  import %s from '%s';\n", fn->name, mod);
      break;
    }
  }
  if (u->use_cord_icon)
    svelte_sb_append(script, "  import CordIcon from '../CordIcon.svelte';\n");
  if (u->use_cord_motion)
    svelte_sb_append(script, "  import CordMotion from '../CordMotion.svelte';\n");
  if (u->use_cord_chart)
    svelte_sb_append(script, "  import CordChart from '../CordChart.svelte';\n");
}
