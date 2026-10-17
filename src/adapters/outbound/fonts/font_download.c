#include "adapters/outbound/fonts/font_cache.h"
#include "adapters/outbound/fonts/font_cache_internal.h"
#include "application/ports/fs_port.h"
#include "adapters/outbound/process/process_spawn.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FONT_UA \
  "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
  "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
static int file_nonempty(const char *path) {
  size_t len = 0;
  char *b = fs_read_file(path, &len);
  if (!b) return 0;
  free(b);
  return len > 64;
}

static void url_encode_family(const char *family, char *out, size_t n) {
  size_t j = 0;
  for (const char *p = family; *p && j + 4 < n; p++) {
    unsigned char c = (unsigned char)*p;
    if (c == ' ') {
      out[j++] = '+';
    } else if (isalnum(c) || c == '-' || c == '.') {
      out[j++] = (char)c;
    } else {
      int w = snprintf(out + j, n - j, "%%%02X", c);
      if (w > 0) j += (size_t)w;
    }
  }
  out[j] = '\0';
}

static int extract_woff2_url(const char *css, char *url, size_t n) {
  if (!css || !url || n < 16) return -1;
  const char *p = css;
  while ((p = strstr(p, "url(")) != NULL) {
    p += 4;
    while (*p == ' ' || *p == '"' || *p == '\'') p++;
    const char *end = p;
    while (*end && *end != ')' && *end != '"' && *end != '\'' && *end != ' ')
      end++;
    size_t len = (size_t)(end - p);
    if (len > 12 && len + 1 < n && strstr(p, "woff2") != NULL) {
      memcpy(url, p, len);
      url[len] = '\0';
      return 0;
    }
    if (len > 12 && len + 1 < n && strncmp(p, "http", 4) == 0) {
      /* Prefer any gstatic url if woff2 token not in path fragment yet */
      memcpy(url, p, len);
      url[len] = '\0';
      if (strstr(url, "gstatic") || strstr(url, "fonts")) return 0;
    }
  }
  return -1;
}

static int curl_download(const char *url, const char *out_path,
                         const char *user_agent) {
  if (!url || !out_path) return -1;
#ifdef _WIN32
  char *argv[] = {"curl.exe", "-fsSL", "-A", (char *)user_agent, "-o",
                  (char *)out_path, (char *)url, NULL};
#else
  char *argv[] = {"curl", "-fsSL", "-A", (char *)user_agent, "-o",
                  (char *)out_path, (char *)url, NULL};
#endif
  int rc = process_run(NULL, argv, 1);
  if (rc != 0) {
    /* Retry without .exe on Windows / bare curl */
    char *argv2[] = {"curl", "-fsSL", "-A", (char *)user_agent, "-o",
                     (char *)out_path, (char *)url, NULL};
    rc = process_run(NULL, argv2, 1);
  }
  return rc == 0 && file_nonempty(out_path) ? 0 : -1;
}

int font_cache_ensure(const char *family, int weight, char *out_path,
                      size_t out_sz) {
  if (!family || !*family || !out_path || out_sz < 8) return -1;
  if (weight < 100 || weight > 900) weight = 400;

  char *clean = font_cache_strip_quotes_dup(family);
  if (!clean) return -1;

  char slug[128];
  font_cache_slug(clean, slug, sizeof(slug));

  char fname[160];
  snprintf(fname, sizeof(fname), "%s-%d.woff2", slug, weight);

  char *dir = font_cache_dir();
  if (!dir) {
    free(clean);
    return -1;
  }
  char *path = fs_join(dir, fname);
  if (!path) {
    free(dir);
    free(clean);
    return -1;
  }

  if (file_nonempty(path)) {
    snprintf(out_path, out_sz, "%s", path);
    free(path);
    free(dir);
    free(clean);
    return 0;
  }

  /* Fetch CSS face list from Google Fonts CSS API, then the WOFF2. */
  char enc[256];
  url_encode_family(clean, enc, sizeof(enc));
  char css_url[512];
  snprintf(css_url, sizeof(css_url),
           "https://fonts.googleapis.com/css2?family=%s:wght@%d&display=swap",
           enc, weight);

  char *css_tmp = fs_join(dir, "_fetch.css");
  if (!css_tmp) {
    free(path);
    free(dir);
    free(clean);
    return -1;
  }

  if (curl_download(css_url, css_tmp, FONT_UA) != 0) {
    fprintf(stderr,
            "cordlang font: could not fetch CSS for \"%s\" %d (need curl + "
            "network); using system fonts\n",
            clean, weight);
    free(css_tmp);
    free(path);
    free(dir);
    free(clean);
    return -1;
  }

  size_t css_len = 0;
  char *css = fs_read_file(css_tmp, &css_len);
  remove(css_tmp);
  free(css_tmp);
  if (!css) {
    free(path);
    free(dir);
    free(clean);
    return -1;
  }

  char woff_url[1024];
  if (extract_woff2_url(css, woff_url, sizeof(woff_url)) != 0) {
    fprintf(stderr, "cordlang font: no woff2 URL in CSS for \"%s\" %d\n", clean,
            weight);
    free(css);
    free(path);
    free(dir);
    free(clean);
    return -1;
  }
  free(css);

  if (curl_download(woff_url, path, FONT_UA) != 0) {
    fprintf(stderr, "cordlang font: download failed for \"%s\" %d\n", clean,
            weight);
    free(path);
    free(dir);
    free(clean);
    return -1;
  }

  snprintf(out_path, out_sz, "%s", path);
  free(path);
  free(dir);
  free(clean);
  return 0;
}

