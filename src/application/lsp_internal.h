#ifndef CORDLANG_LSP_INTERNAL_H
#define CORDLANG_LSP_INTERNAL_H

#include "application/symbols_service.h"
#include "application/check_service.h"
#include "application/fmt_service.h"
#include "application/ports/compiler_port.h"
#include "application/ports/fs_port.h"
#include "adapters/outbound/json/json_mini.h"
#include "domain/ast.h"
#include "domain/diag.h"
#include "domain/known_attrs.h"
#include "domain/version.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#define MAX_DOCS 64
#define MAX_MSG (4 * 1024 * 1024)

typedef struct {
  char *uri;
  char *path;
  char *text;
} LspDoc;

extern LspDoc g_docs[MAX_DOCS];
extern int g_ndocs;
extern int g_shutdown;

char *uri_to_path(const char *uri);
void path_to_uri(const char *path, char *out, size_t out_sz);
LspDoc *doc_find(const char *uri);
LspDoc *doc_upsert(const char *uri, const char *text);
void docs_free(void);
void doc_ensure_text(LspDoc *d);
char *json_escape_dup(const char *s);
void lsp_send(const char *body);
int json_get_str(const char *json, const char *key, char *out, size_t out_sz);
char *json_get_str_dup(const char *json, const char *key);
int json_get_int(const char *json, const char *key, int *out);
int json_get_bool(const char *json, const char *key, int *out);
const char *json_method(const char *msg);
int json_id(const char *msg, char *out, size_t out_sz);
void respond_err(const char *id, int code, const char *message);
void respond_ok(const char *id, const char *result_json);
void notify(const char *method, const char *params_json);
void publish_diagnostics(const char *uri, LspDoc *d);
void handle_document_symbol(const char *id, const char *msg);
void ident_at(const char *text, int line, int character, char *out,
              size_t out_sz);
void handle_definition(const char *id, const char *msg);
const char *lsp_overlay(const char *abs, void *ud);
int lsp_extra_paths(const char **out, int maxn);
int loc_cmp(const void *a, const void *b);
void abs_uri(const char *path, char *out, size_t n);
int parse_pos_uri(const char *msg, char *uri, size_t uri_sz, int *line,
                  int *character);
int ident_range(const char *text, int line, int character, char *name,
                size_t name_sz, int *start_ch, int *end_ch);
int cursor_component(const char *id, const char *msg, char *name,
                     size_t name_sz, LspDoc **out_doc, int err_if_none);
void handle_references(const char *id, const char *msg);
void handle_prepare_rename(const char *id, const char *msg);
char *renamed_file_path(const char *old_path, const char *new_name);
int basename_stem_eq(const char *path, const char *name);
void handle_rename(const char *id, const char *msg);
void handle_completion(const char *id, const char *msg);
void handle_formatting(const char *id, const char *msg);
void handle_hover(const char *id, const char *msg);
void handle_code_action(const char *id, const char *msg);
void handle_message(const char *msg);

#endif
