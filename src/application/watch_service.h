#ifndef CORDLANG_WATCH_SERVICE_H
#define CORDLANG_WATCH_SERVICE_H

/* Callback invoked when watched sources change. Return value is ignored
 * (watch keeps running so a failed rebuild does not exit the loop). */
typedef int (*WatchRebuildFn)(void *userdata);

/* Poll project_dir/src for .cord files (recursive) and cordlang.json
 * every ~500 ms. Blocks until Ctrl+C. Returns 0 on clean exit, non-zero
 * on setup failure. Does not perform the initial build - caller scaffolds
 * first. */
int watch_service_run(const char *project_dir, WatchRebuildFn rebuild,
                      void *userdata);

#endif
