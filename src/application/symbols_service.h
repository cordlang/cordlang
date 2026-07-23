#ifndef CORDLANG_SYMBOLS_SERVICE_H
#define CORDLANG_SYMBOLS_SERVICE_H

/* C7 — basic language tools (CLI, not full LSP)
 * List components / routes / layouts from a project parse.
 * entry_path: path to entry .cord (e.g. src/app.cord), or NULL to use cordlang.json
 */
int symbols_service_list(const char *project_dir, const char *entry_path);

/* Print file path of a component/layout/page definition, or 1 if not found. */
int symbols_service_goto(const char *project_dir, const char *name,
                         const char *entry_path);

#endif
