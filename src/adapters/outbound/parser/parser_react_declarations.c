#include "adapters/outbound/parser/parser_internal.h"

Node *parser_parse_react_declarations(Parser *p, Token kw_tok, char *kw) {
if (strcmp(kw, "ref") == 0) {
    free(kw);
    Node *ref = node_create(NODE_REF_DECL, NULL, kw_tok.line, kw_tok.col);
    if (parser_peek(p).type == TOKEN_IDENTIFIER) {
      Token n = parser_advance(p);
      ref->value = parser_token_str(n);
    }
    if (parser_match(p, TOKEN_EQUALS)) ref->value2 = parser_parse_default_value(p);
    while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
    return ref;
  }

  if (strcmp(kw, "context") == 0) {
    free(kw);
    /* context Theme [ = default ]  → createContext */
    Node *ctx = node_create(NODE_CONTEXT_DECL, NULL, kw_tok.line, kw_tok.col);
    if (parser_peek(p).type == TOKEN_IDENTIFIER) {
      Token n = parser_advance(p);
      ctx->value = parser_token_str(n);
    }
    if (parser_match(p, TOKEN_EQUALS)) ctx->value2 = parser_parse_default_value(p);
    while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
    return ctx;
  }

  if (strcmp(kw, "ctx") == 0) {
    free(kw);
    /* ctx theme = Theme  → useContext(Theme) */
    Node *use = node_create(NODE_CONTEXT_USE, NULL, kw_tok.line, kw_tok.col);
    if (parser_peek(p).type == TOKEN_IDENTIFIER) {
      Token n = parser_advance(p);
      use->value = parser_token_str(n);
    }
    if (parser_match(p, TOKEN_EQUALS)) {
      if (parser_peek(p).type == TOKEN_IDENTIFIER) {
        Token c = parser_advance(p);
        use->value2 = parser_token_str(c);
      }
    }
    while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
    return use;
  }

  if (strcmp(kw, "reducer") == 0) {
    free(kw);
    /* reducer cart = cartReducer init=[] */
    Node *red = node_create(NODE_REDUCER_DECL, NULL, kw_tok.line, kw_tok.col);
    if (parser_peek(p).type == TOKEN_IDENTIFIER) {
      Token n = parser_advance(p);
      red->value = parser_token_str(n);
    }
    if (parser_match(p, TOKEN_EQUALS)) {
      if (parser_peek(p).type == TOKEN_IDENTIFIER) {
        Token fn = parser_advance(p);
        red->value2 = parser_token_str(fn);
      }
    }
    if (parser_peek(p).type == TOKEN_IDENTIFIER) {
      Token maybe = parser_peek(p);
      char *k = parser_token_str(maybe);
      if (strcmp(k, "init") == 0) {
        parser_advance(p);
        if (parser_match(p, TOKEN_EQUALS)) {
          Node *init = node_create(NODE_ATTR, "init", maybe.line, maybe.col);
          /* always capture full expr so init=[] works */
          init->value2 = parser_capture_expr_line(p);
          node_add_child(red, init);
        }
      }
      free(k);
    }
    while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
    return red;
  }

  if (strcmp(kw, "params") == 0) {
    free(kw);
    /* params id, slug */
    Node *par = node_create(NODE_PARAMS_DECL, NULL, kw_tok.line, kw_tok.col);
    char names[256] = {0};
    size_t nn = 0;
    while (parser_peek(p).type == TOKEN_IDENTIFIER || parser_peek(p).type == TOKEN_COMMA) {
      if (parser_peek(p).type == TOKEN_COMMA) {
        parser_advance(p);
        if (nn + 2 < sizeof(names)) {
          names[nn++] = ',';
          names[nn++] = ' ';
        }
        continue;
      }
      Token t = parser_advance(p);
      char *s = parser_token_str(t);
      if (s) {
        size_t sl = strlen(s);
        if (nn + sl < sizeof(names)) {
          memcpy(names + nn, s, sl);
          nn += sl;
        }
        free(s);
      }
    }
    par->value = strdup(names);
    while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
    return par;
  }

  if (strcmp(kw, "navigate") == 0) {
    free(kw);
    Node *nav = node_create(NODE_NAVIGATE_DECL, "navigate", kw_tok.line, kw_tok.col);
    if (parser_peek(p).type == TOKEN_IDENTIFIER) {
      Token n = parser_advance(p);
      free(nav->value);
      nav->value = parser_token_str(n);
    }
    while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
    return nav;
  }

  if (strcmp(kw, "callback") == 0) {
    free(kw);
    /* callback onAdd = (id) => add(id) deps=(x) */
    Node *cb = node_create(NODE_CALLBACK_DECL, NULL, kw_tok.line, kw_tok.col);
    if (parser_peek(p).type == TOKEN_IDENTIFIER) {
      Token n = parser_advance(p);
      cb->value = parser_token_str(n);
    }
    static const char *cb_stop[] = {"deps", NULL};
    if (parser_match(p, TOKEN_EQUALS))
      cb->value2 = parser_capture_expr_line_stop(p, cb_stop);
    if (parser_peek(p).type == TOKEN_IDENTIFIER) {
      Token maybe = parser_peek(p);
      char *k = parser_token_str(maybe);
      if (strcmp(k, "deps") == 0) {
        parser_advance(p);
        if (parser_match(p, TOKEN_EQUALS)) {
          Node *d = node_create(NODE_ATTR, "deps", maybe.line, maybe.col);
          if (parser_peek(p).type == TOKEN_LPAREN) {
            parser_advance(p);
            char deps[256] = {0};
            size_t dn = 0;
            while (parser_peek(p).type != TOKEN_RPAREN && parser_peek(p).type != TOKEN_EOF) {
              if (parser_peek(p).type == TOKEN_COMMA) {
                parser_advance(p);
                if (dn + 2 < sizeof(deps)) {
                  deps[dn++] = ',';
                  deps[dn++] = ' ';
                }
                continue;
              }
              Token t = parser_advance(p);
              char *s = parser_token_str(t);
              if (s) {
                size_t sl = strlen(s);
                if (dn + sl < sizeof(deps)) {
                  memcpy(deps + dn, s, sl);
                  dn += sl;
                }
                free(s);
              }
            }
            if (parser_peek(p).type == TOKEN_RPAREN) parser_advance(p);
            d->value2 = strdup(deps);
          } else {
            d->value2 = parser_capture_expr_line(p);
          }
          node_add_child(cb, d);
        }
      }
      free(k);
    }
    while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
    return cb;
  }

  if (strcmp(kw, "memo") == 0) {
    free(kw);
    /* memo total = items.length  (alias of computed → useMemo) */
    Node *m = node_create(NODE_COMPUTED_DECL, NULL, kw_tok.line, kw_tok.col);
    if (parser_peek(p).type == TOKEN_IDENTIFIER) {
      Token n = parser_advance(p);
      m->value = parser_token_str(n);
    }
    if (parser_match(p, TOKEN_EQUALS)) m->value2 = parser_capture_expr_line(p);
    while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
    return m;
  }

  if (strcmp(kw, "id") == 0) {
    free(kw);
    Node *id = node_create(NODE_ID_DECL, NULL, kw_tok.line, kw_tok.col);
    if (parser_peek(p).type == TOKEN_IDENTIFIER) {
      Token n = parser_advance(p);
      id->value = parser_token_str(n);
    }
    while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
    return id;
  }

  if (strcmp(kw, "transition") == 0) {
    free(kw);
    /* transition isPending, startTransition */
    Node *tr = node_create(NODE_TRANSITION_DECL, "isPending", kw_tok.line, kw_tok.col);
    tr->value2 = strdup("startTransition");
    if (parser_peek(p).type == TOKEN_IDENTIFIER) {
      Token a = parser_advance(p);
      free(tr->value);
      tr->value = parser_token_str(a);
      if (parser_peek(p).type == TOKEN_COMMA) {
        parser_advance(p);
        if (parser_peek(p).type == TOKEN_IDENTIFIER) {
          Token b = parser_advance(p);
          free(tr->value2);
          tr->value2 = parser_token_str(b);
        }
      }
    }
    while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
    return tr;
  }

  if (strcmp(kw, "deferred") == 0) {
    free(kw);
    /* deferred deferredQuery = query */
    Node *d = node_create(NODE_DEFERRED_DECL, NULL, kw_tok.line, kw_tok.col);
    if (parser_peek(p).type == TOKEN_IDENTIFIER) {
      Token n = parser_advance(p);
      d->value = parser_token_str(n);
    }
    if (parser_match(p, TOKEN_EQUALS)) {
      if (parser_peek(p).type == TOKEN_IDENTIFIER) {
        Token s = parser_advance(p);
        d->value2 = parser_token_str(s);
      } else {
        d->value2 = parser_capture_expr_line(p);
      }
    }
    while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
    return d;
  }

  if (strcmp(kw, "action") == 0) {
    free(kw);
    /* action form = submitForm init=null  → useActionState */
    Node *act = node_create(NODE_ACTION_DECL, NULL, kw_tok.line, kw_tok.col);
    if (parser_peek(p).type == TOKEN_IDENTIFIER) {
      Token n = parser_advance(p);
      act->value = parser_token_str(n);
    }
    if (parser_match(p, TOKEN_EQUALS)) {
      if (parser_peek(p).type == TOKEN_IDENTIFIER) {
        Token fn = parser_advance(p);
        act->value2 = parser_token_str(fn);
      }
    }
    while (parser_peek(p).type == TOKEN_IDENTIFIER) {
      Token maybe = parser_peek(p);
      char *k = parser_token_str(maybe);
      if (strcmp(k, "init") == 0) {
        parser_advance(p);
        free(k);
        if (parser_match(p, TOKEN_EQUALS)) {
          Node *a = node_create(NODE_ATTR, "init", maybe.line, maybe.col);
          static const char *act_stop[] = {"pending", NULL};
          a->value2 = parser_capture_expr_line_stop(p, act_stop);
          node_add_child(act, a);
        }
      } else if (strcmp(k, "pending") == 0) {
        parser_advance(p);
        free(k);
        if (parser_match(p, TOKEN_EQUALS)) {
          Node *a = node_create(NODE_ATTR, "pending", maybe.line, maybe.col);
          if (parser_peek(p).type == TOKEN_IDENTIFIER) {
            Token v = parser_advance(p);
            a->value2 = parser_token_str(v);
          }
          node_add_child(act, a);
        }
      } else {
        free(k);
        break;
      }
    }
    while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
    return act;
  }

  
  return NULL;
}
