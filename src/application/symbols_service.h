#ifndef CORDLANG_SYMBOLS_SERVICE_H
#define CORDLANG_SYMBOLS_SERVICE_H

#include <stddef.h>

/* C7 — basic language tools (CLI, not full LSP)
 * List components / routes / layouts from a project parse.
 * entry_path: path to entry .cord (e.g. src/app.cord), or NULL to use cordlang.json
 */
int symbols_service_list(const char *project_dir, const char *entry_path);

/* Print file path of a component/layout/page definition, or 1 if not found. */
int symbols_service_goto(const char *project_dir, const char *name,
                         const char *entry_path);

/* ── LSP rename / references (components, same model as goto) ─────────── */

typedef struct {
  char *path;  /* absolute, malloc */
  int line;    /* 1-based */
  int col;     /* 1-based start of the identifier */
  int length;  /* UTF-8 bytes == chars for ASCII names */
  int is_decl; /* 1 = def / layout name */
  int is_path; /* 1 = last segment of a use/route/lazy module path */
} SymbolLoc;

typedef struct {
  SymbolLoc *items;
  size_t len;
  size_t cap;
  char *decl_path; /* first defining .cord (malloc), or NULL */
  int decl_line;   /* 1-based; 0 if unknown / synthetic wrapper */
} SymbolLocList;

void symbol_loc_list_init(SymbolLocList *l);
void symbol_loc_list_free(SymbolLocList *l);

/* Optional overlay: return in-memory .cord text for abs_path, or NULL to
 * read from disk. Pointer is borrowed (not freed by the collector). */
typedef const char *(*SymbolsOverlayFn)(const char *abs_path, void *ud);

/* True if `name` is a component/layout goto would resolve (explicit def or
 * body-only module export). */
int symbols_name_is_component(const char *anchor_path, const char *name,
                              SymbolsOverlayFn overlay, void *ud);

/* Collect declaration + use/tag/path references for a component name.
 * include_decl: 0 skips def/layout name locations (LSP includeDeclaration). */
int symbols_collect_refs(const char *anchor_path, const char *name,
                         int include_decl, SymbolsOverlayFn overlay, void *ud,
                         const char **extra_paths, int n_extra,
                         SymbolLocList *out);

/* Validate a component rename. Returns 0 if ok.
 * 1 = invalid identifier / reserved; 2 = collides with another component.
 * msg_buf optional. */
int symbols_rename_check(const char *anchor_path, const char *old_name,
                         const char *new_name, SymbolsOverlayFn overlay,
                         void *ud, char *msg_buf, size_t msg_sz);

/* True if `name` is a legal Cord component/layout identifier (not a keyword
 * or built-in tag). */
int symbols_valid_component_name(const char *name);

#endif
