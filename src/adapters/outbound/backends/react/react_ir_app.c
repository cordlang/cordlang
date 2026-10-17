#include "adapters/outbound/backends/react/react_ir_internal.h"

char *react_ir_gen_contexts_module_ir(ReactProject *proj) {
  if (proj->n_contexts == 0) return NULL;
  StrBuf out;
  react_ir_sb_init(&out);
  react_ir_sb_append(&out, "import { createContext } from 'react';\n\n");
  for (int i = 0; i < proj->n_contexts; i++) {
    IrNode *c = proj->contexts[i];
    const char *name = c->value ? c->value : "AppContext";
    react_ir_sb_appendf(&out, "export const %s = createContext(", name);
    if (c->value2)
      react_ir_emit_js_literal(&out, c->value2);
    else
      react_ir_sb_append(&out, "null");
    react_ir_sb_append(&out, ");\n");
  }
  return out.buf;
}

const char *react_ir_route_layout_attr_ir(IrNode *route) {
  if (!route) return NULL;
  for (size_t i = 0; i < route->n_kids; i++) {
    IrNode *c = route->kids[i];
    if (c && c->kind == IR_ATTR && c->name && strcmp(c->name, "layout") == 0 &&
        c->value && c->value[0])
      return c->value;
  }
  return NULL;
}

