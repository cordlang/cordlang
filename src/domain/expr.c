#include "domain/expr.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── growable string buffer ─────────────────────────────── */

typedef struct {
  char *data;
  size_t len;
  size_t cap;
} Buf;

static void buf_init(Buf *b) {
  b->cap = 64;
  b->len = 0;
  b->data = malloc(b->cap);
  if (b->data) b->data[0] = '\0';
}

static int buf_reserve(Buf *b, size_t need) {
  if (!b->data) return 0;
  if (b->len + need + 1 <= b->cap) return 1;
  size_t ncap = b->cap;
  while (b->len + need + 1 > ncap) ncap *= 2;
  char *nd = realloc(b->data, ncap);
  if (!nd) return 0;
  b->data = nd;
  b->cap = ncap;
  return 1;
}

static void buf_putc(Buf *b, char c) {
  if (!buf_reserve(b, 1)) return;
  b->data[b->len++] = c;
  b->data[b->len] = '\0';
}

static void buf_puts(Buf *b, const char *s) {
  if (!s) return;
  size_t n = strlen(s);
  if (!buf_reserve(b, n)) return;
  memcpy(b->data + b->len, s, n);
  b->len += n;
  b->data[b->len] = '\0';
}

static void buf_free(Buf *b) {
  free(b->data);
  b->data = NULL;
  b->len = b->cap = 0;
}

/* ── lexer ──────────────────────────────────────────────── */

typedef enum {
  ET_EOF,
  ET_IDENT,
  ET_NUMBER,
  ET_STRING,
  ET_DOT,
  ET_LPAREN,
  ET_RPAREN,
  ET_COMMA,
  ET_PLUS,
  ET_MINUS,
  ET_STAR,
  ET_SLASH,
  ET_PERCENT,
  ET_BANG,
  ET_EQEQ,
  ET_NEQ,
  ET_LT,
  ET_GT,
  ET_LE,
  ET_GE,
  ET_ANDAND,
  ET_OROR,
  ET_EQ,
  ET_QUESTION,
  ET_COLON,
  ET_LBRACKET,
  ET_RBRACKET,
  ET_ERROR
} ETok;

typedef struct {
  const char *src;
  size_t pos;
  size_t len;
  ETok type;
  const char *start;
  size_t tlen;
  char err[128];
  int failed;
} ELex;

