#include "adapters/outbound/backends/esm/esm_css_internal.h"
int esm_css_decls_for(const char *cls, char *out, size_t outn) {
  return decls_for_colors(cls, out, outn) ||
         decls_for_spacing(cls, out, outn) ||
         decls_for_sizing(cls, out, outn) ||
         decls_for_radius(cls, out, outn) ||
         decls_for_display(cls, out, outn) ||
         decls_for_keywords(cls, out, outn);
}
