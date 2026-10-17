#include "adapters/outbound/backends/esm/esm_ir_internal.h"

void esm_ir_gen_prelude(Sb *sb, IrNode *def, EsmCtx *c) {
  /* props destructuring with defaults */
  IrNode *props_decl = NULL;
  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *k = def->kids[i];
    if (k->kind == IR_PROP && (!k->name || strcmp(k->name, "__props__") == 0)) {
      props_decl = k;
      break;
    }
  }
  if (props_decl && props_decl->n_kids) {
    esm_ir_sb_add(sb, "  const {");
    int n = 0;
    for (size_t i = 0; i < props_decl->n_kids; i++) {
      IrNode *p = props_decl->kids[i];
      if (!p->name || !esm_ir_is_safe_js_ident(p->name)) continue;
      if (n++) esm_ir_sb_add(sb, ",");
      esm_ir_sb_addf(sb, " %s", p->name);
      esm_ir_scope_add(c, p->name);
      if (p->value) {
        esm_ir_sb_add(sb, " = ");
        esm_ir_emit_literal(sb, p->value);
      }
    }
    esm_ir_sb_add(sb, " } = props;\n");
  }

  /* route params */
  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *k = def->kids[i];
    if (!esm_ir_ir_hook_is(k, "params")) continue;
    const char *names = k->value ? k->value : "id";
    esm_ir_sb_addf(sb, "  const { %s } = $.params();\n", names);
    esm_ir_scope_add_list(c, names);
  }

  /* state */
  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *k = def->kids[i];
    if (k->kind != IR_STATE) continue;
    if (k->name && strcmp(k->name, "__states__") == 0) {
      for (size_t j = 0; j < k->n_kids; j++) {
        IrNode *st = k->kids[j];
        if (!st->name || !esm_ir_is_safe_js_ident(st->name)) continue;
        esm_ir_sb_addf(sb, "  const [%s, set%c%s] = $.state('%s', ", st->name,
                (char)toupper((unsigned char)st->name[0]), st->name + 1,
                st->name);
        esm_ir_emit_literal(sb, st->value ? st->value : "null");
        esm_ir_sb_add(sb, ");\n");
        esm_ir_scope_add(c, st->name);
      }
    } else if (k->name && esm_ir_is_safe_js_ident(k->name)) {
      esm_ir_sb_addf(sb, "  const [%s, set%c%s] = $.state('%s', ", k->name,
              (char)toupper((unsigned char)k->name[0]), k->name + 1, k->name);
      esm_ir_emit_literal(sb, k->value ? k->value : "null");
      esm_ir_sb_add(sb, ");\n");
      esm_ir_scope_add(c, k->name);
    }
  }

  /* refs */
  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *k = def->kids[i];
    if (!esm_ir_ir_hook_is(k, "ref") || !k->value || !esm_ir_is_safe_js_ident(k->value))
      continue;
    esm_ir_sb_addf(sb, "  const %s = $.ref(", k->value);
    if (k->value2) esm_ir_emit_literal(sb, k->value2);
    esm_ir_sb_add(sb, ");\n");
    esm_ir_scope_add(c, k->value);
  }

  /* navigate */
  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *k = def->kids[i];
    if (!esm_ir_ir_hook_is(k, "navigate")) continue;
    const char *nm = k->value && esm_ir_is_safe_js_ident(k->value) ? k->value : "navigate";
    esm_ir_sb_addf(sb, "  const %s = $.navigate;\n", nm);
    esm_ir_scope_add(c, nm);
  }

  /* fetch → resource; expose data under the declared name */
  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *k = def->kids[i];
    if (k->kind != IR_FETCH || !k->name || !esm_ir_is_safe_js_ident(k->name)) continue;
    esm_ir_sb_addf(sb, "  const %s$res = $.resource('%s', ", k->name, k->name);
    esm_ir_emit_value(sb, k->value, 0, c);
    esm_ir_sb_add(sb, ");\n");
    esm_ir_sb_addf(sb, "  const %s = %s$res.data;\n", k->name, k->name);
    esm_ir_sb_addf(sb, "  const %sLoading = %s$res.loading;\n", k->name, k->name);
    esm_ir_sb_addf(sb, "  const %sError = %s$res.error;\n", k->name, k->name);
    esm_ir_scope_add(c, k->name);
    {
      char buf[96];
      snprintf(buf, sizeof(buf), "%sLoading", k->name);
      esm_ir_scope_add(c, buf);
      snprintf(buf, sizeof(buf), "%sError", k->name);
      esm_ir_scope_add(c, buf);
    }
  }

  /*
   * computed: the whole body re-runs on every render, so a plain const is both
   * simpler and fresher than a memo with empty deps.
   */
  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *k = def->kids[i];
    if (k->kind != IR_COMPUTED || !k->name || !esm_ir_is_safe_js_ident(k->name)) continue;
    esm_ir_sb_addf(sb, "  const %s = (%s);\n", k->name, k->value ? k->value : "null");
    esm_ir_scope_add(c, k->name);
  }

  /* effects */
  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *k = def->kids[i];
    if (k->kind != IR_EFFECT) continue;
    const char *deps = k->value ? k->value : "";
    const char *body = k->value2 ? k->value2 : "";
    const char *cleanup = NULL;
    for (size_t j = 0; j < k->n_kids; j++) {
      IrNode *a = k->kids[j];
      if (a && a->kind == IR_ATTR && a->name && strcmp(a->name, "cleanup") == 0)
        cleanup = a->value;
    }
    esm_ir_sb_add(sb, "  $.effect(() => {\n");
    if (body && *body) esm_ir_sb_addf(sb, "    %s;\n", body);
    if (cleanup && *cleanup) {
      esm_ir_sb_add(sb, "    return () => {\n");
      esm_ir_sb_addf(sb, "      %s;\n", cleanup);
      esm_ir_sb_add(sb, "    };\n");
    }
    esm_ir_sb_addf(sb, "  }, [%s]);\n", deps);
  }
}

