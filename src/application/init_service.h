#ifndef CORDLANG_INIT_SERVICE_H
#define CORDLANG_INIT_SERVICE_H

/* Use case: scaffold a new Cordlang project.
 * template_name: NULL → default blank project; else copy templates/<name>. */
int init_service_run(const char *project_name, const char *template_name);

#endif
