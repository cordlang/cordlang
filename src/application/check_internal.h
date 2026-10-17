#ifndef CORDLANG_APPLICATION_CHECK_INTERNAL_H
#define CORDLANG_APPLICATION_CHECK_INTERNAL_H

#include "application/check_service.h"
#include "application/ports/compiler_port.h"
#include "application/ports/fs_port.h"
#include "adapters/outbound/backends/preset_registry.h"
#include "domain/ast.h"
#include "domain/known_attrs.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NAMES 256
#define MAX_ROUTES 128
#define MAX_CONTEXTS 64
#define NAME_LEN 96

typedef struct {
  char names[MAX_NAMES][NAME_LEN];
  int count;
  int lines[MAX_NAMES];
  int cols[MAX_NAMES];
} NameSet;

typedef struct {
  char path[256];
  char target[NAME_LEN];
  int line, col;
} RouteInfo;

typedef struct {
  RouteInfo routes[MAX_ROUTES];
  int n_routes;
  char contexts[MAX_CONTEXTS][NAME_LEN];
  int n_contexts;
  char presets[16][PRESET_NAME_LEN];
  int n_presets;
} CheckCtx;

void load_presets_for_entry(const char *entry_path, CheckCtx *ctx);
int is_pascal_case(const char *s);
int name_set_has(const NameSet *s, const char *name);
int name_set_add(NameSet *s, const char *name, int line, int col);
int ctx_has_context(const CheckCtx *c, const char *name);
void ctx_add_context(CheckCtx *c, const char *name);
int extract_route_params(const char *path, char names[][NAME_LEN], int max_names);
void collect_defs(Node *n, NameSet *defs, NameSet *dup_report, DiagList *out,
                  const char *file);
void collect_contexts_and_routes(Node *n, CheckCtx *ctx);
void check_prop_types(Node *n, DiagList *out, const char *file);
void walk_checks(Node *n, const NameSet *defs, CheckCtx *ctx,
                 const char *enclosing_comp, DiagList *out, const char *file);
void check_routes(const CheckCtx *ctx, const NameSet *defs, DiagList *out,
                  const char *file);
void collect_use_names(Node *n, NameSet *defs);

#endif
