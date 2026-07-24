#ifndef CORDLANG_JSON_MINI_H
#define CORDLANG_JSON_MINI_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Copy string value for "key" from a JSON object into out.
 * Handles whitespace around ':' and simple \" escapes.
 * Returns 1 on success, 0 if missing/invalid. */
int json_object_copy_string(const char *json, const char *key, char *out,
                            size_t out_sz);

/* malloc'd copy of the string value, or NULL. Caller frees. */
char *json_object_get_string(const char *json, const char *key);

#ifdef __cplusplus
}
#endif

#endif
