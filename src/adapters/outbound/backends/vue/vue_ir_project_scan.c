#include "adapters/outbound/backends/vue/vue_ir_internal.h"

int vue_name_ends_with(const char *s, const char *suffix) {
  size_t ls = s ? strlen(s) : 0, lf = strlen(suffix);
  return ls >= lf && strcmp(s + ls - lf, suffix) == 0;
}

int vue_is_route_target_ir(const char *name, IrNode **routes, int n) {
  if (!name) return 0;
  for (int i = 0; i < n; i++)
    if (routes[i] && routes[i]->value && strcmp(routes[i]->value, name) == 0)
      return 1;
  return 0;
}

void vue_unit_add_used(VueUnit *u, const char *name) {
  if (!name || !irw_is_pascal(name)) return;
  if (strcmp(name, u->name) == 0) return;
  for (int i = 0; i < u->n_used; i++)
    if (strcmp(u->used[i], name) == 0) return;
  if (u->n_used < VU_MAX_USED) {
    snprintf(u->used[u->n_used], sizeof(u->used[0]), "%s", name);
    u->n_used++;
  }
}

int vue_route_has_lazy_attr_ir(IrNode *route) {
  if (!route) return 0;
  for (size_t i = 0; i < route->n_kids; i++) {
    IrNode *c = route->kids[i];
    if (c && c->kind == IR_ATTR && c->name && strcmp(c->name, "lazy") == 0 &&
        vue_attr_is_true(c))
      return 1;
  }
  return 0;
}

int vue_project_is_lazy_route_name(VueProject *proj, const char *name) {
  if (!proj || !name) return 0;
  for (int i = 0; i < proj->n_lazy_routes; i++) {
    if (strcmp(proj->lazy_route_names[i], name) == 0) return 1;
  }
  return 0;
}

int vue_route_is_lazy_ir(VueProject *proj, IrNode *route) {
  if (!route) return 0;
  if (vue_route_has_lazy_attr_ir(route)) return 1;
  if (route->value && vue_project_is_lazy_route_name(proj, route->value)) return 1;
  return 0;
}

void vue_scan_deps_ir(IrNode *n, VueUnit *u) {
  if (!n || !u) return;
  if (n->kind == IR_SLOT) u->use_slot = 1;
  if (irw_hook_is(n, "ctx") || irw_hook_is(n, "context")) u->use_context = 1;
  if (irw_hook_is(n, "params")) u->use_params = 1;
  if (n->kind == IR_ELEMENT && n->name) {
    if (irw_is_pascal(n->name)) vue_unit_add_used(u, n->name);
    if (strcmp(n->name, "link") == 0) u->use_link = 1;
    if (strcmp(n->name, "provide") == 0) u->use_context = 1;
    if (strcmp(n->name, "icon") == 0) u->use_cord_icon = 1;
    if (strcmp(n->name, "motion") == 0 || strcmp(n->name, "Motion") == 0)
      u->use_cord_motion = 1;
    if (strcmp(n->name, "chart") == 0 || strcmp(n->name, "Chart") == 0)
      u->use_cord_chart = 1;
  }
  for (size_t i = 0; i < n->n_kids; i++) vue_scan_deps_ir(n->kids[i], u);
}

void vue_scan_module_needs_ir(IrNode *n, VuModuleNeeds *needs) {
  if (!n || !needs) return;
  if (irw_hook_is(n, "portal")) needs->need_portal = 1;
  if (n->kind == IR_STORE) needs->need_writable = 1;
  if (n->kind == IR_ATTR && n->name && n->value &&
      vue_is_vue_directive_attr(n->name)) {
    char fn[64];
    vue_directive_fn_name(n->value, fn, sizeof(fn));
    if (strcmp(n->name, "animate") == 0)
      vue_vu_needs_add_anim(needs, fn);
    else if (strcmp(n->name, "transition") == 0 || strcmp(n->name, "in") == 0 ||
             strcmp(n->name, "out") == 0)
      vue_vu_needs_add_trans(needs, fn);
  }
  for (size_t i = 0; i < n->n_kids; i++)
    vue_scan_module_needs_ir(n->kids[i], needs);
}

void vue_project_add_context_ir(VueProject *p, IrNode *c) {
  if (!p || !c || !c->value || p->n_contexts >= 32) return;
  for (int k = 0; k < p->n_contexts; k++) {
    if (p->contexts[k]->value && strcmp(p->contexts[k]->value, c->value) == 0)
      return;
  }
  p->contexts[p->n_contexts++] = c;
}

void vue_harvest_contexts_ir(VueProject *p, IrNode *root) {
  if (!p) return;
  if (root) {
    for (size_t i = 0; i < root->n_kids; i++) {
      IrNode *c = root->kids[i];
      if (irw_hook_is(c, "context")) vue_project_add_context_ir(p, c);
    }
  }
  for (int i = 0; i < p->n_units; i++) {
    IrNode *def = p->units[i].ir_def;
    if (!def) continue;
    for (size_t j = 0; j < def->n_kids; j++) {
      IrNode *c = def->kids[j];
      if (irw_hook_is(c, "context")) vue_project_add_context_ir(p, c);
    }
  }
}

void vue_collect_provides_ir(IrNode *n, ProvideInfo *out, int *count,
                                int max) {
  if (!n || *count >= max) return;
  if (n->kind == IR_ELEMENT && n->name && strcmp(n->name, "provide") == 0) {
    const char *ctx_name = "Context";
    const char *val = NULL;
    for (size_t i = 0; i < n->n_kids; i++) {
      IrNode *ch = n->kids[i];
      if (ch->kind == IR_ATTR && ch->name) {
        if (strcmp(ch->name, "context") == 0 && ch->value) ctx_name = ch->value;
        if (strcmp(ch->name, "value") == 0) val = ch->value;
      }
    }
    out[*count].ctx_name = ctx_name;
    out[*count].val = val;
    (*count)++;
  }
  for (size_t i = 0; i < n->n_kids; i++)
    vue_collect_provides_ir(n->kids[i], out, count, max);
}
