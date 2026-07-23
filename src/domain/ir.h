#ifndef CORDLANG_DOMAIN_IR_H
#define CORDLANG_DOMAIN_IR_H

#include "domain/ast.h"
#include <stddef.h>

/*
 * Canonical post-AST Intermediate Representation.
 *
 * Pipeline:
 *   .cord → Lexer → Parser → AST → ir_from_ast() → IrProgram
 *                                              ↓
 *                              backends (React / Svelte / HTML)
 *
 * Each IrNode keeps a weak `origin` pointer into the AST used to build it
 * (valid while the AST lives). Backends consume IrProgram at the port
 * boundary; body codegen may still use origin for full fidelity until
 * pure-IR walkers land (IR-2).
 */

typedef enum {
  IR_PROJECT,
  IR_COMPONENT,
  IR_LAYOUT,
  IR_ROUTE,
  IR_PROP,
  IR_STATE,
  IR_COMPUTED,
  IR_EFFECT,
  IR_ELEMENT,
  IR_TEXT,
  IR_INTERP,
  IR_IF,
  IR_FOR,
  IR_EVENT,
  IR_ATTR,
  IR_SLOT,
  IR_FETCH,
  IR_HOOK, /* generic hook bag: kind string in name */
  IR_MODULE_USE,
  IR_AWAIT,   /* {#await} / Suspense-like promise block */
  IR_SNIPPET, /* {#snippet} */
  IR_STORE,   /* writable store decl */
  IR_RENDER,  /* {@render} */
} IrKind;

typedef struct IrNode {
  IrKind kind;
  char *name;  /* tag, component name, hook kind */
  char *value; /* path, expr, etc */
  char *value2;
  int line, col;
  char *file; /* optional source file */
  /* Weak pointer to the AST node that produced this IR node (not owned). */
  Node *origin;
  struct IrNode **kids;
  size_t n_kids, cap;
} IrNode;

typedef struct {
  IrNode *root; /* IR_PROJECT */
  /* Weak: AST that produced this program (not owned; caller keeps AST alive). */
  AST *origin_ast;
  const char *entry_file;
} IrProgram;

/* Convert AST root (NODE_ROOT) into IR_PROJECT tree. entry_file is optional.
 * origin pointers remain valid only while `ast_root` / its AST lives. */
IrProgram *ir_from_ast(Node *ast_root, const char *entry_file);
void ir_free(IrProgram *p);

/* Human-readable tree dump for `cordlang compile file.cord --ir`. Caller frees. */
char *ir_dump(const IrProgram *p);

const char *ir_kind_name(IrKind k);

/* ── IR walk helpers (shared by backends) ───────────────── */

/* First direct child with kind, or NULL. */
IrNode *ir_find_child(const IrNode *n, IrKind kind);

/* First IR_HOOK child with hook kind name (e.g. "context", "action"). */
IrNode *ir_find_hook(const IrNode *n, const char *hook_kind);

/* ATTR value for name under node, or NULL. */
const char *ir_attr(const IrNode *n, const char *attr_name);

/* Count direct kids of kind. */
size_t ir_count_kind(const IrNode *n, IrKind kind);

/* Prefer origin AST node; falls back to NULL. */
Node *ir_origin(const IrNode *n);

/* Project root origin (NODE_ROOT), or NULL. */
Node *ir_project_origin(const IrProgram *p);

#endif
