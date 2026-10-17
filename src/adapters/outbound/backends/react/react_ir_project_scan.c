#include "adapters/outbound/backends/react/react_ir_internal.h"

int react_ir_ir_hook_is(const IrNode *n, const char *kind) {
  return n && n->kind == IR_HOOK && n->name && kind && strcmp(n->name, kind) == 0;
}

int react_ir_is_decl_ir(const IrNode *n) {
  if (!n) return 0;
  if (n->kind == IR_PROP || n->kind == IR_STATE || n->kind == IR_COMPUTED ||
      n->kind == IR_EFFECT || n->kind == IR_FETCH || n->kind == IR_MODULE_USE)
    return 1;
  if (n->kind == IR_HOOK) {
    /* body-emitting hooks */
    if (react_ir_ir_hook_is(n, "portal") || react_ir_ir_hook_is(n, "errorBoundary") ||
        react_ir_ir_hook_is(n, "suspense") || react_ir_ir_hook_is(n, "loading") ||
        react_ir_ir_hook_is(n, "empty"))
      return 0;
    return 1; /* ref, ctx, action, head, lazy, … */
  }
  if (n->kind == IR_ATTR && n->name &&
      (strcmp(n->name, "forwardRef") == 0 || strcmp(n->name, "__file__") == 0))
    return 1;
  return 0;
}

int react_ir_route_has_lazy_attr_ir(IrNode *route) {
  return irw_route_has_lazy(route);
}

int react_ir_project_is_lazy_route_name(ReactProject *proj, const char *name) {
  if (!proj || !name) return 0;
  for (int i = 0; i < proj->n_lazy_routes; i++) {
    if (strcmp(proj->lazy_route_names[i], name) == 0) return 1;
  }
  return 0;
}

int react_ir_route_is_lazy_ir(ReactProject *proj, IrNode *route) {
  if (!route) return 0;
  if (react_ir_route_has_lazy_attr_ir(route)) return 1;
  if (route->value && react_ir_project_is_lazy_route_name(proj, route->value)) return 1;
  return 0;
}

void react_ir_layout_export_name(const char *orig, char *out, size_t n) {
  if (!orig || !*orig || strcmp(orig, "default") == 0) {
    snprintf(out, n, "DefaultLayout");
    return;
  }
  if (react_ir_is_pascal_case(orig)) {
    snprintf(out, n, "%s", orig);
    return;
  }
  snprintf(out, n, "%c%s", (char)toupper((unsigned char)orig[0]), orig + 1);
}

int react_ir_name_ends_with(const char *s, const char *suffix) {
  size_t ls = s ? strlen(s) : 0, lf = strlen(suffix);
  return ls >= lf && strcmp(s + ls - lf, suffix) == 0;
}

int react_ir_is_route_target_ir(const char *name, IrNode **routes, int n_routes) {
  if (!name) return 0;
  for (int i = 0; i < n_routes; i++) {
    if (routes[i]->value && strcmp(routes[i]->value, name) == 0) return 1;
  }
  return 0;
}

void react_ir_unit_add_used(ReactUnit *u, const char *name) {
  if (!name || !react_ir_is_pascal_case(name)) return;
  if (strcmp(name, u->name) == 0) return;
  for (int i = 0; i < u->n_used; i++) {
    if (strcmp(u->used[i], name) == 0) return;
  }
  if (u->n_used < REACT_MAX_USED) {
    snprintf(u->used[u->n_used], sizeof(u->used[0]), "%s", name);
    u->n_used++;
  }
}

