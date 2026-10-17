#include "adapters/outbound/backends/react/react_ir_internal.h"

void react_ir_gen_component_derived_hooks_ir(StrBuf *sb, IrNode *def, GenCtx *ctx) {
  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (c->kind == IR_COMPUTED && c->name) {
      react_ir_sb_appendf(sb, "  const %s = useMemo(() => (%s), []);\n", c->name,
                 c->value ? c->value : "null");
      ctx->use_memo = 1;
    }
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (!react_ir_ir_hook_is(c, "callback") || !c->value) continue;
    const char *deps = "";
    for (size_t j = 0; j < c->n_kids; j++) {
      if (c->kids[j]->kind == IR_ATTR && c->kids[j]->name &&
          strcmp(c->kids[j]->name, "deps") == 0 && c->kids[j]->value)
        deps = c->kids[j]->value;
    }
    const char *fn = c->value2 ? c->value2 : "() => {}";
    react_ir_sb_appendf(sb, "  const %s = useCallback(%s, [%s]);\n", c->value, fn, deps);
    ctx->use_callback = 1;
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (c->kind != IR_EFFECT) continue;
    const char *deps = c->value ? c->value : "";
    const char *body = c->value2 ? c->value2 : "";
    const char *cleanup = NULL;
    for (size_t j = 0; j < c->n_kids; j++) {
      if (c->kids[j]->kind == IR_ATTR && c->kids[j]->name &&
          strcmp(c->kids[j]->name, "cleanup") == 0)
        cleanup = c->kids[j]->value;
    }
    const char *hook = "useEffect";
    if (c->name && strcmp(c->name, "layoutEffect") == 0)
      hook = "useLayoutEffect";
    else if (c->name && strcmp(c->name, "insertionEffect") == 0)
      hook = "useInsertionEffect";
    react_ir_sb_appendf(sb, "  %s(() => {\n", hook);
    if (body && body[0]) react_ir_sb_appendf(sb, "    %s;\n", body);
    if (cleanup && cleanup[0]) {
      react_ir_sb_append(sb, "    return () => {\n");
      react_ir_sb_appendf(sb, "      %s;\n", cleanup);
      react_ir_sb_append(sb, "    };\n");
    }
    react_ir_sb_appendf(sb, "  }, [%s]);\n", deps);
    if (c->name && strcmp(c->name, "layoutEffect") == 0)
      ctx->use_layout_effect = 1;
    else if (c->name && strcmp(c->name, "insertionEffect") == 0)
      ctx->use_insertion_effect = 1;
    else
      ctx->use_effect = 1;
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (!react_ir_ir_hook_is(c, "effectEvent") || !c->value) continue;
    const char *fn = c->value2 ? c->value2 : "() => {}";
    int is_fn = (fn[0] == '(' || strncmp(fn, "function", 8) == 0 ||
                 strstr(fn, "=>") != NULL);
    if (is_fn) {
      react_ir_sb_appendf(sb, "  const %s = useEffectEvent(%s);\n", c->value, fn);
    } else {
      int simple_id = 1;
      for (const char *p = fn; *p; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') || *p == '_' || *p == '$')) {
          simple_id = 0;
          break;
        }
      }
      if (simple_id)
        react_ir_sb_appendf(sb, "  const %s = useEffectEvent(%s);\n", c->value, fn);
      else
        react_ir_sb_appendf(sb, "  const %s = useEffectEvent((..._args) => { %s; });\n",
                   c->value, fn);
    }
    ctx->use_effect_event = 1;
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (!react_ir_ir_hook_is(c, "externalStore") || !c->value) continue;
    const char *sub = c->value2 ? c->value2 : "() => () => {}";
    const char *get_snap = "() => null";
    const char *get_server = NULL;
    for (size_t j = 0; j < c->n_kids; j++) {
      if (c->kids[j]->kind != IR_ATTR || !c->kids[j]->name) continue;
      if (strcmp(c->kids[j]->name, "getSnapshot") == 0 && c->kids[j]->value)
        get_snap = c->kids[j]->value;
      if (strcmp(c->kids[j]->name, "getServerSnapshot") == 0 &&
          c->kids[j]->value)
        get_server = c->kids[j]->value;
    }
    if (get_server && get_server[0] && strcmp(get_server, "null") != 0)
      react_ir_sb_appendf(sb, "  const %s = useSyncExternalStore(%s, %s, %s);\n",
                 c->value, sub, get_snap, get_server);
    else
      react_ir_sb_appendf(sb, "  const %s = useSyncExternalStore(%s, %s);\n", c->value,
                 sub, get_snap);
    ctx->use_sync_external_store = 1;
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (!react_ir_ir_hook_is(c, "imperativeHandle")) continue;
    const char *refn = c->value ? c->value : "ref";
    const char *body = c->value2 ? c->value2 : "";
    const char *deps = "";
    for (size_t j = 0; j < c->n_kids; j++) {
      if (c->kids[j]->kind == IR_ATTR && c->kids[j]->name &&
          strcmp(c->kids[j]->name, "deps") == 0 && c->kids[j]->value)
        deps = c->kids[j]->value;
      if (c->kids[j]->kind == IR_ATTR && c->kids[j]->name &&
          strcmp(c->kids[j]->name, "ref") == 0 && c->kids[j]->value)
        refn = c->kids[j]->value;
    }
    react_ir_sb_appendf(sb, "  useImperativeHandle(%s, () => ({\n", refn);
    if (body && body[0]) react_ir_sb_appendf(sb, "    %s\n", body);
    react_ir_sb_appendf(sb, "  }), [%s]);\n", deps);
    ctx->use_imperative_handle = 1;
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (!react_ir_ir_hook_is(c, "action") || !c->value) continue;
    const char *init = "null";
    const char *pending = NULL;
    for (size_t j = 0; j < c->n_kids; j++) {
      if (c->kids[j]->kind != IR_ATTR || !c->kids[j]->name) continue;
      if (strcmp(c->kids[j]->name, "init") == 0 && c->kids[j]->value)
        init = c->kids[j]->value;
      if (strcmp(c->kids[j]->name, "pending") == 0 && c->kids[j]->value)
        pending = c->kids[j]->value;
    }
    char action_name[128], pending_name[128];
    snprintf(action_name, sizeof(action_name), "%sAction", c->value);
    if (pending)
      snprintf(pending_name, sizeof(pending_name), "%s", pending);
    else
      snprintf(pending_name, sizeof(pending_name), "%sPending", c->value);
    react_ir_sb_appendf(sb, "  const [%s, %s, %s] = useActionState(%s, %s);\n", c->value,
               action_name, pending_name,
               c->value2 ? c->value2 : "async () => null", init);
    ctx->use_action = 1;
    if (!ctx->action_pending[0]) {
      snprintf(ctx->action_pending, sizeof(ctx->action_pending), "%s",
               pending_name);
      snprintf(ctx->action_handler, sizeof(ctx->action_handler), "%s",
               action_name);
      if (c->value2)
        snprintf(ctx->action_fn, sizeof(ctx->action_fn), "%s", c->value2);
    }
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (c->kind != IR_FETCH || !c->name) continue;
    const char *nm = c->name;
    const char *url = c->value ? c->value : "/";
    char set_n[128], set_err[128], set_load[128];
    snprintf(set_n, sizeof(set_n), "set%c%s",
             (char)toupper((unsigned char)nm[0]), nm + 1);
    snprintf(set_err, sizeof(set_err), "set%c%sError",
             (char)toupper((unsigned char)nm[0]), nm + 1);
    snprintf(set_load, sizeof(set_load), "set%c%sLoading",
             (char)toupper((unsigned char)nm[0]), nm + 1);
    react_ir_sb_appendf(sb, "  const [%s, %s] = useState(null);\n", nm, set_n);
    react_ir_sb_appendf(sb, "  const [%sError, %s] = useState(null);\n", nm, set_err);
    react_ir_sb_appendf(sb, "  const [%sLoading, %s] = useState(true);\n", nm, set_load);
    react_ir_sb_append(sb, "  useEffect(() => {\n");
    react_ir_sb_append(sb, "    let cancelled = false;\n");
    react_ir_sb_appendf(sb, "    %s(true);\n", set_load);
    react_ir_sb_appendf(sb, "    %s(null);\n", set_err);
    {
      const char *safe_url = url_href_is_safe(url) ? url : "/";
      char *esc = js_escape_dq_dup(safe_url);
      react_ir_sb_appendf(sb, "    fetch(\"%s\")\n", esc ? esc : "/");
      free(esc);
    }
    react_ir_sb_append(sb, "      .then((r) => {\n");
    react_ir_sb_append(sb, "        if (!r.ok) throw new Error(String(r.status));\n");
    react_ir_sb_append(sb, "        return r.json();\n");
    react_ir_sb_append(sb, "      })\n");
    react_ir_sb_appendf(sb, "      .then((data) => { if (!cancelled) %s(data); })\n",
               set_n);
    react_ir_sb_appendf(sb, "      .catch((err) => { if (!cancelled) %s(err); })\n",
               set_err);
    react_ir_sb_appendf(sb, "      .finally(() => { if (!cancelled) %s(false); });\n",
               set_load);
    react_ir_sb_append(sb, "    return () => { cancelled = true; };\n");
    react_ir_sb_append(sb, "  }, []);\n");
    ctx->use_state = 1;
    ctx->use_effect = 1;
  }

  {
    char binds[32][64];
    int n_binds = 0;
    react_ir_collect_bind_names_ir(def, binds, &n_binds, 32);
    for (int bi = 0; bi < n_binds; bi++) {
      if (react_ir_state_name_exists_ir(def, binds[bi])) continue;
      char setter[128];
      snprintf(setter, sizeof(setter), "set%c%s",
               (char)toupper((unsigned char)binds[bi][0]), binds[bi] + 1);
      react_ir_sb_appendf(sb, "  const [%s, %s] = useState(\"\");\n", binds[bi], setter);
      ctx->use_state = 1;
    }
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (!react_ir_ir_hook_is(c, "head") || !c->value) continue;
    react_ir_sb_append(sb, "  useEffect(() => {\n");
    react_ir_sb_appendf(sb, "    document.title = \"%s\";\n", c->value);
    react_ir_sb_append(sb, "  }, []);\n");
    ctx->use_effect = 1;
  }
}
