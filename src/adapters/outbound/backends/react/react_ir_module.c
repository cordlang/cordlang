#include "adapters/outbound/backends/react/react_ir_internal.h"

char *react_ir_gen_unit_module_ir(ReactProject *proj, ReactUnit *u) {
  GenCtx ctx = {0};
  ctx.use_router = proj->has_router || u->use_link || u->use_outlet;
  ctx.in_component = 1;
  ctx.is_layout = (u->kind == RK_LAYOUT);
  ctx.use_children = u->use_children;

  StrBuf body;
  react_ir_sb_init(&body);
  react_ir_gen_component_fn_ir(&body, u->ir_def, u->name, &ctx);

  if (ctx.use_state) u->use_state = 1;
  if (ctx.use_memo) u->use_memo = 1;
  if (ctx.use_effect) u->use_effect = 1;
  if (ctx.use_ref) u->use_ref = 1;
  if (ctx.use_context) u->use_context = 1;
  if (ctx.use_reducer) u->use_reducer = 1;
  if (ctx.use_params) u->use_params = 1;
  if (ctx.use_navigate) u->use_navigate = 1;
  if (ctx.use_callback) u->use_callback = 1;
  if (ctx.use_id) u->use_id = 1;
  if (ctx.use_transition) u->use_transition = 1;
  if (ctx.use_deferred) u->use_deferred = 1;
  if (ctx.use_action) u->use_action = 1;
  if (ctx.use_layout_effect) u->use_layout_effect = 1;
  if (ctx.use_insertion_effect) u->use_insertion_effect = 1;
  if (ctx.use_effect_event) u->use_effect_event = 1;
  if (ctx.use_sync_external_store) u->use_sync_external_store = 1;
  if (ctx.use_imperative_handle) u->use_imperative_handle = 1;
  if (ctx.use_forward_ref) u->use_forward_ref = 1;
  if (ctx.use_suspense) u->use_suspense = 1;
  if (ctx.use_portal) u->use_portal = 1;
  if (ctx.use_error_boundary) u->use_error_boundary = 1;
  if (ctx.use_children) u->use_children = 1;
  if (u->use_link || u->use_outlet || ctx.use_params || ctx.use_navigate)
    ctx.use_router = 1;

  StrBuf out;
  react_ir_sb_init(&out);

  if (u->source_path[0])
    react_ir_sb_appendf(&out, "/* cordlang: source=%s */\n", u->source_path);

  {
    const char *hooks[32];
    int nh = 0;
    if (u->use_state || ctx.use_state) hooks[nh++] = "useState";
    if (u->use_memo || ctx.use_memo) hooks[nh++] = "useMemo";
    if (u->use_effect || ctx.use_effect) hooks[nh++] = "useEffect";
    if (u->use_layout_effect || ctx.use_layout_effect)
      hooks[nh++] = "useLayoutEffect";
    if (u->use_insertion_effect || ctx.use_insertion_effect)
      hooks[nh++] = "useInsertionEffect";
    if (u->use_effect_event || ctx.use_effect_event)
      hooks[nh++] = "useEffectEvent";
    if (u->use_sync_external_store || ctx.use_sync_external_store)
      hooks[nh++] = "useSyncExternalStore";
    if (u->use_ref || ctx.use_ref) hooks[nh++] = "useRef";
    if (u->use_imperative_handle || ctx.use_imperative_handle)
      hooks[nh++] = "useImperativeHandle";
    if (u->use_forward_ref || ctx.use_forward_ref) hooks[nh++] = "forwardRef";
    if (u->use_context || ctx.use_context) hooks[nh++] = "useContext";
    if (u->use_reducer || ctx.use_reducer) hooks[nh++] = "useReducer";
    if (u->use_callback || ctx.use_callback) hooks[nh++] = "useCallback";
    if (u->use_id || ctx.use_id) hooks[nh++] = "useId";
    if (u->use_transition || ctx.use_transition) hooks[nh++] = "useTransition";
    if (u->use_deferred || ctx.use_deferred) hooks[nh++] = "useDeferredValue";
    if (u->use_action || ctx.use_action) hooks[nh++] = "useActionState";
    if (u->use_lazy || u->n_lazy > 0) hooks[nh++] = "lazy";
    if (u->use_suspense || ctx.use_suspense) hooks[nh++] = "Suspense";
    if (u->use_portal || ctx.use_portal) hooks[nh++] = "createPortal";
    if (u->use_fragment) hooks[nh++] = "Fragment";
    if (nh > 0) {
      react_ir_sb_append(&out, "import { ");
      for (int i = 0; i < nh; i++) {
        if (i) react_ir_sb_append(&out, ", ");
        react_ir_sb_append(&out, hooks[i]);
      }
      react_ir_sb_append(&out, " } from 'react';\n");
    }
  }

  if (u->use_error_boundary || ctx.use_error_boundary)
    react_ir_sb_append(&out, "import ErrorBoundary from '../ErrorBoundary';\n");
  if (ctx.use_cord_icon)
    react_ir_sb_append(&out, "import CordIcon from '../CordIcon';\n");
  if (ctx.use_cord_motion)
    react_ir_sb_append(&out, "import CordMotion from '../CordMotion';\n");
  if (ctx.use_cord_chart)
    react_ir_sb_append(&out, "import CordChart from '../CordChart';\n");

  for (int i = 0; i < u->n_lazy; i++) {
    char ipath[256];
    const char *path = u->lazy_paths[i];
    if (strncmp(path, "pages/", 6) == 0)
      snprintf(ipath, sizeof(ipath), "../pages/%s", path + 6);
    else if (strncmp(path, "components/", 11) == 0)
      snprintf(ipath, sizeof(ipath), "../components/%s", path + 11);
    else if (strncmp(path, "layouts/", 8) == 0)
      snprintf(ipath, sizeof(ipath), "../layouts/%s", path + 8);
    else
      snprintf(ipath, sizeof(ipath), "./%s", path);
    size_t pl = strlen(ipath);
    if (pl > 5 && strcmp(ipath + pl - 5, ".cord") == 0) ipath[pl - 5] = '\0';
    react_ir_sb_appendf(&out, "const %s = lazy(() => import('%s'));\n", u->lazy_names[i],
               ipath);
  }

  if (u->use_link || u->use_outlet || ctx.use_params || ctx.use_navigate) {
    react_ir_sb_append(&out, "import { ");
    int first = 1;
    if (u->use_link) {
      react_ir_sb_append(&out, "NavLink");
      first = 0;
    }
    if (u->use_outlet) {
      if (!first) react_ir_sb_append(&out, ", ");
      react_ir_sb_append(&out, "Outlet");
      first = 0;
    }
    if (ctx.use_params || u->use_params) {
      if (!first) react_ir_sb_append(&out, ", ");
      react_ir_sb_append(&out, "useParams");
      first = 0;
    }
    if (ctx.use_navigate || u->use_navigate) {
      if (!first) react_ir_sb_append(&out, ", ");
      react_ir_sb_append(&out, "useNavigate");
    }
    react_ir_sb_append(&out, " } from 'react-router-dom';\n");
  }

  if (proj->n_contexts > 0 && (u->use_context || ctx.use_context)) {
    react_ir_sb_append(&out, "import { ");
    for (int i = 0; i < proj->n_contexts; i++) {
      if (i) react_ir_sb_append(&out, ", ");
      react_ir_sb_append(&out,
                proj->contexts[i]->value ? proj->contexts[i]->value : "Ctx");
    }
    react_ir_sb_append(&out, " } from '../contexts';\n");
  }

  for (int i = 0; i < u->n_used; i++) {
    ReactUnit *dep = react_ir_project_find_unit(proj, u->used[i]);
    if (dep) {
      char ipath[256];
      react_ir_import_path_between(u, dep, ipath, sizeof(ipath));
      react_ir_sb_appendf(&out, "import %s from '%s';\n", dep->name, ipath);
      continue;
    }
    /* foreign multi-target: prefer react binding */
    for (int f = 0; f < proj->n_foreigns; f++) {
      IrNode *fn = proj->foreigns[f];
      if (!fn || !fn->name || strcmp(fn->name, u->used[i]) != 0) continue;
      const char *mod = fn->value; /* default module */
      for (size_t k = 0; k < fn->n_kids; k++) {
        IrNode *a = fn->kids[k];
        if (a && a->kind == IR_ATTR && a->name && a->value &&
            strcmp(a->name, "react") == 0)
          mod = a->value;
      }
      if (mod && mod[0])
        react_ir_sb_appendf(&out, "import %s from '%s';\n", fn->name, mod);
      break;
    }
  }

  if (out.len > 0) react_ir_sb_append(&out, "\n");

  for (int si = 0; si < u->n_action_stubs; si++) {
    react_ir_sb_appendf(&out,
               "/* cordlang: default action stub - replace with your server "
               "action */\n"
               "async function %s(prev, formData) {\n"
               "  return null;\n"
               "}\n\n",
               u->action_stubs[si]);
  }

  if (u->use_forward_ref || ctx.use_forward_ref) {
    react_ir_sb_append(&out, "export default forwardRef(");
    if (body.buf) {
      size_t bl = strlen(body.buf);
      while (bl > 0 &&
             (body.buf[bl - 1] == '\n' || body.buf[bl - 1] == '\r')) {
        body.buf[--bl] = '\0';
      }
      react_ir_sb_append(&out, body.buf);
    } else {
      react_ir_sb_append(&out, "function Empty(props, ref){ return null; }");
    }
    react_ir_sb_append(&out, ");\n");
  } else {
    react_ir_sb_append(&out, "export default ");
    react_ir_sb_append(&out, body.buf ? body.buf : "function Empty(){ return null; }\n");
  }

  free(body.buf);
  return out.buf;
}
