#include "adapters/inbound/cli_internal.h"
#include "adapters/outbound/backends/preset_registry.h"
#include "application/add_service.h"
#include "application/init_service.h"
#include "application/ports/backend_port.h"
#include "application/preset_service.h"
#include "application/preview_service.h"
#include "application/run_service.h"
#include "application/symbols_service.h"
#include "application/lsp_service.h"
#include "adapters/outbound/term/term_log.h"
#include "domain/version.h"
#include <stdio.h>
#include <string.h>

int cli_run(int argc, char **argv) {
  term_init();
  backend_register_all();

  if (argc < 2) {
    cli_print_usage();
    return 1;
  }

  const char *cmd = argv[1];

  if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 ||
      strcmp(cmd, "-h") == 0) {
    cli_print_usage();
    return 0;
  }

  if (strcmp(cmd, "--version") == 0 || strcmp(cmd, "-v") == 0 ||
      strcmp(cmd, "--v") == 0 || strcmp(cmd, "-V") == 0 ||
      strcmp(cmd, "version") == 0) {
    printf("cordlang %s\n", CORDLANG_VERSION);
    return 0;
  }

  if (strcmp(cmd, "init") == 0) {
    const char *name = ".";
    const char *tmpl = NULL;
    for (int i = 2; i < argc; i++) {
      if (strcmp(argv[i], "--template") == 0 || strcmp(argv[i], "-t") == 0) {
        if (i + 1 >= argc) {
          fprintf(stderr, "Error: --template requires a name\n");
          fprintf(stderr,
                  "Usage: cordlang init [name] --template <id>\n"
                  "Templates: counter, landing, dashboard, form-fetch, "
                  "docs-shell\n");
          return 1;
        }
        tmpl = argv[++i];
      } else if (argv[i][0] == '-') {
        fprintf(stderr, "Error: unknown init flag: %s\n", argv[i]);
        return 1;
      } else if (strcmp(name, ".") == 0) {
        name = argv[i];
      } else {
        fprintf(stderr, "Error: unexpected argument: %s\n", argv[i]);
        return 1;
      }
    }
    return init_service_run(name, tmpl);
  }

  if (strcmp(cmd, "add") == 0) {
    const char *pkg = NULL;
    int dest_lib = 0;
    for (int i = 2; i < argc; i++) {
      if (strcmp(argv[i], "--lib") == 0) {
        dest_lib = 1;
      } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
        printf("Usage: cordlang add <path-or-name> [--lib]\n\n");
        printf("Copy a local Cord package (folder of .cord files, optional\n");
        printf("cordlang.pkg.json) into the current project:\n");
        printf("  default → src/vendor/<name>/\n");
        printf("  --lib   → src/lib/<name>/\n\n");
        printf("See docs/PACKAGES.md\n");
        return 0;
      } else if (argv[i][0] == '-') {
        fprintf(stderr, "Error: unknown add flag: %s\n", argv[i]);
        return 1;
      } else if (!pkg) {
        pkg = argv[i];
      } else {
        fprintf(stderr, "Error: unexpected argument: %s\n", argv[i]);
        return 1;
      }
    }
    return add_service_run(pkg, dest_lib);
  }

  if (strcmp(cmd, "preset") == 0) {
    return preset_service_run(argc - 2, argv + 2);
  }

  if (strcmp(cmd, "run") == 0) {
    /*
     * No backend / preview → native ESM dev server in the .exe: each .cord is
     * compiled per request and served as a real ES module.
     * `run html` keeps the old single-document preview as an escape hatch.
     */
    int no_open = 0;
    for (int i = 2; i < argc; i++)
      if (strcmp(argv[i], "--no-open") == 0) no_open = 1;

    if (argc < 3 || argv[2][0] == '-' || strcmp(argv[2], "preview") == 0) {
      for (int i = 2; i < argc; i++)
        if (strcmp(argv[i], "--smoke") == 0)
          return preview_service_smoke(".");
      return preview_service_run(".", !no_open);
    }
    if (strcmp(argv[2], "html") == 0) return preview_service_run_html(".");
    int check = 0;
    int watch = 0;
    for (int i = 3; i < argc; i++) {
      if (strcmp(argv[i], "--check") == 0) check = 1;
      else if (strcmp(argv[i], "--watch") == 0) watch = 1;
    }
    return run_service_run(argv[2], ".", check, watch);
  }

  if (strcmp(cmd, "build") == 0) {
    return cli_cmd_build(argc - 2, argv + 2);
  }

  if (strcmp(cmd, "check") == 0) {
    return cli_cmd_check(argc - 2, argv + 2);
  }

  if (strcmp(cmd, "analyze") == 0) {
    return cli_cmd_analyze(argc - 2, argv + 2);
  }

  if (strcmp(cmd, "ai") == 0) {
    return cli_cmd_ai(argc - 2, argv + 2);
  }

  if (strcmp(cmd, "compile") == 0) {
    return cli_cmd_compile(argc - 2, argv + 2);
  }

  if (strcmp(cmd, "fmt") == 0) {
    return cli_cmd_fmt(argc - 2, argv + 2);
  }

  /* C7 — language tools */
  if (strcmp(cmd, "symbols") == 0) {
    const char *entry = argc > 2 ? argv[2] : NULL;
    return symbols_service_list(".", entry);
  }

  if (strcmp(cmd, "goto") == 0) {
    if (argc < 3) {
      fprintf(stderr, "Error: missing symbol name\n");
      fprintf(stderr, "Usage: cordlang goto <ComponentName> [entry.cord]\n");
      return 1;
    }
    const char *name = argv[2];
    const char *entry = argc > 3 ? argv[3] : NULL;
    return symbols_service_goto(".", name, entry);
  }

  if (strcmp(cmd, "lsp") == 0) {
    return lsp_service_run();
  }

  /* Back-compat: cordlang file.cord */
  if (strstr(cmd, ".cord")) {
    char *fake_argv[] = {(char *)cmd};
    return cli_cmd_compile(1, fake_argv);
  }

  fprintf(stderr, "Unknown command: %s\n\n", cmd);
  cli_print_usage();
  return 1;
}
