#include "adapters/outbound/parser/parser_internal.h"

Node *parser_parse_style_map(Parser *p) {
  parser_advance(p);
  Node *map = node_create(NODE_STYLE_MAP, NULL, p->current.line, p->current.col);

  while (parser_peek(p).type != TOKEN_RPAREN && parser_peek(p).type != TOKEN_EOF && parser_peek(p).type != TOKEN_NEWLINE) {
    if (parser_peek(p).type == TOKEN_IDENTIFIER) {
      Token key = parser_advance(p);
      char *key_str = parser_token_str(key);

      if (parser_match(p, TOKEN_EQUALS)) {
        char *val_str = NULL;
        if (parser_peek(p).type == TOKEN_IDENTIFIER || parser_peek(p).type == TOKEN_NUMBER) {
          Token val = parser_advance(p);
          val_str = parser_token_str(val);
        } else if (parser_peek(p).type == TOKEN_STRING) {
          Token val = parser_advance(p);
          val_str = parser_token_str(val);
        }

        Node *entry = node_adopt(NODE_STYLE_ENTRY, key_str, key.line, key.col);
        if (val_str) entry->value2 = val_str;
        node_add_child(map, entry);
      } else if (parser_peek(p).type == TOKEN_IDENTIFIER || parser_peek(p).type == TOKEN_NUMBER || parser_peek(p).type == TOKEN_STRING) {
        Token val = parser_advance(p);
        char *val_str = parser_token_str(val);
        Node *entry = node_adopt(NODE_STYLE_ENTRY, key_str, key.line, key.col);
        entry->value2 = val_str;
        node_add_child(map, entry);
      } else {
        Node *entry = node_adopt(NODE_STYLE_ENTRY, key_str, key.line, key.col);
        entry->value2 = strdup("true");
        node_add_child(map, entry);
      }
    } else {
      break;
    }
  }

  parser_consume(p, TOKEN_RPAREN, "Expected ')' to close style map");
  return map;
}
