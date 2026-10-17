#include "adapters/outbound/backends/react/react_ir_internal.h"

ReactUnit *react_ir_project_find_unit(ReactProject *p, const char *name) {
  if (!name) return NULL;
  for (int i = 0; i < p->n_units; i++) {
    if (strcmp(p->units[i].name, name) == 0) return &p->units[i];
  }
  return NULL;
}

void react_ir_project_partition_from_ir(ReactProject *p, IrProgram *ir) {
  memset(p, 0, sizeof(*p));
  if (!ir || !ir->root) return;
  IrNode *root = ir->root;

  IrNode *raw_comps[REACT_MAX_UNITS];
  IrNode *raw_layouts[16];
  int n_comp = 0, n_layout = 0;

  for (size_t i = 0; i < root->n_kids; i++) {
    IrNode *c = root->kids[i];
    if (!c) continue;
    if (c->kind == IR_COMPONENT) {
      if (n_comp < REACT_MAX_UNITS) raw_comps[n_comp++] = c;
      else p->truncated = 1;
    } else if (c->kind == IR_LAYOUT) {
      if (n_layout < 16) raw_layouts[n_layout++] = c;
      else p->truncated = 1;
    } else if (c->kind == IR_ROUTE) {
      if (p->n_routes < REACT_MAX_ROUTES) p->routes[p->n_routes++] = c;
      else p->truncated = 1;
    } else if (react_ir_ir_hook_is(c, "lazy") && c->value) {
      if (p->n_lazy_routes < 16) {
        snprintf(p->lazy_route_names[p->n_lazy_routes],
                 sizeof(p->lazy_route_names[0]), "%s", c->value);
        p->n_lazy_routes++;
      } else {
        p->truncated = 1;
      }
    } else if (react_ir_ir_hook_is(c, "context")) {
      if (p->n_contexts < 32) p->contexts[p->n_contexts++] = c;
      else p->truncated = 1;
    } else if (c->kind == IR_FOREIGN) {
      if (p->n_foreigns < 32) p->foreigns[p->n_foreigns++] = c;
      else p->truncated = 1;
    } else if (!react_ir_ir_hook_is(c, "theme")) {
      if (p->n_page_nodes < 64) p->page_nodes[p->n_page_nodes++] = c;
      else p->truncated = 1;
    }
  }

  for (int r = 0; r < p->n_routes; r++) {
    if (!react_ir_route_has_lazy_attr_ir(p->routes[r]) || !p->routes[r]->value) continue;
    if (react_ir_project_is_lazy_route_name(p, p->routes[r]->value)) continue;
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
    if (p->n_units >= REACT_MAX_UNITS) {
      p->truncated = 1;
      break;
    }
    ReactUnit *u = &p->units[p->n_units++];
    memset(u, 0, sizeof(*u));
    snprintf(u->name, sizeof(u->name), "%s",
             raw_comps[i]->name ? raw_comps[i]->name : "Component");
    u->ir_def = raw_comps[i];
    if (react_ir_is_route_target_ir(u->name, p->routes, p->n_routes) ||
        react_ir_name_ends_with(u->name, "Page")) {
      u->kind = RK_PAGE;
      snprintf(u->dir, sizeof(u->dir), "%s", "pages");
    } else {
      u->kind = RK_COMPONENT;
      snprintf(u->dir, sizeof(u->dir), "%s", "components");
    }
    {
      char relbuf[128];
      snprintf(relbuf, sizeof(relbuf), "%s/%s.jsx", u->dir, u->name);
      snprintf(u->rel, sizeof(u->rel), "%s", relbuf);
    }
    cord_guess_source_path(u->name, u->dir, u->source_path,
                           sizeof(u->source_path));
    react_ir_scan_tree_deps_ir(u->ir_def, u);
  }

  for (int i = 0; i < n_layout; i++) {
    if (p->n_units >= REACT_MAX_UNITS) {
      p->truncated = 1;
      break;
    }
    ReactUnit *u = &p->units[p->n_units++];
    memset(u, 0, sizeof(*u));
    react_ir_layout_export_name(raw_layouts[i]->name, u->name, sizeof(u->name));
    snprintf(u->layout_src, sizeof(u->layout_src), "%s",
             raw_layouts[i]->name ? raw_layouts[i]->name : "default");
    u->ir_def = raw_layouts[i];
    u->kind = RK_LAYOUT;
    snprintf(u->dir, sizeof(u->dir), "%s", "layouts");
    {
      char relbuf[128];
      snprintf(relbuf, sizeof(relbuf), "layouts/%s.jsx", u->name);
      snprintf(u->rel, sizeof(u->rel), "%s", relbuf);
    }
    cord_guess_source_path(u->name, u->dir, u->source_path,
                           sizeof(u->source_path));
    u->use_outlet = 1;
    p->has_router = 1;
    react_ir_scan_tree_deps_ir(u->ir_def, u);
  }

  if (p->truncated) {
    fprintf(stderr,
            "error: project exceeds React backend limits "
            "(max %d units, %d routes) — split the app or raise limits\n",
            REACT_MAX_UNITS, REACT_MAX_ROUTES);
  }
}

void react_ir_import_path_between(const ReactUnit *from, const ReactUnit *to,
                                char *out, size_t n) {
  if (strcmp(from->dir, to->dir) == 0)
    snprintf(out, n, "./%s", to->name);
  else
    snprintf(out, n, "../%s/%s", to->dir, to->name);
}

void react_ir_collect_bind_names_ir(IrNode *n, char names[][64], int *count,
                                  int max) {
  if (!n || *count >= max) return;
  if (n->kind == IR_ATTR && n->name && strcmp(n->name, "bind") == 0 &&
      n->value && !strchr(n->value, '.')) {
    int found = 0;
    for (int i = 0; i < *count; i++) {
      if (strcmp(names[i], n->value) == 0) {
        found = 1;
        break;
      }
    }
    if (!found && *count < max) {
      snprintf(names[*count], sizeof(names[0]), "%s", n->value);
      (*count)++;
    }
  }
  for (size_t i = 0; i < n->n_kids; i++)
    react_ir_collect_bind_names_ir(n->kids[i], names, count, max);
}

int react_ir_state_name_exists_ir(IrNode *def, const char *name) {
  if (!def || !name) return 0;
  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (!c) continue;
    if (c->kind == IR_STATE) {
      if (c->name && strcmp(c->name, "__states__") == 0) {
        for (size_t j = 0; j < c->n_kids; j++) {
          if (c->kids[j] && c->kids[j]->name &&
              strcmp(c->kids[j]->name, name) == 0)
            return 1;
        }
      } else if (c->name && strcmp(c->name, name) == 0)
        return 1;
    }
    if (react_ir_ir_hook_is(c, "action") && c->value && strcmp(c->value, name) == 0)
      return 1;
    if (c->kind == IR_FETCH && c->name && strcmp(c->name, name) == 0) return 1;
  }
  return 0;
}
