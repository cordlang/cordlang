#include "adapters/inbound/cli_internal.h"
#include "application/compile_service.h"
#include "application/ports/fs_port.h"
#include "application/preview_service.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cli_cmd_build(int argc, char **argv) {
  const char *backend = argc > 0 ? argv[0] : "react";
  const char *dir = ".";

  if (strcmp(backend, "esm") == 0)
    return preview_service_build_esm(dir);

  char *entry_cfg = fs_join(dir, "cordlang.json");
  if (!fs_exists(entry_cfg)) {
    fprintf(stderr, "Error: not a Cordlang project. Run: cordlang init\n");
    free(entry_cfg);
    return 1;
  }
  free(entry_cfg);

  char *out_dir = fs_join(dir, "dist");
  char *out_file = fs_join(out_dir, strcmp(backend, "react") == 0 ? "App.jsx" : "out.txt");
  free(out_dir);

  int rc = compile_service_to_file("src/app.cord", backend, out_file);
  free(out_file);
  return rc;
}
