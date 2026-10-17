#include "adapters/outbound/backends/esm/esm_css_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int class_set_has_prefix(ClassSet *set, const char *prefix) {
  if (!set || !prefix) return 0;
  size_t n = strlen(prefix);
  for (int i = 0; i < set->count; i++) {
    if (strncmp(set->names[i], prefix, n) == 0) return 1;
  }
  return 0;
}

char *esm_base_css(IrProgram *ir) {
  Cb out;
  cb_init(&out);
  cb_add(&out, esm_base_css_main);

  ClassSet *set = NULL;
  if (ir && ir->root) {
    set = calloc(1, sizeof(ClassSet));
    if (set) collect_from_ir(ir->root, set);
  }

  if (set && class_set_has_prefix(set, "cord-chart"))
    cb_add(&out, esm_base_css_chart);
  if (set && class_set_has_prefix(set, "cord-portal"))
    cb_add(&out, esm_base_css_portal);

  if (!set) return out.buf;

  cb_add(&out, "\n/* utilities for the classes this project emits */\n");

  /* Responsive variants are grouped so the media query is emitted once. */
  const char *bps[] = {"sm", "md", "lg"};
  const char *bp_min[] = {"640px", "768px", "1024px"};

  for (int i = 0; i < set->count; i++) {
    const char *cls = set->names[i];
    if (strncmp(cls, "sm:", 3) == 0 || strncmp(cls, "md:", 3) == 0 ||
        strncmp(cls, "lg:", 3) == 0)
      continue;
    char decls[512];
    if (!esm_css_decls_for(cls, decls, sizeof(decls))) continue;
    char sel[256];
    sel_escape(cls, sel, sizeof(sel));
    cb_addf(&out, ".%s { %s; }\n", sel, decls);
  }

  for (int b = 0; b < 3; b++) {
    int wrote_open = 0;
    char prefix[8];
    snprintf(prefix, sizeof(prefix), "%s:", bps[b]);
    for (int i = 0; i < set->count; i++) {
      const char *cls = set->names[i];
      if (strncmp(cls, prefix, strlen(prefix)) != 0) continue;
      char decls[512];
      if (!esm_css_decls_for(cls + strlen(prefix), decls, sizeof(decls))) continue;
      if (!wrote_open) {
        cb_addf(&out, "\n@media (min-width: %s) {\n", bp_min[b]);
        wrote_open = 1;
      }
      char sel[256];
      sel_escape(cls, sel, sizeof(sel));
      cb_addf(&out, "  .%s { %s; }\n", sel, decls);
    }
    if (wrote_open) cb_add(&out, "}\n");
  }

  free(set);
  return out.buf;
}
