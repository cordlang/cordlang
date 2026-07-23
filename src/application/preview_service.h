#ifndef CORDLANG_PREVIEW_SERVICE_H
#define CORDLANG_PREVIEW_SERVICE_H

/* Use case: parse project entry, generate HTML runtime preview,
 * serve it locally and open the browser. */
int preview_service_run(const char *project_dir);

#endif
