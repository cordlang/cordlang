#include "domain/ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ast_oom(const char *what) {
  fprintf(stderr, "FATAL: out of memory in %s\n", what ? what : "ast");
  abort();
}

Node *node_create(NodeType type, const char *value, int line, int col) {
  Node *n = calloc(1, sizeof(Node));
  if (!n) ast_oom("node_create");
  n->magic = NODE_MAGIC;
  n->type = type;
  if (value) {
    n->value = strdup(value);
    if (!n->value) ast_oom("node_create value");
  }
  n->line = line;
  n->col = col;
  n->children_cap = 4;
  n->children = calloc(n->children_cap, sizeof(Node *));
  if (!n->children) ast_oom("node_create children");
  return n;
}

static void node_validate(Node *n) {
  if (!n) {
    fprintf(stderr, "FATAL: node_validate called with NULL\n");
    abort();
  }
  if (n->magic != NODE_MAGIC) {
    fprintf(stderr, "FATAL: Invalid node %p (magic=0x%08x, type=%d)\n",
            (void *)n, n->magic, n->type);
    abort();
  }
}

void node_add_child(Node *parent, Node *child) {
  node_validate(parent);
  node_validate(child);
  if (parent == child) {
    fprintf(stderr, "FATAL: Attempted to add node as its own child!\n");
    abort();
  }
  if (parent->children_len >= parent->children_cap) {
    size_t ncap = parent->children_cap * 2;
    Node **nch =
        realloc(parent->children, ncap * sizeof(Node *));
    if (!nch) ast_oom("node_add_child realloc");
    parent->children = nch;
    parent->children_cap = ncap;
  }
  parent->children[parent->children_len++] = child;
}

void node_free(Node *node) {
  if (!node) return;
  if (node->magic != NODE_MAGIC) {
    fprintf(stderr, "FATAL: node_free on invalid node %p\n", (void *)node);
    abort();
  }
  for (size_t i = 0; i < node->children_len; i++) {
    node_free(node->children[i]);
  }
  free(node->children);
  free(node->value);
  free(node->value2);
  node->magic = 0;
  free(node);
}

Node *node_clone(const Node *node) {
  node_validate((Node *)node);
  Node *n = node_create(node->type, node->value, node->line, node->col);
  if (node->value2) {
    n->value2 = strdup(node->value2);
    if (!n->value2) ast_oom("node_clone value2");
  }
  for (size_t i = 0; i < node->children_len; i++) {
    node_add_child(n, node_clone(node->children[i]));
  }
  return n;
}

AST *ast_create(void) {
  AST *ast = calloc(1, sizeof(AST));
  if (!ast) ast_oom("ast_create");
  ast->root = node_create(NODE_ROOT, NULL, 0, 0);
  return ast;
}

void ast_free(AST *ast) {
  if (ast) {
    node_free(ast->root);
    free(ast->source);
    free(ast);
  }
}
