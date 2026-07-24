#include "adapters/outbound/html_escape.h"

#include <stdio.h>
#include <string.h>

char *html_escape_to(char *out, size_t out_sz, const char *in) {
  if (!out || out_sz == 0) return out;
  out[0] = '\0';
  if (!in) return out;

  size_t n = 0;
  for (const char *p = in; *p; p++) {
    const char *rep = NULL;
    switch (*p) {
      case '&': rep = "&amp;"; break;
      case '<': rep = "&lt;"; break;
      case '>': rep = "&gt;"; break;
      case '"': rep = "&quot;"; break;
      case '\'': rep = "&#39;"; break;
      default: break;
    }
    if (rep) {
      size_t rl = strlen(rep);
      if (n + rl + 1 > out_sz) break;
      memcpy(out + n, rep, rl);
      n += rl;
    } else {
      if (n + 2 > out_sz) break;
      out[n++] = *p;
    }
  }
  out[n] = '\0';
  return out;
}