static void elex_init(ELex *L, const char *src) {
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

static void elex_next(ELex *L) {
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

/* ── parser (builds normalized string) ──────────────────── */

typedef struct {
  ELex lex;
  Buf out;
  char err[128];
  int failed;
} EParser;

static void efail(EParser *P, const char *msg) {
  if (P->failed) return;
  P->failed = 1;
  snprintf(P->err, sizeof(P->err), "%s", msg);
}

static int eaccept(EParser *P, ETok t) {
  if (P->lex.type == t) {
    elex_next(&P->lex);
    return 1;
  }
  return 0;
}

static void eexpect(EParser *P, ETok t, const char *msg) {
  if (!eaccept(P, t)) efail(P, msg);
}

/* forward decls */
static void parse_expr(EParser *P);
static void parse_or(EParser *P);
static void parse_and(EParser *P);
static void parse_cmp(EParser *P);
static void parse_add(EParser *P);
static void parse_mul(EParser *P);
static void parse_unary(EParser *P);
static void parse_postfix(EParser *P);
static void parse_primary(EParser *P);

static void emit_tok(EParser *P) {
  if (P->lex.tlen > 0) {
    for (size_t i = 0; i < P->lex.tlen; i++)
      buf_putc(&P->out, P->lex.start[i]);
  }
}

static void parse_primary(EParser *P) {
  if (P->failed) return;

  if (P->lex.type == ET_IDENT || P->lex.type == ET_NUMBER) {
    emit_tok(P);
    elex_next(&P->lex);
    return;
  }
  if (P->lex.type == ET_STRING) {
    emit_tok(P);
    elex_next(&P->lex);
    return;
  }
  if (eaccept(P, ET_LPAREN)) {
    buf_putc(&P->out, '(');
    parse_expr(P);
    eexpect(P, ET_RPAREN, "expected ')'");
    buf_putc(&P->out, ')');
    return;
  }
  efail(P, "expected expression");
}

static void parse_postfix(EParser *P) {
  parse_primary(P);
  if (P->failed) return;

  for (;;) {
    if (eaccept(P, ET_DOT)) {
      buf_putc(&P->out, '.');
      if (P->lex.type != ET_IDENT) {
        efail(P, "expected identifier after '.'");
        return;
      }
      emit_tok(P);
      elex_next(&P->lex);
    } else if (eaccept(P, ET_LPAREN)) {
      buf_putc(&P->out, '(');
      if (P->lex.type != ET_RPAREN) {
        parse_expr(P);
        while (eaccept(P, ET_COMMA)) {
          buf_puts(&P->out, ", ");
          parse_expr(P);
        }
      }
      eexpect(P, ET_RPAREN, "expected ')' after call args");
      buf_putc(&P->out, ')');
    } else if (eaccept(P, ET_LBRACKET)) {
      buf_putc(&P->out, '[');
      parse_expr(P);
      eexpect(P, ET_RBRACKET, "expected ']'");
      buf_putc(&P->out, ']');
    } else {
      break;
    }
  }
}

static void parse_unary(EParser *P) {
  if (P->failed) return;
  if (eaccept(P, ET_BANG)) {
    buf_putc(&P->out, '!');
    parse_unary(P);
    return;
  }
  if (eaccept(P, ET_MINUS)) {
    buf_putc(&P->out, '-');
    parse_unary(P);
    return;
  }
  if (eaccept(P, ET_PLUS)) {
    buf_putc(&P->out, '+');
    parse_unary(P);
    return;
  }
  parse_postfix(P);
}

static void parse_mul(EParser *P) {
  parse_unary(P);
  while (!P->failed &&
         (P->lex.type == ET_STAR || P->lex.type == ET_SLASH ||
          P->lex.type == ET_PERCENT)) {
    char op = P->lex.type == ET_STAR ? '*' : P->lex.type == ET_SLASH ? '/' : '%';
    elex_next(&P->lex);
    buf_putc(&P->out, ' ');
    buf_putc(&P->out, op);
    buf_putc(&P->out, ' ');
    parse_unary(P);
  }
}

static void parse_add(EParser *P) {
  parse_mul(P);
  while (!P->failed && (P->lex.type == ET_PLUS || P->lex.type == ET_MINUS)) {
    char op = P->lex.type == ET_PLUS ? '+' : '-';
    elex_next(&P->lex);
    buf_putc(&P->out, ' ');
    buf_putc(&P->out, op);
    buf_putc(&P->out, ' ');
    parse_mul(P);
  }
}

static void parse_cmp(EParser *P) {
  parse_add(P);
  for (;;) {
    const char *op = NULL;
    if (P->lex.type == ET_EQEQ)
      op = "==";
    else if (P->lex.type == ET_NEQ)
      op = "!=";
    else if (P->lex.type == ET_LT)
      op = "<";
    else if (P->lex.type == ET_GT)
      op = ">";
    else if (P->lex.type == ET_LE)
      op = "<=";
    else if (P->lex.type == ET_GE)
      op = ">=";
    else
      break;
    elex_next(&P->lex);
    buf_putc(&P->out, ' ');
    buf_puts(&P->out, op);
    buf_putc(&P->out, ' ');
    parse_add(P);
  }
}

static void parse_and(EParser *P) {
  parse_cmp(P);
  while (!P->failed && eaccept(P, ET_ANDAND)) {
    buf_puts(&P->out, " && ");
    parse_cmp(P);
  }
}

static void parse_or(EParser *P) {
  parse_and(P);
  while (!P->failed && eaccept(P, ET_OROR)) {
    buf_puts(&P->out, " || ");
    parse_and(P);
  }
}

/* ternary: a ? b : c  and  assignment-like a = b (for event handlers) */
static void parse_expr(EParser *P) {
  parse_or(P);
  if (P->failed) return;
  if (eaccept(P, ET_QUESTION)) {
    buf_puts(&P->out, " ? ");
    parse_expr(P);
    eexpect(P, ET_COLON, "expected ':' in ternary");
    buf_puts(&P->out, " : ");
    parse_expr(P);
    return;
  }
  if (eaccept(P, ET_EQ)) {
    buf_puts(&P->out, " = ");
    parse_expr(P);
  }
}

char *expr_normalize(const char *src) {
  if (!src) return NULL;
  /* empty / whitespace-only → empty string (valid) */
  const char *p = src;
  while (*p && isspace((unsigned char)*p)) p++;
  if (!*p) return strdup("");

  EParser P;
  memset(&P, 0, sizeof(P));
  elex_init(&P.lex, src);
  buf_init(&P.out);
  if (!P.out.data) return NULL;

  elex_next(&P.lex);
  if (P.lex.failed) {
    buf_free(&P.out);
    return NULL;
  }

  parse_expr(&P);
  if (P.failed || P.lex.failed) {
    buf_free(&P.out);
    return NULL;
  }
  if (P.lex.type != ET_EOF) {
    buf_free(&P.out);
    return NULL;
  }

  char *result = P.out.data;
  P.out.data = NULL; /* transfer ownership */
  buf_free(&P.out);
  return result;
}

int expr_validate(const char *src, char *err, size_t errlen) {
  if (err && errlen) err[0] = '\0';
  if (!src) {
    if (err && errlen) snprintf(err, errlen, "null expression");
    return 0;
  }

  const char *p = src;
  while (*p && isspace((unsigned char)*p)) p++;
  if (!*p) return 1; /* empty ok */

  EParser P;
  memset(&P, 0, sizeof(P));
  elex_init(&P.lex, src);
  buf_init(&P.out);

  elex_next(&P.lex);
  if (P.lex.failed) {
    if (err && errlen) snprintf(err, errlen, "%s", P.lex.err);
    buf_free(&P.out);
    return 0;
  }

  parse_expr(&P);
  if (P.failed || P.lex.failed) {
    if (err && errlen)
      snprintf(err, errlen, "%s", P.failed ? P.err : P.lex.err);
    buf_free(&P.out);
    return 0;
  }
  if (P.lex.type != ET_EOF) {
    if (err && errlen) snprintf(err, errlen, "trailing tokens in expression");
    buf_free(&P.out);
    return 0;
  }

  buf_free(&P.out);
  return 1;
}
