#ifndef CORDLANG_RUN_SERVICE_H
#define CORDLANG_RUN_SERVICE_H

/* Use case: compile project entry + scaffold full backend project.
 * If check != 0, after first scaffold run npm install (if needed) + vite build.
 * If watch != 0, after first scaffold (and optional check) poll .cord sources
 * under src/ and rebuild on change until Ctrl+C. */
int run_service_run(const char *backend_name, const char *project_dir,
                    int check, int watch);

#endif
