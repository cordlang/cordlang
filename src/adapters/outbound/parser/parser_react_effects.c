#include "adapters/outbound/parser/parser_internal.h"

Node *parser_parse_react_effects(Parser *p, Token kw_tok, char *kw) {
if (strcmp(kw, "effect") == 0 || strcmp(kw, "layoutEffect") == 0 ||
      strcmp(kw, "insertionEffect") == 0) {
    int is_layout = (strcmp(kw, "layoutEffect") == 0);
    int is_insertion = (strcmp(kw, "insertionEffect") == 0);
    free(kw);
    Node *eff = node_create(is_insertion ? NODE_INSERTION_EFFECT
                            : is_layout   ? NODE_LAYOUT_EFFECT
                                         : NODE_EFFECT_DECL,
                            NULL, kw_tok.line, kw_tok.col);
    /* effect deps=(a,b) [cleanup=fn] [run=expr] | indented body */
    while (parser_peek(p).type == TOKEN_IDENTIFIER) {
      Token k = parser_peek(p);
      char *key = parser_token_str(k);
      if (strcmp(key, "deps") == 0) {
        parser_advance(p);
        free(key);
        if (parser_match(p, TOKEN_EQUALS)) {
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
            eff->value = strdup(deps);
          } else {
            eff->value = parser_capture_expr_line(p);
          }
        }
      } else if (strcmp(key, "cleanup") == 0) {
        parser_advance(p);
        free(key);
        if (parser_match(p, TOKEN_EQUALS)) {
          Node *c = node_create(NODE_ATTR, "cleanup", k.line, k.col);
          c->value2 = parser_capture_expr_line(p);
          node_add_child(eff, c);
        }
      } else if (strcmp(key, "run") == 0) {
        parser_advance(p);
        free(key);
        if (parser_match(p, TOKEN_EQUALS)) eff->value2 = parser_capture_expr_line(p);
      } else {
        free(key);
        break;
      }
    }
    while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
    if (!eff->value2) {
      char *body = parser_capture_indent_block(p);
      if (body) eff->value2 = body;
    }
    return eff;
  }

  if (strcmp(kw, "effectEvent") == 0) {
    free(kw);
    /* effectEvent onMsg = handleMessage | effectEvent onMsg\n  body */
    Node *ev = node_create(NODE_EFFECT_EVENT, NULL, kw_tok.line, kw_tok.col);
    if (parser_peek(p).type == TOKEN_IDENTIFIER) {
      Token n = parser_advance(p);
      ev->value = parser_token_str(n);
    }
    if (parser_match(p, TOKEN_EQUALS)) {
      ev->value2 = parser_capture_expr_line(p);
    }
    while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
    if (!ev->value2) {
      char *body = parser_capture_indent_block(p);
      if (body) ev->value2 = body;
    }
    return ev;
  }

  if (strcmp(kw, "externalStore") == 0 || strcmp(kw, "syncStore") == 0) {
    free(kw);
    /* externalStore snapshot = subscribe getSnapshot [getServerSnapshot=null]
     * syncStore selected = store.subscribe store.getSnapshot */
    Node *es = node_create(NODE_EXTERNAL_STORE, NULL, kw_tok.line, kw_tok.col);
    if (parser_peek(p).type == TOKEN_IDENTIFIER) {
      Token n = parser_advance(p);
      es->value = parser_token_str(n);
    }
    if (parser_match(p, TOKEN_EQUALS)) {
      es->value2 = parser_capture_dotted_ident(p);
      if (!es->value2) es->value2 = parser_capture_expr_line(p);
      /* second arg: getSnapshot */
      if (parser_peek(p).type == TOKEN_IDENTIFIER) {
        Token maybe = parser_peek(p);
        char *k = parser_token_str(maybe);
        if (k && strcmp(k, "getServerSnapshot") != 0) {
          free(k);
          Node *gs = node_create(NODE_ATTR, "getSnapshot", maybe.line, maybe.col);
          gs->value2 = parser_capture_dotted_ident(p);
          if (!gs->value2) gs->value2 = parser_capture_expr_line(p);
          node_add_child(es, gs);
        } else {
          free(k);
        }
      }
      /* optional getServerSnapshot=... */
      if (parser_peek(p).type == TOKEN_IDENTIFIER) {
        Token maybe = parser_peek(p);
        char *k = parser_token_str(maybe);
        if (k && strcmp(k, "getServerSnapshot") == 0) {
          parser_advance(p);
          free(k);
          if (parser_match(p, TOKEN_EQUALS)) {
            Node *ss =
                node_create(NODE_ATTR, "getServerSnapshot", maybe.line, maybe.col);
            ss->value2 = parser_capture_expr_line(p);
            node_add_child(es, ss);
          }
        } else {
          free(k);
        }
      }
    }
    while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
    return es;
  }

  if (strcmp(kw, "imperativeHandle") == 0) {
    free(kw);
    /* imperativeHandle [ref=inputEl] [deps=(...)]
     *   focus: () => ...
     */
    Node *ih =
        node_create(NODE_IMPERATIVE_HANDLE, "ref", kw_tok.line, kw_tok.col);
    while (parser_peek(p).type == TOKEN_IDENTIFIER) {
      Token maybe = parser_peek(p);
      char *k = parser_token_str(maybe);
      if (strcmp(k, "ref") == 0) {
        parser_advance(p);
        free(k);
        if (parser_match(p, TOKEN_EQUALS)) {
          if (parser_peek(p).type == TOKEN_IDENTIFIER) {
            Token v = parser_advance(p);
            free(ih->value);
            ih->value = parser_token_str(v);
          }
        }
      } else if (strcmp(k, "deps") == 0) {
        parser_advance(p);
        free(k);
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
          node_add_child(ih, d);
        }
      } else {
        free(k);
        break;
      }
    }
    while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
    {
      char *body = parser_capture_indent_object_body(p);
      if (body) ih->value2 = body;
    }
    return ih;
  }

  
  return NULL;
}
