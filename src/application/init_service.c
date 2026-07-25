#include "application/init_service.h"
#include "application/ports/fs_port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int write_rel(const char *root, const char *rel, const char *content) {
  char *path = fs_join(root, rel);
  if (!path) return -1;
  int rc = fs_write_file(path, content);
  free(path);
  return rc;
}

/* Walk up from start looking for relpath that exists. Caller frees. */
static char *find_up(const char *start, const char *relpath) {
  char *cur = fs_norm_path(start);
  if (!cur) return NULL;
  for (int depth = 0; depth < 12; depth++) {
    char *cand = fs_join(cur, relpath);
    if (cand && fs_exists(cand)) {
      free(cur);
      return cand;
    }
    free(cand);
    char *parent = fs_dirname(cur);
    if (!parent) break;
    if (strcmp(parent, cur) == 0) {
      free(parent);
      break;
    }
    free(cur);
    cur = parent;
  }
  free(cur);
  return NULL;
}

static char *resolve_template_dir(const char *template_name) {
  if (!template_name || !*template_name) return NULL;
  char *cwd = fs_cwd();
  if (!cwd) return NULL;

  char rel[512];
  snprintf(rel, sizeof(rel), "templates/%s", template_name);
  char *found = find_up(cwd, rel);
  free(cwd);
  return found;
}

static int init_from_template(const char *root, const char *template_name,
                              int in_place) {
  char *src = resolve_template_dir(template_name);
  if (!src) {
    fprintf(stderr, "Error: unknown template '%s'\n", template_name);
    fprintf(stderr,
            "Available (bundled): counter, landing, dashboard, form-fetch, "
            "docs-shell\n");
    fprintf(stderr,
            "Hint: run from a Cordlang checkout so templates/ is visible, "
            "or set CWD above templates/.\n");
    return 1;
  }

  if (!fs_is_dir(src)) {
    fprintf(stderr, "Error: template path is not a directory: %s\n", src);
    free(src);
    return 1;
  }

  /* Copy tree into root. For in-place, copy children; for new dir, copy into it. */
  if (fs_copy_tree(src, root) != 0) {
    fprintf(stderr, "Error: failed to copy template '%s' → '%s'\n",
            template_name, root);
    free(src);
    return 1;
  }

  printf("Created Cordlang project from template '%s'%s%s\n", template_name,
         in_place ? "" : " in ", in_place ? "" : root);
  printf("\n");
  printf("  Template source: %s\n", src);
  printf("\n");
  printf("Next:\n");
  if (!in_place) printf("  cd %s\n", root);
  printf("  cordlang check\n");
  printf("  cordlang run          # preview\n");
  printf("  cordlang run react    # export React\n");
  free(src);
  return 0;
}

