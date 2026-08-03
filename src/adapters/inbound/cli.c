#include "application/preset_service.h"
#include "adapters/outbound/backends/preset_registry.h"
#include "application/init_service.h"
#include "application/add_service.h"
#include "application/compile_service.h"
#include "application/check_service.h"
#include "application/analyze_service.h"
#include "application/fmt_service.h"
#include "application/run_service.h"
#include "application/preview_service.h"
#include "application/symbols_service.h"
#include "application/lsp_service.h"
#include "application/ports/backend_port.h"
#include "application/ports/compiler_port.h"
#include "application/ports/fs_port.h"
#include "adapters/outbound/lexer/lexer.h"
#include "adapters/outbound/json/json_mini.h"
#include "adapters/outbound/term/term_log.h"
#include "domain/ast.h"
#include "domain/diag.h"
#include "domain/ir.h"
#include "domain/ir_pass.h"
#include "domain/version.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(void) {
  printf("Cordlang — universal UI intermediate language\n\n");
  printf("Usage:\n");
  printf("  cordlang init [name] [--template <id>]  Create a new project\n");
  printf("  cordlang add <path-or-name> [--lib]     Copy a local Cord package\n");
  printf("  cordlang preset list|add <id>…         Project capabilities (icons/motion/…)\n");
  printf("  cordlang run                   Dev server, native ES modules (no Node)\n");
  printf("  cordlang run preview           Same as: cordlang run\n");
  printf("  cordlang run html              Legacy single-document preview\n");
  printf("  cordlang run --no-open         Dev server without opening a browser\n");
  printf("  cordlang run <backend>         Compile + scaffold full project\n");
  printf("  cordlang run <backend> --check Scaffold + npm install (if needed) + vite build\n");
  printf("  cordlang run <backend> --watch Watch src/**/*.cord and rebuild on change\n");
  printf("  cordlang build <backend>       Compile entry to dist only\n");
  printf("  cordlang build esm             Static ESM export → dist/esm (no Node)\n");
  printf("  cordlang check [path] [--json] Semantic checks (diagnostics)\n");
  printf("  cordlang analyze [path] [--json] Deterministic score / heuristics (no LLM)\n");
  printf("  cordlang ai                    AI workflow help (propose → check)\n");
  printf("  cordlang ai check [path]       Same as: cordlang check [path]\n");
  printf("  cordlang ai context            Print compact AI contract\n");
  printf("  cordlang ai doctor [path]      check + analyze summary\n");
  printf("  cordlang compile <file.cord>   Compile a single file to stdout\n");
  printf("  cordlang fmt [path]            Format .cord file(s) in place\n");
  printf("  cordlang fmt --check [path]    Exit 1 if formatting would change files\n");
  printf("  cordlang symbols [entry]       List components, routes, layouts\n");
  printf("  cordlang goto <Name> [entry]   Print definition path of a symbol\n");
  printf("  cordlang lsp                   Minimal Language Server (stdio JSON-RPC)\n");
  printf("  cordlang --version, -v, --v   Print CLI version\n");
  printf("  cordlang help                  Show this help\n\n");
  printf("Backends (see docs/BACKENDS.md):\n");
  printf("  Official:\n");
  printf("  preview                        Native ESM dev server (default for run)\n");
  printf("  esm                            ES modules + built-in runtime (no npm)\n");
  printf("  react                          React + Vite + Tailwind scaffold\n");
  printf("  svelte                         Svelte 5 + Vite + Tailwind + hash router\n");
  printf("  Candidate:\n");
  printf("  vue                            Vue 3 + Vite + Tailwind + vue-router\n");
  printf("  Experimental / meta:\n");
  printf("  html                           Legacy static single-document preview\n");
  printf("  solid                          SolidJS + Vite + Tailwind + @solidjs/router\n");
  printf("  email                          Static email-safe HTML (tables, inline CSS)\n");
  printf("  pdf                            Static HTML for external PDF conversion\n");
  printf("  next                           Next wrap of React emit (SPA; not full RSC)\n");
  printf("  sveltekit                      Kit wrap of Svelte emit (SPA; not full SSR)\n\n");
  printf("Flags (init):\n");
  printf("  --template <id>                Seed from templates/ (counter, landing, …)\n");
  printf("Flags (add):\n");
  printf("  --lib                          Install into src/lib/<name>/ (default: src/vendor/)\n\n");
  printf("Flags (run react|svelte|vue|solid|next|sveltekit):\n");
  printf("  --check                        After first scaffold: npm install if needed + build\n");
  printf("  --watch                        Poll .cord files; rebuild dist/<backend> on change\n");
  printf("                                 (combine: --watch --check runs check only once)\n");
  printf("Flags (run pdf):\n");
  printf("  --check                        Soft hint for weasyprint/npx (never fails if absent)\n\n");
  printf("Flags (compile):\n");
  printf("  --check                        Run semantic checker after parse; fail on errors\n");
  printf("  --backend <name>               Target backend (default: react)\n");
  printf("  -o <file>                      Write output to file\n");
  printf("  --sourcemap                    Write Source Map v3 stub (.map)\n");
  printf("  --pass <name>                  Apply in-tree IR pass (repeatable; opt-in)\n");
  printf("  --list-passes                  List IR passes and exit\n");
  printf("  --ast / --tokens / --ir        Debug parse / IR output\n\n");
  printf("Examples:\n");
  printf("  cordlang init my-app\n");
  printf("  cordlang init my-app --template counter\n");
  printf("  cordlang add ../pkgs/ui-kit\n");
  printf("  cordlang add counter --lib\n");
  printf("  cd my-app && cordlang run         # preview in browser\n");
  printf("  cd my-app && cordlang run react   # generate React app\n");
  printf("  cd my-app && cordlang run svelte  # generate Svelte app\n");
  printf("  cd my-app && cordlang run vue     # generate Vue app\n");
  printf("  cd my-app && cordlang run solid   # generate Solid app\n");
  printf("  cd my-app && cordlang run react --check\n");
  printf("  cd my-app && cordlang run react --watch\n");
  printf("  cd my-app && cordlang run svelte --watch --check\n");
  printf("  cordlang check                    # check project entry\n");
  printf("  cordlang check src/app.cord\n");
  printf("  cordlang analyze\n");
  printf("  cordlang ai check\n");
  printf("  cd my-app && cordlang symbols\n");
  printf("  cd my-app && cordlang goto Counter\n");
  printf("  cordlang compile src/app.cord --backend svelte\n");
  printf("  cordlang compile src/app.cord --backend vue\n");
  printf("  cordlang compile src/app.cord --backend solid\n");
  printf("  cordlang compile src/app.cord --backend email\n");
  printf("  cordlang compile src/app.cord --backend next\n");
  printf("  cordlang compile src/app.cord --backend react --sourcemap -o App.jsx\n");
  printf("  cordlang fmt src/\n");
  printf("  cordlang fmt --check src/\n");
  printf("  cordlang compile src/app.cord --check\n");
  printf("  cordlang compile src/app.cord --tokens\n");
  printf("  cordlang compile src/app.cord --ast\n");
  printf("  cordlang compile src/app.cord --ir\n");
}

