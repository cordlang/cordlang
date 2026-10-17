#include "domain/expr_internal.h"
#include <stdio.h>
#include <string.h>

static void efail(EParser *P, const char *msg) {
  if (P->failed) return;
  P->failed = 1;
  snprintf(P->err, sizeof(P->err), "%s", msg);
}
static int eaccept(EParser *P, ETok t) {
  if (P->lex.type == t) {
    expr_lexer_next(&P->lex);
    return 1;
  }
  return 0;
}
static void eexpect(EParser *P, ETok t, const char *msg) {
  if (!eaccept(P, t)) efail(P, msg);
}
static void emit_tok(EParser *P) {
  if (P->lex.tlen > 0) {
    for (size_t i = 0; i < P->lex.tlen; i++)
      expr_buf_putc(&P->out, P->lex.start[i]);
  }
}
static void parse_primary(EParser *P) {
  if (P->failed) return;

  if (P->lex.type == ET_IDENT || P->lex.type == ET_NUMBER) {
    emit_tok(P);
    expr_lexer_next(&P->lex);
    return;
  }
  if (P->lex.type == ET_STRING) {
    emit_tok(P);
    expr_lexer_next(&P->lex);
    return;
  }
  if (eaccept(P, ET_LPAREN)) {
    expr_buf_putc(&P->out, '(');
    expr_parse(P);
    eexpect(P, ET_RPAREN, "expected ')'");
    expr_buf_putc(&P->out, ')');
    return;
  }
  efail(P, "expected expression");
}
static void parse_postfix(EParser *P) {
  parse_primary(P);
  if (P->failed) return;

  for (;;) {
    if (eaccept(P, ET_DOT)) {
      expr_buf_putc(&P->out, '.');
      if (P->lex.type != ET_IDENT) {
        efail(P, "expected identifier after '.'");
        return;
      }
      emit_tok(P);
      expr_lexer_next(&P->lex);
    } else if (eaccept(P, ET_LPAREN)) {
      expr_buf_putc(&P->out, '(');
      if (P->lex.type != ET_RPAREN) {
        expr_parse(P);
        while (eaccept(P, ET_COMMA)) {
          expr_buf_puts(&P->out, ", ");
          expr_parse(P);
        }
      }
      eexpect(P, ET_RPAREN, "expected ')' after call args");
      expr_buf_putc(&P->out, ')');
    } else if (eaccept(P, ET_LBRACKET)) {
      expr_buf_putc(&P->out, '[');
      expr_parse(P);
      eexpect(P, ET_RBRACKET, "expected ']'");
      expr_buf_putc(&P->out, ']');
    } else {
      break;
    }
  }
}
static void parse_unary(EParser *P) {
  if (P->failed) return;
  if (eaccept(P, ET_BANG)) {
    expr_buf_putc(&P->out, '!');
    parse_unary(P);
    return;
  }
  if (eaccept(P, ET_MINUS)) {
    expr_buf_putc(&P->out, '-');
    parse_unary(P);
    return;
  }
  if (eaccept(P, ET_PLUS)) {
    expr_buf_putc(&P->out, '+');
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
    expr_lexer_next(&P->lex);
    expr_buf_putc(&P->out, ' ');
    expr_buf_putc(&P->out, op);
    expr_buf_putc(&P->out, ' ');
    parse_unary(P);
  }
}
static void parse_add(EParser *P) {
  parse_mul(P);
  while (!P->failed && (P->lex.type == ET_PLUS || P->lex.type == ET_MINUS)) {
    char op = P->lex.type == ET_PLUS ? '+' : '-';
    expr_lexer_next(&P->lex);
    expr_buf_putc(&P->out, ' ');
    expr_buf_putc(&P->out, op);
    expr_buf_putc(&P->out, ' ');
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
    expr_lexer_next(&P->lex);
    expr_buf_putc(&P->out, ' ');
    expr_buf_puts(&P->out, op);
    expr_buf_putc(&P->out, ' ');
    parse_add(P);
  }
}
static void parse_and(EParser *P) {
  parse_cmp(P);
  while (!P->failed && eaccept(P, ET_ANDAND)) {
    expr_buf_puts(&P->out, " && ");
    parse_cmp(P);
  }
}
static void parse_or(EParser *P) {
  parse_and(P);
  while (!P->failed && eaccept(P, ET_OROR)) {
    expr_buf_puts(&P->out, " || ");
    parse_and(P);
  }
}
void expr_parse(EParser *P) {
  parse_or(P);
  if (P->failed) return;
  if (eaccept(P, ET_QUESTION)) {
    expr_buf_puts(&P->out, " ? ");
    expr_parse(P);
    eexpect(P, ET_COLON, "expected ':' in ternary");
    expr_buf_puts(&P->out, " : ");
    expr_parse(P);
    return;
  }
  if (eaccept(P, ET_EQ)) {
    expr_buf_puts(&P->out, " = ");
    expr_parse(P);
  }
}
