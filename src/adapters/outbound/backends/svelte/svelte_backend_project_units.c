#include "adapters/outbound/backends/svelte/svelte_backend_internal.h"

SvelteUnit *svelte_find_unit(SvelteProject *p, const char *name) {
  if (!name) return NULL;
  for (int i = 0; i < p->n_units; i++)
    if (strcmp(p->units[i].name, name) == 0) return &p->units[i];
  return NULL;
}

void svelte_layout_name(const char *orig, char *out, size_t n) {
  if (!orig || !*orig || strcmp(orig, "default") == 0) {
    snprintf(out, n, "DefaultLayout");
    return;
  }
  if (irw_is_pascal(orig)) {
    snprintf(out, n, "%s", orig);
    return;
  }
  snprintf(out, n, "%c%s", (char)toupper((unsigned char)orig[0]), orig + 1);
}

void svelte_project_partition_from_ir(SvelteProject *p, IrNode *root) {
  memset(p, 0, sizeof(*p));
  if (!root) return;

  IrNode *raw_comps[SV_MAX_UNITS];
  IrNode *raw_layouts[16];
  int n_comp = 0, n_layout = 0;

  for (size_t i = 0; i < root->n_kids; i++) {
    IrNode *c = root->kids[i];
    if (!c) continue;
    if (c->kind == IR_COMPONENT) {
      if (n_comp < SV_MAX_UNITS) raw_comps[n_comp++] = c;
      else p->truncated = 1;
    } else if (c->kind == IR_LAYOUT) {
      if (n_layout < 16) raw_layouts[n_layout++] = c;
      else p->truncated = 1;
    } else if (c->kind == IR_ROUTE) {
      if (p->n_routes < SV_MAX_ROUTES) p->routes[p->n_routes++] = c;
      else p->truncated = 1;
    } else if (irw_hook_is(c, "lazy") && c->value) {
      if (p->n_lazy_routes < 16) {
        snprintf(p->lazy_route_names[p->n_lazy_routes],
                 sizeof(p->lazy_route_names[0]), "%s", c->value);
        p->n_lazy_routes++;
      } else {
        p->truncated = 1;
      }
    } else if (irw_hook_is(c, "context")) {
      svelte_project_add_context_ir(p, c);
    } else if (c->kind == IR_FOREIGN) {
      if (p->n_foreigns < 32) p->foreigns[p->n_foreigns++] = c;
      else p->truncated = 1;
    }
  }

  for (int r = 0; r < p->n_routes; r++) {
    if (!svelte_route_has_lazy_attr_ir(p->routes[r]) || !p->routes[r]->value) continue;
    if (svelte_project_is_lazy_route_name(p, p->routes[r]->value)) continue;
    if (p->n_lazy_routes < 16) {
      snprintf(p->lazy_route_names[p->n_lazy_routes],
               sizeof(p->lazy_route_names[0]), "%s", p->routes[r]->value);
      p->n_lazy_routes++;
    } else {
      p->truncated = 1;
    }
  }
  p->has_router = (p->n_routes > 0 || n_layout > 0);

  for (int i = 0; i < n_comp; i++) {
    if (p->n_units >= SV_MAX_UNITS) {
      p->truncated = 1;
      break;
    }
    SvelteUnit *u = &p->units[p->n_units++];
    memset(u, 0, sizeof(*u));
    snprintf(u->name, sizeof(u->name), "%s",
             raw_comps[i]->name ? raw_comps[i]->name : "Component");
    u->ir_def = raw_comps[i];
    if (svelte_is_route_target_ir(u->name, p->routes, p->n_routes) ||
        svelte_name_ends_with(u->name, "Page")) {
      u->kind = SK_PAGE;
      snprintf(u->dir, sizeof(u->dir), "pages");
    } else {
      u->kind = SK_COMPONENT;
      snprintf(u->dir, sizeof(u->dir), "components");
    }
    {
      char relbuf[128];
      snprintf(relbuf, sizeof(relbuf), "%s/%s.svelte", u->dir, u->name);
      snprintf(u->rel, sizeof(u->rel), "%s", relbuf);
    }
    cord_guess_source_path(u->name, u->dir, u->source_path,
                           sizeof(u->source_path));
    svelte_scan_deps_ir(u->ir_def, u);
  }

  for (int i = 0; i < n_layout; i++) {
    if (p->n_units >= SV_MAX_UNITS) {
      p->truncated = 1;
      break;
    }
    SvelteUnit *u = &p->units[p->n_units++];
    memset(u, 0, sizeof(*u));
    svelte_layout_name(raw_layouts[i]->name, u->name, sizeof(u->name));
    u->ir_def = raw_layouts[i];
    u->kind = SK_LAYOUT;
    snprintf(u->dir, sizeof(u->dir), "layouts");
    {
      char relbuf[128];
      snprintf(relbuf, sizeof(relbuf), "layouts/%s.svelte", u->name);
      snprintf(u->rel, sizeof(u->rel), "%s", relbuf);
    }
    cord_guess_source_path(u->name, u->dir, u->source_path,
                           sizeof(u->source_path));
    u->use_slot = 1;
    p->has_router = 1;
    svelte_scan_deps_ir(u->ir_def, u);
  }

  if (p->truncated) {
    fprintf(stderr,
            "error: project exceeds Svelte backend limits "
            "(max %d units, %d routes) — split the app or raise limits\n",
            SV_MAX_UNITS, SV_MAX_ROUTES);
  }
}