/* Resolve entry .cord from a path (file, project dir, or "."). Caller frees. */
static char *resolve_check_entry(const char *path) {
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

static int cmd_check(int argc, char **argv) {
  int as_json = 0;
  const char *path = ".";
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "--json") == 0)
      as_json = 1;
    else if (argv[i][0] != '-')
      path = argv[i];
  }
  char *entry = resolve_check_entry(path);
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

static void print_tokens_from_file(const char *path) {
  size_t len = 0;
  char *source = fs_read_file(path, &len);
  if (!source) {
    fprintf(stderr, "Error: cannot read '%s'\n", path);
    return;
  }
  Lexer *lexer = lexer_create(source, len);
  lexer_tokenize(lexer);
  Token t;
  while ((t = lexer_next(lexer)).type != TOKEN_EOF) {
    printf("Token: ");
    switch (t.type) {
      case TOKEN_IDENTIFIER:
        printf("IDENTIFIER '%.*s'", (int)t.len, t.start);
        break;
      case TOKEN_STRING:
        printf("STRING '%.*s'", (int)t.len, t.start);
        break;
      case TOKEN_NUMBER:
        printf("NUMBER %.*s", (int)t.len, t.start);
        break;
      case TOKEN_AT:
        printf("@");
        break;
      case TOKEN_EQUALS:
        printf("=");
        break;
      case TOKEN_INDENT:
        printf("INDENT");
        break;
      case TOKEN_DEDENT:
        printf("DEDENT");
        break;
      case TOKEN_NEWLINE:
        printf("NEWLINE");
        break;
      default:
        printf("(%d)", t.type);
        break;
    }
    printf(" [line %d]\n", t.line);
  }
  lexer_destroy(lexer);
  free(source);
}

