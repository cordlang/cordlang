#include "adapters/outbound/parser/parser_internal.h"

#include "domain/interp.h"

Node *parser_parse_attrs(Parser *p, Node *element) {
  while (parser_peek(p).type == TOKEN_AT ||
         parser_peek(p).type == TOKEN_IDENTIFIER ||
         parser_peek(p).type == TOKEN_STRING) {

    if (parser_peek(p).type == TOKEN_STRING) {
      Token str_tok = parser_advance(p);
      char *str = parser_token_str(str_tok);
      /* Expand "Hello #{name}!" into TEXT + INTERPOLATION children */
      interp_add_to_node(element, str ? str : "", str_tok.line, str_tok.col);
      free(str);
      continue;
    }

    if (parser_peek(p).type == TOKEN_AT) {
      parser_parse_event_attr(p, element);
      continue;
    }

    if (parser_peek(p).type == TOKEN_IDENTIFIER) {
      Token key = parser_advance(p);
      char *key_str = parser_token_str(key);

      /* Responsive style attrs: sm:gap=8 / md:p=24 / lg:cols=3 */
      if (key_str &&
          (strcmp(key_str, "sm") == 0 || strcmp(key_str, "md") == 0 ||
           strcmp(key_str, "lg") == 0) &&
          parser_peek(p).type == TOKEN_COLON) {
        parser_advance(p); /* : */
        if (parser_peek(p).type == TOKEN_IDENTIFIER) {
          Token rest = parser_advance(p);
          char *rest_str = parser_token_str(rest);
          if (rest_str) {
            size_t nlen = strlen(key_str) + 1 + strlen(rest_str) + 1;
            char *combined = malloc(nlen);
            if (combined) {
              snprintf(combined, nlen, "%s:%s", key_str, rest_str);
              free(key_str);
              free(rest_str);
              key_str = combined;
            } else {
              free(rest_str);
            }
          }
        }
      }

      if (strcmp(key_str, "for") == 0) {
        Node *loop = node_create(NODE_FOR, NULL, key.line, key.col);
        parser_consume(p, TOKEN_IDENTIFIER, "Expected variable name after 'for'");
        Token var = p->current;
        loop->value = parser_token_str(var);
        parser_consume(p, TOKEN_IDENTIFIER, "Expected 'in' after for variable");
        {
          char *in_chk = parser_token_str(p->current);
          int is_in = in_chk && strcmp(in_chk, "in") == 0;
          free(in_chk);
          Token tok = parser_advance(p);
          loop->value2 = parser_token_str(tok);
          (void)is_in; /* both branches took the next token historically */
        }
        node_add_child(element, loop);
        free(key_str);
        continue;
      }

      if (strcmp(key_str, "if") == 0) {
        Node *cond = node_create(NODE_IF, NULL, key.line, key.col);
        if (parser_peek(p).type == TOKEN_IDENTIFIER) {
          Token tok = parser_advance(p);
          cond->value = parser_token_str(tok);
        }
        node_add_child(element, cond);
        free(key_str);
        continue;
      }

      if (strcmp(key_str, "else") == 0) {
        node_add_child(element, node_create(NODE_TEXT, "__else__", key.line, key.col));
        free(key_str);
        continue;
      }

      // Check if it's a boolean attribute or key=value
      if (parser_match(p, TOKEN_EQUALS)) {
        if (parser_peek(p).type == TOKEN_LPAREN) {
          Node *style_map = parser_parse_style_map(p);
          style_map->value = key_str;
          node_add_child(element, style_map);
        } else if (parser_peek(p).type == TOKEN_STRING) {
          Token val = parser_advance(p);
          char *val_str = parser_token_str(val);
          Node *attr = node_adopt(NODE_ATTR, key_str, key.line, key.col);
          attr->value2 = val_str;
          node_add_child(element, attr);
        } else if (parser_peek(p).type == TOKEN_IDENTIFIER || parser_peek(p).type == TOKEN_NUMBER) {
          Token val = parser_advance(p);
          char *val_str = parser_token_str(val);
          /* use=tooltip(opts) / transition=fade({duration:200}) — capture call */
          if (val_str && parser_peek(p).type == TOKEN_LPAREN) {
            char buf[512];
            size_t n = 0;
            size_t vlen = strlen(val_str);
            if (vlen < sizeof(buf)) {
              memcpy(buf, val_str, vlen);
              n = vlen;
            }
            int depth = 0;
            while (parser_peek(p).type != TOKEN_EOF && parser_peek(p).type != TOKEN_NEWLINE) {
              Token t = parser_advance(p);
              if (t.type == TOKEN_LPAREN) {
                depth++;
                if (n + 1 < sizeof(buf)) buf[n++] = '(';
              } else if (t.type == TOKEN_RPAREN) {
                if (n + 1 < sizeof(buf)) buf[n++] = ')';
                depth--;
                if (depth <= 0) break;
              } else if (t.type == TOKEN_COMMA) {
                if (n + 2 < sizeof(buf)) {
                  buf[n++] = ',';
                  buf[n++] = ' ';
                }
              } else if (t.type == TOKEN_DOT) {
                if (n + 1 < sizeof(buf)) buf[n++] = '.';
              } else if (t.type == TOKEN_EQUALS) {
                if (n + 1 < sizeof(buf)) buf[n++] = '=';
              } else if (t.type == TOKEN_COLON) {
                if (n + 1 < sizeof(buf)) buf[n++] = ':';
              } else {
                char *s = parser_token_str(t);
                if (s) {
                  size_t sl = strlen(s);
                  if (n + sl < sizeof(buf)) {
                    memcpy(buf + n, s, sl);
                    n += sl;
                  }
                  free(s);
                }
              }
            }
            buf[n] = '\0';
            free(val_str);
            val_str = strdup(buf);
          }
          Node *attr = node_adopt(NODE_ATTR, key_str, key.line, key.col);
          attr->value2 = val_str;
          node_add_child(element, attr);
        } else {
          Node *attr = node_adopt(NODE_ATTR, key_str, key.line, key.col);
          attr->value2 = strdup("true");
          node_add_child(element, attr);
        }
      } else if (parser_match(p, TOKEN_ARROW)) {
        if (parser_peek(p).type == TOKEN_IDENTIFIER) {
          Token val = parser_advance(p);
          char *val_str = parser_token_str(val);
          Node *attr = node_adopt(NODE_ATTR, key_str, key.line, key.col);
          attr->value2 = val_str;
          node_add_child(element, attr);
        }
      } else {
        Node *bool_attr = node_adopt(NODE_BOOL_ATTR, key_str, key.line, key.col);
        node_add_child(element, bool_attr);
      }
    }
  }

  return element;
}
