#include "adapters/outbound/parser/parser_internal.h"

Node *parser_parse_react_decl(Parser *p) {
  Token kw_tok = parser_peek(p);
  char *kw = parser_token_str(kw_tok);
  parser_advance(p);
  if (strcmp(kw, "effect") == 0 || strcmp(kw, "layoutEffect") == 0 ||
      strcmp(kw, "insertionEffect") == 0 || strcmp(kw, "effectEvent") == 0 ||
      strcmp(kw, "externalStore") == 0 || strcmp(kw, "syncStore") == 0 ||
      strcmp(kw, "imperativeHandle") == 0)
    return parser_parse_react_effects(p, kw_tok, kw);
  if (strcmp(kw, "ref") == 0 || strcmp(kw, "context") == 0 ||
      strcmp(kw, "ctx") == 0 || strcmp(kw, "reducer") == 0 ||
      strcmp(kw, "params") == 0 || strcmp(kw, "navigate") == 0 ||
      strcmp(kw, "callback") == 0 || strcmp(kw, "memo") == 0 ||
      strcmp(kw, "id") == 0 || strcmp(kw, "transition") == 0 ||
      strcmp(kw, "deferred") == 0 || strcmp(kw, "action") == 0)
    return parser_parse_react_declarations(p, kw_tok, kw);
  if (strcmp(kw, "fetch") == 0 || strcmp(kw, "load") == 0 ||
      strcmp(kw, "lazy") == 0 || strcmp(kw, "suspense") == 0 ||
      strcmp(kw, "portal") == 0)
    return parser_parse_react_resources(p, kw_tok, kw);
  if (strcmp(kw, "errorBoundary") == 0 || strcmp(kw, "provide") == 0 ||
      strcmp(kw, "loading") == 0)
    return parser_parse_react_boundaries(p, kw_tok, kw);
  free(kw);
  return NULL;
}
