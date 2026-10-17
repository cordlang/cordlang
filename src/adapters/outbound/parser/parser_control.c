#include "adapters/outbound/parser/parser_internal.h"

Node *parser_parse_for(Parser *p) {
  (void)parser_advance(p);
  Node *loop = node_create(NODE_FOR, NULL, p->current.line, p->current.col);

  if (parser_peek(p).type == TOKEN_IDENTIFIER) {
    Token var = parser_advance(p);
    loop->value = parser_token_str(var);
  }

  if (parser_peek(p).type == TOKEN_IDENTIFIER) {
    Token in_kw = parser_advance(p);
    char *in_str = parser_token_str(in_kw);
    if (strcmp(in_str, "in") != 0) {
      if (loop->value2) free(loop->value2);
      loop->value2 = in_str;
    } else {
      free(in_str);
    }
  }

  if (parser_peek(p).type == TOKEN_IDENTIFIER) {
    Token list = parser_advance(p);
    loop->value2 = parser_token_str(list);
  }

  /* optional key=expr for React list keys */
  if (parser_peek(p).type == TOKEN_IDENTIFIER) {
    Token maybe = parser_peek(p);
    char *k = parser_token_str(maybe);
    if (strcmp(k, "key") == 0) {
      parser_advance(p);
      if (parser_match(p, TOKEN_EQUALS)) {
        Node *attr = node_create(NODE_ATTR, "key", maybe.line, maybe.col);
        if (parser_peek(p).type == TOKEN_IDENTIFIER || parser_peek(p).type == TOKEN_STRING ||
            parser_peek(p).type == TOKEN_NUMBER) {
          Token v = parser_advance(p);
          attr->value2 = parser_token_str(v);
          /* allow key=item.id — identifier may already include dots */
        }
        node_add_child(loop, attr);
      }
    }
    free(k);
  }

  if (parser_peek(p).type == TOKEN_NEWLINE) {
    parser_advance(p);
    Node *body = parser_parse_body(p);
    if (body) {
      for (size_t i = 0; i < body->children_len; i++) {
        node_add_child(loop, body->children[i]);
      }
      body->children_len = 0;
      node_free(body);
    }
  }

  return loop;
}

Node *parser_parse_if(Parser *p) {
  (void)parser_advance(p);
  Node *cond_node = node_create(NODE_IF, NULL, p->current.line, p->current.col);

  if (parser_peek(p).type == TOKEN_IDENTIFIER) {
    Token cond = parser_advance(p);
    cond_node->value = parser_token_str(cond);
  }

  if (parser_peek(p).type == TOKEN_NEWLINE) {
    parser_advance(p);
    Node *body = parser_parse_body(p);
    if (body) {
      for (size_t i = 0; i < body->children_len; i++) {
        node_add_child(cond_node, body->children[i]);
      }
      body->children_len = 0;
      node_free(body);
    }
  }

  return cond_node;
}

char *parser_capture_expr_line_stop(Parser *p, const char **stop_keywords) {
  char buf[1024];
  size_t n = 0;
  buf[0] = '\0';
  while (parser_peek(p).type != TOKEN_NEWLINE && parser_peek(p).type != TOKEN_EOF &&
         parser_peek(p).type != TOKEN_DEDENT && parser_peek(p).type != TOKEN_INDENT) {
    Token t = parser_peek(p);
    if (t.type == TOKEN_IDENTIFIER && stop_keywords) {
      char *kw = parser_token_str(t);
      int stop = 0;
      for (int i = 0; stop_keywords[i]; i++) {
        if (kw && strcmp(kw, stop_keywords[i]) == 0) {
          stop = 1;
          break;
        }
      }
      free(kw);
      if (stop) break;
    }
    parser_advance(p);
    char *s = parser_token_str(t);
    if (t.type == TOKEN_LPAREN) {
      if (n + 1 < sizeof(buf)) buf[n++] = '(';
    } else if (t.type == TOKEN_RPAREN) {
      if (n + 1 < sizeof(buf)) buf[n++] = ')';
    } else if (t.type == TOKEN_COMMA) {
      if (n + 2 < sizeof(buf)) {
        buf[n++] = ',';
        buf[n++] = ' ';
      }
    } else if (t.type == TOKEN_DOT) {
      if (n + 1 < sizeof(buf)) buf[n++] = '.';
    } else if (t.type == TOKEN_COLON) {
      if (n + 2 < sizeof(buf)) {
        buf[n++] = ':';
        buf[n++] = ' ';
      }
    } else if (t.type == TOKEN_ARROW) {
      if (n + 4 < sizeof(buf)) {
        buf[n++] = ' ';
        buf[n++] = '=';
        buf[n++] = '>';
        buf[n++] = ' ';
      }
    } else if (t.type == TOKEN_EQUALS) {
      if (n + 1 < sizeof(buf)) buf[n++] = '=';
    } else if (t.type == TOKEN_STRING) {
      /* re-quote string literals / path tokens for JS */
      size_t sl = s ? strlen(s) : 0;
      if (n + sl + 3 < sizeof(buf)) {
        buf[n++] = '"';
        if (s && sl) {
          memcpy(buf + n, s, sl);
          n += sl;
        }
        buf[n++] = '"';
      }
      free(s);
      s = NULL;
    } else if (s) {
      size_t sl = strlen(s);
      int is_bracket = (sl == 1 && (s[0] == '[' || s[0] == ']'));
      int is_op = (sl == 1 && (s[0] == '+' || s[0] == '-' || s[0] == '*' ||
                               s[0] == '!' || s[0] == '<' || s[0] == '>' ||
                               s[0] == '&' || s[0] == '|'));
      if (n + sl + 3 < sizeof(buf)) {
        if (is_op && n > 0 && buf[n - 1] != ' ') buf[n++] = ' ';
        memcpy(buf + n, s, sl);
        n += sl;
        if (is_op) buf[n++] = ' ';
        (void)is_bracket;
      }
      free(s);
    }
  }
  buf[n] = '\0';
  return strdup(buf);
}