ReactUnit *react_ir_find_layout_for_route_ir(ReactProject *proj, IrNode *route) {
  const char *attr = react_ir_route_layout_attr_ir(route);
  char export_name[96];

  if (attr) {
    react_ir_layout_export_name(attr, export_name, sizeof(export_name));
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

  ReactUnit *first = NULL;
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

char *react_ir_gen_app_module_ir(ReactProject *proj) {
  StrBuf out;
  react_ir_sb_init(&out);
  react_ir_sb_append(&out, "/* cordlang: source=src/app.cord */\n");

  int imported[256];
  memset(imported, 0, sizeof(imported));

  int any_lazy_route = 0;
  for (int r = 0; r < proj->n_routes; r++) {
    if (react_ir_route_is_lazy_ir(proj, proj->routes[r])) {
      any_lazy_route = 1;
      break;
    }
  }

  if (proj->n_routes > 0 || proj->has_router) {
    react_ir_sb_append(&out,
              "import { BrowserRouter, Routes, Route } from 'react-router-dom';\n");
    if (any_lazy_route)
      react_ir_sb_append(&out, "import { lazy, Suspense } from 'react';\n");

    for (int i = 0; i < proj->n_units; i++) {
      if (proj->units[i].kind == RK_LAYOUT) {
        react_ir_sb_appendf(&out, "import %s from './layouts/%s';\n",
                   proj->units[i].name, proj->units[i].name);
        imported[i] = 1;
      }
    }

    for (int r = 0; r < proj->n_routes; r++) {
      const char *comp = proj->routes[r]->value;
      if (!comp) continue;
      if (react_ir_route_is_lazy_ir(proj, proj->routes[r])) continue;
      for (int i = 0; i < proj->n_units; i++) {
        if (imported[i]) continue;
        if (strcmp(proj->units[i].name, comp) == 0) {
          react_ir_sb_appendf(&out, "import %s from './%s/%s';\n", proj->units[i].name,
                     proj->units[i].dir, proj->units[i].name);
          imported[i] = 1;
          break;
        }
      }
    }

    for (int r = 0; r < proj->n_routes; r++) {
      const char *comp = proj->routes[r]->value;
      if (!comp || !react_ir_route_is_lazy_ir(proj, proj->routes[r])) continue;
      int already = 0;
      for (int i = 0; i < proj->n_units; i++) {
        if (imported[i] && strcmp(proj->units[i].name, comp) == 0) already = 1;
      }
      for (int prev = 0; prev < r; prev++) {
        if (proj->routes[prev]->value &&
            strcmp(proj->routes[prev]->value, comp) == 0 &&
            react_ir_route_is_lazy_ir(proj, proj->routes[prev]))
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
      react_ir_sb_appendf(&out, "const %s = lazy(() => import('./%s/%s'));\n", comp, dir,
                 comp);
    }
  } else if (proj->n_units > 0) {
    ReactUnit *root = NULL;
    for (int i = 0; i < proj->n_units; i++) {
      if (proj->units[i].kind == RK_PAGE) {
        root = &proj->units[i];
        break;
      }
    }
    if (!root) root = &proj->units[0];
    react_ir_sb_appendf(&out, "import %s from './%s/%s';\n", root->name, root->dir,
               root->name);
  }

  react_ir_sb_append(&out, "\nexport default function App() {\n");

  if (proj->n_routes > 0) {
    react_ir_sb_append(&out, "  return (\n");
    react_ir_sb_append(&out, "    <BrowserRouter>\n");
    if (any_lazy_route)
      react_ir_sb_append(&out,
                "      <Suspense fallback={<div>Loading...</div>}>\n");
    react_ir_sb_append(&out, "      <Routes>\n");

    int route_done[REACT_MAX_ROUTES];
    memset(route_done, 0, sizeof(route_done));

    for (int r = 0; r < proj->n_routes; r++) {
      if (route_done[r]) continue;
      ReactUnit *layout = react_ir_find_layout_for_route_ir(proj, proj->routes[r]);
      if (layout) {
        react_ir_sb_appendf(&out, "        <Route element={<%s />}>\n", layout->name);
        for (int r2 = 0; r2 < proj->n_routes; r2++) {
          if (route_done[r2]) continue;
          if (react_ir_find_layout_for_route_ir(proj, proj->routes[r2]) != layout)
            continue;
          const char *path =
              proj->routes[r2]->name ? proj->routes[r2]->name : "/";
          const char *comp =
              proj->routes[r2]->value ? proj->routes[r2]->value : "Home";
          react_ir_sb_appendf(&out, "          <Route path=\"%s\" element={<%s />} />\n",
                     path, comp);
          route_done[r2] = 1;
        }
        react_ir_sb_append(&out, "        </Route>\n");
      } else {
        const char *path =
            proj->routes[r]->name ? proj->routes[r]->name : "/";
        const char *comp =
            proj->routes[r]->value ? proj->routes[r]->value : "Home";
        react_ir_sb_appendf(&out, "        <Route path=\"%s\" element={<%s />} />\n",
                   path, comp);
        route_done[r] = 1;
      }
    }

    react_ir_sb_append(&out, "      </Routes>\n");
    if (any_lazy_route) react_ir_sb_append(&out, "      </Suspense>\n");
    react_ir_sb_append(&out, "    </BrowserRouter>\n");
    react_ir_sb_append(&out, "  );\n");
  } else if (proj->n_page_nodes > 0 && proj->n_units == 0) {
    GenCtx ctx = {0};
    react_ir_sb_append(&out, "  return (\n");
    if (proj->n_page_nodes > 1) react_ir_sb_append(&out, "    <>\n");
    for (int i = 0; i < proj->n_page_nodes; i++) {
      react_ir_gen_ir_node(&out, proj->page_nodes[i],
                  proj->n_page_nodes > 1 ? 3 : 2, &ctx);
    }
    if (proj->n_page_nodes > 1) react_ir_sb_append(&out, "    </>\n");
    react_ir_sb_append(&out, "  );\n");
  } else if (proj->n_units > 0) {
    ReactUnit *root = NULL;
    for (int i = 0; i < proj->n_units; i++) {
      if (proj->units[i].kind == RK_PAGE) {
        root = &proj->units[i];
        break;
      }
    }
    if (!root) root = &proj->units[0];
    react_ir_sb_appendf(&out, "  return <%s />;\n", root->name);
  } else {
    react_ir_sb_append(&out, "  return null;\n");
  }

  react_ir_sb_append(&out, "}\n");
  return out.buf;
}
