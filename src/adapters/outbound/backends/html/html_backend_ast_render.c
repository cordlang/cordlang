#include "html_backend_internal.h"

void htmlb_gen_children(StrBuf *sb, Node *node, int depth) {
  for (size_t i = 0; i < node->children_len; i++) {
    Node *child = node->children[i];
    if (child->type == NODE_FOR) {
      const char *var = child->value ? child->value : "item";
      const char *list = child->value2 ? child->value2 : "items";
      htmlb_sb_indent(sb, depth);
      htmlb_sb_append(sb, "<div class=\"cl-loop\">\n");
      htmlb_sb_indent(sb, depth + 1);
      htmlb_sb_appendf(sb, "<div class=\"cl-loop-label\">for %s in %s — preview (2 samples)</div>\n",
                 var, list);
      /* Render body twice as demo samples */
      for (int sample = 0; sample < 2; sample++) {
        for (size_t j = 0; j < child->children_len; j++) {
          htmlb_gen_node(sb, child->children[j], depth + 1);
        }
      }
      htmlb_sb_indent(sb, depth);
      htmlb_sb_append(sb, "</div>\n");
    } else if (child->type == NODE_IF) {
      const char *cond = child->value ? child->value : "true";
      htmlb_sb_indent(sb, depth);
      htmlb_sb_appendf(sb, "<!-- if %s (preview shows true branch) -->\n", cond);
      for (size_t j = 0; j < child->children_len; j++) {
        htmlb_gen_node(sb, child->children[j], depth);
      }
      /* skip paired else marker + body for now (preview shows true branch) */
      if (i + 1 < node->children_len) {
        Node *next = node->children[i + 1];
        if (next->type == NODE_TEXT && next->value && strcmp(next->value, "__else__") == 0) {
          i++; /* skip marker; leave else body unrendered in preview */
          /* skip until non-else-related? keep simple: only skip marker */
        }
      }
    } else if (child->type == NODE_INTERPOLATION) {
      htmlb_sb_indent(sb, depth);
      if (child->value && child->children_len == 0) {
        htmlb_emit_bound_interp(sb, child->value);
        htmlb_sb_append(sb, "\n");
      } else if (child->children_len > 0) {
        for (size_t j = 0; j < child->children_len; j++) {
          Node *c = child->children[j];
          if (c->type == NODE_TEXT && c->value) {
            htmlb_html_escape_append(sb, c->value);
          } else if (c->type == NODE_INTERPOLATION && c->value) {
            htmlb_emit_bound_interp(sb, c->value);
          }
        }
        htmlb_sb_append(sb, "\n");
      }
    } else if (child->type == NODE_TEXT) {
      if (child->value && strcmp(child->value, "__else__") == 0) continue;
      htmlb_sb_indent(sb, depth);
      if (child->value && interp_has(child->value)) {
        /* Prefer live data-bind spans for known state inside mixed text */
        htmlb_emit_text_with_live_interp(sb, child->value);
      } else {
        char *plain = interp_plain_text(child->value);
        htmlb_html_escape_append(sb, plain ? plain : "");
        free(plain);
      }
      htmlb_sb_append(sb, "\n");
    } else if (child->type == NODE_STRING) {
      htmlb_sb_indent(sb, depth);
      if (child->value && interp_has(child->value)) {
        htmlb_emit_text_with_live_interp(sb, child->value);
      } else {
        char *plain = interp_plain_text(child->value);
        htmlb_html_escape_append(sb, plain ? plain : "");
        free(plain);
      }
      htmlb_sb_append(sb, "\n");
    } else if (!htmlb_is_decl_only(child->type)) {
      htmlb_gen_node(sb, child, depth);
    }
  }
}

void htmlb_gen_node(StrBuf *sb, Node *node, int depth) {
  if (!node) return;
  if (htmlb_is_decl_only(node->type)) return;
  switch (node->type) {
    case NODE_ROOT:
      htmlb_gen_children(sb, node, depth);
      break;
    case NODE_COMPONENT_DEF:
      /* Render component body (state/props skipped via is_decl_only) */
      htmlb_gen_children(sb, node, depth);
      break;
    case NODE_ELEMENT:
      htmlb_gen_element(sb, node, depth);
      break;
    case NODE_INTERPOLATION:
      htmlb_sb_indent(sb, depth);
      if (node->value && node->children_len == 0) {
        htmlb_emit_bound_interp(sb, node->value);
        htmlb_sb_append(sb, "\n");
      } else {
        htmlb_gen_children(sb, node, depth);
      }
      break;
    case NODE_TEXT:
    case NODE_STRING:
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
    default:
      htmlb_gen_children(sb, node, depth);
      break;
  }
}

char *htmlb_html_generate_impl(Node *root) {
  htmlb_state_reset();
  htmlb_collect_state_from_node(root);

  StrBuf body;
  htmlb_sb_init(&body);
  htmlb_gen_node(&body, root, 2);

  StrBuf doc;
  htmlb_sb_init(&doc);
  htmlb_sb_append(&doc,
    "<!DOCTYPE html>\n"
    "<html lang=\"en\">\n"
    "<head>\n"
    "  <meta charset=\"UTF-8\" />\n"
    "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" />\n"
    "  <title>Cordlang Runtime Preview</title>\n"
    "  <style>\n");
  htmlb_sb_append(&doc, htmlb_runtime_css);
  {
    char *theme = theme_css_generate(root);
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
    "    <div><strong>Cordlang</strong> <span>native runtime preview</span></div>\n"
    "    <div class=\"cl-runtime-badge\"><span class=\"cl-runtime-dot\"></span> state · setX · #{x} · bind — no SPA parity</div>\n"
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
