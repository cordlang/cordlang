#ifndef CORDLANG_ADD_SERVICE_H
#define CORDLANG_ADD_SERVICE_H

/* Use case: copy/link a local Cord package into the current project.
 * dest_lib != 0 → src/lib/<name>/ ; else src/vendor/<name>/
 * Returns 0 on success. */
int add_service_run(const char *path_or_name, int dest_lib);

#endif
