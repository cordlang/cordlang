#include "application/symbols_service.h"
#include "application/ports/compiler_port.h"
#include "application/ports/fs_port.h"
#include "adapters/outbound/json/json_mini.h"
#include "domain/ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_entry_from_config(const char *project_dir) {
  char *cfg = fs_join(project_dir, "cordlang.json");
  if (!cfg) return NULL;

  size_t len = 0;
  char *json = fs_read_file(cfg, &len);
  free(cfg);
  if (!json) return NULL;

  char *entry = json_object_get_string(json, "entry");
  free(json);
  if (!entry) entry = strdup("src/app.cord");
  return entry;
}

/* Prefer paths relative to project (e.g. src/components/Counter.cord). */
static char *display_path(const char *file, const char *project_dir) {
  if (!file) return strdup("?");
  char *norm = fs_norm_path(file);
  const char *p = norm ? norm : file;

  /* Strip project_dir prefix if present */
  if (project_dir && *project_dir) {
    char *pnorm = fs_norm_path(project_dir);
    if (pnorm) {
      size_t pl = strlen(pnorm);
      if (pl > 0 && strncmp(p, pnorm, pl) == 0) {
        const char *rest = p + pl;
        while (*rest == '/' || *rest == '\\') rest++;
        if (*rest) {
          char *out = strdup(rest);
          free(pnorm);
          free(norm);
          return out;
        }
      }
      free(pnorm);
    }
  }

  /* If path contains src/, show from there */
  const char *src = strstr(p, "src/");
  if (!src) {
#ifdef _WIN32
    src = strstr(p, "src\\");
#endif
  }
  if (src) {
    char *out = strdup(src);
    free(norm);
    /* normalize backslashes */
    for (char *c = out; *c; c++)
      if (*c == '\\') *c = '/';
    return out;
  }

  char *out = strdup(p);
  free(norm);
  if (out) {
    for (char *c = out; *c; c++)
      if (*c == '\\') *c = '/';
  }
  return out ? out : strdup(file);
}

static const char *component_file(Node *def) {
  if (!def) return NULL;
  for (size_t i = 0; i < def->children_len; i++) {
    Node *c = def->children[i];
    if (c && c->type == NODE_ATTR && c->value &&
        strcmp(c->value, "__file__") == 0 && c->value2)
      return c->value2;
  }
  return NULL;
}

static int is_layout_def(Node *def) {
  return def && def->type == NODE_COMPONENT_DEF && def->value2 &&
         strcmp(def->value2, "__layout__") == 0;
}

static int resolve_entry(const char *project_dir, const char *entry_path,
                         char **out_entry) {
  const char *dir = project_dir && *project_dir ? project_dir : ".";
  if (entry_path && *entry_path) {
    if (fs_exists(entry_path)) {
      *out_entry = strdup(entry_path);
      return 0;
    }
    char *joined = fs_join(dir, entry_path);
    if (joined && fs_exists(joined)) {
      *out_entry = joined;
      return 0;
    }
    free(joined);
    fprintf(stderr, "Error: entry not found: %s\n", entry_path);
    return 1;
  }

  char *cfg = fs_join(dir, "cordlang.json");
  if (cfg && fs_exists(cfg)) {
    free(cfg);
    char *rel = read_entry_from_config(dir);
    char *joined = fs_join(dir, rel ? rel : "src/app.cord");
    free(rel);
    if (joined && fs_exists(joined)) {
      *out_entry = joined;
      return 0;
    }
    free(joined);
  } else {
    free(cfg);
  }

  /* bare .cord file as project dir? */
  if (entry_path && strstr(entry_path, ".cord")) {
    fprintf(stderr, "Error: entry not found: %s\n", entry_path);
    return 1;
  }

  fprintf(stderr, "Error: not a Cordlang project (missing cordlang.json)\n");
  fprintf(stderr, "Usage: cordlang symbols [entry.cord]\n");
  return 1;
}

