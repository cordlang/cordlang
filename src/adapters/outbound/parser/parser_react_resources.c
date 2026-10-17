#include "adapters/outbound/parser/parser_internal.h"

Node *parser_parse_react_resources(Parser *p, Token kw_tok, char *kw) {
if (strcmp(kw, "fetch") == 0 || strcmp(kw, "load") == 0) {
    free(kw);
    /* fetch products = "/api/products.json"
     * load products = fetchJson("/api/products")  → same NODE_FETCH_DECL */
    Node *fd = node_create(NODE_FETCH_DECL, NULL, kw_tok.line, kw_tok.col);
    if (parser_peek(p).type == TOKEN_IDENTIFIER) {
      Token n = parser_advance(p);
      fd->value = parser_token_str(n);
    }
    if (parser_match(p, TOKEN_EQUALS)) {
      /* String / path token: "/api/x.json" or /api/x.json */
      if (parser_peek(p).type == TOKEN_STRING) {
        Token u = parser_advance(p);
        fd->value2 = parser_token_str(u);
      } else if (parser_peek(p).type == TOKEN_IDENTIFIER) {
        Token maybe = parser_peek(p);
        char *fn = parser_token_str(maybe);
        /* load products = fetchJson("/url") → extract URL */
        if (fn && (strcmp(fn, "fetchJson") == 0 || strcmp(fn, "fetch") == 0)) {
          parser_advance(p);
          free(fn);
          if (parser_match(p, TOKEN_LPAREN)) {
            if (parser_peek(p).type == TOKEN_STRING) {
              Token u = parser_advance(p);
              fd->value2 = parser_token_str(u);
            }
            if (parser_peek(p).type == TOKEN_RPAREN) parser_advance(p);
          }
        } else {
          free(fn);
          fd->value2 = parser_capture_expr_line(p);
        }
      } else {
        fd->value2 = parser_capture_expr_line(p);
      }
    }
    while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
    return fd;
  }

  if (strcmp(kw, "lazy") == 0) {
    free(kw);
    /* lazy Chart = components/Chart */
    Node *lz = node_create(NODE_LAZY_DECL, NULL, kw_tok.line, kw_tok.col);
    if (parser_peek(p).type == TOKEN_IDENTIFIER) {
      Token n = parser_advance(p);
      lz->value = parser_token_str(n);
    }
    if (parser_match(p, TOKEN_EQUALS)) {
      char *path = parser_parse_module_path(p);
      lz->value2 = path;
    }
    while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
    return lz;
  }

  if (strcmp(kw, "suspense") == 0) {
    free(kw);
    /* suspense fallback=... | indented fallback block + children */
    Node *sus = node_create(NODE_SUSPENSE, NULL, kw_tok.line, kw_tok.col);
    if (parser_peek(p).type == TOKEN_IDENTIFIER) {
      Token maybe = parser_peek(p);
      char *k = parser_token_str(maybe);
      if (strcmp(k, "fallback") == 0) {
        parser_advance(p);
        if (parser_match(p, TOKEN_EQUALS)) {
          Node *fb = node_create(NODE_ATTR, "fallback", maybe.line, maybe.col);
          if (parser_peek(p).type == TOKEN_STRING) {
            Token v = parser_advance(p);
            fb->value2 = parser_token_str(v);
          } else {
            fb->value2 = parser_capture_expr_line(p);
          }
          node_add_child(sus, fb);
        }
      }
      free(k);
    }
    while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
    if (parser_peek(p).type == TOKEN_INDENT) {
      parser_advance(p);
      while (parser_peek(p).type != TOKEN_DEDENT && parser_peek(p).type != TOKEN_EOF) {
        while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
        if (parser_peek(p).type == TOKEN_DEDENT || parser_peek(p).type == TOKEN_EOF) break;
        /* special: fallback block as first child keyword */
        if (parser_peek(p).type == TOKEN_IDENTIFIER) {
          Token t = parser_peek(p);
          char *k = parser_token_str(t);
          if (strcmp(k, "fallback") == 0) {
            parser_advance(p);
            free(k);
            while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
            Node *fb_wrap = node_create(NODE_ELEMENT, "__fallback__", t.line, t.col);
            if (parser_peek(p).type == TOKEN_INDENT) {
              Node *body = parser_parse_body(p);
              if (body) {
                for (size_t i = 0; i < body->children_len; i++) {
                  node_add_child(fb_wrap, body->children[i]);
                  body->children[i] = NULL;
                }
                body->children_len = 0;
                node_free(body);
              }
            } else {
              Node *one = parser_parse_stmt(p);
              if (one) node_add_child(fb_wrap, one);
            }
            node_add_child(sus, fb_wrap);
            continue;
          }
          free(k);
        }
        Node *child = parser_parse_stmt(p);
        if (child) node_add_child(sus, child);
        else if (parser_peek(p).type != TOKEN_DEDENT && parser_peek(p).type != TOKEN_EOF)
          parser_advance(p);
      }
      if (parser_peek(p).type == TOKEN_DEDENT) parser_advance(p);
    }
    return sus;
  }

  if (strcmp(kw, "portal") == 0) {
    free(kw);
    /* portal to=document.body ...children */
    Node *por = node_create(NODE_PORTAL, "document.body", kw_tok.line, kw_tok.col);
    if (parser_peek(p).type == TOKEN_IDENTIFIER) {
      Token maybe = parser_peek(p);
      char *k = parser_token_str(maybe);
      if (strcmp(k, "to") == 0) {
        parser_advance(p);
        free(k);
        if (parser_match(p, TOKEN_EQUALS)) {
          free(por->value);
          por->value = parser_capture_expr_line(p);
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
          node_add_child(por, body->children[i]);
          body->children[i] = NULL;
        }
        body->children_len = 0;
        node_free(body);
      }
    }
    return por;
  }

  
  return NULL;
}
