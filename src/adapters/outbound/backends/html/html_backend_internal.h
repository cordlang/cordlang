#ifndef CORDLANG_HTML_BACKEND_INTERNAL_H
#define CORDLANG_HTML_BACKEND_INTERNAL_H
#include "adapters/outbound/backends/html/html_backend.h"
#include "adapters/outbound/backends/ir_walk.h"
#include "adapters/outbound/backends/theme_css.h"
#include "application/ports/fs_port.h"
#include "domain/interp.h"
#include "domain/ir.h"
#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#include <strings.h>
#endif
typedef struct { char *buf; size_t len; size_t cap; } StrBuf;
typedef struct { const char *style_key; const char *tw_prefix; int is_value_appended; } StyleMap;
extern const char *htmlb_runtime_css;
extern const char *htmlb_runtime_js_core;

void htmlb_sb_init(StrBuf *sb);
void htmlb_sb_oom(void);
void htmlb_sb_append(StrBuf *sb, const char *s);
void htmlb_sb_appendf(StrBuf *sb, const char *fmt, ...);
void htmlb_sb_indent(StrBuf *sb, int depth);
void htmlb_html_escape_append(StrBuf *sb, const char *s);
void htmlb_gen_style_classes(StrBuf *sb, Node *style_map);
const char *htmlb_tag_to_div_plus_class(const char *tag);
int htmlb_html_tag_is_forbidden(const char *tag);
int htmlb_html_tag_is_safe_name(const char *tag);
const char *htmlb_html_tag_for(const char *tag);
void htmlb_state_reset(void);
int htmlb_state_has(const char *name);
void htmlb_state_add(const char *name, const char *init);
void htmlb_collect_state_from_node(Node *node);
int htmlb_is_simple_state_expr(const char *expr);
const char *htmlb_state_init_for(const char *name);
void htmlb_make_setter_name(char *out, size_t out_sz, const char *name);
int htmlb_is_safe_js_ident(const char *s);
int htmlb_is_safe_js_init(const char *s);
int htmlb_href_is_safe(const char *href);
void htmlb_emit_state_runtime_js(StrBuf *sb);
void htmlb_emit_bound_interp(StrBuf *sb, const char *expr);
void htmlb_emit_text_with_live_interp(StrBuf *sb, const char *str);
int htmlb_is_decl_only(NodeType t);
void htmlb_collect_classes(char *classes, size_t classes_sz, Node *node, const char *base_class);
int htmlb_is_style_attr(const char *name);
void htmlb_gen_element(StrBuf *sb, Node *node, int depth);
void htmlb_gen_children(StrBuf *sb, Node *node, int depth);
void htmlb_gen_node(StrBuf *sb, Node *node, int depth);
char *htmlb_html_generate_impl(Node *root);
int htmlb_ir_attr_is_true(const char *v);
void htmlb_collect_state_from_ir(IrNode *node);
int htmlb_is_ir_decl_only(const IrNode *n);
void htmlb_gen_style_classes_ir(StrBuf *sb, IrNode *style_map);
void htmlb_collect_classes_ir(char *classes, size_t classes_sz, IrNode *node,
                               const char *base_class);
void htmlb_gen_ir_element(StrBuf *sb, IrNode *node, int depth);
void htmlb_gen_ir_children(StrBuf *sb, IrNode *node, int depth);
void htmlb_gen_ir_node(StrBuf *sb, IrNode *node, int depth);
char *htmlb_html_generate_from_ir_root(IrNode *root);
int htmlb_write_path(const char *dir, const char *rel, const char *content);
#endif
