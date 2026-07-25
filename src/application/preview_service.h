#ifndef CORDLANG_PREVIEW_SERVICE_H
#define CORDLANG_PREVIEW_SERVICE_H

/*
 * `cordlang run` — serve the project as native ES modules.
 *
 * The browser statically imports the entry .cord; every request for a .cord URL
 * is compiled on demand and answered as JavaScript, so the real module graph is
 * what loads. No bundler, no Node, no npm install.
 */
int preview_service_run(const char *project_dir, int open_browser);

/*
 * `cordlang run html` — the legacy preview: one pre-rendered static document
 * with data-bind attributes over window globals. Kept as an escape hatch for
 * single-file demos and offline snapshots; it flattens components and routing.
 */
int preview_service_run_html(const char *project_dir);

#endif
