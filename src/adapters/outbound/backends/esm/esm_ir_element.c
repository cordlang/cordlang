#include "adapters/outbound/backends/esm/esm_ir_internal.h"

void esm_ir_gen_element(Sb *sb, IrNode *node, int depth, EsmCtx *c) {
  const char *tag = node->name ? node->name : "div";

  /* Component reference: PascalCase tag, imported or locally defined. */
  if (esm_ir_is_pascal(tag)) {
    esm_ir_sb_pad(sb, depth);
    esm_ir_sb_addf(sb, "h(%s, ", tag);
    esm_ir_gen_props_object(sb, node, tag, NULL, NULL, 1, c);
    if (esm_ir_node_has_renderable_kids(node)) {
      esm_ir_sb_add(sb, ", [\n");
      esm_ir_gen_children_array(sb, node, depth + 1, c);
      esm_ir_sb_pad(sb, depth);
      esm_ir_sb_add(sb, "])");
    } else {
      esm_ir_sb_add(sb, ")");
    }
    return;
  }

  if (node->kind == IR_SLOT || strcmp(tag, "slot") == 0) {
    esm_ir_sb_pad(sb, depth);
    esm_ir_sb_add(sb, "props.children");
    c->uses_children = 1;
    return;
  }

  const char *dom_tag = cord_html_tag_for(tag);
  if (!dom_tag) {
    /* `fragment` → children only */
    esm_ir_sb_pad(sb, depth);
    esm_ir_sb_add(sb, "frag([\n");
    esm_ir_gen_children_array(sb, node, depth + 1, c);
    esm_ir_sb_pad(sb, depth);
    esm_ir_sb_add(sb, "])");
    return;
  }

  /*
   * Preset tags (icon / motion / chart) have no npm component here. Degrade to
   * a useful semantic stub (SVG / motion class / chart axes) rather than an
   * unresolved import.
   */
  int preset_icon = 0, preset_motion = 0, preset_chart = 0;
  if (strcmp(dom_tag, "CordIcon") == 0) {
    dom_tag = "span";
    preset_icon = 1;
  } else if (strcmp(dom_tag, "CordMotion") == 0) {
    dom_tag = "div";
    preset_motion = 1;
  } else if (strcmp(dom_tag, "CordChart") == 0) {
    dom_tag = "div";
    preset_chart = 1;
  }
  int preset_tag = preset_icon || preset_motion || preset_chart;

  const char *base_class = cord_tag_base_class(tag);
  char classes[2048];
  cord_collect_classes(classes, sizeof(classes), node, base_class);
  if (preset_tag) {
    char extra[2048];
    if (preset_icon)
      snprintf(extra, sizeof(extra), "cord-icon cord-%s%s%s", tag,
               classes[0] ? " " : "", classes);
    else if (preset_motion)
      snprintf(extra, sizeof(extra), "cord-motion cord-motion-fade cord-%s%s%s",
               tag, classes[0] ? " " : "", classes);
    else
      snprintf(extra, sizeof(extra), "cord-chart cord-%s%s%s", tag,
               classes[0] ? " " : "", classes);
    snprintf(classes, sizeof(classes), "%s", extra);
  }
  const char *cls = classes;
  while (*cls == ' ') cls++;

  esm_ir_sb_pad(sb, depth);
  if (preset_icon) {
    esm_ir_sb_add(sb, "h('span', ");
    esm_ir_gen_props_object(sb, node, tag, "span", cls, 0, c);
    esm_ir_sb_add(sb,
           ", [h('svg', { class: 'cord-icon-svg', viewBox: '0 0 24 24', width: "
           "'1.25em', height: '1.25em', 'aria-hidden': 'true' }, ["
           "h('path', { d: 'M12 2 L15 9 L22 9 L17 14 L19 21 L12 17 L5 21 L7 "
           "14 L2 9 L9 9 Z', fill: 'currentColor' })])])");
    return;
  }
  if (preset_chart) {
    esm_ir_sb_add(sb, "h('div', ");
    esm_ir_gen_props_object(sb, node, tag, "div", cls, 0, c);
    esm_ir_sb_add(sb,
           ", [h('div', { class: 'cord-chart-axes' }, ["
           "h('div', { class: 'cord-chart-y' }),"
           "h('div', { class: 'cord-chart-plot' }, 'chart')"
           "])])");
    return;
  }

  esm_ir_sb_addf(sb, "h('%s', ", dom_tag);
  esm_ir_gen_props_object(sb, node, tag, dom_tag, cls, 0, c);

  /* checkbox / radio carry an implicit input type */
  if (strcmp(tag, "checkbox") == 0 || strcmp(tag, "radio") == 0) {
    /* rewritten inline: props object already closed, so patch via Object.assign
       would be noisy — emit type through a second props entry instead. */
  }

  int self_closing = (strcmp(dom_tag, "img") == 0 || strcmp(dom_tag, "input") == 0 ||
                      strcmp(dom_tag, "br") == 0 || strcmp(dom_tag, "hr") == 0);

  int has_kids = esm_ir_node_has_renderable_kids(node);
  int has_bare_content = 0;
  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *a = node->kids[i];
    if (a && a->kind == IR_ATTR && a->name && cord_attr_is_true(a->value) &&
        esm_ir_bare_attr_is_content(a->name, preset_tag))
      has_bare_content = 1;
  }

  if (self_closing || (!has_kids && !has_bare_content)) {
    esm_ir_sb_add(sb, ")");
    return;
  }

  esm_ir_sb_add(sb, ", [\n");
  /* Bare identifiers as text content, before real children (React order). */
  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *a = node->kids[i];
    if (!a || a->kind != IR_ATTR || !a->name) continue;
    if (!cord_attr_is_true(a->value)) continue;
    if (!esm_ir_bare_attr_is_content(a->name, preset_tag)) continue;
    esm_ir_sb_pad(sb, depth + 1);
    esm_ir_sb_addf(sb, "%s,\n", a->name);
  }
  esm_ir_gen_children_array(sb, node, depth + 1, c);
  esm_ir_sb_pad(sb, depth);
  esm_ir_sb_add(sb, "])");
}

