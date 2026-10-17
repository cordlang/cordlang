#ifndef CORDLANG_DOMAIN_IR_INTERNAL_H
#define CORDLANG_DOMAIN_IR_INTERNAL_H

#include "domain/ir.h"

char *ir_xstrdup(const char *s);
IrNode *ir_node_create(IrKind kind, const char *name, const char *value,
                       const char *value2, int line, int col, const char *file);
IrNode *ir_tag(IrNode *node, Node *ast);
void ir_add_kid(IrNode *parent, IrNode *child);
char *ir_maybe_normalize(const char *src);
IrNode *ir_make_hook(const char *hook_name, const Node *node, const char *file);
IrNode *ir_convert_node(const Node *node, const char *file);
void ir_convert_children(const Node *node, IrNode *parent, const char *file);
int ir_is_declaration_node(NodeType type);
IrNode *ir_convert_declaration(const Node *node, const char *file);

#endif
