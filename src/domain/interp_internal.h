#ifndef CORDLANG_INTERP_INTERNAL_H
#define CORDLANG_INTERP_INTERNAL_H

#include "domain/interp.h"

char *interp_trim_dup(const char *start, size_t len);
char *interp_substr_dup(const char *start, size_t len);
const char *interp_find_end(const char *p);

#endif