static int init_default(const char *root, int in_place) {
  char *cfg_path = fs_join(root, "cordlang.json");
  if (!cfg_path) return 1;
  if (fs_exists(cfg_path)) {
    fprintf(stderr, "Error: already a Cordlang project (%s)\n", cfg_path);
    free(cfg_path);
    return 1;
  }
  free(cfg_path);

  const char *config =
      "{\n"
      "  \"name\": \"cordlang-app\",\n"
      "  \"version\": \"0.1.0\",\n"
      "  \"entry\": \"src/app.cord\",\n"
      "  \"defaultBackend\": \"react\",\n"
      "  \"outDir\": \"dist\"\n"
      "}\n";

  const char *app_cord =
      "# Entry: layout + routes (pages/components are separate files)\n"
      "\n"
      "use layouts/default\n"
      "\n"
      "route / => pages/HomePage\n"
      "route /about => pages/AboutPage\n";

  const char *layout_cord =
      "layout default\n"
      "  header sticky bg=white shadow=sm\n"
      "    row between center p=16\n"
      "      h1 \"Cordlang App\" size=xl bold color=primary\n"
      "      nav\n"
      "        row gap=16 center\n"
      "          link \"Home\" to=/\n"
      "          link \"About\" to=/about\n"
      "  main p=24\n"
      "    slot\n"
      "  footer muted center p=16\n"
      "    p \"Built with Cordlang\"\n";

  const char *home_cord =
      "col gap=16 max-w=640\n"
      "  h1 \"Welcome\" size=2xl bold\n"
      "  p \"Edit pages and components — app.cord only wires routes.\" muted\n"
      "  row gap=8\n"
      "    link \"About\" to=/about\n"
      "    btn \"Primary\" variant=primary\n";

  const char *about_cord =
      "col gap=16 max-w=640\n"
      "  h1 \"About\" size=2xl bold\n"
      "  p \"This page lives in src/pages/AboutPage.cord\" muted\n"
      "  link \"Back home\" to=/\n";

  const char *readme =
      "# Cordlang project\n"
      "\n"
      "## Structure\n"
      "\n"
      "```\n"
      "src/\n"
      "  app.cord              # layout + routes only\n"
      "  layouts/default.cord\n"
      "  pages/\n"
      "  components/\n"
      "public/                 # static assets (copied to dist/*/public)\n"
      "```\n"
      "\n"
      "Static files in `public/` are served at the site root after export:\n"
      "`public/logo.png` → `/logo.png` in React/Svelte (Vite `public/`).\n"
      "\n"
      "```cord\n"
      "use layouts/default\n"
      "route / => pages/HomePage\n"
      "\n"
      "# in a page:\n"
      "use ../components/Button\n"
      "img src=\"/logo.png\" alt=\"Logo\"\n"
      "```\n"
      "\n"
      "## Commands\n"
      "\n"
      "```bash\n"
      "cordlang run              # native preview\n"
      "cordlang run react        # export React (pages/components/layouts)\n"
      "```\n";

  const char *public_readme =
      "# Static assets\n"
      "\n"
      "Files here are copied into `dist/react/public` and `dist/svelte/public`\n"
      "on `cordlang run react|svelte`, and served at the site root by Vite.\n"
      "\n"
      "Examples:\n"
      "- `public/logo.png` → `/logo.png`\n"
      "- `public/api/products.json` → `/api/products.json`\n";

  const char *gitignore =
      "dist/\n"
      "node_modules/\n"
      ".DS_Store\n"
      "Thumbs.db\n";

  int rc = 0;
  rc |= write_rel(root, "cordlang.json", config);
  rc |= write_rel(root, "src/app.cord", app_cord);
  rc |= write_rel(root, "src/layouts/default.cord", layout_cord);
  rc |= write_rel(root, "src/pages/HomePage.cord", home_cord);
  rc |= write_rel(root, "src/pages/AboutPage.cord", about_cord);
  rc |= write_rel(root, "README.md", readme);
  rc |= write_rel(root, ".gitignore", gitignore);
  rc |= write_rel(root, "public/README.md", public_readme);

  char *comp = fs_join(root, "src/components");
  if (comp) {
    rc |= fs_mkdir_p(comp);
    free(comp);
  }
  char *pub = fs_join(root, "public");
  if (pub) {
    rc |= fs_mkdir_p(pub);
    free(pub);
  }

  if (rc != 0) {
    fprintf(stderr, "Error: failed to create project files\n");
    return 1;
  }

  printf("Created Cordlang project%s%s\n", in_place ? "" : " in ",
         in_place ? "" : root);
  printf("\n");
  printf("  %s/\n", in_place ? "." : root);
  printf("  +-- cordlang.json\n");
  printf("  +-- public/              # static assets → /file in dist\n");
  printf("  +-- src/\n");
  printf("  |   +-- app.cord           # routes only\n");
  printf("  |   +-- layouts/\n");
  printf("  |   +-- pages/\n");
  printf("  |   \\-- components/\n");
  printf("  \\-- README.md\n");
  printf("\n");
  printf("Next:\n");
  if (!in_place) printf("  cd %s\n", root);
  printf("  cordlang run          # preview\n");
  printf("  cordlang run react    # export React\n");
  return 0;
}

int init_service_run(const char *project_name, const char *template_name) {
  const char *name = project_name && *project_name ? project_name : ".";
  int in_place = strcmp(name, ".") == 0;

  if (!in_place) {
    if (fs_exists(name)) {
      fprintf(stderr, "Error: '%s' already exists\n", name);
      return 1;
    }
    if (fs_mkdir_p(name) != 0) {
      fprintf(stderr, "Error: cannot create '%s'\n", name);
      return 1;
    }
  }

  const char *root = name;

  if (template_name && *template_name) {
    char *cfg_path = fs_join(root, "cordlang.json");
    if (cfg_path && fs_exists(cfg_path)) {
      fprintf(stderr, "Error: already a Cordlang project (%s)\n", cfg_path);
      free(cfg_path);
      return 1;
    }
    free(cfg_path);
    return init_from_template(root, template_name, in_place);
  }

  return init_default(root, in_place);
}
