#ifndef CORDLANG_SYMBOLS_INTERNAL_H
#define CORDLANG_SYMBOLS_INTERNAL_H

#include "application/symbols_service.h"
#include "application/ports/compiler_port.h"
#include "application/ports/fs_port.h"
#include "domain/ast.h"
#include "domain/known_attrs.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <strings.h>
#define STRICMP strcasecmp
#else
#define STRICMP _stricmp
#endif

typedef struct {
  char **paths;
  size_t n, cap;
} PathList;

int path_same(const char *a, const char *b);
int pick_name_col(const char *text, int line_1, int hint_col_1,
                   const char *name, int prefer_last);
void str_list_add(PathList *pl, const char *s);
void path_list_add(PathList *pl, const char *path);
void path_list_free(PathList *pl);
void walk_add_path(const char *abs, const char *rel, void *ud);
char *file_text(const char *path, SymbolsOverlayFn overlay, void *ud,
                int *owned);
void parse_file_into(const char *path, const char *text, const char *name,
                     int include_decl, int want_names, PathList *names,
                     SymbolLocList *refs);

#endif