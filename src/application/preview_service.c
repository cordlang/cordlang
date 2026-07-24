#include "application/preview_service.h"
#include "application/compile_service.h"
#include "application/ports/backend_port.h"
#include "application/ports/fs_port.h"
#include "adapters/outbound/runtime/preview_server.h"
#include "adapters/outbound/json/json_mini.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_entry_from_config(const char *project_dir) {
  char *cfg = fs_join(project_dir, "cordlang.json");
  if (!cfg) return NULL;

  size_t len = 0;
  char *json = fs_read_file(cfg, &len);
  free(cfg);
  if (!json) return NULL;

  char *entry = json_object_get_string(json, "entry");
  free(json);

  if (!entry) entry = strdup("src/app.cord");
  return entry;
}

int preview_service_run(const char *project_dir) {
  const char *dir = project_dir && *project_dir ? project_dir : ".";
  backend_register_all();

  const BackendPort *backend = backend_find("html");
  if (!backend) {
    fprintf(stderr, "Error: HTML runtime backend not registered\n");
    return 1;
  }

  char *cfg = fs_join(dir, "cordlang.json");
  if (!fs_exists(cfg)) {
    fprintf(stderr, "Error: not a Cordlang project (missing cordlang.json)\n");
    fprintf(stderr, "Run: cordlang init\n");
    free(cfg);
    return 1;
  }
  free(cfg);

  char *entry_rel = read_entry_from_config(dir);
  char *entry = fs_join(dir, entry_rel);
  free(entry_rel);

  if (!fs_exists(entry)) {
    fprintf(stderr, "Error: entry file not found: %s\n", entry);
    free(entry);
    return 1;
  }

  printf("Compiling %s -> native HTML runtime...\n", entry);
  char *html = compile_service_file(entry, "html");
  free(entry);
  if (!html) return 1;

  /* Also dump to dist/preview for inspection / offline open */
  if (backend->scaffold) {
    backend->scaffold(dir, html);
  }

  size_t html_len = strlen(html);
  int rc = preview_server_serve(html, html_len, 4173);
  free(html);
  return rc;
}
