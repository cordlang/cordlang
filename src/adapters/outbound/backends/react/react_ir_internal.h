#ifndef CORDLANG_REACT_IR_INTERNAL_H
#define CORDLANG_REACT_IR_INTERNAL_H
#include "adapters/outbound/backends/react/react_backend.h"
#include "adapters/outbound/backends/cord_class.h"
#include "adapters/outbound/backends/source_attr.h"
#include "adapters/outbound/backends/theme_css.h"
#include "adapters/outbound/backends/ir_walk.h"
#include "adapters/outbound/html_escape.h"
#include "domain/interp.h"
#include "domain/ir.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



typedef struct {
  char *buf;
  size_t len;
  size_t cap;
} StrBuf;










































typedef struct {
  int use_router;
  int use_state;
  int use_memo;
  int use_effect;
  int use_ref;
  int use_context;
  int use_reducer;
  int use_params;
  int use_navigate;
  int use_callback;
  int use_id;
  int use_transition;
  int use_deferred;
  int use_action;
  int use_lazy;
  int use_suspense;
  int use_layout_effect;
  int use_insertion_effect;
  int use_effect_event;
  int use_sync_external_store;
  int use_imperative_handle;
  int use_forward_ref;
  int use_portal;
  int use_error_boundary;
  int use_children;
  int use_cord_icon;
  int use_cord_motion;
  int use_cord_chart;
  int in_component;
  int is_layout;
  char action_pending[64];
  char action_fn[64];
  char action_handler[64];
  int in_form_with_action;
} GenCtx;
















typedef enum { RK_PAGE = 0, RK_COMPONENT = 1, RK_LAYOUT = 2 } ReactKind;

#define REACT_MAX_UNITS 64
#define REACT_MAX_USED 24
#define REACT_MAX_ROUTES 64

typedef struct {
  char name[96];
  char dir[16];
  char rel[128];
  char source_path[256];
  char layout_src[96]; 
  IrNode *ir_def;
  ReactKind kind;
  int use_state;
  int use_memo;
  int use_effect;
  int use_ref;
  int use_context;
  int use_reducer;
  int use_params;
  int use_navigate;
  int use_callback;
  int use_id;
  int use_transition;
  int use_deferred;
  int use_action;
  int use_lazy;
  int use_suspense;
  int use_layout_effect;
  int use_insertion_effect;
  int use_effect_event;
  int use_sync_external_store;
  int use_imperative_handle;
  int use_forward_ref;
  int use_portal;
  int use_error_boundary;
  int use_link;
  int use_outlet;
  int use_children;
  int use_fragment;
  char action_stubs[8][64];
  int n_action_stubs;
  char used[REACT_MAX_USED][96];
  int n_used;
  char ctx_names[16][64];
  int n_ctx;
  char lazy_names[16][64];
  char lazy_paths[16][128];
  int n_lazy;
} ReactUnit;

typedef struct {
  ReactUnit units[REACT_MAX_UNITS];
  int n_units;
  IrNode *routes[REACT_MAX_ROUTES];
  int n_routes;
  IrNode *page_nodes[64];
  int n_page_nodes;
  IrNode *contexts[32]; 
  int n_contexts;
  IrNode *foreigns[32];
  int n_foreigns;
  int has_router;
  char lazy_route_names[16][64];
  int n_lazy_routes;
  int truncated; 
} ReactProject;