char *parser_capture_expr_line(Parser *p) {
  return parser_capture_expr_line_stop(p, NULL);
}

char *parser_capture_indent_block(Parser *p) {
  if (parser_peek(p).type != TOKEN_INDENT) return NULL;
  parser_advance(p); /* INDENT */
  char buf[4096];
  size_t n = 0;
  buf[0] = '\0';
  while (parser_peek(p).type != TOKEN_DEDENT && parser_peek(p).type != TOKEN_EOF) {
    while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
    if (parser_peek(p).type == TOKEN_DEDENT || parser_peek(p).type == TOKEN_EOF) break;
    char *line = parser_capture_expr_line(p);
    if (line && line[0]) {
      size_t ll = strlen(line);
      if (n + ll + 3 < sizeof(buf)) {
        if (n > 0) {
          buf[n++] = ';';
          buf[n++] = ' ';
        }
        memcpy(buf + n, line, ll);
        n += ll;
        buf[n] = '\0';
      }
    }
    free(line);
    while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
  }
  if (parser_peek(p).type == TOKEN_DEDENT) parser_advance(p);
  return strdup(buf);
}

char *parser_capture_indent_object_body(Parser *p) {
  if (parser_peek(p).type != TOKEN_INDENT) return NULL;
  parser_advance(p); /* INDENT */
  char buf[4096];
  size_t n = 0;
  buf[0] = '\0';
  while (parser_peek(p).type != TOKEN_DEDENT && parser_peek(p).type != TOKEN_EOF) {
    while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
    if (parser_peek(p).type == TOKEN_DEDENT || parser_peek(p).type == TOKEN_EOF) break;
    char *line = parser_capture_expr_line(p);
    if (line && line[0]) {
      size_t ll = strlen(line);
      if (n + ll + 4 < sizeof(buf)) {
        if (n > 0) {
          buf[n++] = ',';
          buf[n++] = ' ';
        }
        memcpy(buf + n, line, ll);
        n += ll;
        buf[n] = '\0';
      }
    }
    free(line);
    while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
  }
  if (parser_peek(p).type == TOKEN_DEDENT) parser_advance(p);
  return strdup(buf);
}

char *parser_capture_dotted_ident(Parser *p) {
  if (parser_peek(p).type != TOKEN_IDENTIFIER) return NULL;
  char buf[256];
  size_t n = 0;
  buf[0] = '\0';
  while (parser_peek(p).type == TOKEN_IDENTIFIER) {
    Token t = parser_advance(p);
    char *s = parser_token_str(t);
    if (s) {
      size_t sl = strlen(s);
      if (n + sl < sizeof(buf)) {
        memcpy(buf + n, s, sl);
        n += sl;
        buf[n] = '\0';
      }
      free(s);
    }
    if (parser_peek(p).type == TOKEN_DOT) {
      parser_advance(p);
      if (n + 1 < sizeof(buf)) {
        buf[n++] = '.';
        buf[n] = '\0';
      }
    } else {
      break;
    }
  }
  return n > 0 ? strdup(buf) : NULL;
}
