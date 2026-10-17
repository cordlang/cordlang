#include "adapters/outbound/parser/parser_internal.h"

Node *parser_parse_layout(Parser *p) {
  (void)parser_advance(p); /* layout */
  const char *lname = "default";
  int line = parser_peek(p).line, col = parser_peek(p).col;
  if (parser_peek(p).type == TOKEN_IDENTIFIER) {
    Token name_tok = parser_advance(p);
    line = name_tok.line;
    col = name_tok.col;
    lname = NULL; /* will use parser_token_str */
    Node *layout = node_adopt(NODE_COMPONENT_DEF, parser_token_str(name_tok), line, col);
    layout->value2 = strdup("__layout__");

    while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
    if (parser_peek(p).type == TOKEN_INDENT) {
      parser_advance(p);
      while (parser_peek(p).type != TOKEN_DEDENT && parser_peek(p).type != TOKEN_EOF) {
        while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
        if (parser_peek(p).type == TOKEN_DEDENT || parser_peek(p).type == TOKEN_EOF) break;
        Node *stmt = parser_parse_stmt(p);
        if (stmt) node_add_child(layout, stmt);
      }
      if (parser_peek(p).type == TOKEN_DEDENT) parser_advance(p);
    }
    return layout;
  }

  Node *layout = node_create(NODE_COMPONENT_DEF, strdup(lname), line, col);
  layout->value2 = strdup("__layout__");
  while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
  if (parser_peek(p).type == TOKEN_INDENT) {
    parser_advance(p);
    while (parser_peek(p).type != TOKEN_DEDENT && parser_peek(p).type != TOKEN_EOF) {
      while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
      if (parser_peek(p).type == TOKEN_DEDENT || parser_peek(p).type == TOKEN_EOF) break;
      Node *stmt = parser_parse_stmt(p);
      if (stmt) node_add_child(layout, stmt);
    }
    if (parser_peek(p).type == TOKEN_DEDENT) parser_advance(p);
  }
  return layout;
}

Node *parser_parse_theme(Parser *p) {
  parser_advance(p);
  Token name_tok = parser_advance(p);
  Node *theme = node_adopt(NODE_THEME, parser_token_str(name_tok), name_tok.line, name_tok.col);

  while (parser_peek(p).type != TOKEN_DEDENT && parser_peek(p).type != TOKEN_EOF) {
    if (parser_peek(p).type == TOKEN_NEWLINE) { parser_advance(p); continue; }
    if (parser_peek(p).type == TOKEN_INDENT) { parser_advance(p); continue; }
    if (parser_peek(p).type == TOKEN_IDENTIFIER) {
      Token key = parser_advance(p);
      if (parser_match(p, TOKEN_COLON)) {
        if (parser_peek(p).type == TOKEN_STRING || parser_peek(p).type == TOKEN_IDENTIFIER || parser_peek(p).type == TOKEN_NUMBER) {
          Token val = parser_advance(p);
          char *val_str = parser_token_str(val);
          Node *entry = node_adopt(NODE_ATTR, parser_token_str(key), key.line, key.col);
          entry->value2 = val_str;
          node_add_child(theme, entry);
        }
      }
    }
  }

  return theme;
}

char *parser_parse_module_path(Parser *p) {
  char buf[512];
  size_t n = 0;
  buf[0] = '\0';

  if (parser_peek(p).type == TOKEN_STRING) {
    Token t = parser_advance(p);
    char *s = parser_token_str(t);
    if (!s) return strdup("");
    /* quoted path or bare URL-like path token */
    return s;
  }

  while (parser_peek(p).type == TOKEN_IDENTIFIER || parser_peek(p).type == TOKEN_DOT ||
         parser_peek(p).type == TOKEN_STRING || parser_peek(p).type == TOKEN_NUMBER) {
    Token t = parser_peek(p);
    if (t.type == TOKEN_IDENTIFIER && n > 0) {
      /* stop before keywords that follow the path */
      char *kw = parser_token_str(t);
      int stop = (strcmp(kw, "as") == 0 || strcmp(kw, "props") == 0 ||
                  strcmp(kw, "from") == 0 || strcmp(kw, "layout") == 0 ||
                  strcmp(kw, "lazy") == 0);
      free(kw);
      if (stop) break;
    }
    parser_advance(p);
    char *s = parser_token_str(t);
    if (!s && t.type == TOKEN_DOT) {
      if (n + 1 < sizeof(buf)) buf[n++] = '.';
      buf[n] = '\0';
      continue;
    }
    if (!s) continue;
    size_t sl = strlen(s);
    if (n + sl < sizeof(buf)) {
      memcpy(buf + n, s, sl);
      n += sl;
      buf[n] = '\0';
    }
    free(s);
  }
  return strdup(buf);
}

