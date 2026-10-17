#ifndef CORDLANG_FS_INTERNAL_H
#define CORDLANG_FS_INTERNAL_H

#include "application/ports/fs_port.h"

int fs_memory_is_mounted(void);
char *fs_memory_current_cwd(void);
char *fs_memory_norm_path(const char *path);
char *fs_memory_read_copy(const char *path, size_t *out_len);
int fs_memory_path_exists(const char *path);
int fs_memory_path_is_dir(const char *path);

#endif
