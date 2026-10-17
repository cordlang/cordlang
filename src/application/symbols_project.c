#include "application/symbols_internal.h"

static char *project_root_of(const char *anchor_path) {
  if (!anchor_path || !*anchor_path) return fs_cwd();
  char *root = compiler_project_root(anchor_path);
  if (root) return root;
  return fs_dirname(anchor_path);
}

static void gather_cord_files(const char *anchor_path, const char **extra,
                              int n_extra, PathList *pl) {
  char *root = project_root_of(anchor_path);
  if (root) {
    fs_walk_cord(root, walk_add_path, pl);
    free(root);
  }
  if (anchor_path) path_list_add(pl, anchor_path);
  for (int i = 0; i < n_extra; i++) {
    if (extra[i]) path_list_add(pl, extra[i]);
  }
}

static void scan_project(const char *anchor_path, const char *name,
                         int include_decl, SymbolsOverlayFn overlay, void *ud,
                         const char **extra_paths, int n_extra, PathList *names,
                         SymbolLocList *refs) {
  PathList files = {0};
  gather_cord_files(anchor_path, extra_paths, n_extra, &files);
  for (size_t i = 0; i < files.n; i++) {
    int owned = 0;
    char *text = file_text(files.paths[i], overlay, ud, &owned);
    if (!text) continue;
    parse_file_into(files.paths[i], text, name, include_decl, names != NULL,
                    names, refs);
    if (owned) free(text);
  }
  path_list_free(&files);
}

int symbols_name_is_component(const char *anchor_path, const char *name,
                              SymbolsOverlayFn overlay, void *ud) {
  if (!name || !*name) return 0;
  PathList names = {0};
  scan_project(anchor_path, NULL, 1, overlay, ud, NULL, 0, &names, NULL);
  int ok = 0;
  for (size_t i = 0; i < names.n; i++) {
    if (strcmp(names.paths[i], name) == 0) {
      ok = 1;
      break;
    }
  }
  path_list_free(&names);
  return ok;
}

int symbols_collect_refs(const char *anchor_path, const char *name,
                         int include_decl, SymbolsOverlayFn overlay, void *ud,
                         const char **extra_paths, int n_extra,
                         SymbolLocList *out) {
  if (!out) return 1;
  symbol_loc_list_init(out);
  if (!name || !*name) return 1;
  scan_project(anchor_path, name, include_decl, overlay, ud, extra_paths,
               n_extra, NULL, out);
  return 0;
}

int symbols_rename_check(const char *anchor_path, const char *old_name,
                         const char *new_name, SymbolsOverlayFn overlay,
                         void *ud, char *msg_buf, size_t msg_sz) {
  if (msg_buf && msg_sz) msg_buf[0] = '\0';
  if (!new_name || !symbols_valid_component_name(new_name)) {
    if (msg_buf && msg_sz)
      snprintf(msg_buf, msg_sz, "invalid component name '%s'",
               new_name ? new_name : "");
    return 1;
  }
  if (old_name && strcmp(old_name, new_name) == 0) return 0;
  PathList names = {0};
  scan_project(anchor_path, NULL, 1, overlay, ud, NULL, 0, &names, NULL);
  int collide = 0;
  for (size_t i = 0; i < names.n; i++) {
    if (strcmp(names.paths[i], new_name) == 0) {
      collide = 1;
      break;
    }
  }
  path_list_free(&names);
  if (collide) {
    if (msg_buf && msg_sz)
      snprintf(msg_buf, msg_sz, "component '%s' already exists", new_name);
    return 2;
  }
  return 0;
}
