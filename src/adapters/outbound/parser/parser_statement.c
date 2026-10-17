#include "adapters/outbound/parser/parser_internal.h"

Node *parser_parse_stmt(Parser *p) {
  while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);

  if (parser_peek(p).type == TOKEN_EOF) return NULL;
  if (parser_peek(p).type == TOKEN_DEDENT) return NULL;
  if (parser_peek(p).type == TOKEN_INDENT) { parser_advance(p); return NULL; }

  Token check = parser_peek(p);

  /* @render name(args) — Phase E snippets */
  if (check.type == TOKEN_AT) {
    parser_advance(p);
    if (parser_peek(p).type == TOKEN_IDENTIFIER) {
      Token id = parser_peek(p);
      char *idk = parser_token_str(id);
      if (idk && strcmp(idk, "render") == 0) {
        free(idk);
        return parser_parse_render(p);
      }
      free(idk);
    }
    return NULL;
  }

  if (check.type != TOKEN_IDENTIFIER) {
    parser_advance(p);
    return NULL;
  }

  char *kw = parser_token_str(check);

  if (strcmp(kw, "def") == 0) {
    free(kw);
    return parser_parse_def(p);
  } else if (strcmp(kw, "use") == 0 || strcmp(kw, "import") == 0) {
    free(kw);
    return parser_parse_use(p);
  } else if (strcmp(kw, "effect") == 0 || strcmp(kw, "layoutEffect") == 0 ||
             strcmp(kw, "insertionEffect") == 0 || strcmp(kw, "effectEvent") == 0 ||
             strcmp(kw, "externalStore") == 0 || strcmp(kw, "syncStore") == 0 ||
             strcmp(kw, "imperativeHandle") == 0 ||
             strcmp(kw, "ref") == 0 || strcmp(kw, "context") == 0 ||
             strcmp(kw, "ctx") == 0 || strcmp(kw, "reducer") == 0 ||
             strcmp(kw, "params") == 0 || strcmp(kw, "navigate") == 0 ||
             strcmp(kw, "callback") == 0 || strcmp(kw, "memo") == 0 ||
             strcmp(kw, "id") == 0 || strcmp(kw, "transition") == 0 ||
             strcmp(kw, "deferred") == 0 || strcmp(kw, "action") == 0 ||
             strcmp(kw, "fetch") == 0 || strcmp(kw, "load") == 0 ||
             strcmp(kw, "lazy") == 0 || strcmp(kw, "suspense") == 0 ||
             strcmp(kw, "portal") == 0 || strcmp(kw, "errorBoundary") == 0 ||
             strcmp(kw, "provide") == 0 || strcmp(kw, "loading") == 0) {
    free(kw);
    return parser_parse_react_decl(p);
  } else if (strcmp(kw, "await") == 0) {
    free(kw);
    return parser_parse_await(p);
  } else if (strcmp(kw, "snippet") == 0) {
    free(kw);
    return parser_parse_snippet(p);
  } else if (strcmp(kw, "store") == 0 || strcmp(kw, "writable") == 0) {
    free(kw);
    return parser_parse_store(p);
  } else if (strcmp(kw, "render") == 0) {
    free(kw);
    return parser_parse_render(p);
  } else if (strcmp(kw, "title") == 0 || strcmp(kw, "head") == 0) {
    free(kw);
    return parser_parse_title_or_head(p);
  } else if (strcmp(kw, "empty") == 0) {
    free(kw);
    return parser_parse_empty(p);
  } else if (strcmp(kw, "props") == 0) {
    /* Top-level props (body-only component files) */
    free(kw);
    Token tok = parser_advance(p);
    Node *props = node_create(NODE_PROPS_DECL, NULL, tok.line, tok.col);
    while (parser_peek(p).type == TOKEN_IDENTIFIER || parser_peek(p).type == TOKEN_COMMA) {
      if (parser_peek(p).type == TOKEN_COMMA) {
        parser_advance(p);
        continue;
      }
      node_add_child(props, parser_parse_prop_node(p));
    }
    while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
    return props;
  } else if (strcmp(kw, "state") == 0) {
    free(kw);
    Token tok = parser_advance(p);
    Node *state = node_create(NODE_STATE_DECL, NULL, tok.line, tok.col);
    while (parser_peek(p).type == TOKEN_IDENTIFIER || parser_peek(p).type == TOKEN_COMMA) {
      if (parser_peek(p).type == TOKEN_COMMA) {
        parser_advance(p);
        continue;
      }
      Token st = parser_advance(p);
      Node *st_node = node_adopt(NODE_STATE_DECL, parser_token_str(st), st.line, st.col);
      if (parser_match(p, TOKEN_EQUALS)) st_node->value2 = parser_parse_default_value(p);
      node_add_child(state, st_node);
    }
    while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
    return state;
  } else if (strcmp(kw, "computed") == 0) {
    free(kw);
    Token tok = parser_advance(p);
    (void)tok;
    if (parser_peek(p).type == TOKEN_IDENTIFIER) {
      Token comp = parser_advance(p);
      Node *computed =
          node_adopt(NODE_COMPUTED_DECL, parser_token_str(comp), comp.line, comp.col);
      if (parser_match(p, TOKEN_EQUALS)) {
        char expr[512] = {0};
        size_t elen = 0;
        while (parser_peek(p).type != TOKEN_NEWLINE && parser_peek(p).type != TOKEN_EOF &&
               parser_peek(p).type != TOKEN_DEDENT) {
          Token t = parser_advance(p);
          char *s = parser_token_str(t);
          if (s) {
            size_t slen = strlen(s);
            if (elen + slen + 2 < sizeof(expr)) {
              if (elen > 0) expr[elen++] = ' ';
              memcpy(expr + elen, s, slen);
              elen += slen;
            }
            free(s);
          }
        }
        if (elen > 0) computed->value2 = strdup(expr);
      }
      while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
      return computed;
    }
    return NULL;
  } else if (strcmp(kw, "layout") == 0) {
    free(kw);
    return parser_parse_layout(p);
  } else if (strcmp(kw, "theme") == 0) {
    free(kw);
    return parser_parse_theme(p);
  } else if (strcmp(kw, "route") == 0) {
    free(kw);
    return parser_parse_route(p);
  } else if (strcmp(kw, "foreign") == 0) {
    free(kw);
    return parser_parse_foreign(p);
  } else if (strcmp(kw, "slot") == 0) {
    free(kw);
    Token tok = parser_advance(p);
    Node *slot = node_create(NODE_SLOT, NULL, tok.line, tok.col);
    while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
    return slot;
  } else if (strcmp(kw, "for") == 0) {
    free(kw);
    return parser_parse_for(p);
  } else if (strcmp(kw, "if") == 0) {
    free(kw);
    return parser_parse_if(p);
  } else if (strcmp(kw, "else") == 0) {
    free(kw);
    Node *else_marker = node_create(NODE_TEXT, "__else__", check.line, check.col);
    (void)parser_advance(p);
    if (parser_peek(p).type == TOKEN_NEWLINE) {
      parser_advance(p);
      Node *body = parser_parse_body(p);
      if (body) {
        for (size_t i = 0; i < body->children_len; i++) {
          node_add_child(else_marker, body->children[i]);
        }
        body->children_len = 0;
        node_free(body);
      }
    }
    return else_marker;
  } else {
    free(kw);
    return parser_parse_element(p);
  }
}