void esm_ir_gen_text(Sb *sb, const char *value, int depth) {
  esm_ir_sb_pad(sb, depth);
  if (interp_has(value)) {
    char *body = interp_to_js_template_body(value);
    esm_ir_sb_addf(sb, "`%s`", body ? body : "");
    free(body);
    return;
  }
  char *plain = interp_plain_text(value);
  char *esc = js_escape_sq_dup(plain ? plain : (value ? value : ""));
  esm_ir_sb_addf(sb, "'%s'", esc ? esc : "");
  free(esc);
  free(plain);
}

void esm_ir_gen_interp(Sb *sb, IrNode *n, int depth) {
  esm_ir_sb_pad(sb, depth);
  if (n->value && n->n_kids == 0) {
    esm_ir_sb_addf(sb, "(%s)", n->value);
    return;
  }
  esm_ir_sb_add(sb, "`");
  for (size_t i = 0; i < n->n_kids; i++) {
    IrNode *k = n->kids[i];
    if (!k) continue;
    if (k->kind == IR_TEXT && k->value) {
      for (const char *p = k->value; *p; p++) {
        if (*p == '`' || *p == '\\') {
          char esc[3] = {'\\', *p, 0};
          esm_ir_sb_add(sb, esc);
        } else if (*p == '$' && p[1] == '{') {
          esm_ir_sb_add(sb, "\\$");
        } else {
          char ch[2] = {*p, 0};
          esm_ir_sb_add(sb, ch);
        }
      }
    } else if (k->kind == IR_INTERP && k->value) {
      esm_ir_sb_addf(sb, "${%s}", k->value);
    }
  }
  esm_ir_sb_add(sb, "`");
}
