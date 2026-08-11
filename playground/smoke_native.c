/* Native smoke for wasm_api without Emscripten.
 * Built by playground/smoke_native.ps1 — same CORDLANG_WASM source set.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *cordlang_version(void);
char *cordlang_compile(const char *source, const char *backend);
char *cordlang_compile_project(const char *entry_path, const char *files_json,
                               const char *backend);
void cordlang_free(char *p);

static const char *SAMPLE =
    "def Counter\n"
    "  state count=0\n"
    "  span \"#{count}\"\n"
    "  btn \"+\" @click=setCount(count + 1)\n";

static const char *PROJECT_FILES =
    "{"
    "\"/playground/cordlang.json\":\"{\\\"name\\\":\\\"wasm-project\\\",\\\"entry\\\":\\\"src/app.cord\\\"}\","
    "\"/playground/src/app.cord\":\"use ./layouts/default\\nuse ./components/Counter\\nroute / => pages/HomePage\\ndef App\\n  HomePage\\n\","
    "\"/playground/src/layouts/default.cord\":\"layout default\\n  col\\n    slot\\n\","
    "\"/playground/src/pages/HomePage.cord\":\"use ../components/Counter\\ndef HomePage\\n  col\\n    h1 \\\"Home\\\"\\n    Counter\\n\","
    "\"/playground/src/components/Counter.cord\":\"def Counter\\n  state count=0\\n  btn \\\"#{count}\\\" @click=setCount(count + 1)\\n\""
    "}";

static const char *MISSING_MODULE_FILES =
    "{"
    "\"/playground/cordlang.json\":\"{}\","
    "\"/playground/src/app.cord\":\"use ./Missing\\n\""
    "}";

static const char *OUTSIDE_MODULE_FILES =
    "{"
    "\"/playground/cordlang.json\":\"{}\","
    "\"/playground/src/app.cord\":\"use ../../outside/Secret\\n\","
    "\"/outside/Secret.cord\":\"def Secret\\n  span \\\"outside\\\"\\n\""
    "}";

static const char *INVALID_MANIFEST =
    "{\"/playground/src/app.cord\":\"def App\\n\",}";

static int expect_project_success(void) {
  char *json = cordlang_compile_project("/playground/src/app.cord",
                                        PROJECT_FILES, "react");
  if (!json) {
    fprintf(stderr, "FAIL: cordlang_compile_project returned NULL\n");
    return 0;
  }
  int ok = strstr(json, "\"ok\":true") != NULL;
  int has_modules = strstr(json, "HomePage") != NULL &&
                    strstr(json, "Counter") != NULL &&
                    strstr(json, "DefaultLayout") != NULL;
  if (!ok || !has_modules) {
    fprintf(stderr, "FAIL: expected multi-file React project output\n%s\n", json);
    cordlang_free(json);
    return 0;
  }
  cordlang_free(json);

  json = cordlang_compile_project("/playground/src/app.cord", PROJECT_FILES,
                                  "ir");
  if (!json) {
    fprintf(stderr, "FAIL: project IR compile returned NULL\n");
    return 0;
  }
  ok = strstr(json, "\"ok\":true") != NULL &&
       strstr(json, "PROJECT file=/playground/src/app.cord") != NULL;
  if (!ok) fprintf(stderr, "FAIL: expected project IR output\n%s\n", json);
  cordlang_free(json);
  return ok;
}

static int expect_missing_module_failure(void) {
  char *json = cordlang_compile_project("/playground/src/app.cord",
                                        MISSING_MODULE_FILES, "react");
  if (!json) {
    fprintf(stderr, "FAIL: missing-module project returned NULL\n");
    return 0;
  }
  int failed = strstr(json, "\"ok\":false") != NULL &&
               strstr(json, "cannot resolve module") != NULL &&
               strstr(json, "\"file\":\"/playground/src/app.cord\"") != NULL;
  if (!failed)
    fprintf(stderr, "FAIL: expected missing-module diagnostic\n%s\n", json);
  cordlang_free(json);
  return failed;
}

static int expect_project_jail_failure(void) {
  char *json = cordlang_compile_project("/playground/src/app.cord",
                                        OUTSIDE_MODULE_FILES, "react");
  if (!json) {
    fprintf(stderr, "FAIL: jail project returned NULL\n");
    return 0;
  }
  int failed = strstr(json, "\"ok\":false") != NULL &&
               strstr(json, "outside project") != NULL;
  if (!failed)
    fprintf(stderr, "FAIL: expected project jail diagnostic\n%s\n", json);
  cordlang_free(json);
  return failed;
}

static int expect_invalid_manifest_failure(void) {
  char *json = cordlang_compile_project("/playground/src/app.cord",
                                        INVALID_MANIFEST, "react");
  if (!json) {
    fprintf(stderr, "FAIL: invalid manifest returned NULL\n");
    return 0;
  }
  int failed = strstr(json, "\"ok\":false") != NULL &&
               strstr(json, "cannot end with ','") != NULL;
  if (!failed)
    fprintf(stderr, "FAIL: expected strict manifest diagnostic\n%s\n", json);
  cordlang_free(json);
  return failed;
}

int main(void) {
  printf("cordlang wasm-api smoke %s\n", cordlang_version());
  char *json = cordlang_compile(SAMPLE, "react");
  if (!json) {
    fprintf(stderr, "FAIL: cordlang_compile returned NULL\n");
    return 1;
  }
  int ok = strstr(json, "\"ok\":true") != NULL;
  int has_code = strstr(json, "export default") != NULL ||
                 strstr(json, "function") != NULL ||
                 strstr(json, "\"code\":") != NULL;
  printf("%s\n", json);
  cordlang_free(json);
  if (!ok || !has_code) {
    fprintf(stderr, "FAIL: expected ok codegen JSON\n");
    return 1;
  }
  if (!expect_project_success()) return 1;
  /* A second map must not see modules mounted by the successful call above. */
  if (!expect_missing_module_failure()) return 1;
  if (!expect_project_jail_failure()) return 1;
  if (!expect_invalid_manifest_failure()) return 1;
  printf("PASS: wasm_api native smoke\n");
  return 0;
}