void react_ir_sb_init(StrBuf *sb);
void react_ir_sb_oom(void);
void react_ir_sb_append(StrBuf *sb, const char *s);
void react_ir_sb_appendf(StrBuf *sb, const char *fmt, ...);
void react_ir_sb_indent(StrBuf *sb, int depth);
int react_ir_is_pascal_case(const char *s);
int react_ir_looks_like_number(const char *s);
int react_ir_looks_like_bool(const char *s);
int react_ir_is_dom_a11y_or_data_attr(const char *name);
int react_ir_looks_like_js_expr(const char *s);
int react_ir_is_stringish_prop_name(const char *name);
int react_ir_needs_js_arrow(const char *handler);
int react_ir_attr_is_true(const char *v);
int react_ir_is_style_attr_name(const char *name);
int react_ir_is_style_bool_name(const char *name);
const char *react_ir_tag_to_div_plus_class(const char *tag);
const char *react_ir_html_tag_for(const char *tag);
void react_ir_emit_jsx_value(StrBuf *sb, const char *val, int force_expr);
void react_ir_emit_js_literal(StrBuf *sb, const char *val);
void react_ir_collect_classes_ir(char *classes, size_t classes_sz, IrNode *node,
                               const char *base_class);
int react_ir_ir_hook_is(const IrNode *n, const char *kind);
int react_ir_is_decl_ir(const IrNode *n);
int react_ir_route_has_lazy_attr_ir(IrNode *route);
int react_ir_project_is_lazy_route_name(ReactProject *proj, const char *name);
int react_ir_route_is_lazy_ir(ReactProject *proj, IrNode *route);
void react_ir_layout_export_name(const char *orig, char *out, size_t n);
int react_ir_name_ends_with(const char *s, const char *suffix);
int react_ir_is_route_target_ir(const char *name, IrNode **routes, int n_routes);
void react_ir_unit_add_used(ReactUnit *u, const char *name);
void react_ir_scan_tree_deps_ir(IrNode *n, ReactUnit *u);
ReactUnit *react_ir_project_find_unit(ReactProject *p, const char *name);
void react_ir_project_partition_from_ir(ReactProject *p, IrProgram *ir);
void react_ir_import_path_between(const ReactUnit *from, const ReactUnit *to,
                                char *out, size_t n);
void react_ir_collect_bind_names_ir(IrNode *n, char names[][64], int *count,
                                  int max);
int react_ir_state_name_exists_ir(IrNode *def, const char *name);
int react_ir_gen_ir_component_element(StrBuf *sb, IrNode *node, int depth,
                                    GenCtx *ctx, const char *tag);
int react_ir_gen_ir_special_element(StrBuf *sb, IrNode *node, int depth,
                                  GenCtx *ctx, const char *tag);
void react_ir_gen_ir_dom_attributes(StrBuf *sb, IrNode *node, const char *tag,
                                  const char *html_tag, int use_link,
                                  int skip_style_size, GenCtx *ctx);
void react_ir_gen_ir_element(StrBuf *sb, IrNode *node, int depth, GenCtx *ctx);
void react_ir_gen_ir_children(StrBuf *sb, IrNode *node, int depth, GenCtx *ctx);
void react_ir_gen_ir_interpolation(StrBuf *sb, IrNode *node, int depth);
void react_ir_gen_ir_node(StrBuf *sb, IrNode *node, int depth, GenCtx *ctx);
void react_ir_gen_component_state_hooks_ir(StrBuf *sb, IrNode *def, GenCtx *ctx);
void react_ir_gen_component_derived_hooks_ir(StrBuf *sb, IrNode *def, GenCtx *ctx);
void react_ir_gen_component_fn_ir(StrBuf *sb, IrNode *def, const char *name,
                                GenCtx *ctx);
char *react_ir_gen_unit_module_ir(ReactProject *proj, ReactUnit *u);
char *react_ir_gen_contexts_module_ir(ReactProject *proj);
const char *react_ir_route_layout_attr_ir(IrNode *route);
ReactUnit *react_ir_find_layout_for_route_ir(ReactProject *proj, IrNode *route);
char *react_ir_gen_app_module_ir(ReactProject *proj);
void react_ir_attach_root_lazies_ir(ReactProject *proj, IrNode *root);
int react_emit_modules_from_ir(IrProgram *ir, ReactWriteFn write_fn,
                               void *userdata);
char *react_ir_react_generate_impl_ir(IrProgram *ir);
char *react_generate_from_ir(IrProgram *ir);
#endif
