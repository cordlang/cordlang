#ifndef CORDLANG_PRESET_REGISTRY_H
#define CORDLANG_PRESET_REGISTRY_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Known capability / preset ids (product surface). */
#define PRESET_TAILWIND "tailwind"
#define PRESET_ICONS "icons"
#define PRESET_MOTION "motion"
#define PRESET_CHARTS "charts"

/* Backend ids matching CLI: react | svelte | vue | solid */
typedef enum {
  PRESET_BACKEND_REACT = 0,
  PRESET_BACKEND_SVELTE,
  PRESET_BACKEND_VUE,
  PRESET_BACKEND_SOLID,
  PRESET_BACKEND_COUNT
} PresetBackend;

PresetBackend preset_backend_from_name(const char *name);
const char *preset_backend_name(PresetBackend b);

/* Returns 1 if name is a known preset id. */
int preset_is_known(const char *name);

/* List known presets to stdout (for CLI). */
void preset_list_known(void);

/* Load presets[] from cordlang.json in project_dir into names[] (each NAME_LEN).
 * Returns count (0 if missing). max_names capped. */
#define PRESET_NAME_LEN 32
int preset_load_from_project(const char *project_dir, char names[][PRESET_NAME_LEN],
                             int max_names);

/* 1 if preset id is enabled in the loaded list. */
int preset_list_has(const char names[][PRESET_NAME_LEN], int n, const char *id);

/* Append npm dependency lines for enabled presets into package.json at path.
 * Merges into "dependencies" object. Returns 0 on success. */
int preset_merge_package_json(const char *package_json_path,
                              const char *project_dir, PresetBackend backend);

/* Extra CSS import lines for index.css (caller appends). Writes into buf.
 * Returns bytes written (0 if none). */
int preset_css_imports(const char *project_dir, char *buf, size_t buf_sz);

/* npm package for capability+backend (NULL if none / use native). */
const char *preset_npm_pkg(const char *preset_id, PresetBackend backend);

/* Write CordIcon / CordMotion / CordChart bridges into SPA out/src when presets on.
 * React: .jsx ; others write a minimal stub .js note. Returns 0 on success. */
int preset_write_bridges(const char *out_dir, const char *project_dir,
                         PresetBackend backend);

#ifdef __cplusplus
}
#endif

#endif
