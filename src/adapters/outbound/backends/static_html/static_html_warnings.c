#include "static_html_internal.h"

void static_html_warn_spa_hooks(IrProgram *ir, const char *backend_name) {
  unsigned m = static_html_scan_spa_hooks(ir);
  if (!m) return;
  fprintf(stderr, "note: backend '%s' emits a static HTML snapshot; ",
          backend_name ? backend_name : "static");
  int first = 1;
  if (m & 1u) {
    fputs("state", stderr);
    first = 0;
  }
  if (m & 2u) {
    if (!first) fputs("/", stderr);
    fputs("@click/events", stderr);
    first = 0;
  }
  if (m & 4u) {
    if (!first) fputs("/", stderr);
    fputs("route", stderr);
  }
  fputs(" are not interactive (see docs/EMAIL.md / docs/PDF.md)\n", stderr);
}
