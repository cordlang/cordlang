/* Native smoke for wasm_api without Emscripten.
 * Built by playground/smoke_native.ps1 — same CORDLANG_WASM source set.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *cordlang_version(void);
char *cordlang_compile(const char *source, const char *backend);
void cordlang_free(char *p);

static const char *SAMPLE =
    "def Counter\n"
    "  state count=0\n"
    "  span \"#{count}\"\n"
    "  btn \"+\" @click=setCount(count + 1)\n";

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
  printf("PASS: wasm_api native smoke\n");
  return 0;
}
