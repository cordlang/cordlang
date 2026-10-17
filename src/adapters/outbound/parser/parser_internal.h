#ifndef CORDLANG_PARSER_INTERNAL_H
#define CORDLANG_PARSER_INTERNAL_H

#include "adapters/outbound/parser/parser.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Token parser_peek(Parser *p);
Token parser_advance(Parser *p);
int parser_match(Parser *p, TokenType type);
void parser_fail(Parser *p, const char *msg, int line, int col);
int parser_consume(Parser *p, TokenType type, const char *msg);
char *parser_token_str(Token t);
char * parser_capture_dotted_ident(Parser *p);
char * parser_capture_expr_line(Parser *p);
char * parser_capture_expr_line_stop(Parser *p, const char **stop_keywords);
char * parser_capture_indent_block(Parser *p);
char * parser_capture_indent_object_body(Parser *p);
Node * parser_parse_attrs(Parser *p, Node *element);
Node * parser_parse_await(Parser *p);
Node * parser_parse_body(Parser *p);
Node * parser_parse_def(Parser *p);
char * parser_parse_default_value(Parser *p);
Node * parser_parse_element(Parser *p);
Node * parser_parse_empty(Parser *p);
Node * parser_parse_for(Parser *p);
Node * parser_parse_foreign(Parser *p);
Node * parser_parse_if(Parser *p);
Node * parser_parse_layout(Parser *p);
char * parser_parse_module_path(Parser *p);
Node * parser_parse_prop_node(Parser *p);
Node * parser_parse_react_decl(Parser *p);
Node * parser_parse_render(Parser *p);
Node * parser_parse_route(Parser *p);
Node * parser_parse_snippet(Parser *p);
Node * parser_parse_stmt(Parser *p);
Node * parser_parse_store(Parser *p);
Node * parser_parse_style_map(Parser *p);
Node * parser_parse_theme(Parser *p);
Node * parser_parse_title_or_head(Parser *p);
Node * parser_parse_use(Parser *p);
void parser_parse_event_attr(Parser *p, Node *element);
Node *parser_parse_react_effects(Parser *p, Token kw_tok, char *kw);
Node *parser_parse_react_declarations(Parser *p, Token kw_tok, char *kw);
Node *parser_parse_react_resources(Parser *p, Token kw_tok, char *kw);
Node *parser_parse_react_boundaries(Parser *p, Token kw_tok, char *kw);

#endif
