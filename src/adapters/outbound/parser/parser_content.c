#include "adapters/outbound/parser/parser_internal.h"

Node *parser_parse_title_or_head(Parser *p) {
  Token kw_tok = parser_advance(p);
  char *kw = parser_token_str(kw_tok);

  if (strcmp(kw, "title") == 0) {
    free(kw);
    Node *head = node_create(NODE_HEAD, NULL, kw_tok.line, kw_tok.col);
    if (parser_peek(p).type == TOKEN_STRING) {
      Token t = parser_advance(p);
      head->value = parser_token_str(t);
    } else if (parser_peek(p).type == TOKEN_IDENTIFIER) {
      /* bare identifier title text (rare) */
      Token t = parser_advance(p);
      head->value = parser_token_str(t);
    }
    while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
    return head;
  }

  if (strcmp(kw, "head") == 0) {
    free(kw);
    Node *head = node_create(NODE_HEAD, NULL, kw_tok.line, kw_tok.col);
    while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
    if (parser_peek(p).type == TOKEN_INDENT) {
      parser_advance(p);
      while (parser_peek(p).type != TOKEN_DEDENT && parser_peek(p).type != TOKEN_EOF) {
        while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
        if (parser_peek(p).type == TOKEN_DEDENT || parser_peek(p).type == TOKEN_EOF) break;
        if (parser_peek(p).type == TOKEN_IDENTIFIER) {
          Token t = parser_peek(p);
          char *k = parser_token_str(t);
          if (strcmp(k, "title") == 0) {
            parser_advance(p);
            free(k);
            if (parser_peek(p).type == TOKEN_STRING) {
              Token s = parser_advance(p);
              char *title = parser_token_str(s);
              if (!head->value) head->value = title;
              else {
                Node *ch = node_create(NODE_ATTR, "title", t.line, t.col);
                ch->value2 = title;
                node_add_child(head, ch);
              }
            }
            while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
            continue;
          }
          free(k);
        }
        Node *child = parser_parse_stmt(p);
        if (child) node_add_child(head, child);
        else if (parser_peek(p).type != TOKEN_DEDENT && parser_peek(p).type != TOKEN_EOF)
          parser_advance(p);
      }
      if (parser_peek(p).type == TOKEN_DEDENT) parser_advance(p);
    }
    return head;
  }

  free(kw);
  return NULL;
}

