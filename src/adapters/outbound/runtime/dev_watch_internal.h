#ifndef CORDLANG_DEV_WATCH_INTERNAL_H
#define CORDLANG_DEV_WATCH_INTERNAL_H

#include "adapters/outbound/runtime/dev_server.h"
#include <time.h>

#define MAX_WATCH_FILES 512

typedef struct {
  unsigned long long hash;
  int files;
} WatchStamp;

typedef struct {
  char rel[512];
  unsigned long long sig;
} WatchFile;

typedef struct {
  WatchStamp stamp;
  WatchFile files[MAX_WATCH_FILES];
  int n_files;
} WatchSnapshot;

WatchSnapshot dev_watch_take_snapshot(const char *root);
int dev_watch_stamp_equal(const WatchStamp *a, const WatchStamp *b);
int dev_watch_classify_change(const WatchSnapshot *prev,
                              const WatchSnapshot *now,
                              const char *entry_rel, char *out_url,
                              size_t out_n);

#endif