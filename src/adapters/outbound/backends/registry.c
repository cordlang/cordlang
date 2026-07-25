#include "application/ports/backend_port.h"
#include "adapters/outbound/backends/react/react_backend.h"
#include "adapters/outbound/backends/html/html_backend.h"
#include "adapters/outbound/backends/svelte/svelte_backend.h"
#include "adapters/outbound/backends/vue/vue_backend.h"
#include "adapters/outbound/backends/solid/solid_backend.h"
#include "adapters/outbound/backends/email/email_backend.h"
#include "adapters/outbound/backends/pdf/pdf_backend.h"
#include "adapters/outbound/backends/next/next_backend.h"
#include "adapters/outbound/backends/sveltekit/sveltekit_backend.h"
#include "adapters/outbound/backends/esm/esm_backend.h"
#include <string.h>

#define MAX_BACKENDS 16

static const BackendPort *backends[MAX_BACKENDS];
static int backend_count = 0;
static int registered = 0;

void backend_register_all(void) {
  if (registered) return;
  backends[backend_count++] = html_backend_port();
  backends[backend_count++] = react_backend_port();
  backends[backend_count++] = svelte_backend_port();
  backends[backend_count++] = vue_backend_port();
  backends[backend_count++] = solid_backend_port();
  backends[backend_count++] = email_backend_port();
  backends[backend_count++] = pdf_backend_port();
  backends[backend_count++] = next_backend_port();
  backends[backend_count++] = sveltekit_backend_port();
  backends[backend_count++] = esm_backend_port();
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

int backend_list(const char **names, int max) {
  backend_register_all();
  int n = 0;
  for (int i = 0; i < backend_count && n < max; i++) {
    if (backends[i] && backends[i]->name) names[n++] = backends[i]->name;
  }
  return n;
}
