#ifndef CORDLANG_FS_PORT_H
#define CORDLANG_FS_PORT_H

#include <stddef.h>

/* Outbound port: filesystem operations */
char *fs_read_file(const char *path, size_t *out_len);
int fs_write_file(const char *path, const char *content);
int fs_mkdir_p(const char *path);
int fs_exists(const char *path);
int fs_is_dir(const char *path);
char *fs_join(const char *a, const char *b);
char *fs_cwd(void);
/* Directory part of path (malloc). "." if no separator. */
char *fs_dirname(const char *path);
/* Final path segment without directory (malloc). */
char *fs_basename(const char *path);
/* Normalize separators to / and collapse ./ (malloc). */
char *fs_norm_path(const char *path);
/* Copy a single file (creates parent dirs). 0 = ok. */
int fs_copy_file(const char *src, const char *dst);
/* Recursively copy directory tree (creates dst). 0 = ok. */
int fs_copy_tree(const char *src, const char *dst);

/*
 * Walk `root` for *.cord files (skips node_modules, .git, dist).
 * Invokes cb(abs_path, project_rel_with_slash, userdata) for each.
 */
void fs_walk_cord(const char *root,
                  void (*cb)(const char *abs, const char *rel, void *ud),
                  void *ud);

#endif
