#ifndef CORDLANG_STATIC_HTML_INTERNAL_H
#define CORDLANG_STATIC_HTML_INTERNAL_H
#include "adapters/outbound/backends/static_html/static_html.h"
#include "domain/interp.h"
#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif
typedef struct { char *buf; size_t len; size_t cap; } StrBuf;
typedef enum { LAY_NONE, LAY_COL, LAY_ROW } LayKind;

void statich_sb_oom(void);
void statich_sb_init(StrBuf *sb);
void statich_sb_append(StrBuf *sb, const char *s);
void statich_sb_appendf(StrBuf *sb, const char *fmt, ...);
void statich_sb_indent(StrBuf *sb, int depth);
void statich_html_escape_append(StrBuf *sb, const char *s);
void statich_bind_reset(void);
const char *statich_bind_get(const char *name);
void statich_bind_add(const char *name, const char *val);
void statich_strip_quotes(char *s);
void statich_collect_binds(IrNode *node);
int statich_ir_attr_is_true(const char *v);
int statich_is_decl_only(const IrNode *n);
int statich_tag_forbidden(const char *tag);
int statich_tag_safe_name(const char *tag);
LayKind statich_lay_kind(const char *tag);
const char *statich_html_tag_for(const char *tag);
int statich_px_from_attr(const char *v, int fallback);
void statich_collect_inline_style(StrBuf *style, IrNode *node, LayKind lay);
int statich_attr_gap(IrNode *node);
void statich_emit_snapshot_text(StrBuf *sb, const char *expr_or_text);
void statich_emit_html_attrs(StrBuf *sb, IrNode *node, const char *tag);
void statich_gen_layout_table(StrBuf *sb, IrNode *node, int depth, LayKind lay);
void statich_gen_element(StrBuf *sb, IrNode *node, int depth);
void statich_gen_children(StrBuf *sb, IrNode *node, int depth);
void statich_gen_node(StrBuf *sb, IrNode *node, int depth);
#endif
