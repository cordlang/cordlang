#include "html_backend_internal.h"

void htmlb_gen_ir_children(StrBuf *sb, IrNode *node, int depth) {
  if (!node) return;
  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *child = node->kids[i];
    if (!child) continue;

    if (child->kind == IR_FOR) {
      const char *var = child->name ? child->name : "item";
      const char *list = child->value ? child->value : "items";
      htmlb_sb_indent(sb, depth);
      htmlb_sb_append(sb, "<div class=\"cl-loop\">\n");
      htmlb_sb_indent(sb, depth + 1);
      htmlb_sb_appendf(sb,
                 "<div class=\"cl-loop-label\">for %s in %s — lista estática "
                 "(preview; no each dinámico)</div>\n",
                 var, list);
      for (int sample = 0; sample < 2; sample++) {
        for (size_t j = 0; j < child->n_kids; j++)
          htmlb_gen_ir_node(sb, child->kids[j], depth + 1);
      }
      htmlb_sb_indent(sb, depth);
      htmlb_sb_append(sb, "</div>\n");
    } else if (child->kind == IR_IF) {
      const char *cond = child->value ? child->value : "true";
      /* Collect true-branch kids until __else__ marker */
      size_t else_at = child->n_kids;
      for (size_t j = 0; j < child->n_kids; j++) {
        IrNode *kc = child->kids[j];
        if (kc && kc->kind == IR_TEXT && kc->value &&
            strcmp(kc->value, "__else__") == 0) {
          else_at = j;
          break;
        }
      }
      htmlb_sb_indent(sb, depth);
      htmlb_sb_append(sb, "<div data-if=\"");
      htmlb_html_escape_append(sb, cond);
      htmlb_sb_append(sb, "\">\n");
      for (size_t j = 0; j < else_at; j++)
        htmlb_gen_ir_node(sb, child->kids[j], depth + 1);
      htmlb_sb_indent(sb, depth);
      htmlb_sb_append(sb, "</div>\n");
      /* Else branch under sibling marker or remaining kids */
      int has_else = 0;
      if (else_at < child->n_kids) {
        has_else = 1;
        htmlb_sb_indent(sb, depth);
        htmlb_sb_append(sb, "<div data-else-if=\"");
        htmlb_html_escape_append(sb, cond);
        htmlb_sb_append(sb, "\" hidden>\n");
        for (size_t j = else_at + 1; j < child->n_kids; j++)
          htmlb_gen_ir_node(sb, child->kids[j], depth + 1);
        htmlb_sb_indent(sb, depth);
        htmlb_sb_append(sb, "</div>\n");
      }
      if (!has_else && i + 1 < node->n_kids) {
        IrNode *next = node->kids[i + 1];
        if (next && next->kind == IR_TEXT && next->value &&
            strcmp(next->value, "__else__") == 0) {
          i++; /* consume marker */
          htmlb_sb_indent(sb, depth);
          htmlb_sb_append(sb, "<div data-else-if=\"");
          htmlb_html_escape_append(sb, cond);
          htmlb_sb_append(sb, "\" hidden>\n");
          for (size_t j = 0; j < next->n_kids; j++)
            htmlb_gen_ir_node(sb, next->kids[j], depth + 1);
          htmlb_sb_indent(sb, depth);
          htmlb_sb_append(sb, "</div>\n");
        }
      }
    } else if (child->kind == IR_INTERP) {
      htmlb_sb_indent(sb, depth);
      if (child->value && child->n_kids == 0) {
        htmlb_emit_bound_interp(sb, child->value);
        htmlb_sb_append(sb, "\n");
      } else if (child->n_kids > 0) {
        for (size_t j = 0; j < child->n_kids; j++) {
          IrNode *c = child->kids[j];
          if (!c) continue;
          if (c->kind == IR_TEXT && c->value) {
            htmlb_html_escape_append(sb, c->value);
          } else if (c->kind == IR_INTERP && c->value) {
            htmlb_emit_bound_interp(sb, c->value);
          }
        }
        htmlb_sb_append(sb, "\n");
      } else {
        htmlb_emit_bound_interp(sb, child->value);
        htmlb_sb_append(sb, "\n");
      }
    } else if (child->kind == IR_TEXT) {
      if (child->value && strcmp(child->value, "__else__") == 0) continue;
      htmlb_sb_indent(sb, depth);
      if (child->value && interp_has(child->value)) {
        htmlb_emit_text_with_live_interp(sb, child->value);
      } else {
        char *plain = interp_plain_text(child->value);
        htmlb_html_escape_append(sb, plain ? plain : "");
        free(plain);
      }
      htmlb_sb_append(sb, "\n");
    } else if (child->kind == IR_SLOT) {
      htmlb_sb_indent(sb, depth);
      htmlb_sb_append(sb, "<!-- slot");
      if (child->name) htmlb_sb_appendf(sb, " %s", child->name);
      htmlb_sb_append(sb, " -->\n");
      htmlb_gen_ir_children(sb, child, depth);
    } else if (!htmlb_is_ir_decl_only(child)) {
      htmlb_gen_ir_node(sb, child, depth);
    }
  }
}