int symbols_service_list(const char *project_dir, const char *entry_path) {
  char *entry = NULL;
  if (resolve_entry(project_dir, entry_path, &entry) != 0) return 1;

  CompileResult r = compiler_parse_project(entry);
  if (!r.ok || !r.ast || !r.ast->root) {
    fprintf(stderr, "Error: parse failed%s%s\n", r.error ? ": " : "",
            r.error ? r.error : "");
    free(entry);
    compiler_result_free(&r);
    return 1;
  }

  const char *dir = project_dir && *project_dir ? project_dir : ".";
  Node *root = r.ast->root;

  /* Components (non-layout) */
  for (size_t i = 0; i < root->children_len; i++) {
    Node *c = root->children[i];
    if (!c || c->type != NODE_COMPONENT_DEF || !c->value) continue;
    if (is_layout_def(c)) continue;
    const char *file = component_file(c);
    char *disp = display_path(file ? file : entry, dir);
    int line = c->line > 0 ? c->line : 1;
    printf("COMPONENT %s  %s:%d\n", c->value, disp, line);
    free(disp);
  }

  /* Routes */
  for (size_t i = 0; i < root->children_len; i++) {
    Node *c = root->children[i];
    if (!c || c->type != NODE_ROUTE) continue;
    const char *path = c->value ? c->value : "?";
    const char *target = c->value2 ? c->value2 : "?";
    printf("ROUTE %s => %s\n", path, target);
  }

  /* Layouts */
  for (size_t i = 0; i < root->children_len; i++) {
    Node *c = root->children[i];
    if (!c || c->type != NODE_COMPONENT_DEF || !c->value) continue;
    if (!is_layout_def(c)) continue;
    printf("LAYOUT %s\n", c->value);
  }

  compiler_result_free(&r);
  free(entry);
  return 0;
}

int symbols_service_goto(const char *project_dir, const char *name,
                         const char *entry_path) {
  if (!name || !*name) {
    fprintf(stderr, "Error: missing symbol name\n");
    fprintf(stderr, "Usage: cordlang goto <ComponentName>\n");
    return 1;
  }

  char *entry = NULL;
  if (resolve_entry(project_dir, entry_path, &entry) != 0) return 1;

  CompileResult r = compiler_parse_project(entry);
  if (!r.ok || !r.ast || !r.ast->root) {
    fprintf(stderr, "Error: parse failed%s%s\n", r.error ? ": " : "",
            r.error ? r.error : "");
    free(entry);
    compiler_result_free(&r);
    return 1;
  }

  const char *dir = project_dir && *project_dir ? project_dir : ".";
  Node *root = r.ast->root;
  int found = 0;

  for (size_t i = 0; i < root->children_len; i++) {
    Node *c = root->children[i];
    if (!c || c->type != NODE_COMPONENT_DEF || !c->value) continue;
    if (strcmp(c->value, name) != 0) continue;

    const char *file = component_file(c);
    char *disp = display_path(file ? file : entry, dir);
    int line = c->line > 0 ? c->line : 1;
    printf("%s:%d\n", disp, line);
    free(disp);
    found = 1;
    break;
  }

  /* Also match route target names that might only exist as rewritten exports */
  if (!found) {
    for (size_t i = 0; i < root->children_len; i++) {
      Node *c = root->children[i];
      if (!c || c->type != NODE_ROUTE || !c->value2) continue;
      if (strcmp(c->value2, name) != 0) continue;
      /* Find matching component (should have been loaded) */
      for (size_t j = 0; j < root->children_len; j++) {
        Node *d = root->children[j];
        if (d && d->type == NODE_COMPONENT_DEF && d->value &&
            strcmp(d->value, name) == 0) {
          const char *file = component_file(d);
          char *disp = display_path(file ? file : entry, dir);
          int line = d->line > 0 ? d->line : 1;
          printf("%s:%d\n", disp, line);
          free(disp);
          found = 1;
          break;
        }
      }
      if (found) break;
    }
  }

  compiler_result_free(&r);
  free(entry);

  if (!found) {
    fprintf(stderr, "Error: symbol '%s' not found\n", name);
    return 1;
  }
  return 0;
}
