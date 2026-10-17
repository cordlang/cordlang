#include "domain/expr_internal.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

void expr_lexer_init(ELex *L, const char *src) {
  memset(L, 0, sizeof(*L));
  L->src = src ? src : "";
  L->len = strlen(L->src);
  L->type = ET_EOF;
}
static void elex_skip_ws(ELex *L) {
  while (L->pos < L->len) {
    char c = L->src[L->pos];
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
      L->pos++;
    else
      break;
  }
}
void expr_lexer_next(ELex *L) {
  elex_skip_ws(L);
  if (L->pos >= L->len) {
    L->type = ET_EOF;
    L->start = L->src + L->pos;
    L->tlen = 0;
    return;
  }
  const char *p = L->src + L->pos;
  char c = *p;

  if (isalpha((unsigned char)c) || c == '_' || c == '$') {
    size_t i = 0;
    while (L->pos + i < L->len) {
      char ch = p[i];
      if (isalnum((unsigned char)ch) || ch == '_' || ch == '$')
        i++;
      else
        break;
    }
    L->type = ET_IDENT;
    L->start = p;
    L->tlen = i;
    L->pos += i;
    return;
  }

  if (isdigit((unsigned char)c) ||
      (c == '.' && L->pos + 1 < L->len &&
       isdigit((unsigned char)L->src[L->pos + 1]))) {
    size_t i = 0;
    while (L->pos + i < L->len && isdigit((unsigned char)p[i])) i++;
    if (L->pos + i < L->len && p[i] == '.') {
      i++;
      while (L->pos + i < L->len && isdigit((unsigned char)p[i])) i++;
    }
    L->type = ET_NUMBER;
    L->start = p;
    L->tlen = i;
    L->pos += i;
    return;
  }

  if (c == '"' || c == '\'') {
    char q = c;
    size_t i = 1;
    while (L->pos + i < L->len && p[i] != q) {
      if (p[i] == '\\' && L->pos + i + 1 < L->len) i += 2;
      else i++;
    }
    if (L->pos + i >= L->len || p[i] != q) {
      L->type = ET_ERROR;
      snprintf(L->err, sizeof(L->err), "unterminated string");
      L->failed = 1;
      L->start = p;
      L->tlen = 1;
      L->pos++;
      return;
    }
    i++; /* closing quote */
    L->type = ET_STRING;
    L->start = p;
    L->tlen = i;
    L->pos += i;
    return;
  }

  /* two-char ops */
  if (L->pos + 1 < L->len) {
    char c2 = L->src[L->pos + 1];
    if (c == '&' && c2 == '&') {
      L->type = ET_ANDAND;
      L->start = p;
      L->tlen = 2;
      L->pos += 2;
      return;
    }
    if (c == '|' && c2 == '|') {
      L->type = ET_OROR;
      L->start = p;
      L->tlen = 2;
      L->pos += 2;
      return;
    }
    if (c == '=' && c2 == '=') {
      L->type = ET_EQEQ;
      L->start = p;
      L->tlen = 2;
      L->pos += 2;
      return;
    }
    if (c == '!' && c2 == '=') {
      L->type = ET_NEQ;
      L->start = p;
      L->tlen = 2;
      L->pos += 2;
      return;
    }
    if (c == '<' && c2 == '=') {
      L->type = ET_LE;
      L->start = p;
      L->tlen = 2;
      L->pos += 2;
      return;
    }
    if (c == '>' && c2 == '=') {
      L->type = ET_GE;
      L->start = p;
      L->tlen = 2;
      L->pos += 2;
      return;
    }
  }

  L->start = p;
  L->tlen = 1;
  L->pos++;
  switch (c) {
    case '.':
      L->type = ET_DOT;
      break;
    case '(':
      L->type = ET_LPAREN;
      break;
    case ')':
      L->type = ET_RPAREN;
      break;
    case ',':
      L->type = ET_COMMA;
      break;
    case '+':
      L->type = ET_PLUS;
      break;
    case '-':
      L->type = ET_MINUS;
      break;
    case '*':
      L->type = ET_STAR;
      break;
    case '/':
      L->type = ET_SLASH;
      break;
    case '%':
      L->type = ET_PERCENT;
      break;
    case '!':
      L->type = ET_BANG;
      break;
    case '<':
      L->type = ET_LT;
      break;
    case '>':
      L->type = ET_GT;
      break;
    case '=':
      L->type = ET_EQ;
      break;
    case '?':
      L->type = ET_QUESTION;
      break;
    case ':':
      L->type = ET_COLON;
      break;
    case '[':
      L->type = ET_LBRACKET;
      break;
    case ']':
      L->type = ET_RBRACKET;
      break;
    default:
      L->type = ET_ERROR;
      snprintf(L->err, sizeof(L->err), "unexpected character '%c'", c);
      L->failed = 1;
      break;
  }
}
