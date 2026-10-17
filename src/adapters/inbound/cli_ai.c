#include "adapters/inbound/cli_internal.h"
#include "adapters/outbound/backends/preset_registry.h"
#include "application/analyze_service.h"
#include "application/check_service.h"
#include "application/ports/fs_port.h"
#include "application/preset_service.h"
#include "domain/diag.h"
#include "domain/version.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ai_print_context(void) {
  static const char *paths[] = {
      "docs/AI_CONTEXT.md",
      "cordlang/docs/AI_CONTEXT.md",
      "../docs/AI_CONTEXT.md",
      NULL,
  };
  int printed = 0;
  for (int i = 0; paths[i]; i++) {
    if (!fs_exists(paths[i])) continue;
    size_t len = 0;
    char *body = fs_read_file(paths[i], &len);
    if (!body) continue;
    fwrite(body, 1, len, stdout);
    if (len == 0 || body[len - 1] != '\n') fputc('\n', stdout);
    free(body);
    printed = 1;
    break;
  }
  if (!printed) {
    printf("# Cordlang AI context (embedded fallback)\n\n");
    printf("Prefer .cord over JSX. Use #{expr}, @click=, state/setX.\n");
    printf("Validate: cordlang check [--json]\n");
    printf("Schema: docs/schema/attrs.json · Skill: skills/write-cord/\n");
    printf("Capabilities: presets icons/motion/charts — see docs/LIBRARIES.md\n");
    printf("Full file missing — open docs/AI_CONTEXT.md from the repo.\n");
  }
  /* Append active project presets when cordlang.json is nearby */
  {
    char names[16][PRESET_NAME_LEN];
    const char *dirs[] = {".", "my-app", NULL};
    for (int d = 0; dirs[d]; d++) {
      int n = preset_load_from_project(dirs[d], names, 16);
      if (n < 0) continue;
      printf("\n## Project presets (%s)\n\n", dirs[d]);
      if (n == 0)
        printf("(none — `cordlang preset add icons motion`)\n");
      else {
        for (int i = 0; i < n; i++) printf("- %s\n", names[i]);
      }
      printf("\nBackends: react, svelte, vue, solid (adapters merge npm per target).\n");
      break;
    }
  }
  return 0;
}
static int ai_doctor(int argc, char **argv) {
  const char *path = ".";
  for (int i = 0; i < argc; i++) {
    if (argv[i][0] != '-') path = argv[i];
  }
  char *entry = cli_resolve_check_entry(path);
  if (!entry) return 1;

  printf("cordlang ai doctor\n");
  printf("  version: %s\n", CORDLANG_VERSION);
  printf("  entry: %s\n", entry);

  DiagList check_diags;
  diag_list_init(&check_diags);
  int check_rc = check_service_run(entry, &check_diags);
  printf("  check: %s (%d error(s), %d warning(s), %d info)\n",
         check_rc == 0 ? "ok" : "FAILED", diag_error_count(&check_diags),
         diag_count_level(&check_diags, DIAG_WARN),
         diag_count_level(&check_diags, DIAG_INFO));
  diag_print_all(&check_diags);
  diag_list_free(&check_diags);

  DiagList analyze_diags;
  diag_list_init(&analyze_diags);
  int score = 0;
  int arc = analyze_service_run_opts(entry, &analyze_diags, 1, &score);
  if (arc != 0) {
    printf("  analyze: FAILED (parse)\n");
    diag_print_all(&analyze_diags);
  } else {
    printf("  analyze score: %d/100 (%d warning(s), %d info)\n", score,
           diag_count_level(&analyze_diags, DIAG_WARN),
           diag_count_level(&analyze_diags, DIAG_INFO));
    diag_print_all(&analyze_diags);
  }
  diag_list_free(&analyze_diags);
  free(entry);
  return check_rc != 0 ? 1 : 0;
}
int cli_cmd_ai(int argc, char **argv) {
  if (argc > 0 && strcmp(argv[0], "check") == 0)
    return cli_cmd_check(argc - 1, argv + 1);
  if (argc > 0 && strcmp(argv[0], "context") == 0)
    return ai_print_context();
  if (argc > 0 && strcmp(argv[0], "doctor") == 0)
    return ai_doctor(argc - 1, argv + 1);

  printf("Cordlang AI workflow (LLM outside the compiler)\n\n");
  printf("1. Edit .cord with skills/write-cord (or any model + docs/AI_CONTEXT.md)\n");
  printf("2. Validate:  cordlang check [path] [--json]\n");
  printf("3. Optional:  cordlang analyze [path] [--json]\n");
  printf("4. Preview:   cordlang run react --watch\n");
  printf("              cordlang run svelte --check\n\n");
  printf("Docs: docs/AI_CONTEXT.md · docs/AI.md · docs/AI_WORKFLOW.md · docs/schema/attrs.json\n");
  printf("Skills: skills/write-cord/ · skills/fix-cord-check/\n\n");
  printf("Shortcuts:\n");
  printf("  cordlang ai check [path] [--json]\n");
  printf("  cordlang ai context\n");
  printf("  cordlang ai doctor [path]\n");
  return 0;
}