static void print_ast_node(Node *node, int depth) {
  for (int i = 0; i < depth; i++) printf("  ");
  switch (node->type) {
    case NODE_ROOT:
      printf("ROOT\n");
      break;
    case NODE_ELEMENT:
      printf("ELEMENT '%s'\n", node->value ? node->value : "");
      break;
    case NODE_TEXT:
      printf("TEXT '%s'\n", node->value ? node->value : "");
      break;
    case NODE_ATTR:
      printf("ATTR %s = %s\n", node->value ? node->value : "",
             node->value2 ? node->value2 : "");
      break;
    case NODE_EVENT:
      printf("EVENT @%s = %s\n", node->value ? node->value : "",
             node->value2 ? node->value2 : "");
      break;
    case NODE_BOOL_ATTR:
      printf("BOOL '%s'\n", node->value ? node->value : "");
      break;
    case NODE_FOR:
      printf("FOR %s in %s\n", node->value ? node->value : "",
             node->value2 ? node->value2 : "");
      break;
    case NODE_IF:
      printf("IF %s\n", node->value ? node->value : "");
      break;
    case NODE_COMPONENT_DEF:
      printf("COMPONENT def %s%s%s\n", node->value ? node->value : "?",
             node->value2 ? " " : "", node->value2 ? node->value2 : "");
      break;
    case NODE_PROPS_DECL:
      printf("PROPS\n");
      break;
    case NODE_STATE_DECL:
      printf("STATE %s%s%s\n", node->value ? node->value : "(block)",
             node->value2 ? " = " : "", node->value2 ? node->value2 : "");
      break;
    case NODE_COMPUTED_DECL:
      printf("COMPUTED %s = %s\n", node->value ? node->value : "?",
             node->value2 ? node->value2 : "?");
      break;
    case NODE_ROUTE:
      printf("ROUTE %s => %s", node->value ? node->value : "?",
             node->value2 ? node->value2 : "?");
      for (size_t ai = 0; ai < node->children_len; ai++) {
        Node *ch = node->children[ai];
        if (ch && ch->type == NODE_ATTR && ch->value &&
            strcmp(ch->value, "layout") == 0 && ch->value2)
          printf(" layout=%s", ch->value2);
      }
      printf("\n");
      break;
    case NODE_SLOT:
      printf("SLOT\n");
      break;
    case NODE_THEME:
      printf("THEME %s\n", node->value ? node->value : "?");
      break;
    case NODE_INTERPOLATION:
      if (node->value)
        printf("INTERP #{%s}\n", node->value);
      else
        printf("INTERP (template)\n");
      break;
    case NODE_USE:
      printf("USE %s%s%s\n", node->value ? node->value : "?",
             node->value2 ? " as " : "", node->value2 ? node->value2 : "");
      break;
    default:
      printf("NODE(%d)\n", node->type);
      break;
  }
  for (size_t i = 0; i < node->children_len; i++) {
    print_ast_node(node->children[i], depth + 1);
  }
}

