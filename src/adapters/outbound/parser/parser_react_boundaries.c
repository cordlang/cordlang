#include "adapters/outbound/parser/parser_internal.h"

Node *parser_parse_react_boundaries(Parser *p, Token kw_tok, char *kw) {
if (strcmp(kw, "errorBoundary") == 0) {
    free(kw);
    Node *eb = node_create(NODE_ERROR_BOUNDARY, NULL, kw_tok.line, kw_tok.col);
    while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
    if (parser_peek(p).type == TOKEN_INDENT) {
      parser_advance(p);
      while (parser_peek(p).type != TOKEN_DEDENT && parser_peek(p).type != TOKEN_EOF) {
        while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
        if (parser_peek(p).type == TOKEN_DEDENT || parser_peek(p).type == TOKEN_EOF) break;
        if (parser_peek(p).type == TOKEN_IDENTIFIER) {
          Token t = parser_peek(p);
          char *k = parser_token_str(t);
          if (strcmp(k, "fallback") == 0) {
            parser_advance(p);
            free(k);
            while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
            Node *fb = node_create(NODE_ELEMENT, "__fallback__", t.line, t.col);
            if (parser_peek(p).type == TOKEN_INDENT) {
              Node *body = parser_parse_body(p);
              if (body) {
                for (size_t i = 0; i < body->children_len; i++) {
                  node_add_child(fb, body->children[i]);
                  body->children[i] = NULL;
                }
                body->children_len = 0;
                node_free(body);
              }
            } else {
              Node *one = parser_parse_stmt(p);
              if (one) node_add_child(fb, one);
            }
            node_add_child(eb, fb);
            continue;
          }
          free(k);
        }
        Node *child = parser_parse_stmt(p);
        if (child) node_add_child(eb, child);
        else if (parser_peek(p).type != TOKEN_DEDENT && parser_peek(p).type != TOKEN_EOF)
          parser_advance(p);
      }
      if (parser_peek(p).type == TOKEN_DEDENT) parser_advance(p);
    }
    return eb;
  }

  if (strcmp(kw, "provide") == 0) {
    free(kw);
    /* provide Theme value=dark → <Theme.Provider> */
    Node *el = node_create(NODE_ELEMENT, "provide", kw_tok.line, kw_tok.col);
    if (parser_peek(p).type == TOKEN_IDENTIFIER) {
      Token ctx = parser_advance(p);
      Node *attr = node_create(NODE_ATTR, "context", ctx.line, ctx.col);
      attr->value2 = parser_token_str(ctx);
      node_add_child(el, attr);
    }
    parser_parse_attrs(p, el);
    if (parser_peek(p).type == TOKEN_NEWLINE) {
      parser_advance(p);
      Node *body = parser_parse_body(p);
      if (body) {
        for (size_t i = 0; i < body->children_len; i++) {
          Node *ch = body->children[i];
          body->children[i] = NULL;
          node_add_child(el, ch);
        }
        body->children_len = 0;
        node_free(body);
      }
    }
    return el;
  }

  if (strcmp(kw, "loading") == 0) {
    free(kw);
    /* loading [fallback=...] | indented fallback block + children → Suspense-like */
    Node *ld = node_create(NODE_LOADING, NULL, kw_tok.line, kw_tok.col);
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
          node_add_child(ld, fb);
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
        if (parser_peek(p).type == TOKEN_IDENTIFIER) {
          Token t = parser_peek(p);
          char *k = parser_token_str(t);
          if (strcmp(k, "fallback") == 0) {
            parser_advance(p);
            free(k);
            while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
            Node *fb_wrap =
                node_create(NODE_ELEMENT, "__fallback__", t.line, t.col);
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
            node_add_child(ld, fb_wrap);
            continue;
          }
          free(k);
        }
        Node *child = parser_parse_stmt(p);
        if (child) node_add_child(ld, child);
        else if (parser_peek(p).type != TOKEN_DEDENT && parser_peek(p).type != TOKEN_EOF)
          parser_advance(p);
      }
      if (parser_peek(p).type == TOKEN_DEDENT) parser_advance(p);
    }
    return ld;
  }


  return NULL;
}
