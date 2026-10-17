#include "adapters/outbound/backends/vue/vue_ir_internal.h"

void vue_vue_emit_module_state_ir(IrNode *def, StrBuf *script,
                                     const VuModuleNeeds *needs,
                                     const ProvideInfo *provides,
                                     int n_provides) {
  {
    StateEmitCtx sctx = {.script = script};
    vue_foreach_state(def, vue_state_emit_one, &sctx);
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (c->kind == IR_COMPUTED && c->name) {
      vue_sb_appendf(script, "const %s = computed(() => (%s));\n", c->name,
                 c->value ? c->value : "null");
    }
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (irw_hook_is(c, "ref") && c->value)
      vue_sb_appendf(script, "const %s = ref(null);\n", c->value);
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (irw_hook_is(c, "ctx") && c->value && c->value2) {
      vue_sb_appendf(script, "const %s = inject(%s);\n", c->value, c->value2);
    }
  }

  for (int i = 0; i < n_provides; i++) {
    const char *ctx_name =
        provides[i].ctx_name ? provides[i].ctx_name : "Context";
    const char *val = provides[i].val;
    vue_sb_appendf(script, "provide(%s, ", ctx_name);
    if (val) {
      if (irw_looks_number(val) || irw_looks_bool(val) ||
          (vue_looks_like_js_expr(val) && (strchr(val, '.') || strchr(val, '('))))
        vue_sb_append(script, val);
      else if (vue_looks_like_js_expr(val))
        vue_sb_append(script, val);
      else
        vue_emit_js_lit(script, val);
    } else {
      vue_sb_append(script, "undefined");
    }
    vue_sb_append(script, ");\n");
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (c->kind != IR_STORE || !c->name) continue;
    vue_sb_appendf(script, "const %s = ref(", c->name);
    if (c->value)
      vue_emit_js_lit(script, c->value);
    else
      vue_sb_append(script, "null");
    vue_sb_append(script, ");\n");
    char setter[128];
    snprintf(setter, sizeof(setter), "set%c%s",
             (char)toupper((unsigned char)c->name[0]), c->name + 1);
    vue_sb_appendf(script, "function %s(v) { %s.value = v; }\n", setter, c->name);
  }

  if (needs->need_portal) {
    vue_sb_append(script,
              "  function portal(node, target) {\n"
              "    const t = target || (typeof document !== 'undefined' ? "
              "document.body : null);\n"
              "    if (t) t.appendChild(node);\n"
              "    return {\n"
              "      update(newTarget) {\n"
              "        const nt = newTarget || (typeof document !== 'undefined' "
              "? document.body : null);\n"
              "        if (nt) nt.appendChild(node);\n"
              "      },\n"
              "      destroy() { node.remove(); }\n"
              "    };\n"
              "  }\n");
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (irw_hook_is(c, "id") && c->value)
      vue_sb_appendf(script, "const %s = undefined; /* id */\n", c->value);
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (c->kind != IR_EFFECT || !c->value2) continue;
    /* only effect / layoutEffect — skip insertionEffect */
    if (c->name && strcmp(c->name, "insertionEffect") == 0) continue;
    if (c->name && strcmp(c->name, "effect") != 0 &&
        strcmp(c->name, "layoutEffect") != 0)
      continue;
    vue_sb_append(script, "watchEffect(() => {\n");
    vue_sb_appendf(script, "    %s;\n", c->value2);
    for (size_t j = 0; j < c->n_kids; j++) {
      if (c->kids[j]->kind == IR_ATTR && c->kids[j]->name &&
          strcmp(c->kids[j]->name, "cleanup") == 0 && c->kids[j]->value) {
        vue_sb_append(script, "    return () => {\n");
        vue_sb_appendf(script, "      %s;\n", c->kids[j]->value);
        vue_sb_append(script, "    };\n");
      }
    }
    vue_sb_append(script, "  });\n");
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (!irw_hook_is(c, "action") || !c->value) continue;
    const char *st = c->value;
    const char *fn = c->value2 ? c->value2 : "async () => null";
    const char *init = "null";
    const char *pending = NULL;
    for (size_t j = 0; j < c->n_kids; j++) {
      if (c->kids[j]->kind != IR_ATTR || !c->kids[j]->name) continue;
      if (strcmp(c->kids[j]->name, "init") == 0 && c->kids[j]->value)
        init = c->kids[j]->value;
      if (strcmp(c->kids[j]->name, "pending") == 0 && c->kids[j]->value)
        pending = c->kids[j]->value;
    }
    char pending_name[128], action_name[128];
    if (pending)
      snprintf(pending_name, sizeof(pending_name), "%s", pending);
    else
      snprintf(pending_name, sizeof(pending_name), "%sPending", st);
    snprintf(action_name, sizeof(action_name), "%sAction", st);

    vue_sb_appendf(script, "const %s = ref(%s);\n", st, init);
    vue_sb_appendf(script, "const %s = ref(false);\n", pending_name);
    vue_sb_appendf(script, "  async function %s(e) {\n", action_name);
    vue_sb_append(script, "    e.preventDefault();\n");
    vue_sb_appendf(script, "    %s.value = true;\n", pending_name);
    vue_sb_append(script, "    try {\n");
    vue_sb_appendf(script, "      %s.value = await %s(%s.value, new FormData(e.target));\n", st,
               fn, st);
    vue_sb_append(script, "    } finally {\n");
    vue_sb_appendf(script, "      %s.value = false;\n", pending_name);
    vue_sb_append(script, "    }\n");
    vue_sb_append(script, "  }\n");
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (c->kind != IR_FETCH || !c->name) continue;
    const char *nm = c->name;
    const char *url = c->value ? c->value : "/";
    vue_sb_appendf(script, "const %s = ref(null);\n", nm);
    vue_sb_appendf(script, "  let %sError = $state(null);\n", nm);
    vue_sb_appendf(script, "  let %sLoading = $state(true);\n", nm);
    vue_sb_append(script, "watchEffect(() => {\n");
    vue_sb_append(script, "    let cancelled = false;\n");
    vue_sb_appendf(script, "    %sLoading.value = true;\n", nm);
    vue_sb_appendf(script, "    %sError.value = null;\n", nm);
    {
      const char *safe_url = url_href_is_safe(url) ? url : "/";
      char *esc = js_escape_dq_dup(safe_url);
      vue_sb_appendf(script, "    fetch(\"%s\")\n", esc ? esc : "/");
      free(esc);
    }
    vue_sb_append(script, "      .then((r) => {\n");
    vue_sb_append(script, "        if (!r.ok) throw new Error(String(r.status));\n");
    vue_sb_append(script, "        return r.json();\n");
    vue_sb_append(script, "      })\n");
    vue_sb_appendf(script,
               "      .then((data) => { if (!cancelled) %s.value = data; })\n", nm);
    vue_sb_appendf(script,
               "      .catch((err) => { if (!cancelled) %sError.value = err; })\n",
               nm);
    vue_sb_appendf(script,
               "      .finally(() => { if (!cancelled) %sLoading.value = false; });\n",
               nm);
    vue_sb_append(script, "    return () => { cancelled = true; };\n");
    vue_sb_append(script, "  });\n");
  }

  {
    char binds[32][64];
    int n_binds = 0;
    vue_vu_collect_binds_ir(def, binds, &n_binds, 32);
    for (int bi = 0; bi < n_binds; bi++) {
      if (vue_vu_state_exists_ir(def, binds[bi])) continue;
      if (vue_vu_is_store_expr(binds[bi])) continue;
      vue_sb_appendf(script, "const %s = ref('');\n", binds[bi]);
    }
  }

  {
    StateEmitCtx sctx = {.script = script};
    vue_foreach_state(def, vue_state_emit_setter, &sctx);
  }

  {
    char binds[32][64];
    int n_binds = 0;
    vue_vu_collect_binds_ir(def, binds, &n_binds, 32);
    for (int bi = 0; bi < n_binds; bi++) {
      if (vue_vu_state_exists_ir(def, binds[bi])) {
        int has_explicit = 0;
        for (size_t i = 0; i < def->n_kids; i++) {
          IrNode *c = def->kids[i];
          if (c->kind == IR_STATE && c->name &&
              strcmp(c->name, "__states__") != 0 &&
              strcmp(c->name, binds[bi]) == 0)
            has_explicit = 1;
          if (c->kind == IR_STATE && c->name &&
              strcmp(c->name, "__states__") == 0) {
            for (size_t j = 0; j < c->n_kids; j++) {
              if (c->kids[j]->name &&
                  strcmp(c->kids[j]->name, binds[bi]) == 0)
                has_explicit = 1;
            }
          }
        }
        if (has_explicit) continue;
      }
      char setter[128];
      snprintf(setter, sizeof(setter), "set%c%s",
               (char)toupper((unsigned char)binds[bi][0]), binds[bi] + 1);
      vue_sb_appendf(script,
                 "function %s(v) { %s.value = typeof v === 'function' ? v(%s.value) "
                 ": v; }\n",
                 setter, binds[bi], binds[bi]);
    }
  }
}