static int cmd_compile(int argc, char **argv) {
  const char *file = NULL;
  const char *backend = "react";
  const char *out = NULL;
  int show_tokens = 0;
  int show_ast = 0;
  int show_ir = 0;
  int do_check = 0;
  int sourcemap = 0;
  const char *pass_buf[32];
  int n_passes = 0;

  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "--backend") == 0 && i + 1 < argc)
      backend = argv[++i];
    else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
      out = argv[++i];
    else if (strcmp(argv[i], "--tokens") == 0)
      show_tokens = 1;
    else if (strcmp(argv[i], "--ast") == 0)
      show_ast = 1;
    else if (strcmp(argv[i], "--ir") == 0)
      show_ir = 1;
    else if (strcmp(argv[i], "--check") == 0)
      do_check = 1;
    else if (strcmp(argv[i], "--sourcemap") == 0)
      sourcemap = 1;
    else if (strcmp(argv[i], "--list-passes") == 0) {
      ir_pass_list();
      return 0;
    } else if (strcmp(argv[i], "--pass") == 0 && i + 1 < argc) {
      if (n_passes < 32) pass_buf[n_passes++] = argv[++i];
      else
        ++i;
    } else if (argv[i][0] != '-')
      file = argv[i];
  }

  if (!file && !show_tokens) {
    /* allow --list-passes already returned */
    fprintf(stderr, "Error: missing .cord file\n");
    return 1;
  }

  if (show_tokens) {
    if (!file) {
      fprintf(stderr, "Error: missing .cord file\n");
      return 1;
    }
    print_tokens_from_file(file);
    return 0;
  }

  if (show_ast) {
    CompileResult r = compiler_parse_project(file);
    if (!r.ok || !r.ast) {
      fprintf(stderr, "Error: parse failed%s%s\n", r.error ? ": " : "",
              r.error ? r.error : "");
      return 1;
    }
    print_ast_node(r.ast->root, 0);
    compiler_result_free(&r);
    return 0;
  }

  if (show_ir) {
    CompileResult r = compiler_parse_project(file);
    if (!r.ok || !r.ast) {
      fprintf(stderr, "Error: parse failed%s%s\n", r.error ? ": " : "",
              r.error ? r.error : "");
      return 1;
    }
    IrProgram *ir = ir_from_ast(r.ast->root, file);
    if (!ir) {
      fprintf(stderr, "Error: IR conversion failed\n");
      compiler_result_free(&r);
      return 1;
    }
    if (n_passes > 0) {
      ir = ir_pass_apply(ir, pass_buf, n_passes);
      if (!ir) {
        compiler_result_free(&r);
        return 1;
      }
    }
    char *dump = ir_dump(ir);
    if (dump) {
      printf("%s", dump);
      free(dump);
    }
    ir_free(ir);
    compiler_result_free(&r);
    return 0;
  }

  if (do_check) {
    DiagList diags;
    diag_list_init(&diags);
    int crc = check_service_run(file, &diags);
    diag_print_all(&diags);
    int nerr = diag_error_count(&diags);
    diag_list_free(&diags);
    if (crc != 0 || nerr > 0) {
      fprintf(stderr, "compile --check: %d error(s), aborting\n", nerr);
      return 1;
    }
  }

  if (out)
    return compile_service_to_file_with_passes(file, backend, out, sourcemap,
                                               pass_buf, n_passes);

  char *code = compile_service_file_with_passes(file, backend, sourcemap, NULL,
                                                pass_buf, n_passes);
  if (!code) return 1;
  printf("%s", code);
  free(code);
  return 0;
}

static int cmd_fmt(int argc, char **argv) {
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

static int cmd_analyze(int argc, char **argv) {
  int as_json = 0;
  const char *path = ".";
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "--json") == 0)
      as_json = 1;
    else if (argv[i][0] != '-')
      path = argv[i];
  }
  char *entry = resolve_check_entry(path);
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
  char *entry = resolve_check_entry(path);
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

static int cmd_ai(int argc, char **argv) {
  if (argc > 0 && strcmp(argv[0], "check") == 0)
    return cmd_check(argc - 1, argv + 1);
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

static int cmd_build(int argc, char **argv) {
  const char *backend = argc > 0 ? argv[0] : "react";
  const char *dir = ".";

  if (strcmp(backend, "esm") == 0)
    return preview_service_build_esm(dir);

  char *entry_cfg = fs_join(dir, "cordlang.json");
  if (!fs_exists(entry_cfg)) {
    fprintf(stderr, "Error: not a Cordlang project. Run: cordlang init\n");
    free(entry_cfg);
    return 1;
  }
  free(entry_cfg);

  char *out_dir = fs_join(dir, "dist");
  char *out_file = fs_join(out_dir, strcmp(backend, "react") == 0 ? "App.jsx" : "out.txt");
  free(out_dir);

  int rc = compile_service_to_file("src/app.cord", backend, out_file);
  free(out_file);
  return rc;
}

int cli_run(int argc, char **argv) {
  term_init();
  backend_register_all();

  if (argc < 2) {
    print_usage();
    return 1;
  }

  const char *cmd = argv[1];

  if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 ||
      strcmp(cmd, "-h") == 0) {
    print_usage();
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
    return cmd_build(argc - 2, argv + 2);
  }

  if (strcmp(cmd, "check") == 0) {
    return cmd_check(argc - 2, argv + 2);
  }

  if (strcmp(cmd, "analyze") == 0) {
    return cmd_analyze(argc - 2, argv + 2);
  }

  if (strcmp(cmd, "ai") == 0) {
    return cmd_ai(argc - 2, argv + 2);
  }

  if (strcmp(cmd, "compile") == 0) {
    return cmd_compile(argc - 2, argv + 2);
  }

  if (strcmp(cmd, "fmt") == 0) {
    return cmd_fmt(argc - 2, argv + 2);
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
    return cmd_compile(1, fake_argv);
  }

  fprintf(stderr, "Unknown command: %s\n\n", cmd);
  print_usage();
  return 1;
}
