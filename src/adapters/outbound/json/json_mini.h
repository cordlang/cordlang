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

/* Parse "key": ["a","b",...] — writes into names[i] (each row elem_sz bytes).
 * Returns count (>=0), or -1 on hard error. */
int json_object_copy_string_array(const char *json, const char *key, char *names,
                                  size_t elem_sz, int max_names);

/* Nested object: "parentKey": { "key": ["a","b"] }. */
int json_object_nested_string_array(const char *json, const char *parent,
                                    const char *key, char *names, size_t elem_sz,
                                    int max_names);

#ifdef __cplusplus
}
#endif

#endif