Node *parser_parse_await(Parser *p) {
  Token kw_tok = parser_advance(p); /* await */
  Node *aw = node_create(NODE_AWAIT, NULL, kw_tok.line, kw_tok.col);

  /* promise=expr  OR bare promiseExpr */
  static const char *await_stop[] = {"then", "catch", NULL};
  if (parser_peek(p).type == TOKEN_IDENTIFIER) {
    Token first = parser_peek(p);
    char *fk = parser_token_str(first);
    if (fk && strcmp(fk, "promise") == 0) {
      parser_advance(p);
      free(fk);
      if (parser_match(p, TOKEN_EQUALS)) aw->value = parser_capture_expr_line_stop(p, await_stop);
    } else if (fk && (strcmp(fk, "then") == 0 || strcmp(fk, "catch") == 0)) {
      free(fk);
    } else {
      free(fk);
      /* bare identifier or call: productsPromise or loadProducts() */
      aw->value = parser_capture_expr_line_stop(p, await_stop);
    }
  }

  while (parser_peek(p).type == TOKEN_IDENTIFIER) {
    Token k = parser_peek(p);
    char *key = parser_token_str(k);
    if (strcmp(key, "then") == 0) {
      parser_advance(p);
      free(key);
      if (parser_match(p, TOKEN_EQUALS) && parser_peek(p).type == TOKEN_IDENTIFIER) {
        Token n = parser_advance(p);
        aw->value2 = parser_token_str(n);
      }
    } else if (strcmp(key, "catch") == 0) {
      parser_advance(p);
      free(key);
      if (parser_match(p, TOKEN_EQUALS) && parser_peek(p).type == TOKEN_IDENTIFIER) {
        Token n = parser_advance(p);
        Node *a = node_create(NODE_ATTR, "catch", k.line, k.col);
        a->value2 = parser_token_str(n);
        node_add_child(aw, a);
      }
    } else {
      free(key);
      break;
    }
  }

  while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
  if (parser_peek(p).type == TOKEN_INDENT) {
    parser_advance(p);
    while (parser_peek(p).type != TOKEN_DEDENT && parser_peek(p).type != TOKEN_EOF) {
      while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
      if (parser_peek(p).type == TOKEN_DEDENT || parser_peek(p).type == TOKEN_EOF) break;
      if (parser_peek(p).type == TOKEN_IDENTIFIER) {
        Token t = parser_peek(p);
        char *k = parser_token_str(t);
        if (k && (strcmp(k, "loading") == 0 || strcmp(k, "error") == 0)) {
          parser_advance(p);
          int is_err = (strcmp(k, "error") == 0);
          const char *wrap_name = is_err ? "__error__" : "__loading__";
          free(k);
          /* optional catch binding: error err */
          if (is_err && parser_peek(p).type == TOKEN_IDENTIFIER) {
            Token cn = parser_advance(p);
            Node *ca = node_create(NODE_ATTR, "catch", cn.line, cn.col);
            ca->value2 = parser_token_str(cn);
            node_add_child(aw, ca);
          }
          while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
          Node *wrap =
              node_create(NODE_ELEMENT, wrap_name, t.line, t.col);
          if (parser_peek(p).type == TOKEN_INDENT) {
            Node *body = parser_parse_body(p);
            if (body) {
              for (size_t i = 0; i < body->children_len; i++) {
                node_add_child(wrap, body->children[i]);
                body->children[i] = NULL;
              }
              body->children_len = 0;
              node_free(body);
            }
          } else {
            Node *one = parser_parse_stmt(p);
            if (one) node_add_child(wrap, one);
          }
          node_add_child(aw, wrap);
          continue;
        }
        free(k);
      }
      Node *child = parser_parse_stmt(p);
      if (child) node_add_child(aw, child);
      else if (parser_peek(p).type != TOKEN_DEDENT && parser_peek(p).type != TOKEN_EOF)
        parser_advance(p);
    }
    if (parser_peek(p).type == TOKEN_DEDENT) parser_advance(p);
  }
  return aw;
}

Node *parser_parse_snippet(Parser *p) {
  Token kw_tok = parser_advance(p); /* snippet */
  Node *sn = node_create(NODE_SNIPPET, NULL, kw_tok.line, kw_tok.col);
  if (parser_peek(p).type == TOKEN_IDENTIFIER) {
    Token n = parser_advance(p);
    sn->value = parser_token_str(n);
  }
  /* optional (params) */
  if (parser_peek(p).type == TOKEN_LPAREN) {
    parser_advance(p);
    char params[256];
    size_t pn = 0;
    params[0] = '\0';
    while (parser_peek(p).type != TOKEN_RPAREN && parser_peek(p).type != TOKEN_EOF &&
           parser_peek(p).type != TOKEN_NEWLINE) {
      if (parser_peek(p).type == TOKEN_COMMA) {
        parser_advance(p);
        if (pn + 2 < sizeof(params)) {
          params[pn++] = ',';
          params[pn++] = ' ';
        }
        continue;
      }
      Token t = parser_advance(p);
      char *s = parser_token_str(t);
      if (s) {
        size_t sl = strlen(s);
        if (pn + sl < sizeof(params)) {
          memcpy(params + pn, s, sl);
          pn += sl;
        }
        free(s);
      }
    }
    if (parser_peek(p).type == TOKEN_RPAREN) parser_advance(p);
    params[pn] = '\0';
    if (pn > 0) sn->value2 = strdup(params);
  }
  while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
  if (parser_peek(p).type == TOKEN_INDENT) {
    Node *body = parser_parse_body(p);
    if (body) {
      for (size_t i = 0; i < body->children_len; i++) {
        node_add_child(sn, body->children[i]);
        body->children[i] = NULL;
      }
      body->children_len = 0;
      node_free(body);
    }
  }
  return sn;
}
