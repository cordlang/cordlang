#include "adapters/outbound/parser/parser.h"
#include "adapters/outbound/lexer/lexer.h"
#include "domain/interp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

Parser *parser_create(Lexer *lexer) {
  Parser *p = calloc(1, sizeof(Parser));
  p->lexer = lexer;
  p->ast = ast_create();
  return p;
}

void parser_destroy(Parser *parser) {
  if (parser) {
    if (parser->ast) ast_free(parser->ast);
    free(parser);
  }
}

static Token peek(Parser *p) {
  return lexer_peek(p->lexer);
}

static Token advance(Parser *p) {
  Token t = lexer_next(p->lexer);
  p->current = t;
  return t;
}

static int match(Parser *p, TokenType type) {
  if (peek(p).type == type) {
    advance(p);
    return 1;
  }
  return 0;
}

static void parser_fail(Parser *p, const char *msg, int line, int col) {
  if (!p || p->had_error) return;
  p->had_error = 1;
  p->error_msg = msg;
  p->error_line = line > 0 ? line : 1;
  p->error_col = col > 0 ? col : 1;
}

static int consume(Parser *p, TokenType type, const char *msg) {
  if (peek(p).type == type) {
    advance(p);
    return 1;
  }
  Token t = peek(p);
  parser_fail(p, msg, t.line, t.col);
  return 0;
}

static char *token_str(Token t) {
  /* Allow empty strings (len == 0) e.g. props label="" */
  if (!t.start) return NULL;
  char *s = malloc(t.len + 1);
  if (!s) return NULL;
  if (t.type == TOKEN_STRING) {
    /* Unescape \" \\ \n \t \r inside string token span.
     * Keep \#{…} escaped so interp can emit literal "#{…}". */
    size_t j = 0;
    for (size_t i = 0; i < t.len; i++) {
      if (t.start[i] == '\\' && i + 1 < t.len) {
        i++;
        char e = t.start[i];
        if (e == 'n')
          s[j++] = '\n';
        else if (e == 't')
          s[j++] = '\t';
        else if (e == 'r')
          s[j++] = '\r';
        else if (e == '#' && i + 1 < t.len && t.start[i + 1] == '{') {
          s[j++] = '\\';
          s[j++] = '#';
        } else
          s[j++] = e;
      } else {
        s[j++] = t.start[i];
      }
    }
    s[j] = '\0';
    return s;
  }
  if (t.len > 0) memcpy(s, t.start, t.len);
  s[t.len] = '\0';
  return s;
}

static Node *parse_stmt(Parser *p);
static Node *parse_react_decl(Parser *p);
static Node *parse_title_or_head(Parser *p);
static Node *parse_empty(Parser *p);
static Node *parse_foreign(Parser *p);
static Node *parse_await(Parser *p);
static Node *parse_snippet(Parser *p);
static Node *parse_store(Parser *p);
static Node *parse_render(Parser *p);
static Node *parse_body(Parser *p);
static char *parse_default_value(Parser *p);

static Node *parse_style_map(Parser *p) {
  advance(p);
  Node *map = node_create(NODE_STYLE_MAP, NULL, p->current.line, p->current.col);

  while (peek(p).type != TOKEN_RPAREN && peek(p).type != TOKEN_EOF && peek(p).type != TOKEN_NEWLINE) {
    if (peek(p).type == TOKEN_IDENTIFIER) {
      Token key = advance(p);
      char *key_str = token_str(key);

      if (match(p, TOKEN_EQUALS)) {
        char *val_str = NULL;
        if (peek(p).type == TOKEN_IDENTIFIER || peek(p).type == TOKEN_NUMBER) {
          Token val = advance(p);
          val_str = token_str(val);
        } else if (peek(p).type == TOKEN_STRING) {
          Token val = advance(p);
          val_str = token_str(val);
        }

        Node *entry = node_adopt(NODE_STYLE_ENTRY, key_str, key.line, key.col);
        if (val_str) entry->value2 = val_str;
        node_add_child(map, entry);
      } else if (peek(p).type == TOKEN_IDENTIFIER || peek(p).type == TOKEN_NUMBER || peek(p).type == TOKEN_STRING) {
        Token val = advance(p);
        char *val_str = token_str(val);
        Node *entry = node_adopt(NODE_STYLE_ENTRY, key_str, key.line, key.col);
        entry->value2 = val_str;
        node_add_child(map, entry);
      } else {
        Node *entry = node_adopt(NODE_STYLE_ENTRY, key_str, key.line, key.col);
        entry->value2 = strdup("true");
        node_add_child(map, entry);
      }
    } else {
      break;
    }
  }

  consume(p, TOKEN_RPAREN, "Expected ')' to close style map");
  return map;
}

