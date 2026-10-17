#include "adapters/outbound/parser/parser_internal.h"

void parser_parse_event_attr(Parser *p, Node *element) {

      parser_advance(p);
      if (parser_peek(p).type == TOKEN_IDENTIFIER) {
        Token event = parser_advance(p);
        char *event_name = parser_token_str(event);
        Node *evt_node = node_adopt(NODE_EVENT, event_name, event.line, event.col);

        if (parser_match(p, TOKEN_EQUALS)) {
          /* Capture full handler: name | name(...) | (expr) */
          char expr_buf[1024] = {0};
          size_t expr_len = 0;

          if (parser_peek(p).type == TOKEN_LPAREN) {
            /* @click=(count + 1) or @click=(setCount(count+1)) */
            parser_advance(p);
            int depth = 1;
            while (depth > 0 && parser_peek(p).type != TOKEN_EOF && parser_peek(p).type != TOKEN_NEWLINE) {
              if (parser_peek(p).type == TOKEN_LPAREN) depth++;
              if (parser_peek(p).type == TOKEN_RPAREN) {
                depth--;
                if (depth == 0) {
                  parser_advance(p);
                  break;
                }
              }
              Token t = parser_advance(p);
              char *s = parser_token_str(t);
              if (s) {
                size_t slen = strlen(s);
                if (expr_len + slen + 2 < sizeof(expr_buf)) {
                  if (expr_len > 0 && t.type != TOKEN_DOT && t.type != TOKEN_COMMA &&
                      t.type != TOKEN_RPAREN && t.type != TOKEN_LPAREN) {
                    /* light spacing for binary ops written as separate tokens */
                    if (s[0] == '+' || s[0] == '-' || s[0] == '*' || s[0] == '/' ||
                        strcmp(s, "&&") == 0 || strcmp(s, "||") == 0) {
                      expr_buf[expr_len++] = ' ';
                      memcpy(expr_buf + expr_len, s, slen);
                      expr_len += slen;
                      expr_buf[expr_len++] = ' ';
                    } else {
                      memcpy(expr_buf + expr_len, s, slen);
                      expr_len += slen;
                    }
                  } else {
                    memcpy(expr_buf + expr_len, s, slen);
                    expr_len += slen;
                  }
                }
                free(s);
              } else if (t.type == TOKEN_LPAREN && expr_len + 1 < sizeof(expr_buf)) {
                expr_buf[expr_len++] = '(';
              } else if (t.type == TOKEN_RPAREN && expr_len + 1 < sizeof(expr_buf)) {
                expr_buf[expr_len++] = ')';
              } else if (t.type == TOKEN_COMMA && expr_len + 1 < sizeof(expr_buf)) {
                expr_buf[expr_len++] = ',';
              } else if (t.type == TOKEN_DOT && expr_len + 1 < sizeof(expr_buf)) {
                expr_buf[expr_len++] = '.';
              }
            }
            expr_buf[expr_len] = '\0';
            evt_node->value2 = strdup(expr_buf);
          } else if (parser_peek(p).type == TOKEN_IDENTIFIER) {
            Token handler = parser_advance(p);
            char *name = parser_token_str(handler);
            if (parser_peek(p).type == TOKEN_LPAREN) {
              /* @click=setCount(count + 1) */
              parser_advance(p);
              size_t nlen = strlen(name);
              if (nlen + 1 < sizeof(expr_buf)) {
                memcpy(expr_buf, name, nlen);
                expr_len = nlen;
                expr_buf[expr_len++] = '(';
              }
              free(name);
              int depth = 1;
              while (depth > 0 && parser_peek(p).type != TOKEN_EOF && parser_peek(p).type != TOKEN_NEWLINE) {
                if (parser_peek(p).type == TOKEN_LPAREN) {
                  depth++;
                  parser_advance(p);
                  if (expr_len + 1 < sizeof(expr_buf)) expr_buf[expr_len++] = '(';
                  continue;
                }
                if (parser_peek(p).type == TOKEN_RPAREN) {
                  depth--;
                  parser_advance(p);
                  if (expr_len + 1 < sizeof(expr_buf)) expr_buf[expr_len++] = ')';
                  if (depth == 0) break;
                  continue;
                }
                if (parser_peek(p).type == TOKEN_COMMA) {
                  parser_advance(p);
                  if (expr_len + 2 < sizeof(expr_buf)) {
                    expr_buf[expr_len++] = ',';
                    expr_buf[expr_len++] = ' ';
                  }
                  continue;
                }
                if (parser_peek(p).type == TOKEN_DOT) {
                  parser_advance(p);
                  if (expr_len + 1 < sizeof(expr_buf)) expr_buf[expr_len++] = '.';
                  continue;
                }
                if (parser_peek(p).type == TOKEN_ARROW) {
                  parser_advance(p);
                  if (expr_len + 4 < sizeof(expr_buf)) {
                    expr_buf[expr_len++] = ' ';
                    expr_buf[expr_len++] = '=';
                    expr_buf[expr_len++] = '>';
                    expr_buf[expr_len++] = ' ';
                  }
                  continue;
                }
                if (parser_peek(p).type == TOKEN_STRING) {
                  Token t = parser_advance(p);
                  char *s = parser_token_str(t);
                  size_t slen = s ? strlen(s) : 0;
                  if (expr_len + slen + 3 < sizeof(expr_buf)) {
                    expr_buf[expr_len++] = '"';
                    if (s && slen) {
                      memcpy(expr_buf + expr_len, s, slen);
                      expr_len += slen;
                    }
                    expr_buf[expr_len++] = '"';
                  }
                  free(s);
                  continue;
                }
                Token t = parser_advance(p);
                char *s = parser_token_str(t);
                if (s) {
                  size_t slen = strlen(s);
                  int is_op = (strcmp(s, "+") == 0 || strcmp(s, "-") == 0 ||
                               strcmp(s, "*") == 0);
                  if (expr_len + slen + 2 < sizeof(expr_buf)) {
                    if (is_op) {
                      if (expr_len > 0 && expr_buf[expr_len - 1] != ' ')
                        expr_buf[expr_len++] = ' ';
                      memcpy(expr_buf + expr_len, s, slen);
                      expr_len += slen;
                      expr_buf[expr_len++] = ' ';
                    } else {
                      memcpy(expr_buf + expr_len, s, slen);
                      expr_len += slen;
                    }
                  }
                  free(s);
                }
              }
              expr_buf[expr_len] = '\0';
              evt_node->value2 = strdup(expr_buf);
            } else {
              evt_node->value2 = name;
            }
          }
        }

        node_add_child(element, evt_node);
      }

}
