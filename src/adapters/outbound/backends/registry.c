#include "application/ports/backend_port.h"
#include "adapters/outbound/backends/react/react_backend.h"
#include "adapters/outbound/backends/html/html_backend.h"
#include "adapters/outbound/backends/svelte/svelte_backend.h"
#include "adapters/outbound/backends/vue/vue_backend.h"
#include "adapters/outbound/backends/email/email_backend.h"
#include "adapters/outbound/backends/esm/esm_backend.h"
#ifndef CORDLANG_WASM
#include "adapters/outbound/backends/solid/solid_backend.h"
#include "adapters/outbound/backends/pdf/pdf_backend.h"
#include "adapters/outbound/backends/next/next_backend.h"
#include "adapters/outbound/backends/sveltekit/sveltekit_backend.h"
#endif
#include <string.h>

#define MAX_BACKENDS 16

static const BackendPort *backends[MAX_BACKENDS];
static int backend_count = 0;
static int registered = 0;

void backend_register_all(void) {
  if (registered) return;
#ifdef CORDLANG_WASM
  /* Generate-only ports — scaffolds / preview servers stay out of the WASM link. */
  static const BackendPort html_p = {
      .name = "html",
      .extension = ".html",
      .needs_node_check = 0,
      .generate_from_ir = html_generate_from_ir,
      .generate = html_generate,
  };
  static const BackendPort react_p = {
      .name = "react",
      .extension = ".jsx",
      .needs_node_check = 0,
      .generate_from_ir = react_generate_from_ir,
      .generate = react_generate,
  };
  static const BackendPort svelte_p = {
      .name = "svelte",
      .extension = ".svelte",
      .needs_node_check = 0,
      .generate_from_ir = svelte_generate_from_ir,
      .generate = svelte_generate,
  };
  static const BackendPort vue_p = {
      .name = "vue",
      .extension = ".vue",
      .needs_node_check = 0,
      .generate_from_ir = vue_generate_from_ir,
      .generate = vue_generate,
  };
  static const BackendPort email_p = {
      .name = "email",
      .extension = ".html",
      .needs_node_check = 0,
      .generate_from_ir = email_generate_from_ir,
      .generate = email_generate,
  };
  static const BackendPort esm_p = {
      .name = "esm",
      .extension = ".js",
      .needs_node_check = 0,
      .generate_from_ir = esm_generate_from_ir,
      .generate = esm_generate,
  };
  backends[backend_count++] = &html_p;
  backends[backend_count++] = &react_p;
  backends[backend_count++] = &svelte_p;
  backends[backend_count++] = &vue_p;
  backends[backend_count++] = &email_p;
  backends[backend_count++] = &esm_p;
#else
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
#endif
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