Node *parser_parse_use(Parser *p) {
  Token use_tok = parser_advance(p); /* use | import */
  Node *use = node_create(NODE_USE, NULL, use_tok.line, use_tok.col);

  /* import Name from path  OR  use path [as Alias] */
  char *first = parser_parse_module_path(p);
  if (!first || !*first) {
    free(first);
    fprintf(stderr, "Error: expected module path after use/import\n");
    return use;
  }

  if (parser_peek(p).type == TOKEN_IDENTIFIER) {
    Token maybe = parser_peek(p);
    char *kw = parser_token_str(maybe);
    if (strcmp(kw, "from") == 0) {
      /* import Name from path — first was the alias/name */
      parser_advance(p);
      free(kw);
      use->value2 = first; /* export name alias */
      use->value = parser_parse_module_path(p);
      while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
      return use;
    }
    if (strcmp(kw, "as") == 0) {
      parser_advance(p);
      free(kw);
      use->value = first;
      if (parser_peek(p).type == TOKEN_IDENTIFIER) {
        Token al = parser_advance(p);
        use->value2 = parser_token_str(al);
      }
      while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
      return use;
    }
    free(kw);
  }

  use->value = first;
  while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
  return use;
}

Node *parser_parse_route(Parser *p) {
  Token route_tok = parser_advance(p);
  Node *route = node_create(NODE_ROUTE, NULL, route_tok.line, route_tok.col);

  /* URL path: "/about" or /about */
  if (parser_peek(p).type == TOKEN_STRING || parser_peek(p).type == TOKEN_IDENTIFIER) {
    Token path = parser_advance(p);
    route->value = parser_token_str(path);
  }

  if (parser_match(p, TOKEN_ARROW)) {
    /* Component name OR module path (pages/HomePage) */
    char *target = parser_parse_module_path(p);
    route->value2 = target;
  }

  /* optional: props id, slug | layout=name | lazy */
  while (parser_peek(p).type == TOKEN_IDENTIFIER) {
    Token maybe = parser_peek(p);
    char *kw = parser_token_str(maybe);
    if (strcmp(kw, "props") == 0) {
      parser_advance(p);
      free(kw);
      Node *props = node_create(NODE_PROPS_DECL, NULL, maybe.line, maybe.col);
      while (parser_peek(p).type == TOKEN_IDENTIFIER || parser_peek(p).type == TOKEN_COMMA) {
        if (parser_peek(p).type == TOKEN_COMMA) {
          parser_advance(p);
          continue;
        }
        /* stop before layout=/lazy so "props id layout=shop" works */
        Token prop = parser_peek(p);
        char *pn = parser_token_str(prop);
        int stop = (pn && (strcmp(pn, "layout") == 0 || strcmp(pn, "lazy") == 0));
        free(pn);
        if (stop) break;
        parser_advance(p);
        node_add_child(props, node_adopt(NODE_TEXT, parser_token_str(prop), prop.line,
                                          prop.col));
      }
      node_add_child(route, props);
    } else if (strcmp(kw, "layout") == 0) {
      parser_advance(p);
      free(kw);
      if (parser_match(p, TOKEN_EQUALS)) {
        if (parser_peek(p).type == TOKEN_IDENTIFIER || parser_peek(p).type == TOKEN_STRING) {
          Token val = parser_advance(p);
          Node *attr = node_create(NODE_ATTR, "layout", maybe.line, maybe.col);
          attr->value2 = parser_token_str(val);
          node_add_child(route, attr);
        }
      }
    } else if (strcmp(kw, "lazy") == 0) {
      parser_advance(p);
      free(kw);
      node_add_child(route,
                     node_create(NODE_BOOL_ATTR, "lazy", maybe.line, maybe.col));
    } else {
      free(kw);
      break;
    }
  }

  while (parser_peek(p).type == TOKEN_NEWLINE) parser_advance(p);
  return route;
}
