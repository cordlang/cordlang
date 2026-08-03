#include "application/compile_service.h"
#include "application/ports/backend_port.h"
#include "application/ports/compiler_port.h"
#include "application/ports/fs_port.h"
#include "adapters/outbound/backends/source_attr.h"
#include "adapters/outbound/json/json_mini.h"
#include "domain/ir.h"
#include "domain/ir_pass.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void collect_sources_for_map(const char *cord_path,
                                    const char **sources, int *n_sources,
                                    int max_sources) {
  *n_sources = 0;
  if (!cord_path || max_sources <= 0) return;

  /* Always include the entry .cord */
  sources[(*n_sources)++] = cord_path;

  /* Prefer normalized project-relative form when under src/ */
  /* Heuristic extras: common multi-file layout (optional, only if exist) */
  static const char *extras[] = {
      "src/app.cord",
      "src/pages/HomePage.cord",
      "src/pages/AboutPage.cord",
      "src/components/Counter.cord",
      "src/layouts/default.cord",
      NULL,
  };
  for (int i = 0; extras[i] && *n_sources < max_sources; i++) {
    if (strcmp(extras[i], cord_path) == 0) continue;
    if (fs_exists(extras[i]) && !fs_is_dir(extras[i]))
      sources[(*n_sources)++] = extras[i];
  }
}

/* Load comma-separated "passes" from nearest cordlang.json (best-effort). */
static void load_passes_from_config(const char *cord_path, char ***names,
                                    int *n_names) {
  *names = NULL;
  *n_names = 0;
  if (!cord_path) return;

  char *dir = fs_dirname(cord_path);
  if (!dir) return;

  char *cfg = fs_join(dir, "cordlang.json");
  free(dir);
  if (!cfg) return;
  if (!fs_exists(cfg)) {
    /* try parent (entry often in src/) */
    char *parent = fs_dirname(cfg);
    free(cfg);
    if (!parent) return;
    cfg = fs_join(parent, "cordlang.json");
    free(parent);
    if (!cfg || !fs_exists(cfg)) {
      free(cfg);
      return;
    }
  }

  char *json = fs_read_file(cfg, NULL);
  free(cfg);
  if (!json) return;
  char *passes = json_object_get_string(json, "passes");
  free(json);
  if (!passes) return;
  *names = ir_pass_parse_list(passes, n_names);
  free(passes);
}

char *compile_service_source(const char *source, size_t source_len,
                             const char *backend_name, const char *file_label) {
  if (!source) return NULL;
  backend_register_all();

  int want_ir = backend_name && strcmp(backend_name, "ir") == 0;
  const BackendPort *backend = NULL;
  if (!want_ir) {
    backend = backend_find(backend_name);
    if (!backend) {
      fprintf(stderr, "Error: unknown backend '%s'\n",
              backend_name ? backend_name : "(null)");
      return NULL;
    }
  }

  CompileResult result = compiler_parse_source(source, source_len);
  if (!result.ok || !result.ast) {
    fprintf(stderr, "Error: failed to parse '%s'%s%s\n",
            file_label ? file_label : "<buffer>", result.error ? ": " : "",
            result.error ? result.error : "");
    compiler_result_free(&result);
    return NULL;
  }

  const char *label = file_label ? file_label : "<buffer>";
  IrProgram *ir = ir_from_ast(result.ast->root, label);
  char *out = NULL;
  if (want_ir) {
    out = ir_dump(ir);
  } else if (ir && backend && backend->generate_from_ir) {
    out = backend->generate_from_ir(ir);
  } else if (backend && backend->generate) {
    out = backend->generate(result.ast->root);
  }
  ir_free(ir);
  compiler_result_free(&result);
  return out;
}

char *compile_service_file(const char *cord_path, const char *backend_name) {
  return compile_service_file_ex(cord_path, backend_name, 0, NULL);
}

char *compile_service_file_ex(const char *cord_path, const char *backend_name,
                              int write_sourcemap, const char *map_out_path) {
  return compile_service_file_with_passes(cord_path, backend_name,
                                          write_sourcemap, map_out_path, NULL,
                                          0);
}

