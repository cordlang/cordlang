#include "adapters/outbound/html_escape.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#include <strings.h>
#endif

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

static char *js_escape_quote_to(char *out, size_t out_sz, const char *in,
                                char quote) {
  if (!out || out_sz == 0) return out;
  out[0] = '\0';
  if (!in) return out;

  size_t n = 0;
  for (const char *p = in; *p; p++) {
    const char *rep = NULL;
    char tmp[8];
    if (*p == '\\' || *p == quote) {
      tmp[0] = '\\';
      tmp[1] = *p;
      tmp[2] = 0;
      rep = tmp;
    } else if (*p == '\n')
      rep = "\\n";
    else if (*p == '\r')
      rep = "\\r";
    else if (*p == '\t')
      rep = "\\t";
    else if ((unsigned char)*p < 0x20) {
      /* drop other controls rather than emit raw */
      continue;
    } else {
      if (n + 2 > out_sz) break;
      out[n++] = *p;
      continue;
    }
    size_t rl = strlen(rep);
    if (n + rl + 1 > out_sz) break;
    memcpy(out + n, rep, rl);
    n += rl;
  }
  out[n] = '\0';
  return out;
}

char *js_escape_dq_to(char *out, size_t out_sz, const char *in) {
  return js_escape_quote_to(out, out_sz, in, '"');
}

char *js_escape_sq_to(char *out, size_t out_sz, const char *in) {
  return js_escape_quote_to(out, out_sz, in, '\'');
}

static char *js_escape_quote_dup(const char *in, char quote) {
  if (!in) return strdup("");
  /* worst case ~6x for \u00XX; use 4x + 1 */
  size_t cap = strlen(in) * 4 + 1;
  if (cap < 16) cap = 16;
  char *out = malloc(cap);
  if (!out) return strdup("");
  js_escape_quote_to(out, cap, in, quote);
  return out;
}

char *js_escape_dq_dup(const char *in) { return js_escape_quote_dup(in, '"'); }

char *js_escape_sq_dup(const char *in) { return js_escape_quote_dup(in, '\''); }

int url_href_is_safe(const char *href) {
  if (!href) return 1;
  while (*href && isspace((unsigned char)*href)) href++;
  if (!*href) return 1;
  if (strncasecmp(href, "javascript:", 11) == 0) return 0;
  if (strncasecmp(href, "data:", 5) == 0) return 0;
  if (strncasecmp(href, "vbscript:", 9) == 0) return 0;
  return 1;
}
