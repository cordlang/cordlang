#include "adapters/outbound/backends/solid/solid_ir_internal.h"

char *solid_ir_gen_contexts_module_ir(SolidProject *proj) {
  if (proj->n_contexts == 0) return NULL;
  StrBuf out;
  solid_ir_sb_init(&out);
  solid_ir_sb_append(&out, "import { createContext } from 'solid-js';\n\n");
  for (int i = 0; i < proj->n_contexts; i++) {
    IrNode *c = proj->contexts[i];
    const char *name = c->value ? c->value : "AppContext";
    solid_ir_sb_appendf(&out, "export const %s = createContext(", name);
    if (c->value2)
      solid_ir_emit_js_literal(&out, c->value2);
    else
      solid_ir_sb_append(&out, "null");
    solid_ir_sb_append(&out, ");\n");
  }
  return out.buf;
}

const char *solid_ir_route_layout_attr_ir(IrNode *route) {
  if (!route) return NULL;
  for (size_t i = 0; i < route->n_kids; i++) {
    IrNode *c = route->kids[i];
    if (c && c->kind == IR_ATTR && c->name && strcmp(c->name, "layout") == 0 &&
        c->value && c->value[0])
      return c->value;
  }
  return NULL;
}

SolidUnit *solid_ir_find_layout_for_route_ir(SolidProject *proj, IrNode *route) {
  const char *attr = solid_ir_route_layout_attr_ir(route);
  char export_name[96];

  if (attr) {
    solid_ir_layout_export_name(attr, export_name, sizeof(export_name));
    for (int i = 0; i < proj->n_units; i++) {
      if (proj->units[i].kind != RK_LAYOUT) continue;
      if (strcmp(proj->units[i].name, export_name) == 0)
        return &proj->units[i];
      if (proj->units[i].layout_src[0] &&
          strcmp(proj->units[i].layout_src, attr) == 0)
        return &proj->units[i];
    }
    return NULL;
  }

  SolidUnit *first = NULL;
  for (int i = 0; i < proj->n_units; i++) {
    if (proj->units[i].kind != RK_LAYOUT) continue;
    if (!first) first = &proj->units[i];
    if (strcmp(proj->units[i].name, "DefaultLayout") == 0)
      return &proj->units[i];
    if (strcmp(proj->units[i].layout_src, "default") == 0)
      return &proj->units[i];
  }
  return first;
}

