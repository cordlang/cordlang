#include "application/ports/backend_port.h"
#include "adapters/outbound/backends/react/react_backend.h"
#include "adapters/outbound/backends/html/html_backend.h"
#include "adapters/outbound/backends/svelte/svelte_backend.h"
#include <string.h>

#define MAX_BACKENDS 8

static const BackendPort *backends[MAX_BACKENDS];
static int backend_count = 0;
static int registered = 0;

void backend_register_all(void) {
  if (registered) return;
  backends[backend_count++] = html_backend_port();
  backends[backend_count++] = react_backend_port();
  backends[backend_count++] = svelte_backend_port();
  registered = 1;
}

const BackendPort *backend_find(const char *name) {
  backend_register_all();
  if (!name) return NULL;
  for (int i = 0; i < backend_count; i++) {
    if (strcmp(backends[i]->name, name) == 0) return backends[i];
  }
  return NULL;
}
