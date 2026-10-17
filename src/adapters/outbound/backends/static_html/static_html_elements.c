#include "static_html_internal.h"

void statich_gen_layout_table(StrBuf *sb, IrNode *node, int depth, LayKind lay) {
  int gap = statich_attr_gap(node);
  StrBuf style;
  statich_sb_init(&style);
  statich_collect_inline_style(&style, node, lay);
  statich_sb_indent(sb, depth);
  statich_sb_append(sb, "<table role=\"presentation\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\"");
  if (style.len > 0) {
    statich_sb_append(sb, " style=\"");
    statich_sb_append(sb, style.buf);
    statich_sb_append(sb, "\"");
  }
  statich_sb_append(sb, ">\n");
  free(style.buf);

  if (lay == LAY_ROW) {
    statich_sb_indent(sb, depth + 1);
    statich_sb_append(sb, "<tr>\n");
    for (size_t i = 0; i < node->n_kids; i++) {
      IrNode *c = node->kids[i];
      if (!c || statich_is_decl_only(c)) continue;
      statich_sb_indent(sb, depth + 2);
      statich_sb_appendf(sb, "<td style=\"padding:%dpx;vertical-align:middle;\">\n",
                 gap / 2);
      statich_gen_node(sb, c, depth + 3);
      statich_sb_indent(sb, depth + 2);
      statich_sb_append(sb, "</td>\n");
    }
    statich_sb_indent(sb, depth + 1);
    statich_sb_append(sb, "</tr>\n");
  } else {
    for (size_t i = 0; i < node->n_kids; i++) {
      IrNode *c = node->kids[i];
      if (!c || statich_is_decl_only(c)) continue;
      statich_sb_indent(sb, depth + 1);
      statich_sb_append(sb, "<tr>\n");
      statich_sb_indent(sb, depth + 2);
      statich_sb_appendf(sb, "<td style=\"padding:%dpx 0;\">\n", gap / 2);
      statich_gen_node(sb, c, depth + 3);
      statich_sb_indent(sb, depth + 2);
      statich_sb_append(sb, "</td>\n");
      statich_sb_indent(sb, depth + 1);
      statich_sb_append(sb, "</tr>\n");
    }
  }
  statich_sb_indent(sb, depth);
  statich_sb_append(sb, "</table>\n");
}

void statich_gen_element(StrBuf *sb, IrNode *node, int depth) {
  const char *tag = node->name ? node->name : "div";
  LayKind lay = statich_lay_kind(tag);
  if (lay != LAY_NONE) {
    statich_gen_layout_table(sb, node, depth, lay);
    return;
  }
  /* card / section-like table wrappers: one cell column */
  if (tag && (strcmp(tag, "card") == 0 || strcmp(tag, "grid") == 0 ||
              strcmp(tag, "group") == 0 || strcmp(tag, "nav") == 0 ||
              strcmp(tag, "header") == 0 || strcmp(tag, "footer") == 0 ||
              strcmp(tag, "main") == 0 || strcmp(tag, "section") == 0 ||
              strcmp(tag, "article") == 0 || strcmp(tag, "aside") == 0)) {
    StrBuf style;
    statich_sb_init(&style);
    statich_collect_inline_style(&style, node, LAY_COL);
    statich_sb_indent(sb, depth);
    statich_sb_append(sb, "<table role=\"presentation\" cellpadding=\"0\" "
                  "cellspacing=\"0\" border=\"0\"");
    if (style.len > 0) {
      statich_sb_append(sb, " style=\"");
      statich_sb_append(sb, style.buf);
      statich_sb_append(sb, "\"");
    }
    statich_sb_append(sb, ">\n");
    free(style.buf);
    for (size_t i = 0; i < node->n_kids; i++) {
      IrNode *c = node->kids[i];
      if (!c || statich_is_decl_only(c)) continue;
      statich_sb_indent(sb, depth + 1);
      statich_sb_append(sb, "<tr>\n");
      statich_sb_indent(sb, depth + 2);
      statich_sb_append(sb, "<td style=\"padding:8px 0;\">\n");
      statich_gen_node(sb, c, depth + 3);
      statich_sb_indent(sb, depth + 2);
      statich_sb_append(sb, "</td>\n");
      statich_sb_indent(sb, depth + 1);
      statich_sb_append(sb, "</tr>\n");
    }
    statich_sb_indent(sb, depth);
    statich_sb_append(sb, "</table>\n");
    return;
  }
  if (strcmp(tag, "fragment") == 0) {
    statich_gen_children(sb, node, depth);
    return;
  }

  const char *html_tag = statich_html_tag_for(tag);
  if (!html_tag) {
    statich_gen_children(sb, node, depth);
    return;
  }

  int self_closing = (strcmp(html_tag, "img") == 0);
  StrBuf style;
  statich_sb_init(&style);
  statich_collect_inline_style(&style, node, LAY_NONE);

  statich_sb_indent(sb, depth);
  statich_sb_appendf(sb, "<%s", html_tag);
  statich_emit_html_attrs(sb, node, tag);
  if (style.len > 0) {
    statich_sb_append(sb, " style=\"");
    statich_sb_append(sb, style.buf);
    statich_sb_append(sb, "\"");
  }
  free(style.buf);

  if (self_closing) {
    statich_sb_append(sb, " />\n");
    return;
  }
  statich_sb_append(sb, ">\n");
  statich_gen_children(sb, node, depth + 1);
  statich_sb_indent(sb, depth);
  statich_sb_appendf(sb, "</%s>\n", html_tag);
}

void statich_gen_children(StrBuf *sb, IrNode *node, int depth) {
  if (!node) return;
  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *c = node->kids[i];
    if (!c || statich_is_decl_only(c)) continue;
    if (c->kind == IR_IF) {
      statich_sb_indent(sb, depth);
      statich_sb_appendf(sb, "<!-- if %s (static: true branch) -->\n",
                 c->value ? c->value : "true");
      for (size_t j = 0; j < c->n_kids; j++) {
        IrNode *kc = c->kids[j];
        if (kc && kc->kind == IR_TEXT && kc->value &&
            strcmp(kc->value, "__else__") == 0)
          break;
        statich_gen_node(sb, kc, depth);
      }
      if (i + 1 < node->n_kids) {
        IrNode *next = node->kids[i + 1];
        if (next && next->kind == IR_TEXT && next->value &&
            strcmp(next->value, "__else__") == 0)
          i++;
      }
    } else if (c->kind == IR_FOR) {
      statich_sb_indent(sb, depth);
      statich_sb_appendf(sb, "<!-- for %s in %s (static: 1 sample) -->\n",
                 c->name ? c->name : "item", c->value ? c->value : "items");
      for (size_t j = 0; j < c->n_kids; j++) statich_gen_node(sb, c->kids[j], depth);
    } else {
      statich_gen_node(sb, c, depth);
    }
  }
}