char *solid_ir_gen_app_module_ir(SolidProject *proj) {
  StrBuf out;
  solid_ir_sb_init(&out);
  solid_ir_sb_append(&out, "/* cordlang: source=src/app.cord */\n");

  int imported[256];
  memset(imported, 0, sizeof(imported));

  int any_lazy_route = 0;
  for (int r = 0; r < proj->n_routes; r++) {
    if (solid_ir_route_is_lazy_ir(proj, proj->routes[r])) {
      any_lazy_route = 1;
      break;
    }
  }

  if (proj->n_routes > 0 || proj->has_router) {
    solid_ir_sb_append(&out,
              "import { Router, Route } from '@solidjs/router';\n");
    if (any_lazy_route)
      solid_ir_sb_append(&out, "import { lazy, Suspense } from 'solid-js';\n");

    for (int i = 0; i < proj->n_units; i++) {
      if (proj->units[i].kind == RK_LAYOUT) {
        solid_ir_sb_appendf(&out, "import %s from './layouts/%s';\n",
                   proj->units[i].name, proj->units[i].name);
        imported[i] = 1;
      }
    }

    for (int r = 0; r < proj->n_routes; r++) {
      const char *comp = proj->routes[r]->value;
      if (!comp) continue;
      if (solid_ir_route_is_lazy_ir(proj, proj->routes[r])) continue;
      for (int i = 0; i < proj->n_units; i++) {
        if (imported[i]) continue;
        if (strcmp(proj->units[i].name, comp) == 0) {
          solid_ir_sb_appendf(&out, "import %s from './%s/%s';\n", proj->units[i].name,
                     proj->units[i].dir, proj->units[i].name);
          imported[i] = 1;
          break;
        }
      }
    }

    for (int r = 0; r < proj->n_routes; r++) {
      const char *comp = proj->routes[r]->value;
      if (!comp || !solid_ir_route_is_lazy_ir(proj, proj->routes[r])) continue;
      int already = 0;
      for (int i = 0; i < proj->n_units; i++) {
        if (imported[i] && strcmp(proj->units[i].name, comp) == 0) already = 1;
      }
      for (int prev = 0; prev < r; prev++) {
        if (proj->routes[prev]->value &&
            strcmp(proj->routes[prev]->value, comp) == 0 &&
            solid_ir_route_is_lazy_ir(proj, proj->routes[prev]))
          already = 1;
      }
      if (already) continue;
      const char *dir = "pages";
      for (int i = 0; i < proj->n_units; i++) {
        if (strcmp(proj->units[i].name, comp) == 0) {
          dir = proj->units[i].dir;
          imported[i] = 1;
          break;
        }
      }
      solid_ir_sb_appendf(&out, "const %s = lazy(() => import('./%s/%s'));\n", comp, dir,
                 comp);
    }
  } else if (proj->n_units > 0) {
    SolidUnit *root = NULL;
    for (int i = 0; i < proj->n_units; i++) {
      if (proj->units[i].kind == RK_PAGE) {
        root = &proj->units[i];
        break;
      }
    }
    if (!root) root = &proj->units[0];
    solid_ir_sb_appendf(&out, "import %s from './%s/%s';\n", root->name, root->dir,
               root->name);
  }

  solid_ir_sb_append(&out, "\nexport default function App() {\n");

  if (proj->n_routes > 0) {
    solid_ir_sb_append(&out, "  return (\n");
    solid_ir_sb_append(&out, "    <Router>\n");
    if (any_lazy_route)
      solid_ir_sb_append(&out,
                "      <Suspense fallback={<div>Loading...</div>}>\n");
    solid_ir_sb_append(&out, "");

    int route_done[SOLID_MAX_ROUTES];
    memset(route_done, 0, sizeof(route_done));

    for (int r = 0; r < proj->n_routes; r++) {
      if (route_done[r]) continue;
      SolidUnit *layout = solid_ir_find_layout_for_route_ir(proj, proj->routes[r]);
      if (layout) {
        solid_ir_sb_appendf(&out, "        <Route path=\"/\" component={%s}>\n",
                   layout->name);
        for (int r2 = 0; r2 < proj->n_routes; r2++) {
          if (route_done[r2]) continue;
          if (solid_ir_find_layout_for_route_ir(proj, proj->routes[r2]) != layout)
            continue;
          const char *path =
              proj->routes[r2]->name ? proj->routes[r2]->name : "/";
          const char *comp =
              proj->routes[r2]->value ? proj->routes[r2]->value : "Home";
          solid_ir_sb_appendf(&out, "          <Route path=\"%s\" component={%s} />\n",
                     path, comp);
          route_done[r2] = 1;
        }
        solid_ir_sb_append(&out, "        </Route>\n");
      } else {
        const char *path =
            proj->routes[r]->name ? proj->routes[r]->name : "/";
        const char *comp =
            proj->routes[r]->value ? proj->routes[r]->value : "Home";
        solid_ir_sb_appendf(&out, "        <Route path=\"%s\" component={%s} />\n",
                   path, comp);
        route_done[r] = 1;
      }
    }

    solid_ir_sb_append(&out, "");
    if (any_lazy_route) solid_ir_sb_append(&out, "      </Suspense>\n");
    solid_ir_sb_append(&out, "    </Router>\n");
    solid_ir_sb_append(&out, "  );\n");
  } else if (proj->n_page_nodes > 0 && proj->n_units == 0) {
    GenCtx ctx = {0};
    solid_ir_sb_append(&out, "  return (\n");
    if (proj->n_page_nodes > 1) solid_ir_sb_append(&out, "    <>\n");
    for (int i = 0; i < proj->n_page_nodes; i++) {
      solid_ir_gen_ir_node(&out, proj->page_nodes[i],
                  proj->n_page_nodes > 1 ? 3 : 2, &ctx);
    }
    if (proj->n_page_nodes > 1) solid_ir_sb_append(&out, "    </>\n");
    solid_ir_sb_append(&out, "  );\n");
  } else if (proj->n_units > 0) {
    SolidUnit *root = NULL;
    for (int i = 0; i < proj->n_units; i++) {
      if (proj->units[i].kind == RK_PAGE) {
        root = &proj->units[i];
        break;
      }
    }
    if (!root) root = &proj->units[0];
    solid_ir_sb_appendf(&out, "  return <%s />;\n", root->name);
  } else {
    solid_ir_sb_append(&out, "  return null;\n");
  }

  solid_ir_sb_append(&out, "}\n");
  return out.buf;
}
