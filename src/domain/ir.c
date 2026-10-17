#include "domain/ir_internal.h"
#include <stdlib.h>

IrProgram *ir_from_ast(Node *ast_root, const char *entry_file) {
  if (!ast_root) return NULL;
  IrProgram *program = calloc(1, sizeof(IrProgram));
  if (!program) return NULL;
  program->entry_file = entry_file;
  program->origin_ast = NULL;
  program->root = ir_convert_node(ast_root, entry_file);
  if (!program->root) {
    free(program);
    return NULL;
  }
  return program;
}

void ir_free(IrProgram *program) {
  if (!program) return;
  ir_free_node(program->root);
  free(program);
}
