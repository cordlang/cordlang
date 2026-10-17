#ifndef CORDLANG_COMPILER_INTERNAL_H
#define CORDLANG_COMPILER_INTERNAL_H
#include "application/ports/compiler_port.h"
#include "application/ports/fs_port.h"
#include "domain/ast.h"
#include <stddef.h>
#define MAX_VISITED 128
typedef struct { char *paths[MAX_VISITED]; int count; } VisitedSet;
int visited_has(VisitedSet *v, const char *path);
void visited_add(VisitedSet *v, const char *path);
void visited_free(VisitedSet *v);
char *module_export_name(const char *mod_path, const char *alias);
int path_is_module_ref(const char *s);
char *path_make_abs(const char *path);
int path_is_under_root(const char *path, const char *root);
char *find_project_root(const char *entry_path);
char *resolve_module_file(const char *from_file, const char *mod_path,
                          const char *project_root);
void merge_ast_children(Node *dst_root, Node *src_root);
void tag_components_in_root(Node *root, const char *file_path);
void ensure_component_wrapper(Node *root, const char *export_name,
                              const char *file_path);
int resolve_uses_in_ast(Node *root, const char *from_file, VisitedSet *visited,
                        char *errbuf, size_t errlen, const char *project_root);
int load_module_into(Node *dst_root, const char *from_file, const char *mod_path,
                     const char *alias, VisitedSet *visited, char *errbuf,
                     size_t errlen, const char *project_root);
#endif
