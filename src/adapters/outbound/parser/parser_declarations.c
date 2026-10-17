#include "adapters/outbound/parser/parser_internal.h"

char *parser_parse_default_value(Parser *p) {
  if (parser_peek(p).type == TOKEN_STRING || parser_peek(p).type == TOKEN_NUMBER ||
      parser_peek(p).type == TOKEN_IDENTIFIER) {
    Token def_val = parser_advance(p);
    return parser_token_str(def_val);
  }
  return NULL;
}

Node *parser_parse_prop_node(Parser *p) {
  Token prop = parser_advance(p);
  Node *prop_node =
      node_adopt(NODE_TEXT, parser_token_str(prop), prop.line, prop.col);
  if (parser_match(p, TOKEN_COLON)) {
    if (parser_peek(p).type == TOKEN_IDENTIFIER) {
      Token ty = parser_advance(p);
      Node *ta = node_create(NODE_ATTR, "type", ty.line, ty.col);
      ta->value2 = parser_token_str(ty);
      node_add_child(prop_node, ta);
    }
  }
  if (parser_match(p, TOKEN_EQUALS)) prop_node->value2 = parser_parse_default_value(p);
  return prop_node;
}

Node *parser_parse_def(Parser *p) {
  (void)parser_advance(p); /* def */

  if (parser_peek(p).type != TOKEN_IDENTIFIER) {
    fprintf(stderr, "Expected component name after 'def'\n");
    return NULL;
  }

  Token name_tok = parser_advance(p);
  char *name = parser_token_str(name_tok);
  Node *def = node_adopt(NODE_COMPONENT_DEF, name, name_tok.line, name_tok.col);

  /* Optional flags after name: def FancyInput forwardRef */
  while (parser_peek(p).type == TOKEN_IDENTIFIER) {
    Token fl = parser_peek(p);
    char *flag = parser_token_str(fl);
    if (flag && strcmp(flag, "forwardRef") == 0) {
      parser_advance(p);
      Node *ba = node_create(NODE_BOOL_ATTR, "forwardRef", fl.line, fl.col);
      node_add_child(def, ba);
      free(flag);
    } else {
      free(flag);
      break;
    }
  }

  while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);

  if (parser_peek(p).type != TOKEN_INDENT) return def;
  parser_advance(p); /* INDENT */

  while (parser_peek(p).type != TOKEN_DEDENT && parser_peek(p).type != TOKEN_EOF) {
    while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
    if (parser_peek(p).type == TOKEN_DEDENT || parser_peek(p).type == TOKEN_EOF) break;
    if (parser_peek(p).type == TOKEN_INDENT) {
      parser_advance(p);
      continue;
    }

    if (parser_peek(p).type != TOKEN_IDENTIFIER) {
      parser_advance(p);
      continue;
    }

    Token check = parser_peek(p);
    char *kw = parser_token_str(check);

    if (strcmp(kw, "effect") == 0 || strcmp(kw, "layoutEffect") == 0 ||
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
      Node *hook = parser_parse_react_decl(p);
      if (hook) node_add_child(def, hook);
      continue;
    }
    /* Phase E — Svelte surface (await / snippet / store / render) */
    if (strcmp(kw, "await") == 0) {
      free(kw);
      Node *n = parser_parse_await(p);
      if (n) node_add_child(def, n);
      continue;
    }
    if (strcmp(kw, "snippet") == 0) {
      free(kw);
      Node *n = parser_parse_snippet(p);
      if (n) node_add_child(def, n);
      continue;
    }
    if (strcmp(kw, "store") == 0 || strcmp(kw, "writable") == 0) {
      free(kw);
      Node *n = parser_parse_store(p);
      if (n) node_add_child(def, n);
      continue;
    }
    if (strcmp(kw, "render") == 0) {
      free(kw);
      Node *n = parser_parse_render(p);
      if (n) node_add_child(def, n);
      continue;
    }
    if (strcmp(kw, "title") == 0 || strcmp(kw, "head") == 0) {
      free(kw);
      Node *h = parser_parse_title_or_head(p);
      if (h) node_add_child(def, h);
      continue;
    }
    if (strcmp(kw, "empty") == 0) {
      free(kw);
      Node *em = parser_parse_empty(p);
      if (em) node_add_child(def, em);
      continue;
    }

    if (strcmp(kw, "props") == 0) {
      Token tok = parser_advance(p);
      Node *props = node_create(NODE_PROPS_DECL, NULL, tok.line, tok.col);
      while (parser_peek(p).type == TOKEN_IDENTIFIER || parser_peek(p).type == TOKEN_COMMA) {
        if (parser_peek(p).type == TOKEN_COMMA) {
          parser_advance(p);
          continue;
        }
        node_add_child(props, parser_parse_prop_node(p));
      }
      node_add_child(def, props);
      free(kw);
      continue;
    }

    if (strcmp(kw, "state") == 0) {
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
      node_add_child(def, state);
      free(kw);
      continue;
    }

    if (strcmp(kw, "computed") == 0) {
      Token tok = parser_advance(p);
      free(kw);
      if (parser_peek(p).type == TOKEN_IDENTIFIER) {
        Token comp = parser_advance(p);
        Node *computed =
            node_adopt(NODE_COMPUTED_DECL, parser_token_str(comp), comp.line, comp.col);
        if (parser_match(p, TOKEN_EQUALS)) {
          /* Capture rest of line as expression */
          char expr[512] = {0};
          size_t elen = 0;
          while (parser_peek(p).type != TOKEN_NEWLINE && parser_peek(p).type != TOKEN_EOF &&
                 parser_peek(p).type != TOKEN_DEDENT && parser_peek(p).type != TOKEN_INDENT) {
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
        node_add_child(def, computed);
      }
      (void)tok;
      continue;
    }

    free(kw);
    /* UI body: element / for / if / else / nested def not expected */
    Node *stmt = parser_parse_stmt(p);
    if (stmt) node_add_child(def, stmt);
  }

  if (parser_peek(p).type == TOKEN_DEDENT) parser_advance(p);
  return def;
}
