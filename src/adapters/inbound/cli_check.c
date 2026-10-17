#include "adapters/inbound/cli_internal.h"
#include "adapters/outbound/json/json_mini.h"
#include "application/analyze_service.h"
#include "application/check_service.h"
#include "application/ports/fs_port.h"
#include "domain/diag.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *cli_resolve_check_entry(const char *path) {
  const char *p = path && *path ? path : ".";

  /* Direct .cord file */
  size_t n = strlen(p);
  if (n > 5 && strcmp(p + n - 5, ".cord") == 0) {
    if (!fs_exists(p)) {
      fprintf(stderr, "Error: file not found: %s\n", p);
      return NULL;
    }
    return strdup(p);
  }

  /* Project directory: cordlang.json entry or src/app.cord */
  char *cfg = fs_join(p, "cordlang.json");
  if (cfg && fs_exists(cfg)) {
    size_t len = 0;
    char *json = fs_read_file(cfg, &len);
    free(cfg);
    char *entry_rel = NULL;
    if (json) {
      entry_rel = json_object_get_string(json, "entry");
      free(json);
    }
    if (!entry_rel) entry_rel = strdup("src/app.cord");
    char *entry = fs_join(p, entry_rel);
    free(entry_rel);
    if (!entry || !fs_exists(entry)) {
      fprintf(stderr, "Error: entry file not found: %s\n",
              entry ? entry : "src/app.cord");
      free(entry);
      return NULL;
    }
    return entry;
  }
  free(cfg);

  /* Fallback: path/src/app.cord or plain src/app.cord */
  char *fallback = fs_join(p, "src/app.cord");
  if (fallback && fs_exists(fallback)) return fallback;
  free(fallback);

  if (fs_exists("src/app.cord")) return strdup("src/app.cord");

  fprintf(stderr,
          "Error: no entry found. Pass a .cord file or run in a project "
          "(cordlang.json / src/app.cord)\n");
  return NULL;
}
int cli_cmd_check(int argc, char **argv) {
  int as_json = 0;
  const char *path = ".";
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "--json") == 0)
      as_json = 1;
    else if (argv[i][0] != '-')
      path = argv[i];
  }
  char *entry = cli_resolve_check_entry(path);
  if (!entry) return 1;

  DiagList diags;
  diag_list_init(&diags);
  int rc = check_service_run(entry, &diags);
  if (as_json) {
    diag_print_json(&diags);
    fputc('\n', stdout);
  } else {
    diag_print_all(&diags);
    if (rc == 0 && diags.len == 0)
      printf("OK: no issues in %s\n", entry);
    else if (rc == 0)
      printf("OK: %zu warning(s), 0 errors in %s\n", diags.len, entry);
    else
      fprintf(stderr, "check failed: %d error(s) in %s\n",
              diag_error_count(&diags), entry);
  }

  diag_list_free(&diags);
  free(entry);
  return rc;
}
int cli_cmd_analyze(int argc, char **argv) {
  int as_json = 0;
  const char *path = ".";
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "--json") == 0)
      as_json = 1;
    else if (argv[i][0] != '-')
      path = argv[i];
  }
  char *entry = cli_resolve_check_entry(path);
  if (!entry) return 1;

  DiagList diags;
  diag_list_init(&diags);
  int score = 0;
  int rc = analyze_service_run_opts(entry, &diags, as_json ? 1 : 0, &score);
  if (as_json) {
    printf("{\"score\":%d,\"diagnostics\":", score);
    diag_print_json(&diags);
    printf("}\n");
  } else {
    diag_print_all(&diags);
  }
  diag_list_free(&diags);
  free(entry);
  return rc;
}
