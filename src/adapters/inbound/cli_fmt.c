#include "adapters/inbound/cli_internal.h"
#include "application/fmt_service.h"
#include <stdio.h>
#include <string.h>

int cli_cmd_fmt(int argc, char **argv) {
  int check = 0;
  const char *path = ".";
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "--check") == 0)
      check = 1;
    else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      printf("Usage: cordlang fmt [--check] [path]\n");
      printf("  path     .cord file or directory (default: .)\n");
      printf("  --check  exit 1 if any file would be reformatted\n");
      return 0;
    } else if (argv[i][0] != '-')
      path = argv[i];
  }
  /* default: write in place; --check: no write, fail if dirty */
  return fmt_service_run(path, check ? 0 : 1, check);
}
