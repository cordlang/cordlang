#include "adapters/outbound/backends/esm/esm_ir_internal.h"

void esm_ir_gen_props_object(Sb *sb, IrNode *node, const char *tag,
                             const char *dom_tag, const char *classes,
                             int is_component, EsmCtx *c) {
  int wrote = 0;
  esm_ir_sb_add(sb, "{");

  if (classes && *classes) {
    esm_ir_sb_add(sb, " class: ");
    esm_ir_emit_sq_string(sb, classes);
    wrote = 1;
  }

  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *a = node->kids[i];
    if (!a || a->kind != IR_ATTR || !a->name) continue;
    const char *k = a->name;
    const char *v = a->value;

    if (strcmp(k, "__file__") == 0 || strcmp(k, "style") == 0) continue;
    if (strcmp(k, "key") == 0) continue; /* emitted by the `for` wrapper */

    if (!is_component) {
      /* Layout attrs already folded into `class`. */
      if (cord_is_style_attr_name(k)) continue;
      if (cord_is_style_bool_name(k) && cord_attr_is_true(v)) continue;
    } else {
      if (cord_is_style_attr_name(k) || cord_is_style_bool_name(k)) continue;
    }

    /* `type=display` is typography, folded into class, not an HTML type. */
    if (strcmp(k, "type") == 0 && v &&
        (strcmp(v, "display") == 0 || strcmp(v, "title") == 0 ||
         strcmp(v, "body") == 0 || strcmp(v, "caption") == 0 ||
         strcmp(v, "code") == 0))
      continue;

    if (cord_attr_is_true(v)) {
      /* Valueless: either a real boolean DOM attr, or text content. */
      if (!is_component && esm_ir_bare_attr_is_content(k, 0)) continue;
      if (is_component && esm_ir_bare_attr_is_content(k, 0)) {
        if (wrote) esm_ir_sb_add(sb, ",");
        esm_ir_sb_addf(sb, " %s: true", esm_ir_is_safe_js_ident(k) ? k : "invalid");
        wrote = 1;
        continue;
      }
      if (strcmp(k, "required") == 0 || strcmp(k, "disabled") == 0 ||
          strcmp(k, "readonly") == 0 || strcmp(k, "checked") == 0) {
        if (wrote) esm_ir_sb_add(sb, ",");
        esm_ir_sb_addf(sb, " %s: true", k);
        wrote = 1;
        continue;
      }
      if (cord_is_dom_a11y_or_data_attr(k)) {
        if (wrote) esm_ir_sb_add(sb, ",");
        esm_ir_sb_addf(sb, " '%s': 'true'", k);
        wrote = 1;
        continue;
      }
      /* input type shorthands: `input text` → type=text */
      if (dom_tag && strcmp(dom_tag, "input") == 0 &&
          (strcmp(k, "text") == 0 || strcmp(k, "email") == 0 ||
           strcmp(k, "password") == 0 || strcmp(k, "search") == 0 ||
           strcmp(k, "number") == 0)) {
        if (wrote) esm_ir_sb_add(sb, ",");
        esm_ir_sb_addf(sb, " type: '%s'", k);
        wrote = 1;
      }
      continue;
    }

    /* `bind=field` → controlled value + setter */
    if (strcmp(k, "bind") == 0 && v) {
      if (wrote) esm_ir_sb_add(sb, ",");
      if (strchr(v, '.')) {
        esm_ir_sb_addf(sb, " value: %s", v);
      } else {
        char setter[128];
        snprintf(setter, sizeof(setter), "set%c%s",
                 (char)toupper((unsigned char)v[0]), v + 1);
        esm_ir_sb_addf(sb, " value: %s, onInput: (e) => %s(e.target.value)", v, setter);
        int has_name = 0;
        for (size_t j = 0; j < node->n_kids; j++) {
          IrNode *o = node->kids[j];
          if (o && o->kind == IR_ATTR && o->name && strcmp(o->name, "name") == 0)
            has_name = 1;
        }
        if (!has_name) esm_ir_sb_addf(sb, ", name: '%s'", v);
      }
      wrote = 1;
      continue;
    }

    if (strcmp(k, "ref") == 0 && v) {
      if (wrote) esm_ir_sb_add(sb, ",");
      esm_ir_sb_addf(sb, " ref: %s", v);
      wrote = 1;
      continue;
    }

    /* `to=/guia` on a link → href, so the delegated router picks it up. */
    if (strcmp(k, "to") == 0) {
      const char *href = v ? v : "/";
      if (!interp_has(href) && !url_href_is_safe(href)) href = "#";
      if (wrote) esm_ir_sb_add(sb, ",");
      esm_ir_sb_add(sb, " href: ");
      esm_ir_emit_value(sb, href, 1, c);
      wrote = 1;
      continue;
    }

    if (wrote) esm_ir_sb_add(sb, ",");
    if (esm_ir_is_safe_js_ident(k))
      esm_ir_sb_addf(sb, " %s: ", k);
    else
      esm_ir_sb_addf(sb, " '%s': ", k);

    if (cord_is_dom_a11y_or_data_attr(k)) {
      /* aria/role/data must be DOM strings, never booleans. */
      char *esc = js_escape_sq_dup(v ? v : "");
      esm_ir_sb_addf(sb, "'%s'", esc ? esc : "");
      free(esc);
    } else if (!is_component && esm_ir_attr_is_dom_literal(k)) {
      esm_ir_emit_dom_attr_value(sb, v, c);
    } else {
      esm_ir_emit_value(sb, v, is_component && esm_ir_prop_is_stringish(k), c);
    }
    wrote = 1;
  }

  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *e = node->kids[i];
    if (!e || e->kind != IR_EVENT || !e->name) continue;
    char pname[64];
    esm_ir_event_prop_name(e->name, pname, sizeof(pname));
    if (wrote) esm_ir_sb_add(sb, ",");
    esm_ir_sb_addf(sb, " %s: ", pname);
    esm_ir_emit_handler(sb, e->name, e->value);
    wrote = 1;
  }

  esm_ir_sb_add(sb, wrote ? " }" : "}");
}
