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

/*
 * `cordlang build esm` — write a static tree under dist/esm/ (shell + runtime +
 * per-file modules). Preview subset only; no Node. Returns 0 on success.
 */
int preview_service_build_esm(const char *project_dir);

/*
 * In-process smoke for the preview URL map (used by tests/run_preview_smoke).
 * Returns 0 when checks pass.
 */
int preview_service_smoke(const char *project_dir);

#endif
