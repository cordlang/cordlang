#include "adapters/inbound/cli_internal.h"
#include "adapters/outbound/lexer/lexer.h"
#include "application/check_service.h"
#include "application/compile_service.h"
#include "application/ports/compiler_port.h"
#include "application/ports/fs_port.h"
#include "domain/ast.h"
#include "domain/diag.h"
#include "domain/ir.h"
#include "domain/ir_pass.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_tokens_from_file(const char *path) {
  size_t len = 0;
  char *source = fs_read_file(path, &len);
  if (!source) {
    fprintf(stderr, "Error: cannot read '%s'\n", path);
    return;
  }
  Lexer *lexer = lexer_create(source, len);
  lexer_tokenize(lexer);
  Token t;
  while ((t = lexer_next(lexer)).type != TOKEN_EOF) {
    printf("Token: ");
    switch (t.type) {
      case TOKEN_IDENTIFIER:
        printf("IDENTIFIER '%.*s'", (int)t.len, t.start);
        break;
      case TOKEN_STRING:
        printf("STRING '%.*s'", (int)t.len, t.start);
        break;
      case TOKEN_NUMBER:
        printf("NUMBER %.*s", (int)t.len, t.start);
        break;
      case TOKEN_AT:
        printf("@");
        break;
      case TOKEN_EQUALS:
        printf("=");
        break;
      case TOKEN_INDENT:
        printf("INDENT");
        break;
      case TOKEN_DEDENT:
        printf("DEDENT");
        break;
      case TOKEN_NEWLINE:
        printf("NEWLINE");
        break;
      default:
        printf("(%d)", t.type);
        break;
    }
    printf(" [line %d]\n", t.line);
  }
  lexer_destroy(lexer);
  free(source);
}
static void print_ast_node(Node *node, int depth) {
  for (int i = 0; i < depth; i++) printf("  ");
  switch (node->type) {
    case NODE_ROOT:
      printf("ROOT\n");
      break;
    case NODE_ELEMENT:
      printf("ELEMENT '%s'\n", node->value ? node->value : "");
      break;
    case NODE_TEXT:
      printf("TEXT '%s'\n", node->value ? node->value : "");
      break;
    case NODE_ATTR:
      printf("ATTR %s = %s\n", node->value ? node->value : "",
             node->value2 ? node->value2 : "");
      break;
    case NODE_EVENT:
      printf("EVENT @%s = %s\n", node->value ? node->value : "",
             node->value2 ? node->value2 : "");
      break;
    case NODE_BOOL_ATTR:
      printf("BOOL '%s'\n", node->value ? node->value : "");
      break;
    case NODE_FOR:
      printf("FOR %s in %s\n", node->value ? node->value : "",
             node->value2 ? node->value2 : "");
      break;
    case NODE_IF:
      printf("IF %s\n", node->value ? node->value : "");
      break;
    case NODE_COMPONENT_DEF:
      printf("COMPONENT def %s%s%s\n", node->value ? node->value : "?",
             node->value2 ? " " : "", node->value2 ? node->value2 : "");
      break;
    case NODE_PROPS_DECL:
      printf("PROPS\n");
      break;
    case NODE_STATE_DECL:
      printf("STATE %s%s%s\n", node->value ? node->value : "(block)",
             node->value2 ? " = " : "", node->value2 ? node->value2 : "");
      break;
    case NODE_COMPUTED_DECL:
      printf("COMPUTED %s = %s\n", node->value ? node->value : "?",
             node->value2 ? node->value2 : "?");
      break;
    case NODE_ROUTE:
      printf("ROUTE %s => %s", node->value ? node->value : "?",
             node->value2 ? node->value2 : "?");
      for (size_t ai = 0; ai < node->children_len; ai++) {
        Node *ch = node->children[ai];
        if (ch && ch->type == NODE_ATTR && ch->value &&
            strcmp(ch->value, "layout") == 0 && ch->value2)
          printf(" layout=%s", ch->value2);
      }
      printf("\n");
      break;
    case NODE_SLOT:
      printf("SLOT\n");
      break;
    case NODE_THEME:
      printf("THEME %s\n", node->value ? node->value : "?");
      break;
    case NODE_INTERPOLATION:
      if (node->value)
        printf("INTERP #{%s}\n", node->value);
      else
        printf("INTERP (template)\n");
      break;
    case NODE_USE:
      printf("USE %s%s%s\n", node->value ? node->value : "?",
             node->value2 ? " as " : "", node->value2 ? node->value2 : "");
      break;
    default:
      printf("NODE(%d)\n", node->type);
      break;
  }
  for (size_t i = 0; i < node->children_len; i++) {
    print_ast_node(node->children[i], depth + 1);
  }
}
int cli_cmd_compile(int argc, char **argv) {
  const char *file = NULL;
  const char *backend = "react";
  const char *out = NULL;
  int show_tokens = 0;
  int show_ast = 0;
  int show_ir = 0;
  int do_check = 0;
  int sourcemap = 0;
  const char *pass_buf[32];
  int n_passes = 0;

  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "--backend") == 0 && i + 1 < argc)
      backend = argv[++i];
    else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
      out = argv[++i];
    else if (strcmp(argv[i], "--tokens") == 0)
      show_tokens = 1;
    else if (strcmp(argv[i], "--ast") == 0)
      show_ast = 1;
    else if (strcmp(argv[i], "--ir") == 0)
      show_ir = 1;
    else if (strcmp(argv[i], "--check") == 0)
      do_check = 1;
    else if (strcmp(argv[i], "--sourcemap") == 0)
      sourcemap = 1;
    else if (strcmp(argv[i], "--list-passes") == 0) {
      ir_pass_list();
      return 0;
    } else if (strcmp(argv[i], "--pass") == 0 && i + 1 < argc) {
      if (n_passes < 32) pass_buf[n_passes++] = argv[++i];
      else
        ++i;
    } else if (argv[i][0] != '-')
      file = argv[i];
  }

  if (!file && !show_tokens) {
    /* allow --list-passes already returned */
    fprintf(stderr, "Error: missing .cord file\n");
    return 1;
  }

  if (show_tokens) {
    if (!file) {
      fprintf(stderr, "Error: missing .cord file\n");
      return 1;
    }
    print_tokens_from_file(file);
    return 0;
  }

  if (show_ast) {
    CompileResult r = compiler_parse_project(file);
    if (!r.ok || !r.ast) {
      fprintf(stderr, "Error: parse failed%s%s\n", r.error ? ": " : "",
              r.error ? r.error : "");
      return 1;
    }
    print_ast_node(r.ast->root, 0);
    compiler_result_free(&r);
    return 0;
  }

  if (show_ir) {
    CompileResult r = compiler_parse_project(file);
    if (!r.ok || !r.ast) {
      fprintf(stderr, "Error: parse failed%s%s\n", r.error ? ": " : "",
              r.error ? r.error : "");
      return 1;
    }
    IrProgram *ir = ir_from_ast(r.ast->root, file);
    if (!ir) {
      fprintf(stderr, "Error: IR conversion failed\n");
      compiler_result_free(&r);
      return 1;
    }
    if (n_passes > 0) {
      ir = ir_pass_apply(ir, pass_buf, n_passes);
      if (!ir) {
        compiler_result_free(&r);
        return 1;
      }
    }
    char *dump = ir_dump(ir);
    if (dump) {
      printf("%s", dump);
      free(dump);
    }
    ir_free(ir);
    compiler_result_free(&r);
    return 0;
  }

  if (do_check) {
    DiagList diags;
    diag_list_init(&diags);
    int crc = check_service_run(file, &diags);
    diag_print_all(&diags);
    int nerr = diag_error_count(&diags);
    diag_list_free(&diags);
    if (crc != 0 || nerr > 0) {
      fprintf(stderr, "compile --check: %d error(s), aborting\n", nerr);
      return 1;
    }
  }

  if (out)
    return compile_service_to_file_with_passes(file, backend, out, sourcemap,
                                               pass_buf, n_passes);

  char *code = compile_service_file_with_passes(file, backend, sourcemap, NULL,
                                                pass_buf, n_passes);
  if (!code) return 1;
  printf("%s", code);
  free(code);
  return 0;
}