static Node *parse_attrs(Parser *p, Node *element) {
  while (peek(p).type == TOKEN_AT ||
         peek(p).type == TOKEN_IDENTIFIER ||
         peek(p).type == TOKEN_STRING) {

    if (peek(p).type == TOKEN_STRING) {
      Token str_tok = advance(p);
      char *str = token_str(str_tok);
      /* Expand "Hello #{name}!" into TEXT + INTERPOLATION children */
      interp_add_to_node(element, str ? str : "", str_tok.line, str_tok.col);
      free(str);
      continue;
    }

    if (peek(p).type == TOKEN_AT) {
      advance(p);
      if (peek(p).type == TOKEN_IDENTIFIER) {
        Token event = advance(p);
        char *event_name = token_str(event);
        Node *evt_node = node_adopt(NODE_EVENT, event_name, event.line, event.col);

        if (match(p, TOKEN_EQUALS)) {
          /* Capture full handler: name | name(...) | (expr) */
          char expr_buf[1024] = {0};
          size_t expr_len = 0;

          if (peek(p).type == TOKEN_LPAREN) {
            /* @click=(count + 1) or @click=(setCount(count+1)) */
            advance(p);
            int depth = 1;
            while (depth > 0 && peek(p).type != TOKEN_EOF && peek(p).type != TOKEN_NEWLINE) {
              if (peek(p).type == TOKEN_LPAREN) depth++;
              if (peek(p).type == TOKEN_RPAREN) {
                depth--;
                if (depth == 0) {
                  advance(p);
                  break;
                }
              }
              Token t = advance(p);
              char *s = token_str(t);
              if (s) {
                size_t slen = strlen(s);
                if (expr_len + slen + 2 < sizeof(expr_buf)) {
                  if (expr_len > 0 && t.type != TOKEN_DOT && t.type != TOKEN_COMMA &&
                      t.type != TOKEN_RPAREN && t.type != TOKEN_LPAREN) {
                    /* light spacing for binary ops written as separate tokens */
                    if (s[0] == '+' || s[0] == '-' || s[0] == '*' || s[0] == '/' ||
                        strcmp(s, "&&") == 0 || strcmp(s, "||") == 0) {
                      expr_buf[expr_len++] = ' ';
                      memcpy(expr_buf + expr_len, s, slen);
                      expr_len += slen;
                      expr_buf[expr_len++] = ' ';
                    } else {
                      memcpy(expr_buf + expr_len, s, slen);
                      expr_len += slen;
                    }
                  } else {
                    memcpy(expr_buf + expr_len, s, slen);
                    expr_len += slen;
                  }
                }
                free(s);
              } else if (t.type == TOKEN_LPAREN && expr_len + 1 < sizeof(expr_buf)) {
                expr_buf[expr_len++] = '(';
              } else if (t.type == TOKEN_RPAREN && expr_len + 1 < sizeof(expr_buf)) {
                expr_buf[expr_len++] = ')';
              } else if (t.type == TOKEN_COMMA && expr_len + 1 < sizeof(expr_buf)) {
                expr_buf[expr_len++] = ',';
              } else if (t.type == TOKEN_DOT && expr_len + 1 < sizeof(expr_buf)) {
                expr_buf[expr_len++] = '.';
              }
            }
            expr_buf[expr_len] = '\0';
            evt_node->value2 = strdup(expr_buf);
          } else if (peek(p).type == TOKEN_IDENTIFIER) {
            Token handler = advance(p);
            char *name = token_str(handler);
            if (peek(p).type == TOKEN_LPAREN) {
              /* @click=setCount(count + 1) */
              advance(p);
              size_t nlen = strlen(name);
              if (nlen + 1 < sizeof(expr_buf)) {
                memcpy(expr_buf, name, nlen);
                expr_len = nlen;
                expr_buf[expr_len++] = '(';
              }
              free(name);
              int depth = 1;
              while (depth > 0 && peek(p).type != TOKEN_EOF && peek(p).type != TOKEN_NEWLINE) {
                if (peek(p).type == TOKEN_LPAREN) {
                  depth++;
                  advance(p);
                  if (expr_len + 1 < sizeof(expr_buf)) expr_buf[expr_len++] = '(';
                  continue;
                }
                if (peek(p).type == TOKEN_RPAREN) {
                  depth--;
                  advance(p);
                  if (expr_len + 1 < sizeof(expr_buf)) expr_buf[expr_len++] = ')';
                  if (depth == 0) break;
                  continue;
                }
                if (peek(p).type == TOKEN_COMMA) {
                  advance(p);
                  if (expr_len + 2 < sizeof(expr_buf)) {
                    expr_buf[expr_len++] = ',';
                    expr_buf[expr_len++] = ' ';
                  }
                  continue;
                }
                if (peek(p).type == TOKEN_DOT) {
                  advance(p);
                  if (expr_len + 1 < sizeof(expr_buf)) expr_buf[expr_len++] = '.';
                  continue;
                }
                if (peek(p).type == TOKEN_ARROW) {
                  advance(p);
                  if (expr_len + 4 < sizeof(expr_buf)) {
                    expr_buf[expr_len++] = ' ';
                    expr_buf[expr_len++] = '=';
                    expr_buf[expr_len++] = '>';
                    expr_buf[expr_len++] = ' ';
                  }
                  continue;
                }
                if (peek(p).type == TOKEN_STRING) {
                  Token t = advance(p);
                  char *s = token_str(t);
                  size_t slen = s ? strlen(s) : 0;
                  if (expr_len + slen + 3 < sizeof(expr_buf)) {
                    expr_buf[expr_len++] = '"';
                    if (s && slen) {
                      memcpy(expr_buf + expr_len, s, slen);
                      expr_len += slen;
                    }
                    expr_buf[expr_len++] = '"';
                  }
                  free(s);
                  continue;
                }
                Token t = advance(p);
                char *s = token_str(t);
                if (s) {
                  size_t slen = strlen(s);
                  int is_op = (strcmp(s, "+") == 0 || strcmp(s, "-") == 0 ||
                               strcmp(s, "*") == 0);
                  if (expr_len + slen + 2 < sizeof(expr_buf)) {
                    if (is_op) {
                      if (expr_len > 0 && expr_buf[expr_len - 1] != ' ')
                        expr_buf[expr_len++] = ' ';
                      memcpy(expr_buf + expr_len, s, slen);
                      expr_len += slen;
                      expr_buf[expr_len++] = ' ';
                    } else {
                      memcpy(expr_buf + expr_len, s, slen);
                      expr_len += slen;
                    }
                  }
                  free(s);
                }
              }
              expr_buf[expr_len] = '\0';
              evt_node->value2 = strdup(expr_buf);
            } else {
              evt_node->value2 = name;
            }
          }
        }

        node_add_child(element, evt_node);
      }
      continue;
    }

    if (peek(p).type == TOKEN_IDENTIFIER) {
      Token key = advance(p);
      char *key_str = token_str(key);

      /* Responsive style attrs: sm:gap=8 / md:p=24 / lg:cols=3 */
      if (key_str &&
          (strcmp(key_str, "sm") == 0 || strcmp(key_str, "md") == 0 ||
           strcmp(key_str, "lg") == 0) &&
          peek(p).type == TOKEN_COLON) {
        advance(p); /* : */
        if (peek(p).type == TOKEN_IDENTIFIER) {
          Token rest = advance(p);
          char *rest_str = token_str(rest);
          if (rest_str) {
            size_t nlen = strlen(key_str) + 1 + strlen(rest_str) + 1;
            char *combined = malloc(nlen);
            if (combined) {
              snprintf(combined, nlen, "%s:%s", key_str, rest_str);
              free(key_str);
              free(rest_str);
              key_str = combined;
            } else {
              free(rest_str);
            }
          }
        }
      }

      if (strcmp(key_str, "for") == 0) {
        Node *loop = node_create(NODE_FOR, NULL, key.line, key.col);
        consume(p, TOKEN_IDENTIFIER, "Expected variable name after 'for'");
        Token var = p->current;
        loop->value = token_str(var);
        consume(p, TOKEN_IDENTIFIER, "Expected 'in' after for variable");
        {
          char *in_chk = token_str(p->current);
          int is_in = in_chk && strcmp(in_chk, "in") == 0;
          free(in_chk);
          Token tok = advance(p);
          loop->value2 = token_str(tok);
          (void)is_in; /* both branches took the next token historically */
        }
        node_add_child(element, loop);
        free(key_str);
        continue;
      }

      if (strcmp(key_str, "if") == 0) {
        Node *cond = node_create(NODE_IF, NULL, key.line, key.col);
        if (peek(p).type == TOKEN_IDENTIFIER) {
          Token tok = advance(p);
          cond->value = token_str(tok);
        }
        node_add_child(element, cond);
        free(key_str);
        continue;
      }

      if (strcmp(key_str, "else") == 0) {
        node_add_child(element, node_create(NODE_TEXT, "__else__", key.line, key.col));
        free(key_str);
        continue;
      }

      // Check if it's a boolean attribute or key=value
      if (match(p, TOKEN_EQUALS)) {
        if (peek(p).type == TOKEN_LPAREN) {
          Node *style_map = parse_style_map(p);
          style_map->value = key_str;
          node_add_child(element, style_map);
        } else if (peek(p).type == TOKEN_STRING) {
          Token val = advance(p);
          char *val_str = token_str(val);
          Node *attr = node_adopt(NODE_ATTR, key_str, key.line, key.col);
          attr->value2 = val_str;
          node_add_child(element, attr);
        } else if (peek(p).type == TOKEN_IDENTIFIER || peek(p).type == TOKEN_NUMBER) {
          Token val = advance(p);
          char *val_str = token_str(val);
          /* use=tooltip(opts) / transition=fade({duration:200}) — capture call */
          if (val_str && peek(p).type == TOKEN_LPAREN) {
            char buf[512];
            size_t n = 0;
            size_t vlen = strlen(val_str);
            if (vlen < sizeof(buf)) {
              memcpy(buf, val_str, vlen);
              n = vlen;
            }
            int depth = 0;
            while (peek(p).type != TOKEN_EOF && peek(p).type != TOKEN_NEWLINE) {
              Token t = advance(p);
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
              } else if (t.type == TOKEN_DOT) {
                if (n + 1 < sizeof(buf)) buf[n++] = '.';
              } else if (t.type == TOKEN_EQUALS) {
                if (n + 1 < sizeof(buf)) buf[n++] = '=';
              } else if (t.type == TOKEN_COLON) {
                if (n + 1 < sizeof(buf)) buf[n++] = ':';
              } else {
                char *s = token_str(t);
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
            free(val_str);
            val_str = strdup(buf);
          }
          Node *attr = node_adopt(NODE_ATTR, key_str, key.line, key.col);
          attr->value2 = val_str;
          node_add_child(element, attr);
        } else {
          Node *attr = node_adopt(NODE_ATTR, key_str, key.line, key.col);
          attr->value2 = strdup("true");
          node_add_child(element, attr);
        }
      } else if (match(p, TOKEN_ARROW)) {
        if (peek(p).type == TOKEN_IDENTIFIER) {
          Token val = advance(p);
          char *val_str = token_str(val);
          Node *attr = node_adopt(NODE_ATTR, key_str, key.line, key.col);
          attr->value2 = val_str;
          node_add_child(element, attr);
        }
      } else {
        Node *bool_attr = node_adopt(NODE_BOOL_ATTR, key_str, key.line, key.col);
        node_add_child(element, bool_attr);
      }
    }
  }

  return element;
}

static Node *parse_stmt(Parser *p);
static Node *parse_body(Parser *p) {
  if (peek(p).type == TOKEN_INDENT) {
    advance(p);
    Node *container = node_create(NODE_ROOT, NULL, 0, 0);
    while (peek(p).type != TOKEN_DEDENT && peek(p).type != TOKEN_EOF) {
      Node *child = parse_stmt(p);
      if (child) {
        node_add_child(container, child);
      } else {
        if (peek(p).type != TOKEN_DEDENT && peek(p).type != TOKEN_EOF) advance(p);
        else break;
      }
    }
    if (peek(p).type == TOKEN_DEDENT) advance(p);
    return container;
  }
  return NULL;
}

static Node *parse_element(Parser *p) {
  if (peek(p).type != TOKEN_IDENTIFIER) return NULL;

  Token tag_tok = advance(p);
  char *tag_name = token_str(tag_tok);
  Node *element = node_adopt(NODE_ELEMENT, tag_name, tag_tok.line, tag_tok.col);

  parse_attrs(p, element);

  if (peek(p).type == TOKEN_NEWLINE) {
    advance(p);
    Node *body = parse_body(p);
    if (body) {
      for (size_t i = 0; i < body->children_len; i++) {
        Node *child = body->children[i];
        body->children[i] = NULL;
        node_add_child(element, child);
      }
      node_free(body);
    }
  }

  return element;
}

/* Parse a default value after '=' for props/state */
static char *parse_default_value(Parser *p) {
  if (peek(p).type == TOKEN_STRING || peek(p).type == TOKEN_NUMBER ||
      peek(p).type == TOKEN_IDENTIFIER) {
    Token def_val = advance(p);
    return token_str(def_val);
  }
  return NULL;
}

/* props name [: type] [= default] — type stored as ATTR type=<name> */
static Node *parse_prop_node(Parser *p) {
  Token prop = advance(p);
  Node *prop_node =
      node_adopt(NODE_TEXT, token_str(prop), prop.line, prop.col);
  if (match(p, TOKEN_COLON)) {
    if (peek(p).type == TOKEN_IDENTIFIER) {
      Token ty = advance(p);
      Node *ta = node_create(NODE_ATTR, "type", ty.line, ty.col);
      ta->value2 = token_str(ty);
      node_add_child(prop_node, ta);
    }
  }
  if (match(p, TOKEN_EQUALS)) prop_node->value2 = parse_default_value(p);
  return prop_node;
}

static Node *parse_def(Parser *p) {
  (void)advance(p); /* def */

  if (peek(p).type != TOKEN_IDENTIFIER) {
    fprintf(stderr, "Expected component name after 'def'\n");
    return NULL;
  }

  Token name_tok = advance(p);
  char *name = token_str(name_tok);
  Node *def = node_adopt(NODE_COMPONENT_DEF, name, name_tok.line, name_tok.col);

  /* Optional flags after name: def FancyInput forwardRef */
  while (peek(p).type == TOKEN_IDENTIFIER) {
    Token fl = peek(p);
    char *flag = token_str(fl);
    if (flag && strcmp(flag, "forwardRef") == 0) {
      advance(p);
      Node *ba = node_create(NODE_BOOL_ATTR, "forwardRef", fl.line, fl.col);
      node_add_child(def, ba);
      free(flag);
    } else {
      free(flag);
      break;
    }
  }

  while (peek(p).type == TOKEN_NEWLINE) advance(p);

  if (peek(p).type != TOKEN_INDENT) return def;
  advance(p); /* INDENT */

  while (peek(p).type != TOKEN_DEDENT && peek(p).type != TOKEN_EOF) {
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
    if (peek(p).type == TOKEN_DEDENT || peek(p).type == TOKEN_EOF) break;
    if (peek(p).type == TOKEN_INDENT) {
      advance(p);
      continue;
    }

    if (peek(p).type != TOKEN_IDENTIFIER) {
      advance(p);
      continue;
    }

    Token check = peek(p);
    char *kw = token_str(check);

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
      Node *hook = parse_react_decl(p);
      if (hook) node_add_child(def, hook);
      continue;
    }
    /* Phase E — Svelte surface (await / snippet / store / render) */
    if (strcmp(kw, "await") == 0) {
      free(kw);
      Node *n = parse_await(p);
      if (n) node_add_child(def, n);
      continue;
    }
    if (strcmp(kw, "snippet") == 0) {
      free(kw);
      Node *n = parse_snippet(p);
      if (n) node_add_child(def, n);
      continue;
    }
    if (strcmp(kw, "store") == 0 || strcmp(kw, "writable") == 0) {
      free(kw);
      Node *n = parse_store(p);
      if (n) node_add_child(def, n);
      continue;
    }
    if (strcmp(kw, "render") == 0) {
      free(kw);
      Node *n = parse_render(p);
      if (n) node_add_child(def, n);
      continue;
    }
    if (strcmp(kw, "title") == 0 || strcmp(kw, "head") == 0) {
      free(kw);
      Node *h = parse_title_or_head(p);
      if (h) node_add_child(def, h);
      continue;
    }
    if (strcmp(kw, "empty") == 0) {
      free(kw);
      Node *em = parse_empty(p);
      if (em) node_add_child(def, em);
      continue;
    }

    if (strcmp(kw, "props") == 0) {
      Token tok = advance(p);
      Node *props = node_create(NODE_PROPS_DECL, NULL, tok.line, tok.col);
      while (peek(p).type == TOKEN_IDENTIFIER || peek(p).type == TOKEN_COMMA) {
        if (peek(p).type == TOKEN_COMMA) {
          advance(p);
          continue;
        }
        node_add_child(props, parse_prop_node(p));
      }
      node_add_child(def, props);
      free(kw);
      continue;
    }

    if (strcmp(kw, "state") == 0) {
      Token tok = advance(p);
      Node *state = node_create(NODE_STATE_DECL, NULL, tok.line, tok.col);
      while (peek(p).type == TOKEN_IDENTIFIER || peek(p).type == TOKEN_COMMA) {
        if (peek(p).type == TOKEN_COMMA) {
          advance(p);
          continue;
        }
        Token st = advance(p);
        Node *st_node = node_adopt(NODE_STATE_DECL, token_str(st), st.line, st.col);
        if (match(p, TOKEN_EQUALS)) st_node->value2 = parse_default_value(p);
        node_add_child(state, st_node);
      }
      node_add_child(def, state);
      free(kw);
      continue;
    }

    if (strcmp(kw, "computed") == 0) {
      Token tok = advance(p);
      free(kw);
      if (peek(p).type == TOKEN_IDENTIFIER) {
        Token comp = advance(p);
        Node *computed =
            node_adopt(NODE_COMPUTED_DECL, token_str(comp), comp.line, comp.col);
        if (match(p, TOKEN_EQUALS)) {
          /* Capture rest of line as expression */
          char expr[512] = {0};
          size_t elen = 0;
          while (peek(p).type != TOKEN_NEWLINE && peek(p).type != TOKEN_EOF &&
                 peek(p).type != TOKEN_DEDENT && peek(p).type != TOKEN_INDENT) {
            Token t = advance(p);
            char *s = token_str(t);
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
    Node *stmt = parse_stmt(p);
    if (stmt) node_add_child(def, stmt);
  }

  if (peek(p).type == TOKEN_DEDENT) advance(p);
  return def;
}

static Node *parse_layout(Parser *p) {
  (void)advance(p); /* layout */
  const char *lname = "default";
  int line = peek(p).line, col = peek(p).col;
  if (peek(p).type == TOKEN_IDENTIFIER) {
    Token name_tok = advance(p);
    line = name_tok.line;
    col = name_tok.col;
    lname = NULL; /* will use token_str */
    Node *layout = node_adopt(NODE_COMPONENT_DEF, token_str(name_tok), line, col);
    layout->value2 = strdup("__layout__");

    while (peek(p).type == TOKEN_NEWLINE) advance(p);
    if (peek(p).type == TOKEN_INDENT) {
      advance(p);
      while (peek(p).type != TOKEN_DEDENT && peek(p).type != TOKEN_EOF) {
        while (peek(p).type == TOKEN_NEWLINE) advance(p);
        if (peek(p).type == TOKEN_DEDENT || peek(p).type == TOKEN_EOF) break;
        Node *stmt = parse_stmt(p);
        if (stmt) node_add_child(layout, stmt);
      }
      if (peek(p).type == TOKEN_DEDENT) advance(p);
    }
    return layout;
  }

  Node *layout = node_create(NODE_COMPONENT_DEF, strdup(lname), line, col);
  layout->value2 = strdup("__layout__");
  while (peek(p).type == TOKEN_NEWLINE) advance(p);
  if (peek(p).type == TOKEN_INDENT) {
    advance(p);
    while (peek(p).type != TOKEN_DEDENT && peek(p).type != TOKEN_EOF) {
      while (peek(p).type == TOKEN_NEWLINE) advance(p);
      if (peek(p).type == TOKEN_DEDENT || peek(p).type == TOKEN_EOF) break;
      Node *stmt = parse_stmt(p);
      if (stmt) node_add_child(layout, stmt);
    }
    if (peek(p).type == TOKEN_DEDENT) advance(p);
  }
  return layout;
}

static Node *parse_theme(Parser *p) {
  advance(p);
  Token name_tok = advance(p);
  Node *theme = node_adopt(NODE_THEME, token_str(name_tok), name_tok.line, name_tok.col);

  while (peek(p).type != TOKEN_DEDENT && peek(p).type != TOKEN_EOF) {
    if (peek(p).type == TOKEN_NEWLINE) { advance(p); continue; }
    if (peek(p).type == TOKEN_INDENT) { advance(p); continue; }
    if (peek(p).type == TOKEN_IDENTIFIER) {
      Token key = advance(p);
      if (match(p, TOKEN_COLON)) {
        if (peek(p).type == TOKEN_STRING || peek(p).type == TOKEN_IDENTIFIER || peek(p).type == TOKEN_NUMBER) {
          Token val = advance(p);
          char *val_str = token_str(val);
          Node *entry = node_adopt(NODE_ATTR, token_str(key), key.line, key.col);
          entry->value2 = val_str;
          node_add_child(theme, entry);
        }
      }
    }
  }

  return theme;
}

/*
 * Module path: pages/HomePage | ./components/Counter | "pages/HomePage"
 * Reconstructs paths split by the lexer (ident + /segment path tokens).
 */
static char *parse_module_path(Parser *p) {
  char buf[512];
  size_t n = 0;
  buf[0] = '\0';

  if (peek(p).type == TOKEN_STRING) {
    Token t = advance(p);
    char *s = token_str(t);
    if (!s) return strdup("");
    /* quoted path or bare URL-like path token */
    return s;
  }

  while (peek(p).type == TOKEN_IDENTIFIER || peek(p).type == TOKEN_DOT ||
         peek(p).type == TOKEN_STRING || peek(p).type == TOKEN_NUMBER) {
    Token t = peek(p);
    if (t.type == TOKEN_IDENTIFIER && n > 0) {
      /* stop before keywords that follow the path */
      char *kw = token_str(t);
      int stop = (strcmp(kw, "as") == 0 || strcmp(kw, "props") == 0 ||
                  strcmp(kw, "from") == 0 || strcmp(kw, "layout") == 0 ||
                  strcmp(kw, "lazy") == 0);
      free(kw);
      if (stop) break;
    }
    advance(p);
    char *s = token_str(t);
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

static Node *parse_use(Parser *p) {
  Token use_tok = advance(p); /* use | import */
  Node *use = node_create(NODE_USE, NULL, use_tok.line, use_tok.col);

  /* import Name from path  OR  use path [as Alias] */
  char *first = parse_module_path(p);
  if (!first || !*first) {
    free(first);
    fprintf(stderr, "Error: expected module path after use/import\n");
    return use;
  }

  if (peek(p).type == TOKEN_IDENTIFIER) {
    Token maybe = peek(p);
    char *kw = token_str(maybe);
    if (strcmp(kw, "from") == 0) {
      /* import Name from path — first was the alias/name */
      advance(p);
      free(kw);
      use->value2 = first; /* export name alias */
      use->value = parse_module_path(p);
      while (peek(p).type == TOKEN_NEWLINE) advance(p);
      return use;
    }
    if (strcmp(kw, "as") == 0) {
      advance(p);
      free(kw);
      use->value = first;
      if (peek(p).type == TOKEN_IDENTIFIER) {
        Token al = advance(p);
        use->value2 = token_str(al);
      }
      while (peek(p).type == TOKEN_NEWLINE) advance(p);
      return use;
    }
    free(kw);
  }

  use->value = first;
  while (peek(p).type == TOKEN_NEWLINE) advance(p);
  return use;
}

static Node *parse_route(Parser *p) {
  Token route_tok = advance(p);
  Node *route = node_create(NODE_ROUTE, NULL, route_tok.line, route_tok.col);

  /* URL path: "/about" or /about */
  if (peek(p).type == TOKEN_STRING || peek(p).type == TOKEN_IDENTIFIER) {
    Token path = advance(p);
    route->value = token_str(path);
  }

  if (match(p, TOKEN_ARROW)) {
    /* Component name OR module path (pages/HomePage) */
    char *target = parse_module_path(p);
    route->value2 = target;
  }

  /* optional: props id, slug | layout=name | lazy */
  while (peek(p).type == TOKEN_IDENTIFIER) {
    Token maybe = peek(p);
    char *kw = token_str(maybe);
    if (strcmp(kw, "props") == 0) {
      advance(p);
      free(kw);
      Node *props = node_create(NODE_PROPS_DECL, NULL, maybe.line, maybe.col);
      while (peek(p).type == TOKEN_IDENTIFIER || peek(p).type == TOKEN_COMMA) {
        if (peek(p).type == TOKEN_COMMA) {
          advance(p);
          continue;
        }
        /* stop before layout=/lazy so "props id layout=shop" works */
        Token prop = peek(p);
        char *pn = token_str(prop);
        int stop = (pn && (strcmp(pn, "layout") == 0 || strcmp(pn, "lazy") == 0));
        free(pn);
        if (stop) break;
        advance(p);
        node_add_child(props, node_adopt(NODE_TEXT, token_str(prop), prop.line,
                                          prop.col));
      }
      node_add_child(route, props);
    } else if (strcmp(kw, "layout") == 0) {
      advance(p);
      free(kw);
      if (match(p, TOKEN_EQUALS)) {
        if (peek(p).type == TOKEN_IDENTIFIER || peek(p).type == TOKEN_STRING) {
          Token val = advance(p);
          Node *attr = node_create(NODE_ATTR, "layout", maybe.line, maybe.col);
          attr->value2 = token_str(val);
          node_add_child(route, attr);
        }
      }
    } else if (strcmp(kw, "lazy") == 0) {
      advance(p);
      free(kw);
      node_add_child(route,
                     node_create(NODE_BOOL_ATTR, "lazy", maybe.line, maybe.col));
    } else {
      free(kw);
      break;
    }
  }

  while (peek(p).type == TOKEN_NEWLINE) advance(p);
  return route;
}

static Node *parse_for(Parser *p) {
  (void)advance(p);
  Node *loop = node_create(NODE_FOR, NULL, p->current.line, p->current.col);

  if (peek(p).type == TOKEN_IDENTIFIER) {
    Token var = advance(p);
    loop->value = token_str(var);
  }

  if (peek(p).type == TOKEN_IDENTIFIER) {
    Token in_kw = advance(p);
    char *in_str = token_str(in_kw);
    if (strcmp(in_str, "in") != 0) {
      if (loop->value2) free(loop->value2);
      loop->value2 = in_str;
    } else {
      free(in_str);
    }
  }

  if (peek(p).type == TOKEN_IDENTIFIER) {
    Token list = advance(p);
    loop->value2 = token_str(list);
  }

  /* optional key=expr for React list keys */
  if (peek(p).type == TOKEN_IDENTIFIER) {
    Token maybe = peek(p);
    char *k = token_str(maybe);
    if (strcmp(k, "key") == 0) {
      advance(p);
      if (match(p, TOKEN_EQUALS)) {
        Node *attr = node_create(NODE_ATTR, "key", maybe.line, maybe.col);
        if (peek(p).type == TOKEN_IDENTIFIER || peek(p).type == TOKEN_STRING ||
            peek(p).type == TOKEN_NUMBER) {
          Token v = advance(p);
          attr->value2 = token_str(v);
          /* allow key=item.id — identifier may already include dots */
        }
        node_add_child(loop, attr);
      }
    }
    free(k);
  }

  if (peek(p).type == TOKEN_NEWLINE) {
    advance(p);
    Node *body = parse_body(p);
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

static Node *parse_if(Parser *p) {
  (void)advance(p);
  Node *cond_node = node_create(NODE_IF, NULL, p->current.line, p->current.col);

  if (peek(p).type == TOKEN_IDENTIFIER) {
    Token cond = advance(p);
    cond_node->value = token_str(cond);
  }

  if (peek(p).type == TOKEN_NEWLINE) {
    advance(p);
    Node *body = parse_body(p);
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

/* Collect tokens until newline/dedent into a single JS-ish expression string.
 * stop_keywords: optional NULL-terminated list of identifiers that end capture
 * without being consumed (e.g. "deps", "cleanup"). */
static char *capture_expr_line_stop(Parser *p, const char **stop_keywords) {
  char buf[1024];
  size_t n = 0;
  buf[0] = '\0';
  while (peek(p).type != TOKEN_NEWLINE && peek(p).type != TOKEN_EOF &&
         peek(p).type != TOKEN_DEDENT && peek(p).type != TOKEN_INDENT) {
    Token t = peek(p);
    if (t.type == TOKEN_IDENTIFIER && stop_keywords) {
      char *kw = token_str(t);
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
    advance(p);
    char *s = token_str(t);
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

static char *capture_expr_line(Parser *p) {
  return capture_expr_line_stop(p, NULL);
}

/* Capture indented block as semicolon-joined statements */
static char *capture_indent_block(Parser *p) {
  if (peek(p).type != TOKEN_INDENT) return NULL;
  advance(p); /* INDENT */
  char buf[4096];
  size_t n = 0;
  buf[0] = '\0';
  while (peek(p).type != TOKEN_DEDENT && peek(p).type != TOKEN_EOF) {
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
    if (peek(p).type == TOKEN_DEDENT || peek(p).type == TOKEN_EOF) break;
    char *line = capture_expr_line(p);
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
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
  }
  if (peek(p).type == TOKEN_DEDENT) advance(p);
  return strdup(buf);
}

/* Capture indented object members (join with comma) for imperativeHandle */
static char *capture_indent_object_body(Parser *p) {
  if (peek(p).type != TOKEN_INDENT) return NULL;
  advance(p); /* INDENT */
  char buf[4096];
  size_t n = 0;
  buf[0] = '\0';
  while (peek(p).type != TOKEN_DEDENT && peek(p).type != TOKEN_EOF) {
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
    if (peek(p).type == TOKEN_DEDENT || peek(p).type == TOKEN_EOF) break;
    char *line = capture_expr_line(p);
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
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
  }
  if (peek(p).type == TOKEN_DEDENT) advance(p);
  return strdup(buf);
}

/* IDENT (. IDENT)*  e.g. store.subscribe */
static char *capture_dotted_ident(Parser *p) {
  if (peek(p).type != TOKEN_IDENTIFIER) return NULL;
  char buf[256];
  size_t n = 0;
  buf[0] = '\0';
  while (peek(p).type == TOKEN_IDENTIFIER) {
    Token t = advance(p);
    char *s = token_str(t);
    if (s) {
      size_t sl = strlen(s);
      if (n + sl < sizeof(buf)) {
        memcpy(buf + n, s, sl);
        n += sl;
        buf[n] = '\0';
      }
      free(s);
    }
    if (peek(p).type == TOKEN_DOT) {
      advance(p);
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

/*
 * React-oriented declarations (hooks + provide).
 * See docs/REACT.md for the full React → Cordlang map.
 */
static Node *parse_react_decl(Parser *p) {
  Token kw_tok = peek(p);
  char *kw = token_str(kw_tok);
  advance(p);

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
    while (peek(p).type == TOKEN_IDENTIFIER) {
      Token k = peek(p);
      char *key = token_str(k);
      if (strcmp(key, "deps") == 0) {
        advance(p);
        free(key);
        if (match(p, TOKEN_EQUALS)) {
          if (peek(p).type == TOKEN_LPAREN) {
            advance(p);
            char deps[256] = {0};
            size_t dn = 0;
            while (peek(p).type != TOKEN_RPAREN && peek(p).type != TOKEN_EOF) {
              if (peek(p).type == TOKEN_COMMA) {
                advance(p);
                if (dn + 2 < sizeof(deps)) {
                  deps[dn++] = ',';
                  deps[dn++] = ' ';
                }
                continue;
              }
              Token t = advance(p);
              char *s = token_str(t);
              if (s) {
                size_t sl = strlen(s);
                if (dn + sl < sizeof(deps)) {
                  memcpy(deps + dn, s, sl);
                  dn += sl;
                }
                free(s);
              }
            }
            if (peek(p).type == TOKEN_RPAREN) advance(p);
            eff->value = strdup(deps);
          } else {
            eff->value = capture_expr_line(p);
          }
        }
      } else if (strcmp(key, "cleanup") == 0) {
        advance(p);
        free(key);
        if (match(p, TOKEN_EQUALS)) {
          Node *c = node_create(NODE_ATTR, "cleanup", k.line, k.col);
          c->value2 = capture_expr_line(p);
          node_add_child(eff, c);
        }
      } else if (strcmp(key, "run") == 0) {
        advance(p);
        free(key);
        if (match(p, TOKEN_EQUALS)) eff->value2 = capture_expr_line(p);
      } else {
        free(key);
        break;
      }
    }
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
    if (!eff->value2) {
      char *body = capture_indent_block(p);
      if (body) eff->value2 = body;
    }
    return eff;
  }

  if (strcmp(kw, "effectEvent") == 0) {
    free(kw);
    /* effectEvent onMsg = handleMessage | effectEvent onMsg\n  body */
    Node *ev = node_create(NODE_EFFECT_EVENT, NULL, kw_tok.line, kw_tok.col);
    if (peek(p).type == TOKEN_IDENTIFIER) {
      Token n = advance(p);
      ev->value = token_str(n);
    }
    if (match(p, TOKEN_EQUALS)) {
      ev->value2 = capture_expr_line(p);
    }
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
    if (!ev->value2) {
      char *body = capture_indent_block(p);
      if (body) ev->value2 = body;
    }
    return ev;
  }

  if (strcmp(kw, "externalStore") == 0 || strcmp(kw, "syncStore") == 0) {
    free(kw);
    /* externalStore snapshot = subscribe getSnapshot [getServerSnapshot=null]
     * syncStore selected = store.subscribe store.getSnapshot */
    Node *es = node_create(NODE_EXTERNAL_STORE, NULL, kw_tok.line, kw_tok.col);
    if (peek(p).type == TOKEN_IDENTIFIER) {
      Token n = advance(p);
      es->value = token_str(n);
    }
    if (match(p, TOKEN_EQUALS)) {
      es->value2 = capture_dotted_ident(p);
      if (!es->value2) es->value2 = capture_expr_line(p);
      /* second arg: getSnapshot */
      if (peek(p).type == TOKEN_IDENTIFIER) {
        Token maybe = peek(p);
        char *k = token_str(maybe);
        if (k && strcmp(k, "getServerSnapshot") != 0) {
          free(k);
          Node *gs = node_create(NODE_ATTR, "getSnapshot", maybe.line, maybe.col);
          gs->value2 = capture_dotted_ident(p);
          if (!gs->value2) gs->value2 = capture_expr_line(p);
          node_add_child(es, gs);
        } else {
          free(k);
        }
      }
      /* optional getServerSnapshot=... */
      if (peek(p).type == TOKEN_IDENTIFIER) {
        Token maybe = peek(p);
        char *k = token_str(maybe);
        if (k && strcmp(k, "getServerSnapshot") == 0) {
          advance(p);
          free(k);
          if (match(p, TOKEN_EQUALS)) {
            Node *ss =
                node_create(NODE_ATTR, "getServerSnapshot", maybe.line, maybe.col);
            ss->value2 = capture_expr_line(p);
            node_add_child(es, ss);
          }
        } else {
          free(k);
        }
      }
    }
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
    return es;
  }

  if (strcmp(kw, "imperativeHandle") == 0) {
    free(kw);
    /* imperativeHandle [ref=inputEl] [deps=(...)]
     *   focus: () => ...
     */
    Node *ih =
        node_create(NODE_IMPERATIVE_HANDLE, "ref", kw_tok.line, kw_tok.col);
    while (peek(p).type == TOKEN_IDENTIFIER) {
      Token maybe = peek(p);
      char *k = token_str(maybe);
      if (strcmp(k, "ref") == 0) {
        advance(p);
        free(k);
        if (match(p, TOKEN_EQUALS)) {
          if (peek(p).type == TOKEN_IDENTIFIER) {
            Token v = advance(p);
            free(ih->value);
            ih->value = token_str(v);
          }
        }
      } else if (strcmp(k, "deps") == 0) {
        advance(p);
        free(k);
        if (match(p, TOKEN_EQUALS)) {
          Node *d = node_create(NODE_ATTR, "deps", maybe.line, maybe.col);
          if (peek(p).type == TOKEN_LPAREN) {
            advance(p);
            char deps[256] = {0};
            size_t dn = 0;
            while (peek(p).type != TOKEN_RPAREN && peek(p).type != TOKEN_EOF) {
              if (peek(p).type == TOKEN_COMMA) {
                advance(p);
                if (dn + 2 < sizeof(deps)) {
                  deps[dn++] = ',';
                  deps[dn++] = ' ';
                }
                continue;
              }
              Token t = advance(p);
              char *s = token_str(t);
              if (s) {
                size_t sl = strlen(s);
                if (dn + sl < sizeof(deps)) {
                  memcpy(deps + dn, s, sl);
                  dn += sl;
                }
                free(s);
              }
            }
            if (peek(p).type == TOKEN_RPAREN) advance(p);
            d->value2 = strdup(deps);
          } else {
            d->value2 = capture_expr_line(p);
          }
          node_add_child(ih, d);
        }
      } else {
        free(k);
        break;
      }
    }
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
    {
      char *body = capture_indent_object_body(p);
      if (body) ih->value2 = body;
    }
    return ih;
  }

  if (strcmp(kw, "ref") == 0) {
    free(kw);
    Node *ref = node_create(NODE_REF_DECL, NULL, kw_tok.line, kw_tok.col);
    if (peek(p).type == TOKEN_IDENTIFIER) {
      Token n = advance(p);
      ref->value = token_str(n);
    }
    if (match(p, TOKEN_EQUALS)) ref->value2 = parse_default_value(p);
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
    return ref;
  }

  if (strcmp(kw, "context") == 0) {
    free(kw);
    /* context Theme [ = default ]  → createContext */
    Node *ctx = node_create(NODE_CONTEXT_DECL, NULL, kw_tok.line, kw_tok.col);
    if (peek(p).type == TOKEN_IDENTIFIER) {
      Token n = advance(p);
      ctx->value = token_str(n);
    }
    if (match(p, TOKEN_EQUALS)) ctx->value2 = parse_default_value(p);
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
    return ctx;
  }

  if (strcmp(kw, "ctx") == 0) {
    free(kw);
    /* ctx theme = Theme  → useContext(Theme) */
    Node *use = node_create(NODE_CONTEXT_USE, NULL, kw_tok.line, kw_tok.col);
    if (peek(p).type == TOKEN_IDENTIFIER) {
      Token n = advance(p);
      use->value = token_str(n);
    }
    if (match(p, TOKEN_EQUALS)) {
      if (peek(p).type == TOKEN_IDENTIFIER) {
        Token c = advance(p);
        use->value2 = token_str(c);
      }
    }
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
    return use;
  }

  if (strcmp(kw, "reducer") == 0) {
    free(kw);
    /* reducer cart = cartReducer init=[] */
    Node *red = node_create(NODE_REDUCER_DECL, NULL, kw_tok.line, kw_tok.col);
    if (peek(p).type == TOKEN_IDENTIFIER) {
      Token n = advance(p);
      red->value = token_str(n);
    }
    if (match(p, TOKEN_EQUALS)) {
      if (peek(p).type == TOKEN_IDENTIFIER) {
        Token fn = advance(p);
        red->value2 = token_str(fn);
      }
    }
    if (peek(p).type == TOKEN_IDENTIFIER) {
      Token maybe = peek(p);
      char *k = token_str(maybe);
      if (strcmp(k, "init") == 0) {
        advance(p);
        if (match(p, TOKEN_EQUALS)) {
          Node *init = node_create(NODE_ATTR, "init", maybe.line, maybe.col);
          /* always capture full expr so init=[] works */
          init->value2 = capture_expr_line(p);
          node_add_child(red, init);
        }
      }
      free(k);
    }
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
    return red;
  }

  if (strcmp(kw, "params") == 0) {
    free(kw);
    /* params id, slug */
    Node *par = node_create(NODE_PARAMS_DECL, NULL, kw_tok.line, kw_tok.col);
    char names[256] = {0};
    size_t nn = 0;
    while (peek(p).type == TOKEN_IDENTIFIER || peek(p).type == TOKEN_COMMA) {
      if (peek(p).type == TOKEN_COMMA) {
        advance(p);
        if (nn + 2 < sizeof(names)) {
          names[nn++] = ',';
          names[nn++] = ' ';
        }
        continue;
      }
      Token t = advance(p);
      char *s = token_str(t);
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
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
    return par;
  }

  if (strcmp(kw, "navigate") == 0) {
    free(kw);
    Node *nav = node_create(NODE_NAVIGATE_DECL, "navigate", kw_tok.line, kw_tok.col);
    if (peek(p).type == TOKEN_IDENTIFIER) {
      Token n = advance(p);
      free(nav->value);
      nav->value = token_str(n);
    }
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
    return nav;
  }

  if (strcmp(kw, "callback") == 0) {
    free(kw);
    /* callback onAdd = (id) => add(id) deps=(x) */
    Node *cb = node_create(NODE_CALLBACK_DECL, NULL, kw_tok.line, kw_tok.col);
    if (peek(p).type == TOKEN_IDENTIFIER) {
      Token n = advance(p);
      cb->value = token_str(n);
    }
    static const char *cb_stop[] = {"deps", NULL};
    if (match(p, TOKEN_EQUALS))
      cb->value2 = capture_expr_line_stop(p, cb_stop);
    if (peek(p).type == TOKEN_IDENTIFIER) {
      Token maybe = peek(p);
      char *k = token_str(maybe);
      if (strcmp(k, "deps") == 0) {
        advance(p);
        if (match(p, TOKEN_EQUALS)) {
          Node *d = node_create(NODE_ATTR, "deps", maybe.line, maybe.col);
          if (peek(p).type == TOKEN_LPAREN) {
            advance(p);
            char deps[256] = {0};
            size_t dn = 0;
            while (peek(p).type != TOKEN_RPAREN && peek(p).type != TOKEN_EOF) {
              if (peek(p).type == TOKEN_COMMA) {
                advance(p);
                if (dn + 2 < sizeof(deps)) {
                  deps[dn++] = ',';
                  deps[dn++] = ' ';
                }
                continue;
              }
              Token t = advance(p);
              char *s = token_str(t);
              if (s) {
                size_t sl = strlen(s);
                if (dn + sl < sizeof(deps)) {
                  memcpy(deps + dn, s, sl);
                  dn += sl;
                }
                free(s);
              }
            }
            if (peek(p).type == TOKEN_RPAREN) advance(p);
            d->value2 = strdup(deps);
          } else {
            d->value2 = capture_expr_line(p);
          }
          node_add_child(cb, d);
        }
      }
      free(k);
    }
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
    return cb;
  }

  if (strcmp(kw, "memo") == 0) {
    free(kw);
    /* memo total = items.length  (alias of computed → useMemo) */
    Node *m = node_create(NODE_COMPUTED_DECL, NULL, kw_tok.line, kw_tok.col);
    if (peek(p).type == TOKEN_IDENTIFIER) {
      Token n = advance(p);
      m->value = token_str(n);
    }
    if (match(p, TOKEN_EQUALS)) m->value2 = capture_expr_line(p);
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
    return m;
  }

  if (strcmp(kw, "id") == 0) {
    free(kw);
    Node *id = node_create(NODE_ID_DECL, NULL, kw_tok.line, kw_tok.col);
    if (peek(p).type == TOKEN_IDENTIFIER) {
      Token n = advance(p);
      id->value = token_str(n);
    }
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
    return id;
  }

  if (strcmp(kw, "transition") == 0) {
    free(kw);
    /* transition isPending, startTransition */
    Node *tr = node_create(NODE_TRANSITION_DECL, "isPending", kw_tok.line, kw_tok.col);
    tr->value2 = strdup("startTransition");
    if (peek(p).type == TOKEN_IDENTIFIER) {
      Token a = advance(p);
      free(tr->value);
      tr->value = token_str(a);
      if (peek(p).type == TOKEN_COMMA) {
        advance(p);
        if (peek(p).type == TOKEN_IDENTIFIER) {
          Token b = advance(p);
          free(tr->value2);
          tr->value2 = token_str(b);
        }
      }
    }
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
    return tr;
  }

  if (strcmp(kw, "deferred") == 0) {
    free(kw);
    /* deferred deferredQuery = query */
    Node *d = node_create(NODE_DEFERRED_DECL, NULL, kw_tok.line, kw_tok.col);
    if (peek(p).type == TOKEN_IDENTIFIER) {
      Token n = advance(p);
      d->value = token_str(n);
    }
    if (match(p, TOKEN_EQUALS)) {
      if (peek(p).type == TOKEN_IDENTIFIER) {
        Token s = advance(p);
        d->value2 = token_str(s);
      } else {
        d->value2 = capture_expr_line(p);
      }
    }
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
    return d;
  }

  if (strcmp(kw, "action") == 0) {
    free(kw);
    /* action form = submitForm init=null  → useActionState */
    Node *act = node_create(NODE_ACTION_DECL, NULL, kw_tok.line, kw_tok.col);
    if (peek(p).type == TOKEN_IDENTIFIER) {
      Token n = advance(p);
      act->value = token_str(n);
    }
    if (match(p, TOKEN_EQUALS)) {
      if (peek(p).type == TOKEN_IDENTIFIER) {
        Token fn = advance(p);
        act->value2 = token_str(fn);
      }
    }
    while (peek(p).type == TOKEN_IDENTIFIER) {
      Token maybe = peek(p);
      char *k = token_str(maybe);
      if (strcmp(k, "init") == 0) {
        advance(p);
        free(k);
        if (match(p, TOKEN_EQUALS)) {
          Node *a = node_create(NODE_ATTR, "init", maybe.line, maybe.col);
          static const char *act_stop[] = {"pending", NULL};
          a->value2 = capture_expr_line_stop(p, act_stop);
          node_add_child(act, a);
        }
      } else if (strcmp(k, "pending") == 0) {
        advance(p);
        free(k);
        if (match(p, TOKEN_EQUALS)) {
          Node *a = node_create(NODE_ATTR, "pending", maybe.line, maybe.col);
          if (peek(p).type == TOKEN_IDENTIFIER) {
            Token v = advance(p);
            a->value2 = token_str(v);
          }
          node_add_child(act, a);
        }
      } else {
        free(k);
        break;
      }
    }
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
    return act;
  }

  if (strcmp(kw, "fetch") == 0 || strcmp(kw, "load") == 0) {
    free(kw);
    /* fetch products = "/api/products.json"
     * load products = fetchJson("/api/products")  → same NODE_FETCH_DECL */
    Node *fd = node_create(NODE_FETCH_DECL, NULL, kw_tok.line, kw_tok.col);
    if (peek(p).type == TOKEN_IDENTIFIER) {
      Token n = advance(p);
      fd->value = token_str(n);
    }
    if (match(p, TOKEN_EQUALS)) {
      /* String / path token: "/api/x.json" or /api/x.json */
      if (peek(p).type == TOKEN_STRING) {
        Token u = advance(p);
        fd->value2 = token_str(u);
      } else if (peek(p).type == TOKEN_IDENTIFIER) {
        Token maybe = peek(p);
        char *fn = token_str(maybe);
        /* load products = fetchJson("/url") → extract URL */
        if (fn && (strcmp(fn, "fetchJson") == 0 || strcmp(fn, "fetch") == 0)) {
          advance(p);
          free(fn);
          if (match(p, TOKEN_LPAREN)) {
            if (peek(p).type == TOKEN_STRING) {
              Token u = advance(p);
              fd->value2 = token_str(u);
            }
            if (peek(p).type == TOKEN_RPAREN) advance(p);
          }
        } else {
          free(fn);
          fd->value2 = capture_expr_line(p);
        }
      } else {
        fd->value2 = capture_expr_line(p);
      }
    }
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
    return fd;
  }

  if (strcmp(kw, "lazy") == 0) {
    free(kw);
    /* lazy Chart = components/Chart */
    Node *lz = node_create(NODE_LAZY_DECL, NULL, kw_tok.line, kw_tok.col);
    if (peek(p).type == TOKEN_IDENTIFIER) {
      Token n = advance(p);
      lz->value = token_str(n);
    }
    if (match(p, TOKEN_EQUALS)) {
      char *path = parse_module_path(p);
      lz->value2 = path;
    }
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
    return lz;
  }

  if (strcmp(kw, "suspense") == 0) {
    free(kw);
    /* suspense fallback=... | indented fallback block + children */
    Node *sus = node_create(NODE_SUSPENSE, NULL, kw_tok.line, kw_tok.col);
    if (peek(p).type == TOKEN_IDENTIFIER) {
      Token maybe = peek(p);
      char *k = token_str(maybe);
      if (strcmp(k, "fallback") == 0) {
        advance(p);
        if (match(p, TOKEN_EQUALS)) {
          Node *fb = node_create(NODE_ATTR, "fallback", maybe.line, maybe.col);
          if (peek(p).type == TOKEN_STRING) {
            Token v = advance(p);
            fb->value2 = token_str(v);
          } else {
            fb->value2 = capture_expr_line(p);
          }
          node_add_child(sus, fb);
        }
      }
      free(k);
    }
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
    if (peek(p).type == TOKEN_INDENT) {
      advance(p);
      while (peek(p).type != TOKEN_DEDENT && peek(p).type != TOKEN_EOF) {
        while (peek(p).type == TOKEN_NEWLINE) advance(p);
        if (peek(p).type == TOKEN_DEDENT || peek(p).type == TOKEN_EOF) break;
        /* special: fallback block as first child keyword */
        if (peek(p).type == TOKEN_IDENTIFIER) {
          Token t = peek(p);
          char *k = token_str(t);
          if (strcmp(k, "fallback") == 0) {
            advance(p);
            free(k);
            while (peek(p).type == TOKEN_NEWLINE) advance(p);
            Node *fb_wrap = node_create(NODE_ELEMENT, "__fallback__", t.line, t.col);
            if (peek(p).type == TOKEN_INDENT) {
              Node *body = parse_body(p);
              if (body) {
                for (size_t i = 0; i < body->children_len; i++) {
                  node_add_child(fb_wrap, body->children[i]);
                  body->children[i] = NULL;
                }
                body->children_len = 0;
                node_free(body);
              }
            } else {
              Node *one = parse_stmt(p);
              if (one) node_add_child(fb_wrap, one);
            }
            node_add_child(sus, fb_wrap);
            continue;
          }
          free(k);
        }
        Node *child = parse_stmt(p);
        if (child) node_add_child(sus, child);
        else if (peek(p).type != TOKEN_DEDENT && peek(p).type != TOKEN_EOF)
          advance(p);
      }
      if (peek(p).type == TOKEN_DEDENT) advance(p);
    }
    return sus;
  }

  if (strcmp(kw, "portal") == 0) {
    free(kw);
    /* portal to=document.body ...children */
    Node *por = node_create(NODE_PORTAL, "document.body", kw_tok.line, kw_tok.col);
    if (peek(p).type == TOKEN_IDENTIFIER) {
      Token maybe = peek(p);
      char *k = token_str(maybe);
      if (strcmp(k, "to") == 0) {
        advance(p);
        free(k);
        if (match(p, TOKEN_EQUALS)) {
          free(por->value);
          por->value = capture_expr_line(p);
        }
      } else {
        free(k);
      }
    }
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
    if (peek(p).type == TOKEN_INDENT) {
      Node *body = parse_body(p);
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

  if (strcmp(kw, "errorBoundary") == 0) {
    free(kw);
    Node *eb = node_create(NODE_ERROR_BOUNDARY, NULL, kw_tok.line, kw_tok.col);
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
    if (peek(p).type == TOKEN_INDENT) {
      advance(p);
      while (peek(p).type != TOKEN_DEDENT && peek(p).type != TOKEN_EOF) {
        while (peek(p).type == TOKEN_NEWLINE) advance(p);
        if (peek(p).type == TOKEN_DEDENT || peek(p).type == TOKEN_EOF) break;
        if (peek(p).type == TOKEN_IDENTIFIER) {
          Token t = peek(p);
          char *k = token_str(t);
          if (strcmp(k, "fallback") == 0) {
            advance(p);
            free(k);
            while (peek(p).type == TOKEN_NEWLINE) advance(p);
            Node *fb = node_create(NODE_ELEMENT, "__fallback__", t.line, t.col);
            if (peek(p).type == TOKEN_INDENT) {
              Node *body = parse_body(p);
              if (body) {
                for (size_t i = 0; i < body->children_len; i++) {
                  node_add_child(fb, body->children[i]);
                  body->children[i] = NULL;
                }
                body->children_len = 0;
                node_free(body);
              }
            } else {
              Node *one = parse_stmt(p);
              if (one) node_add_child(fb, one);
            }
            node_add_child(eb, fb);
            continue;
          }
          free(k);
        }
        Node *child = parse_stmt(p);
        if (child) node_add_child(eb, child);
        else if (peek(p).type != TOKEN_DEDENT && peek(p).type != TOKEN_EOF)
          advance(p);
      }
      if (peek(p).type == TOKEN_DEDENT) advance(p);
    }
    return eb;
  }

  if (strcmp(kw, "provide") == 0) {
    free(kw);
    /* provide Theme value=dark → <Theme.Provider> */
    Node *el = node_create(NODE_ELEMENT, "provide", kw_tok.line, kw_tok.col);
    if (peek(p).type == TOKEN_IDENTIFIER) {
      Token ctx = advance(p);
      Node *attr = node_create(NODE_ATTR, "context", ctx.line, ctx.col);
      attr->value2 = token_str(ctx);
      node_add_child(el, attr);
    }
    parse_attrs(p, el);
    if (peek(p).type == TOKEN_NEWLINE) {
      advance(p);
      Node *body = parse_body(p);
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
    if (peek(p).type == TOKEN_IDENTIFIER) {
      Token maybe = peek(p);
      char *k = token_str(maybe);
      if (strcmp(k, "fallback") == 0) {
        advance(p);
        if (match(p, TOKEN_EQUALS)) {
          Node *fb = node_create(NODE_ATTR, "fallback", maybe.line, maybe.col);
          if (peek(p).type == TOKEN_STRING) {
            Token v = advance(p);
            fb->value2 = token_str(v);
          } else {
            fb->value2 = capture_expr_line(p);
          }
          node_add_child(ld, fb);
        }
      }
      free(k);
    }
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
    if (peek(p).type == TOKEN_INDENT) {
      advance(p);
      while (peek(p).type != TOKEN_DEDENT && peek(p).type != TOKEN_EOF) {
        while (peek(p).type == TOKEN_NEWLINE) advance(p);
        if (peek(p).type == TOKEN_DEDENT || peek(p).type == TOKEN_EOF) break;
        if (peek(p).type == TOKEN_IDENTIFIER) {
          Token t = peek(p);
          char *k = token_str(t);
          if (strcmp(k, "fallback") == 0) {
            advance(p);
            free(k);
            while (peek(p).type == TOKEN_NEWLINE) advance(p);
            Node *fb_wrap =
                node_create(NODE_ELEMENT, "__fallback__", t.line, t.col);
            if (peek(p).type == TOKEN_INDENT) {
              Node *body = parse_body(p);
              if (body) {
                for (size_t i = 0; i < body->children_len; i++) {
                  node_add_child(fb_wrap, body->children[i]);
                  body->children[i] = NULL;
                }
                body->children_len = 0;
                node_free(body);
              }
            } else {
              Node *one = parse_stmt(p);
              if (one) node_add_child(fb_wrap, one);
            }
            node_add_child(ld, fb_wrap);
            continue;
          }
          free(k);
        }
        Node *child = parse_stmt(p);
        if (child) node_add_child(ld, child);
        else if (peek(p).type != TOKEN_DEDENT && peek(p).type != TOKEN_EOF)
          advance(p);
      }
      if (peek(p).type == TOKEN_DEDENT) advance(p);
    }
    return ld;
  }

  free(kw);
  return NULL;
}

/* title "My Page"  OR  head \n title "Shop" */
static Node *parse_title_or_head(Parser *p) {
  Token kw_tok = advance(p);
  char *kw = token_str(kw_tok);

  if (strcmp(kw, "title") == 0) {
    free(kw);
    Node *head = node_create(NODE_HEAD, NULL, kw_tok.line, kw_tok.col);
    if (peek(p).type == TOKEN_STRING) {
      Token t = advance(p);
      head->value = token_str(t);
    } else if (peek(p).type == TOKEN_IDENTIFIER) {
      /* bare identifier title text (rare) */
      Token t = advance(p);
      head->value = token_str(t);
    }
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
    return head;
  }

  if (strcmp(kw, "head") == 0) {
    free(kw);
    Node *head = node_create(NODE_HEAD, NULL, kw_tok.line, kw_tok.col);
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
    if (peek(p).type == TOKEN_INDENT) {
      advance(p);
      while (peek(p).type != TOKEN_DEDENT && peek(p).type != TOKEN_EOF) {
        while (peek(p).type == TOKEN_NEWLINE) advance(p);
        if (peek(p).type == TOKEN_DEDENT || peek(p).type == TOKEN_EOF) break;
        if (peek(p).type == TOKEN_IDENTIFIER) {
          Token t = peek(p);
          char *k = token_str(t);
          if (strcmp(k, "title") == 0) {
            advance(p);
            free(k);
            if (peek(p).type == TOKEN_STRING) {
              Token s = advance(p);
              char *title = token_str(s);
              if (!head->value) head->value = title;
              else {
                Node *ch = node_create(NODE_ATTR, "title", t.line, t.col);
                ch->value2 = title;
                node_add_child(head, ch);
              }
            }
            while (peek(p).type == TOKEN_NEWLINE) advance(p);
            continue;
          }
          free(k);
        }
        Node *child = parse_stmt(p);
        if (child) node_add_child(head, child);
        else if (peek(p).type != TOKEN_DEDENT && peek(p).type != TOKEN_EOF)
          advance(p);
      }
      if (peek(p).type == TOKEN_DEDENT) advance(p);
    }
    return head;
  }

  free(kw);
  return NULL;
}

/*
 * Phase E Svelte: await / snippet / store|writable / render|@render
 * await promiseExpr then=name catch=err
 * await promise=expr then=name
 * snippet card(title) ...body
 * store count = 0  |  writable count = 0
 * render card title="x"  |  @render card("x")
 */
static Node *parse_await(Parser *p) {
  Token kw_tok = advance(p); /* await */
  Node *aw = node_create(NODE_AWAIT, NULL, kw_tok.line, kw_tok.col);

  /* promise=expr  OR bare promiseExpr */
  static const char *await_stop[] = {"then", "catch", NULL};
  if (peek(p).type == TOKEN_IDENTIFIER) {
    Token first = peek(p);
    char *fk = token_str(first);
    if (fk && strcmp(fk, "promise") == 0) {
      advance(p);
      free(fk);
      if (match(p, TOKEN_EQUALS)) aw->value = capture_expr_line_stop(p, await_stop);
    } else if (fk && (strcmp(fk, "then") == 0 || strcmp(fk, "catch") == 0)) {
      free(fk);
    } else {
      free(fk);
      /* bare identifier or call: productsPromise or loadProducts() */
      aw->value = capture_expr_line_stop(p, await_stop);
    }
  }

  while (peek(p).type == TOKEN_IDENTIFIER) {
    Token k = peek(p);
    char *key = token_str(k);
    if (strcmp(key, "then") == 0) {
      advance(p);
      free(key);
      if (match(p, TOKEN_EQUALS) && peek(p).type == TOKEN_IDENTIFIER) {
        Token n = advance(p);
        aw->value2 = token_str(n);
      }
    } else if (strcmp(key, "catch") == 0) {
      advance(p);
      free(key);
      if (match(p, TOKEN_EQUALS) && peek(p).type == TOKEN_IDENTIFIER) {
        Token n = advance(p);
        Node *a = node_create(NODE_ATTR, "catch", k.line, k.col);
        a->value2 = token_str(n);
        node_add_child(aw, a);
      }
    } else {
      free(key);
      break;
    }
  }

  while (peek(p).type == TOKEN_NEWLINE) advance(p);
  if (peek(p).type == TOKEN_INDENT) {
    advance(p);
    while (peek(p).type != TOKEN_DEDENT && peek(p).type != TOKEN_EOF) {
      while (peek(p).type == TOKEN_NEWLINE) advance(p);
      if (peek(p).type == TOKEN_DEDENT || peek(p).type == TOKEN_EOF) break;
      if (peek(p).type == TOKEN_IDENTIFIER) {
        Token t = peek(p);
        char *k = token_str(t);
        if (k && (strcmp(k, "loading") == 0 || strcmp(k, "error") == 0)) {
          advance(p);
          int is_err = (strcmp(k, "error") == 0);
          const char *wrap_name = is_err ? "__error__" : "__loading__";
          free(k);
          /* optional catch binding: error err */
          if (is_err && peek(p).type == TOKEN_IDENTIFIER) {
            Token cn = advance(p);
            Node *ca = node_create(NODE_ATTR, "catch", cn.line, cn.col);
            ca->value2 = token_str(cn);
            node_add_child(aw, ca);
          }
          while (peek(p).type == TOKEN_NEWLINE) advance(p);
          Node *wrap =
              node_create(NODE_ELEMENT, wrap_name, t.line, t.col);
          if (peek(p).type == TOKEN_INDENT) {
            Node *body = parse_body(p);
            if (body) {
              for (size_t i = 0; i < body->children_len; i++) {
                node_add_child(wrap, body->children[i]);
                body->children[i] = NULL;
              }
              body->children_len = 0;
              node_free(body);
            }
          } else {
            Node *one = parse_stmt(p);
            if (one) node_add_child(wrap, one);
          }
          node_add_child(aw, wrap);
          continue;
        }
        free(k);
      }
      Node *child = parse_stmt(p);
      if (child) node_add_child(aw, child);
      else if (peek(p).type != TOKEN_DEDENT && peek(p).type != TOKEN_EOF)
        advance(p);
    }
    if (peek(p).type == TOKEN_DEDENT) advance(p);
  }
  return aw;
}

static Node *parse_snippet(Parser *p) {
  Token kw_tok = advance(p); /* snippet */
  Node *sn = node_create(NODE_SNIPPET, NULL, kw_tok.line, kw_tok.col);
  if (peek(p).type == TOKEN_IDENTIFIER) {
    Token n = advance(p);
    sn->value = token_str(n);
  }
  /* optional (params) */
  if (peek(p).type == TOKEN_LPAREN) {
    advance(p);
    char params[256];
    size_t pn = 0;
    params[0] = '\0';
    while (peek(p).type != TOKEN_RPAREN && peek(p).type != TOKEN_EOF &&
           peek(p).type != TOKEN_NEWLINE) {
      if (peek(p).type == TOKEN_COMMA) {
        advance(p);
        if (pn + 2 < sizeof(params)) {
          params[pn++] = ',';
          params[pn++] = ' ';
        }
        continue;
      }
      Token t = advance(p);
      char *s = token_str(t);
      if (s) {
        size_t sl = strlen(s);
        if (pn + sl < sizeof(params)) {
          memcpy(params + pn, s, sl);
          pn += sl;
        }
        free(s);
      }
    }
    if (peek(p).type == TOKEN_RPAREN) advance(p);
    params[pn] = '\0';
    if (pn > 0) sn->value2 = strdup(params);
  }
  while (peek(p).type == TOKEN_NEWLINE) advance(p);
  if (peek(p).type == TOKEN_INDENT) {
    Node *body = parse_body(p);
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

static Node *parse_store(Parser *p) {
  Token kw_tok = advance(p); /* store | writable */
  Node *st = node_create(NODE_STORE_DECL, NULL, kw_tok.line, kw_tok.col);
  if (peek(p).type == TOKEN_IDENTIFIER) {
    Token n = advance(p);
    st->value = token_str(n);
  }
  if (match(p, TOKEN_EQUALS)) {
    if (peek(p).type == TOKEN_STRING || peek(p).type == TOKEN_NUMBER ||
        peek(p).type == TOKEN_IDENTIFIER) {
      Token v = advance(p);
      st->value2 = token_str(v);
      /* optional call: store x = writable(0) already handled as bare */
      if (st->value2 && peek(p).type == TOKEN_LPAREN) {
        char buf[256];
        size_t n = 0;
        size_t vl = strlen(st->value2);
        if (vl < sizeof(buf)) {
          memcpy(buf, st->value2, vl);
          n = vl;
        }
        int depth = 0;
        while (peek(p).type != TOKEN_EOF && peek(p).type != TOKEN_NEWLINE) {
          Token t = advance(p);
          if (t.type == TOKEN_LPAREN) {
            depth++;
            if (n + 1 < sizeof(buf)) buf[n++] = '(';
          } else if (t.type == TOKEN_RPAREN) {
            if (n + 1 < sizeof(buf)) buf[n++] = ')';
            depth--;
            if (depth <= 0) break;
          } else {
            char *s = token_str(t);
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
      st->value2 = capture_expr_line(p);
    }
  }
  while (peek(p).type == TOKEN_NEWLINE) advance(p);
  return st;
}

static Node *parse_render(Parser *p) {
  Token kw_tok = advance(p); /* render */
  Node *rn = node_create(NODE_RENDER, NULL, kw_tok.line, kw_tok.col);

  /* card("Hello") or card title="Hello" */
  if (peek(p).type == TOKEN_IDENTIFIER) {
    Token name = advance(p);
    char *nm = token_str(name);
    if (peek(p).type == TOKEN_LPAREN) {
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
      while (peek(p).type != TOKEN_EOF && peek(p).type != TOKEN_NEWLINE) {
        Token t = advance(p);
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
          char *s = token_str(t);
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
          char *s = token_str(t);
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
      parse_attrs(p, rn);
      /* rewrite attrs into call: card(title) style — keep as name + attrs
         for backend to emit {@render card({ title: "Hello" })} or
         {@render card("Hello")} when single positional-like attr */
    }
  }
  while (peek(p).type == TOKEN_NEWLINE) advance(p);
  return rn;
}

/* foreign Name [from "mod" for backend]
     props …
     react from "…"
     svelte from "…"
*/
static Node *parse_foreign(Parser *p) {
  Token tok = advance(p); /* foreign */
  if (peek(p).type != TOKEN_IDENTIFIER) {
    Token t = peek(p);
    parser_fail(p, "expected component name after foreign", t.line, t.col);
    return NULL;
  }
  Token name = advance(p);
  char *nm = token_str(name);
  Node *fn = node_adopt(NODE_FOREIGN, nm, tok.line, tok.col);

  /* Optional: from "module" [for backend] */
  if (peek(p).type == TOKEN_IDENTIFIER) {
    char *kw = token_str(peek(p));
    if (kw && strcmp(kw, "from") == 0) {
      advance(p);
      free(kw);
      if (peek(p).type == TOKEN_STRING) {
        Token mod = advance(p);
        fn->value2 = token_str(mod);
      }
      if (peek(p).type == TOKEN_IDENTIFIER) {
        char *fk = token_str(peek(p));
        if (fk && strcmp(fk, "for") == 0) {
          advance(p);
          free(fk);
          if (peek(p).type == TOKEN_IDENTIFIER) {
            Token be = advance(p);
            char *ben = token_str(be);
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

  while (peek(p).type == TOKEN_NEWLINE) advance(p);
  if (peek(p).type == TOKEN_INDENT) {
    advance(p);
    while (peek(p).type != TOKEN_DEDENT && peek(p).type != TOKEN_EOF) {
      while (peek(p).type == TOKEN_NEWLINE) advance(p);
      if (peek(p).type == TOKEN_DEDENT || peek(p).type == TOKEN_EOF) break;
      Token id = peek(p);
      if (id.type != TOKEN_IDENTIFIER) {
        advance(p);
        continue;
      }
      char *k = token_str(id);
      if (!k) {
        advance(p);
        continue;
      }
      if (strcmp(k, "props") == 0) {
        free(k);
        Node *props = parse_stmt(p);
        if (props) node_add_child(fn, props);
        continue;
      }
      /* backend from "module" [as { Export }] — as clause ignored in MVP */
      if (strcmp(k, "react") == 0 || strcmp(k, "svelte") == 0 ||
          strcmp(k, "vue") == 0 || strcmp(k, "solid") == 0) {
        Token be = advance(p);
        free(k);
        char *fromkw = NULL;
        if (peek(p).type == TOKEN_IDENTIFIER) {
          fromkw = token_str(peek(p));
          if (fromkw && strcmp(fromkw, "from") == 0) {
            advance(p);
          }
          free(fromkw);
        }
        char *mod = NULL;
        if (peek(p).type == TOKEN_STRING) {
          Token m = advance(p);
          mod = token_str(m);
        }
        /* skip optional: as { Name } */
        if (peek(p).type == TOKEN_IDENTIFIER) {
          char *ask = token_str(peek(p));
          if (ask && strcmp(ask, "as") == 0) {
            advance(p);
            free(ask);
            while (peek(p).type != TOKEN_NEWLINE && peek(p).type != TOKEN_EOF &&
                   peek(p).type != TOKEN_DEDENT)
              advance(p);
          } else {
            free(ask);
          }
        }
        char *ben = token_str(be);
        Node *a = node_adopt(NODE_ATTR, ben, be.line, be.col);
        if (a) {
          a->value2 = mod;
          node_add_child(fn, a);
        } else {
          free(ben);
          free(mod);
        }
        while (peek(p).type == TOKEN_NEWLINE) advance(p);
        continue;
      }
      free(k);
      Node *other = parse_stmt(p);
      if (other) node_add_child(fn, other);
    }
    if (peek(p).type == TOKEN_DEDENT) advance(p);
  }
  return fn;
}

static Node *parse_empty(Parser *p) {
  Token tok = advance(p); /* empty */
  Node *em = node_create(NODE_EMPTY, NULL, tok.line, tok.col);
  if (peek(p).type == TOKEN_IDENTIFIER) {
    Token maybe = peek(p);
    char *k = token_str(maybe);
    if (strcmp(k, "if") == 0) {
      advance(p);
      free(k);
      if (match(p, TOKEN_EQUALS)) {
        em->value = capture_expr_line(p);
      } else if (peek(p).type == TOKEN_IDENTIFIER) {
        /* empty if items → treat as bare identifier condition */
        Token c = advance(p);
        em->value = token_str(c);
      }
    } else {
      free(k);
    }
  }
  while (peek(p).type == TOKEN_NEWLINE) advance(p);
  if (peek(p).type == TOKEN_INDENT) {
    Node *body = parse_body(p);
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

static Node *parse_stmt(Parser *p) {
  while (peek(p).type == TOKEN_NEWLINE) advance(p);

  if (peek(p).type == TOKEN_EOF) return NULL;
  if (peek(p).type == TOKEN_DEDENT) return NULL;
  if (peek(p).type == TOKEN_INDENT) { advance(p); return NULL; }

  Token check = peek(p);

  /* @render name(args) — Phase E snippets */
  if (check.type == TOKEN_AT) {
    advance(p);
    if (peek(p).type == TOKEN_IDENTIFIER) {
      Token id = peek(p);
      char *idk = token_str(id);
      if (idk && strcmp(idk, "render") == 0) {
        free(idk);
        return parse_render(p);
      }
      free(idk);
    }
    return NULL;
  }

  if (check.type != TOKEN_IDENTIFIER) {
    advance(p);
    return NULL;
  }

  char *kw = token_str(check);

  if (strcmp(kw, "def") == 0) {
    free(kw);
    return parse_def(p);
  } else if (strcmp(kw, "use") == 0 || strcmp(kw, "import") == 0) {
    free(kw);
    return parse_use(p);
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
    return parse_react_decl(p);
  } else if (strcmp(kw, "await") == 0) {
    free(kw);
    return parse_await(p);
  } else if (strcmp(kw, "snippet") == 0) {
    free(kw);
    return parse_snippet(p);
  } else if (strcmp(kw, "store") == 0 || strcmp(kw, "writable") == 0) {
    free(kw);
    return parse_store(p);
  } else if (strcmp(kw, "render") == 0) {
    free(kw);
    return parse_render(p);
  } else if (strcmp(kw, "title") == 0 || strcmp(kw, "head") == 0) {
    free(kw);
    return parse_title_or_head(p);
  } else if (strcmp(kw, "empty") == 0) {
    free(kw);
    return parse_empty(p);
  } else if (strcmp(kw, "props") == 0) {
    /* Top-level props (body-only component files) */
    free(kw);
    Token tok = advance(p);
    Node *props = node_create(NODE_PROPS_DECL, NULL, tok.line, tok.col);
    while (peek(p).type == TOKEN_IDENTIFIER || peek(p).type == TOKEN_COMMA) {
      if (peek(p).type == TOKEN_COMMA) {
        advance(p);
        continue;
      }
      node_add_child(props, parse_prop_node(p));
    }
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
    return props;
  } else if (strcmp(kw, "state") == 0) {
    free(kw);
    Token tok = advance(p);
    Node *state = node_create(NODE_STATE_DECL, NULL, tok.line, tok.col);
    while (peek(p).type == TOKEN_IDENTIFIER || peek(p).type == TOKEN_COMMA) {
      if (peek(p).type == TOKEN_COMMA) {
        advance(p);
        continue;
      }
      Token st = advance(p);
      Node *st_node = node_adopt(NODE_STATE_DECL, token_str(st), st.line, st.col);
      if (match(p, TOKEN_EQUALS)) st_node->value2 = parse_default_value(p);
      node_add_child(state, st_node);
    }
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
    return state;
  } else if (strcmp(kw, "computed") == 0) {
    free(kw);
    Token tok = advance(p);
    (void)tok;
    if (peek(p).type == TOKEN_IDENTIFIER) {
      Token comp = advance(p);
      Node *computed =
          node_adopt(NODE_COMPUTED_DECL, token_str(comp), comp.line, comp.col);
      if (match(p, TOKEN_EQUALS)) {
        char expr[512] = {0};
        size_t elen = 0;
        while (peek(p).type != TOKEN_NEWLINE && peek(p).type != TOKEN_EOF &&
               peek(p).type != TOKEN_DEDENT) {
          Token t = advance(p);
          char *s = token_str(t);
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
      while (peek(p).type == TOKEN_NEWLINE) advance(p);
      return computed;
    }
    return NULL;
  } else if (strcmp(kw, "layout") == 0) {
    free(kw);
    return parse_layout(p);
  } else if (strcmp(kw, "theme") == 0) {
    free(kw);
    return parse_theme(p);
  } else if (strcmp(kw, "route") == 0) {
    free(kw);
    return parse_route(p);
  } else if (strcmp(kw, "foreign") == 0) {
    free(kw);
    return parse_foreign(p);
  } else if (strcmp(kw, "slot") == 0) {
    free(kw);
    Token tok = advance(p);
    Node *slot = node_create(NODE_SLOT, NULL, tok.line, tok.col);
    while (peek(p).type == TOKEN_NEWLINE) advance(p);
    return slot;
  } else if (strcmp(kw, "for") == 0) {
    free(kw);
    return parse_for(p);
  } else if (strcmp(kw, "if") == 0) {
    free(kw);
    return parse_if(p);
  } else if (strcmp(kw, "else") == 0) {
    free(kw);
    Node *else_marker = node_create(NODE_TEXT, "__else__", check.line, check.col);
    (void)advance(p);
    if (peek(p).type == TOKEN_NEWLINE) {
      advance(p);
      Node *body = parse_body(p);
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
    return parse_element(p);
  }
}

AST *parser_parse(Parser *p) {
  lexer_tokenize(p->lexer);

  if (p->lexer && p->lexer->had_error) {
    parser_fail(p,
                p->lexer->error_msg ? p->lexer->error_msg : "lex error",
                p->lexer->error_line, p->lexer->error_col);
    AST *result = p->ast;
    p->ast = NULL;
    return result;
  }

  while (peek(p).type != TOKEN_EOF) {
    if (peek(p).type == TOKEN_ERROR) {
      Token t = peek(p);
      parser_fail(p, "invalid token", t.line, t.col);
      break;
    }
    Node *stmt = parse_stmt(p);
    if (stmt) {
      node_add_child(p->ast->root, stmt);
    } else {
      if (peek(p).type != TOKEN_EOF) advance(p);
    }
  }

  AST *result = p->ast;
  p->ast = NULL;
  return result;
}
