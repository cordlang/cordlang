#include "adapters/outbound/parser/parser_internal.h"

Node *parser_parse_store(Parser *p) {
  Token kw_tok = parser_advance(p); /* store | writable */
  Node *st = node_create(NODE_STORE_DECL, NULL, kw_tok.line, kw_tok.col);
  if (parser_peek(p).type == TOKEN_IDENTIFIER) {
    Token n = parser_advance(p);
    st->value = parser_token_str(n);
  }
  if (parser_match(p, TOKEN_EQUALS)) {
    if (parser_peek(p).type == TOKEN_STRING || parser_peek(p).type == TOKEN_NUMBER ||
        parser_peek(p).type == TOKEN_IDENTIFIER) {
      Token v = parser_advance(p);
      st->value2 = parser_token_str(v);
      /* optional call: store x = writable(0) already handled as bare */
      if (st->value2 && parser_peek(p).type == TOKEN_LPAREN) {
        char buf[256];
        size_t n = 0;
        size_t vl = strlen(st->value2);
        if (vl < sizeof(buf)) {
          memcpy(buf, st->value2, vl);
          n = vl;
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
        free(st->value2);
        st->value2 = strdup(buf);
      }
    } else {
      st->value2 = parser_capture_expr_line(p);
    }
  }
  while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
  return st;
}

Node *parser_parse_render(Parser *p) {
  Token kw_tok = parser_advance(p); /* render */
  Node *rn = node_create(NODE_RENDER, NULL, kw_tok.line, kw_tok.col);

  /* card("Hello") or card title="Hello" */
  if (parser_peek(p).type == TOKEN_IDENTIFIER) {
    Token name = parser_advance(p);
    char *nm = parser_token_str(name);
    if (parser_peek(p).type == TOKEN_LPAREN) {
      /* @render card("Hello") / render card("Hello") */
      char buf[512];
      size_t n = 0;
      size_t nl = nm ? strlen(nm) : 0;
      if (nm && nl < sizeof(buf)) {
        memcpy(buf, nm, nl);
        n = nl;
      }
      free(nm);
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
        } else if (t.type == TOKEN_STRING) {
          char *s = parser_token_str(t);
          if (s) {
            size_t sl = strlen(s);
            if (n + sl + 2 < sizeof(buf)) {
              buf[n++] = '"';
              memcpy(buf + n, s, sl);
              n += sl;
              buf[n++] = '"';
            }
            free(s);
          }
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
      rn->value = strdup(buf);
    } else {
      /* render card title="Hello" — value=name, attrs as props */
      rn->value = nm;
      parser_parse_attrs(p, rn);
      /* rewrite attrs into call: card(title) style — keep as name + attrs
         for backend to emit {@render card({ title: "Hello" })} or
         {@render card("Hello")} when single positional-like attr */
    }
  }
  while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
  return rn;
}

Node *parser_parse_foreign(Parser *p) {
  Token tok = parser_advance(p); /* foreign */
  if (parser_peek(p).type != TOKEN_IDENTIFIER) {
    Token t = parser_peek(p);
    parser_fail(p, "expected component name after foreign", t.line, t.col);
    return NULL;
  }
  Token name = parser_advance(p);
  char *nm = parser_token_str(name);
  Node *fn = node_adopt(NODE_FOREIGN, nm, tok.line, tok.col);

  /* Optional: from "module" [for backend] */
  if (parser_peek(p).type == TOKEN_IDENTIFIER) {
    char *kw = parser_token_str(parser_peek(p));
    if (kw && strcmp(kw, "from") == 0) {
      parser_advance(p);
      free(kw);
      if (parser_peek(p).type == TOKEN_STRING) {
        Token mod = parser_advance(p);
        fn->value2 = parser_token_str(mod);
      }
      if (parser_peek(p).type == TOKEN_IDENTIFIER) {
        char *fk = parser_token_str(parser_peek(p));
        if (fk && strcmp(fk, "for") == 0) {
          parser_advance(p);
          free(fk);
          if (parser_peek(p).type == TOKEN_IDENTIFIER) {
            Token be = parser_advance(p);
            char *ben = parser_token_str(be);
            Node *a = node_create(NODE_ATTR, ben, be.line, be.col);
            free(ben);
            if (a && fn->value2) a->value2 = strdup(fn->value2);
            if (a) node_add_child(fn, a);
          }
        } else {
          free(fk);
        }
      }
    } else {
      free(kw);
    }
  }

  while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
  if (parser_peek(p).type == TOKEN_INDENT) {
    parser_advance(p);
    while (parser_peek(p).type != TOKEN_DEDENT && parser_peek(p).type != TOKEN_EOF) {
      while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
      if (parser_peek(p).type == TOKEN_DEDENT || parser_peek(p).type == TOKEN_EOF) break;
      Token id = parser_peek(p);
      if (id.type != TOKEN_IDENTIFIER) {
        parser_advance(p);
        continue;
      }
      char *k = parser_token_str(id);
      if (!k) {
        parser_advance(p);
        continue;
      }
      if (strcmp(k, "props") == 0) {
        free(k);
        Node *props = parser_parse_stmt(p);
        if (props) node_add_child(fn, props);
        continue;
      }
      /* backend from "module" [as { Export }] — as clause ignored in MVP */
      if (strcmp(k, "react") == 0 || strcmp(k, "svelte") == 0 ||
          strcmp(k, "vue") == 0 || strcmp(k, "solid") == 0) {
        Token be = parser_advance(p);
        free(k);
        char *fromkw = NULL;
        if (parser_peek(p).type == TOKEN_IDENTIFIER) {
          fromkw = parser_token_str(parser_peek(p));
          if (fromkw && strcmp(fromkw, "from") == 0) {
            parser_advance(p);
          }
          free(fromkw);
        }
        char *mod = NULL;
        if (parser_peek(p).type == TOKEN_STRING) {
          Token m = parser_advance(p);
          mod = parser_token_str(m);
        }
        /* skip optional: as { Name } */
        if (parser_peek(p).type == TOKEN_IDENTIFIER) {
          char *ask = parser_token_str(parser_peek(p));
          if (ask && strcmp(ask, "as") == 0) {
            parser_advance(p);
            free(ask);
            while (parser_peek(p).type != TOKEN_NEWLINE && parser_peek(p).type != TOKEN_EOF &&
                   parser_peek(p).type != TOKEN_DEDENT)
              parser_advance(p);
          } else {
            free(ask);
          }
        }
        char *ben = parser_token_str(be);
        Node *a = node_adopt(NODE_ATTR, ben, be.line, be.col);
        if (a) {
          a->value2 = mod;
          node_add_child(fn, a);
        } else {
          free(ben);
          free(mod);
        }
        while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
        continue;
      }
      free(k);
      Node *other = parser_parse_stmt(p);
      if (other) node_add_child(fn, other);
    }
    if (parser_peek(p).type == TOKEN_DEDENT) parser_advance(p);
  }
  return fn;
}

Node *parser_parse_empty(Parser *p) {
  Token tok = parser_advance(p); /* empty */
  Node *em = node_create(NODE_EMPTY, NULL, tok.line, tok.col);
  if (parser_peek(p).type == TOKEN_IDENTIFIER) {
    Token maybe = parser_peek(p);
    char *k = parser_token_str(maybe);
    if (strcmp(k, "if") == 0) {
      parser_advance(p);
      free(k);
      if (parser_match(p, TOKEN_EQUALS)) {
        em->value = parser_capture_expr_line(p);
      } else if (parser_peek(p).type == TOKEN_IDENTIFIER) {
        /* empty if items → treat as bare identifier condition */
        Token c = parser_advance(p);
        em->value = parser_token_str(c);
      }
    } else {
      free(k);
    }
  }
  while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
  if (parser_peek(p).type == TOKEN_INDENT) {
    Node *body = parser_parse_body(p);
    if (body) {
      for (size_t i = 0; i < body->children_len; i++) {
        node_add_child(em, body->children[i]);
        body->children[i] = NULL;
      }
      body->children_len = 0;
      node_free(body);
    }
  }
  return em;
}