void esm_ir_gen_component(Sb *sb, IrNode *def, const char *name, int is_layout,
                          EsmCtx *c) {
  c->is_layout = is_layout;
  c->uses_children = 0;
  c->n_scope = 0; /* scope is per component, imports are per module */

  esm_ir_sb_addf(sb, "export const %s = component('%s', function (props, $) {\n", name,
          name);
  esm_ir_gen_prelude(sb, def, c);

  int count = 0;
  for (size_t i = 0; i < def->n_kids; i++)
    if (esm_ir_is_renderable(def->kids[i])) count++;

  esm_ir_sb_add(sb, "  return ");
  if (count == 1) {
    /* Single root: emit it directly, no fragment wrapper. */
    for (size_t i = 0; i < def->n_kids; i++) {
      IrNode *k = def->kids[i];
      if (!esm_ir_is_renderable(k)) continue;
      if (k->kind == IR_IF) {
        IrNode *else_node = NULL;
        for (size_t j = i + 1; j < def->n_kids; j++) {
          IrNode *s = def->kids[j];
          if (s && s->kind == IR_TEXT && s->value &&
              strcmp(s->value, "__else__") == 0) {
            else_node = s;
            break;
          }
        }
        Sb tmp;
        esm_ir_sb_init(&tmp);
        esm_ir_gen_if(&tmp, k, else_node, 1, c);
        esm_ir_sb_add(sb, tmp.buf ? tmp.buf + 2 : "null");
        free(tmp.buf);
      } else {
        Sb tmp;
        esm_ir_sb_init(&tmp);
        esm_ir_gen_node(&tmp, k, 1, c);
        /* strip the leading indent produced by gen_node */
        const char *p = tmp.buf ? tmp.buf : "";
        while (*p == ' ') p++;
        esm_ir_sb_add(sb, p);
        free(tmp.buf);
      }
      break;
    }
    esm_ir_sb_add(sb, ";\n");
  } else if (count == 0) {
    esm_ir_sb_add(sb, "null;\n");
  } else {
    esm_ir_sb_add(sb, "frag([\n");
    esm_ir_gen_children_array(sb, def, 2, c);
    esm_ir_sb_add(sb, "  ]);\n");
  }
  esm_ir_sb_add(sb, "});\n\n");
}

IrNode *esm_ir_find_theme(IrNode *root) {
  for (size_t i = 0; i < root->n_kids; i++)
    if (esm_ir_ir_hook_is(root->kids[i], "theme")) return root->kids[i];
  return NULL;
}

void esm_ir_gen_theme_export(Sb *sb, IrNode *root) {
  IrNode *theme = esm_ir_find_theme(root);
  if (!theme) {
    esm_ir_sb_add(sb, "export const theme = null;\n\n");
    return;
  }
  esm_ir_sb_add(sb, "export const theme = {");
  int n = 0;
  for (size_t i = 0; i < theme->n_kids; i++) {
    IrNode *a = theme->kids[i];
    if (!a || a->kind != IR_ATTR || !a->name) continue;
    if (n++) esm_ir_sb_add(sb, ",");
    esm_ir_sb_addf(sb, " '%s': ", a->name);
    esm_ir_emit_literal(sb, a->value);
  }
  esm_ir_sb_add(sb, n ? " };\n\n" : "};\n\n");
}
