#include "adapters/outbound/parser/parser_internal.h"

Node *parser_parse_body(Parser *p) {
  if (parser_peek(p).type == TOKEN_INDENT) {
    parser_advance(p);
    Node *container = node_create(NODE_ROOT, NULL, 0, 0);
    while (parser_peek(p).type != TOKEN_DEDENT && parser_peek(p).type != TOKEN_EOF) {
      Node *child = parser_parse_stmt(p);
      if (child) {
        node_add_child(container, child);
      } else {
        if (parser_peek(p).type != TOKEN_DEDENT && parser_peek(p).type != TOKEN_EOF) parser_advance(p);
        else break;
      }
    }
    if (parser_peek(p).type == TOKEN_DEDENT) parser_advance(p);
    return container;
  }
  return NULL;
}

Node *parser_parse_element(Parser *p) {
  if (parser_peek(p).type != TOKEN_IDENTIFIER) return NULL;

  Token tag_tok = parser_advance(p);
  char *tag_name = parser_token_str(tag_tok);
  Node *element = node_adopt(NODE_ELEMENT, tag_name, tag_tok.line, tag_tok.col);

  parser_parse_attrs(p, element);

  if (parser_peek(p).type == TOKEN_NEWLINE) {
    parser_advance(p);
    Node *body = parser_parse_body(p);
    if (body) {
      for (size_t i = 0; i < body->children_len; i++) {
        Node *child = body->children[i];
        body->children[i] = NULL;
        node_add_child(element, child);
      }
      node_free(body);
    }
  }

  return element;
}
