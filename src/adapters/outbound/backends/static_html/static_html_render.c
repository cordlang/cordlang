#include "static_html_internal.h"

void statich_gen_node(StrBuf *sb, IrNode *node, int depth) {
  if (!node) return;
  if (statich_is_decl_only(node)) return;

  switch (node->kind) {
    case IR_PROJECT:
    case IR_COMPONENT:
    case IR_LAYOUT:
      statich_gen_children(sb, node, depth);
      break;
    case IR_ELEMENT:
      statich_gen_element(sb, node, depth);
      break;
    case IR_TEXT: {
      if (node->value && strcmp(node->value, "__else__") == 0) break;
      statich_sb_indent(sb, depth);
      char *plain = interp_plain_text(node->value);
      statich_emit_snapshot_text(sb, plain ? plain : "");
      free(plain);
      statich_sb_append(sb, "\n");
      break;
    }
    case IR_INTERP:
      statich_sb_indent(sb, depth);
      if (node->value && node->n_kids == 0) {
        if (node->value[0] == '#' || strchr(node->value, '{')) {
          statich_emit_snapshot_text(sb, node->value);
        } else {
          const char *v = statich_bind_get(node->value);
          if (v)
            statich_html_escape_append(sb, v);
          else {
            statich_sb_append(sb, "[");
            statich_html_escape_append(sb, node->value);
            statich_sb_append(sb, "]");
          }
        }
      } else {
        for (size_t j = 0; j < node->n_kids; j++) {
          IrNode *c = node->kids[j];
          if (!c) continue;
          if (c->kind == IR_TEXT && c->value) {
            char *plain = interp_plain_text(c->value);
            statich_emit_snapshot_text(sb, plain ? plain : "");
            free(plain);
          } else if (c->kind == IR_INTERP && c->value) {
            const char *v = statich_bind_get(c->value);
            if (v)
              statich_html_escape_append(sb, v);
            else {
              statich_sb_append(sb, "[");
              statich_html_escape_append(sb, c->value);
              statich_sb_append(sb, "]");
            }
          }
        }
      }
      statich_sb_append(sb, "\n");
      break;
    case IR_SLOT:
      statich_sb_indent(sb, depth);
      statich_sb_append(sb, "<!-- slot");
      if (node->name) statich_sb_appendf(sb, " %s", node->name);
      statich_sb_append(sb, " -->\n");
      statich_gen_children(sb, node, depth);
      break;
    case IR_IF:
    case IR_FOR: {
      IrNode fake;
      memset(&fake, 0, sizeof(fake));
      IrNode *kids[1] = {node};
      fake.kids = kids;
      fake.n_kids = 1;
      statich_gen_children(sb, &fake, depth);
      break;
    }
    default:
      statich_gen_children(sb, node, depth);
      break;
  }
}


