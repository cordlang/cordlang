#ifndef CORDLANG_DOMAIN_IR_PASS_H
#define CORDLANG_DOMAIN_IR_PASS_H

#include "domain/ir.h"

/*
 * In-tree IR transforms (Phase H3). Opt-in via CLI --pass and/or
 * cordlang.json "passes" (comma-separated names).
 *
 * Dynamic plugins (dlopen/WASM) are out of scope.
 */

typedef IrProgram *(*IrPassFn)(IrProgram *ir);

typedef struct {
  const char *name;
  IrPassFn fn;
  const char *description;
} IrPassDesc;

/* Built-in registry (static). */
const IrPassDesc *ir_pass_find(const char *name);
void ir_pass_list(void); /* print names to stderr */

/* Apply named passes in order. Mutates ir in place; returns ir (or NULL). */
IrProgram *ir_pass_apply(IrProgram *ir, const char *const *names, int n_names);

/* Parse comma/space-separated names into malloc'd argv-style list.
 * *out_n set; caller frees array with ir_pass_names_free. */
char **ir_pass_parse_list(const char *csv, int *out_n);
void ir_pass_names_free(char **names, int n);

#endif