char *compile_service_file_with_passes(const char *cord_path,
                                       const char *backend_name,
                                       int write_sourcemap,
                                       const char *map_out_path,
                                       const char *const *passes, int n_passes) {
  backend_register_all();
  const BackendPort *backend = backend_find(backend_name);
  if (!backend) {
    fprintf(stderr, "Error: unknown backend '%s'\n", backend_name);
    return NULL;
  }

  /* Project-aware: resolve `use` and route module paths */
  CompileResult result = compiler_parse_project(cord_path);
  if (!result.ok || !result.ast) {
    fprintf(stderr, "Error: failed to parse '%s'%s%s\n", cord_path,
            result.error ? ": " : "", result.error ? result.error : "");
    compiler_result_free(&result);
    return NULL;
  }

  /* Merge config passes + CLI passes (CLI last). */
  char **cfg_names = NULL;
  int n_cfg = 0;
  load_passes_from_config(cord_path, &cfg_names, &n_cfg);

  const char **all = NULL;
  int n_all = 0;
  if (n_cfg + n_passes > 0) {
    all = calloc((size_t)(n_cfg + n_passes), sizeof(char *));
    for (int i = 0; i < n_cfg; i++) all[n_all++] = cfg_names[i];
    for (int i = 0; i < n_passes; i++) all[n_all++] = passes[i];
  }

  /* IR-first: AST → IR → optional passes → backend */
  IrProgram *ir = ir_from_ast(result.ast->root, cord_path);
  if (ir && n_all > 0) {
    ir = ir_pass_apply(ir, all, n_all);
    if (!ir) {
      free(all);
      ir_pass_names_free(cfg_names, n_cfg);
      compiler_result_free(&result);
      return NULL;
    }
  }
  free(all);
  ir_pass_names_free(cfg_names, n_cfg);

  char *out = NULL;
  if (ir && backend->generate_from_ir) {
    out = backend->generate_from_ir(ir);
  } else if (backend->generate) {
    out = backend->generate(result.ast->root);
  }
  ir_free(ir);
  compiler_result_free(&result);

  if (write_sourcemap && out) {
    const char *sources[32];
    int n_sources = 0;
    collect_sources_for_map(cord_path, sources, &n_sources, 32);

    char *base = NULL;
    if (map_out_path) {
      base = fs_basename(map_out_path);
    } else {
      /* default generated name from backend extension */
      size_t bl = strlen(backend->extension ? backend->extension : "txt");
      base = malloc(4 + bl + 1);
      if (base) {
        memcpy(base, "App.", 4);
        memcpy(base + 4, backend->extension ? backend->extension : "txt",
               bl + 1);
      }
    }
    char *json = cord_sourcemap_build(base ? base : "App.jsx", out);
    if (!json)
      json = cord_sourcemap_stub(base ? base : "App.jsx", sources, n_sources);
    free(base);

    if (json) {
      char *map_path = NULL;
      if (map_out_path) {
        size_t len = strlen(map_out_path);
        map_path = malloc(len + 5);
        if (map_path) {
          memcpy(map_path, map_out_path, len);
          memcpy(map_path + len, ".map", 5);
        }
      } else {
        /* next to input: file.cord → file.<ext>.map in cwd using App name */
        char tmp[512];
        const char *ext = backend->extension ? backend->extension : "txt";
        snprintf(tmp, sizeof(tmp), "App.%s.map", ext);
        map_path = strdup(tmp);
      }
      if (map_path) {
        if (fs_write_file(map_path, json) == 0)
          fprintf(stderr, "Wrote %s\n", map_path);
        else
          fprintf(stderr, "Error: cannot write '%s'\n", map_path);
        free(map_path);
      }
      free(json);
    }
  }

  return out;
}

int compile_service_to_file(const char *cord_path, const char *backend_name,
                            const char *out_path) {
  return compile_service_to_file_ex(cord_path, backend_name, out_path, 0);
}

int compile_service_to_file_ex(const char *cord_path, const char *backend_name,
                               const char *out_path, int write_sourcemap) {
  return compile_service_to_file_with_passes(cord_path, backend_name, out_path,
                                             write_sourcemap, NULL, 0);
}

int compile_service_to_file_with_passes(const char *cord_path,
                                        const char *backend_name,
                                        const char *out_path, int write_sourcemap,
                                        const char *const *passes, int n_passes) {
  char *code = compile_service_file_with_passes(
      cord_path, backend_name, 0, NULL, passes, n_passes);
  if (!code) return 1;
  int rc = fs_write_file(out_path, code);
  free(code);
  if (rc != 0) {
    fprintf(stderr, "Error: cannot write '%s'\n", out_path);
    return 1;
  }
  printf("Wrote %s\n", out_path);

  if (write_sourcemap) {
    const char *sources[32];
    int n_sources = 0;
    collect_sources_for_map(cord_path, sources, &n_sources, 32);
    if (cord_write_sourcemap_file(out_path, sources, n_sources) != 0) {
      fprintf(stderr, "Error: cannot write sourcemap for '%s'\n", out_path);
      return 1;
    }
  }
  return 0;
}