void react_ir_scan_tree_deps_ir(IrNode *n, ReactUnit *u) {
  if (!n) return;
  if (n->kind == IR_SLOT) {
    if (u->kind == RK_LAYOUT) u->use_outlet = 1;
    else u->use_children = 1;
  }
  if (n->kind == IR_ELEMENT && n->name) {
    if (react_ir_is_pascal_case(n->name)) react_ir_unit_add_used(u, n->name);
    if (strcmp(n->name, "link") == 0) u->use_link = 1;
    if (strcmp(n->name, "provide") == 0) u->use_context = 1;
    if (strcmp(n->name, "slot") == 0) {
      if (u->kind == RK_LAYOUT) u->use_outlet = 1;
      else u->use_children = 1;
    }
  }
  if (n->kind == IR_STATE) u->use_state = 1;
  if (n->kind == IR_COMPUTED) u->use_memo = 1;
  if (n->kind == IR_EFFECT) {
    if (n->name && strcmp(n->name, "layoutEffect") == 0)
      u->use_layout_effect = 1;
    else if (n->name && strcmp(n->name, "insertionEffect") == 0)
      u->use_insertion_effect = 1;
    else
      u->use_effect = 1;
  }
  if (n->kind == IR_FETCH) {
    u->use_state = 1;
    u->use_effect = 1;
  }
  if (n->kind == IR_FOR) u->use_fragment = 1;
  if (n->kind == IR_HOOK && n->name) {
    if (strcmp(n->name, "ref") == 0) u->use_ref = 1;
    else if (strcmp(n->name, "ctx") == 0) u->use_context = 1;
    else if (strcmp(n->name, "context") == 0) {
      u->use_context = 1;
      if (n->value && u->n_ctx < 16) {
        snprintf(u->ctx_names[u->n_ctx], sizeof(u->ctx_names[0]), "%s",
                 n->value);
        u->n_ctx++;
      }
    } else if (strcmp(n->name, "reducer") == 0)
      u->use_reducer = 1;
    else if (strcmp(n->name, "params") == 0)
      u->use_params = 1;
    else if (strcmp(n->name, "navigate") == 0)
      u->use_navigate = 1;
    else if (strcmp(n->name, "callback") == 0)
      u->use_callback = 1;
    else if (strcmp(n->name, "id") == 0)
      u->use_id = 1;
    else if (strcmp(n->name, "transition") == 0)
      u->use_transition = 1;
    else if (strcmp(n->name, "deferred") == 0)
      u->use_deferred = 1;
    else if (strcmp(n->name, "action") == 0) {
      u->use_action = 1;
      if (n->value2 && n->value2[0]) {
        int simple = 1;
        for (const char *p = n->value2; *p; p++) {
          if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                (*p >= '0' && *p <= '9') || *p == '_' || *p == '$')) {
            simple = 0;
            break;
          }
        }
        if (simple && u->n_action_stubs < 8) {
          int found = 0;
          for (int i = 0; i < u->n_action_stubs; i++) {
            if (strcmp(u->action_stubs[i], n->value2) == 0) {
              found = 1;
              break;
            }
          }
          if (!found) {
            snprintf(u->action_stubs[u->n_action_stubs],
                     sizeof(u->action_stubs[0]), "%s", n->value2);
            u->n_action_stubs++;
          }
        }
      }
    } else if (strcmp(n->name, "lazy") == 0) {
      u->use_lazy = 1;
      if (n->value && n->value2 && u->n_lazy < 16) {
        snprintf(u->lazy_names[u->n_lazy], sizeof(u->lazy_names[0]), "%s",
                 n->value);
        snprintf(u->lazy_paths[u->n_lazy], sizeof(u->lazy_paths[0]), "%s",
                 n->value2);
        u->n_lazy++;
      }
    } else if (strcmp(n->name, "effectEvent") == 0)
      u->use_effect_event = 1;
    else if (strcmp(n->name, "externalStore") == 0)
      u->use_sync_external_store = 1;
    else if (strcmp(n->name, "imperativeHandle") == 0)
      u->use_imperative_handle = 1;
    else if (strcmp(n->name, "head") == 0)
      u->use_effect = 1;
    else if (strcmp(n->name, "portal") == 0)
      u->use_portal = 1;
    else if (strcmp(n->name, "errorBoundary") == 0)
      u->use_error_boundary = 1;
    else if (strcmp(n->name, "suspense") == 0 || strcmp(n->name, "loading") == 0)
      u->use_suspense = 1;
  }
  if (n->kind == IR_ATTR && n->name && strcmp(n->name, "forwardRef") == 0 &&
      react_ir_attr_is_true(n->value))
    u->use_forward_ref = 1;

  for (size_t i = 0; i < n->n_kids; i++) react_ir_scan_tree_deps_ir(n->kids[i], u);
}
