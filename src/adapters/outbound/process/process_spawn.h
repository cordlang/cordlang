#ifndef CORDLANG_PROCESS_SPAWN_H
#define CORDLANG_PROCESS_SPAWN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Run argv (NULL-terminated) without a shell.
 * cwd may be NULL (inherit). If wait_child is non-zero, wait and return the
 * process exit status (0 = success). If wait_child is 0, detach and return 0
 * when spawn succeeds.
 * Returns -1 if the process could not be started. */
int process_run(const char *cwd, char *const argv[], int wait_child);

#ifdef __cplusplus
}
#endif

#endif /* CORDLANG_PROCESS_SPAWN_H */