void htmlb_gen_ir_node(StrBuf *sb, IrNode *node, int depth) {
  if (!node) return;
  if (htmlb_is_ir_decl_only(node) && node->kind != IR_HOOK) return;

  switch (node->kind) {
    case IR_PROJECT:
    case IR_COMPONENT:
    case IR_LAYOUT:
      htmlb_gen_ir_children(sb, node, depth);
      break;
    case IR_ELEMENT:
      htmlb_gen_ir_element(sb, node, depth);
      break;
    case IR_INTERP:
      htmlb_sb_indent(sb, depth);
      if (node->value && node->n_kids == 0) {
        htmlb_emit_bound_interp(sb, node->value);
        htmlb_sb_append(sb, "\n");
      } else {
        htmlb_gen_ir_children(sb, node, depth);
      }
      break;
    case IR_TEXT:
      if (node->value && strcmp(node->value, "__else__") != 0) {
        htmlb_sb_indent(sb, depth);
        if (interp_has(node->value)) {
          htmlb_emit_text_with_live_interp(sb, node->value);
        } else {
          char *plain = interp_plain_text(node->value);
          htmlb_html_escape_append(sb, plain ? plain : "");
          free(plain);
        }
        htmlb_sb_append(sb, "\n");
      }
      break;
    case IR_SLOT:
      htmlb_sb_indent(sb, depth);
      htmlb_sb_append(sb, "<!-- slot");
      if (node->name) htmlb_sb_appendf(sb, " %s", node->name);
      htmlb_sb_append(sb, " -->\n");
      htmlb_gen_ir_children(sb, node, depth);
      break;
    case IR_IF:
    case IR_FOR: {
      /* Top-level: wrap via gen_ir_children sibling logic */
      IrNode fake;
      memset(&fake, 0, sizeof(fake));
      IrNode *kids[1] = {node};
      fake.kids = kids;
      fake.n_kids = 1;
      htmlb_gen_ir_children(sb, &fake, depth);
      break;
    }
    case IR_HOOK:
      if (node->name &&
          (strcmp(node->name, "portal") == 0 ||
           strcmp(node->name, "empty") == 0 ||
           strcmp(node->name, "suspense") == 0 ||
           strcmp(node->name, "loading") == 0 ||
           strcmp(node->name, "errorBoundary") == 0)) {
        htmlb_sb_indent(sb, depth);
        if (strcmp(node->name, "portal") == 0) {
          htmlb_sb_appendf(sb, "<!-- portal %s -->\n",
                     node->value ? node->value : "document.body");
        } else if (strcmp(node->name, "empty") == 0) {
          htmlb_sb_appendf(sb, "<!-- empty when %s -->\n",
                     node->value ? node->value : "?");
        } else {
          htmlb_sb_appendf(sb, "<!-- %s -->\n", node->name);
        }
        for (size_t i = 0; i < node->n_kids; i++) {
          IrNode *c = node->kids[i];
          if (!c || c->kind == IR_ATTR) continue;
          if (c->kind == IR_ELEMENT && c->name &&
              strcmp(c->name, "__fallback__") == 0)
            continue;
          htmlb_gen_ir_node(sb, c, depth);
        }
      }
      break;
    default:
      htmlb_gen_ir_children(sb, node, depth);
      break;
  }
}

char *htmlb_html_generate_from_ir_root(IrNode *root) {
  htmlb_state_reset();
  htmlb_collect_state_from_ir(root);

  StrBuf body;
  htmlb_sb_init(&body);
  htmlb_gen_ir_node(&body, root, 2);

  StrBuf doc;
  htmlb_sb_init(&doc);
  htmlb_sb_append(&doc,
            "<!DOCTYPE html>\n"
            "<html lang=\"en\">\n"
            "<head>\n"
            "  <meta charset=\"UTF-8\" />\n"
            "  <meta name=\"viewport\" content=\"width=device-width, "
            "initial-scale=1.0\" />\n"
            "  <title>Cordlang Runtime Preview</title>\n"
            "  <style>\n");
  htmlb_sb_append(&doc, htmlb_runtime_css);
  {
    IrProgram fake;
    memset(&fake, 0, sizeof(fake));
    fake.root = root;
    char *theme = theme_css_generate_from_ir(&fake);
    if (theme) {
      htmlb_sb_append(&doc, "\n");
      htmlb_sb_append(&doc, theme);
      free(theme);
    }
  }
  htmlb_sb_append(&doc,
            "  </style>\n"
            "</head>\n"
            "<body>\n"
            "  <div class=\"cl-runtime-bar\">\n"
            "    <div><strong>Cordlang</strong> <span>native runtime "
            "preview</span></div>\n"
            "    <div class=\"cl-runtime-badge\"><span "
            "class=\"cl-runtime-dot\"></span> state · setX · #{x} · bind — no "
            "SPA parity</div>\n"
            "  </div>\n"
            "  <div id=\"app\">\n");
  htmlb_sb_append(&doc, body.buf ? body.buf : "");
  htmlb_sb_append(&doc,
            "  </div>\n"
            "  <div id=\"cl-toast\" class=\"cl-toast\"></div>\n"
            "  <script>\n");
  htmlb_emit_state_runtime_js(&doc);
  htmlb_sb_append(&doc, htmlb_runtime_js_core);
  htmlb_sb_append(&doc,
            "  </script>\n"
            "</body>\n"
            "</html>\n");

  free(body.buf);
  htmlb_state_reset();
  return doc.buf;
}


