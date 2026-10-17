#include "adapters/outbound/backends/esm/esm_ir_internal.h"

int esm_ir_ir_hook_is(const IrNode *n, const char *kind) {
  return n && n->kind == IR_HOOK && n->name && kind &&
         strcmp(n->name, kind) == 0;
}

int esm_ir_is_decl_node(const IrNode *n) {
  if (!n) return 0;
  switch (n->kind) {
    case IR_PROP:
    case IR_STATE:
    case IR_COMPUTED:
    case IR_EFFECT:
    case IR_FETCH:
    case IR_MODULE_USE:
    case IR_STORE:
    case IR_COMPONENT:
    case IR_LAYOUT:
    case IR_ROUTE:
    case IR_ATTR:
    case IR_EVENT:
    case IR_FOREIGN:
      return 1;
    case IR_HOOK:
      /* Declaration-ish hooks handled in the prelude, not the tree. */
      return esm_ir_ir_hook_is(n, "ref") || esm_ir_ir_hook_is(n, "context") ||
             esm_ir_ir_hook_is(n, "ctx") || esm_ir_ir_hook_is(n, "params") ||
             esm_ir_ir_hook_is(n, "navigate") || esm_ir_ir_hook_is(n, "theme") ||
             esm_ir_ir_hook_is(n, "id") || esm_ir_ir_hook_is(n, "action") ||
             esm_ir_ir_hook_is(n, "lazy") || esm_ir_ir_hook_is(n, "reducer") ||
             esm_ir_ir_hook_is(n, "callback") || esm_ir_ir_hook_is(n, "memo") ||
             esm_ir_ir_hook_is(n, "store");
    default:
      return 0;
  }
}

int esm_ir_is_renderable(const IrNode *n) {
  if (!n) return 0;
  if (n->kind == IR_TEXT && n->value && strcmp(n->value, "__else__") == 0)
    return 0;
  if (esm_ir_is_decl_node(n)) return 0;
  return 1;
}

int esm_ir_node_has_renderable_kids(IrNode *n) {
  for (size_t i = 0; i < n->n_kids; i++)
    if (esm_ir_is_renderable(n->kids[i])) return 1;
  return 0;
}

int esm_ir_bare_attr_is_content(const char *name, int skip_preset_attrs) {
  if (!name || !*name) return 0;
  if (cord_is_style_bool_name(name) || cord_is_style_attr_name(name)) return 0;
  if (cord_is_dom_a11y_or_data_attr(name)) return 0;
  static const char *never[] = {
      "required", "disabled", "readonly", "checked",  "text",   "email",
      "password", "search",   "number",   "lazy",     "forwardRef",
      "__file__", "style",    "to",       "href",     "src",    "alt",
      "type",     "placeholder", "name",  "value",    "id",     "key",
      "rows",     "layout",   "bind",     "ref",      "action", NULL};
  for (int i = 0; never[i]; i++)
    if (strcmp(name, never[i]) == 0) return 0;
  if (skip_preset_attrs && (strcmp(name, "fade") == 0 || strcmp(name, "data") == 0))
    return 0;
  return 1;
}

int esm_ir_attr_is_dom_literal(const char *name) {
  return name && (strcmp(name, "src") == 0 || strcmp(name, "alt") == 0 ||
                  strcmp(name, "href") == 0 || strcmp(name, "placeholder") == 0 ||
                  strcmp(name, "type") == 0 || strcmp(name, "rows") == 0 ||
                  strcmp(name, "name") == 0 || strcmp(name, "value") == 0 ||
                  strcmp(name, "id") == 0 || strcmp(name, "key") == 0 ||
                  strcmp(name, "title") == 0 || strcmp(name, "label") == 0 ||
                  strcmp(name, "for") == 0 || strcmp(name, "target") == 0 ||
                  strcmp(name, "rel") == 0 || strcmp(name, "lang") == 0);
}

int esm_ir_prop_is_stringish(const char *name) {
  return name && (strcmp(name, "title") == 0 || strcmp(name, "text") == 0 ||
                  strcmp(name, "label") == 0 || strcmp(name, "placeholder") == 0 ||
                  strcmp(name, "alt") == 0 || strcmp(name, "name") == 0);
}

void esm_ir_emit_dom_attr_value(Sb *sb, const char *val, EsmCtx *c) {
  if (!val) {
    esm_ir_sb_add(sb, "null");
    return;
  }
  if (interp_has(val)) {
    char *body = interp_to_js_template_body(val);
    esm_ir_sb_addf(sb, "`%s`", body ? body : "");
    free(body);
    return;
  }
  if (esm_ir_is_safe_js_path(val) && strchr(val, '.') && esm_ir_scope_has(c, val)) {
    esm_ir_sb_add(sb, val);
    return;
  }
  if (cord_looks_like_number(val)) {
    esm_ir_sb_add(sb, val);
    return;
  }
  char *plain = interp_plain_text(val);
  esm_ir_emit_sq_string(sb, plain ? plain : val);
  free(plain);
}
