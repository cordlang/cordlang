#include "adapters/outbound/backends/source_attr.h"
#include "application/ports/fs_port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cord_write_sourcemap_file(const char *generated_path,
                              const char *const *sources, int n_sources) {
  if (!generated_path) return -1;

  char *base = fs_basename(generated_path);
  const char *file_field = base ? base : "out";

  size_t glen = 0;
  char *content = fs_read_file(generated_path, &glen);
  char *json = NULL;
  if (content) {
    json = cord_sourcemap_build(file_field, content);
    free(content);
  }
  if (!json) json = cord_sourcemap_stub(file_field, sources, n_sources);
  free(base);
  if (!json) return -1;

  size_t len = strlen(generated_path);
  char *map_path = malloc(len + 5);
  if (!map_path) {
    free(json);
    return -1;
  }
  memcpy(map_path, generated_path, len);
  memcpy(map_path + len, ".map", 5);

  int rc = fs_write_file(map_path, json);
  if (rc == 0) printf("Wrote %s\n", map_path);
  free(map_path);
  free(json);
  return rc;
}
