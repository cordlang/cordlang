#ifndef CORDLANG_SVELTE_BACKEND_INTERNAL_H
#define CORDLANG_SVELTE_BACKEND_INTERNAL_H
#include "adapters/outbound/backends/svelte/svelte_backend.h"
#include "adapters/outbound/backends/ir_walk.h"
#include "adapters/outbound/backends/source_attr.h"
#include "adapters/outbound/backends/theme_css.h"
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
  char transition_fns[16][32];
  int n_transition;
  char animate_fns[8][32];
  int n_animate;
  int need_portal;
  int need_writable;
} SvModuleNeeds;












typedef enum { SK_PAGE = 0, SK_COMPONENT = 1, SK_LAYOUT = 2 } SvelteKind;

#define SV_MAX_UNITS 64
#define SV_MAX_USED 24
#define SV_MAX_ROUTES 64

typedef struct {
  char name[96];
  char dir[16];
  char rel[128];
  char source_path[256];
  IrNode *ir_def; 
  Node *def;      
  SvelteKind kind;
  int use_link;
  int use_slot;
  int use_context;
  int use_params;
  int use_cord_icon;
  int use_cord_motion;
  int use_cord_chart;
  char used[SV_MAX_USED][96];
  int n_used;
} SvelteUnit;

typedef struct {
  SvelteUnit units[SV_MAX_UNITS];
  int n_units;
  IrNode *routes[SV_MAX_ROUTES];
  int n_routes;
  int has_router;
  IrNode *contexts[32]; 
  int n_contexts;
  IrNode *foreigns[32];
  int n_foreigns;
  char lazy_route_names[16][64];
  int n_lazy_routes;
  int truncated;
} SvelteProject;

typedef struct {
  const char *ctx_name;
  const char *val;
} ProvideInfo;
























































typedef struct {
  StrBuf *script;
  int n;
  IrNode *props_bag; 
  char prop_names[64][64];
  int n_prop_names;
} PropsEmitCtx;





typedef struct {
  StrBuf *script;
} StateEmitCtx;

void svelte_sb_init(StrBuf *sb);
void svelte_sb_oom(void);
void svelte_sb_append(StrBuf *sb, const char *s);
void svelte_sb_appendf(StrBuf *sb, const char *fmt, ...);
void svelte_sb_indent(StrBuf *sb, int depth);
int svelte_looks_like_js_expr(const char *s);
void svelte_emit_svelte_prop_value(StrBuf *sb, const char *val);
int svelte_is_style_attr(const char *name);
void svelte_sv_reset_stores(void);
void svelte_sv_add_store(const char *name);
int svelte_sv_is_store_expr(const char *expr);
int svelte_is_svelte_directive_attr(const char *name);
void svelte_emit_svelte_directive(StrBuf *sb, const char *dir, const char *val);
int svelte_is_svelte_transition_fn(const char *name);
int svelte_is_svelte_animate_fn(const char *name);
void svelte_directive_fn_name(const char *val, char *out, size_t outsz);
void svelte_sv_needs_add_trans(SvModuleNeeds *n, const char *fn);
void svelte_sv_needs_add_anim(SvModuleNeeds *n, const char *fn);
int svelte_attr_is_true(const IrNode *a);
void svelte_collect_classes_ir(char *classes, size_t sz, const IrNode *node,
                               const char *base);
int svelte_name_ends_with(const char *s, const char *suffix);
int svelte_is_route_target_ir(const char *name, IrNode **routes, int n);
void svelte_unit_add_used(SvelteUnit *u, const char *name);
int svelte_route_has_lazy_attr_ir(IrNode *route);
int svelte_project_is_lazy_route_name(SvelteProject *proj, const char *name);
int svelte_route_is_lazy_ir(SvelteProject *proj, IrNode *route);
void svelte_scan_deps_ir(IrNode *n, SvelteUnit *u);
void svelte_scan_module_needs_ir(IrNode *n, SvModuleNeeds *needs);
void svelte_project_add_context_ir(SvelteProject *p, IrNode *c);
void svelte_harvest_contexts_ir(SvelteProject *p, IrNode *root);
void svelte_collect_provides_ir(IrNode *n, ProvideInfo *out, int *count,
                                int max);
SvelteUnit *svelte_find_unit(SvelteProject *p, const char *name);
void svelte_layout_name(const char *orig, char *out, size_t n);
void svelte_project_partition_from_ir(SvelteProject *p, IrNode *root);
void svelte_emit_interp_expr(StrBuf *sb, const char *expr);
void svelte_gen_text_or_interp_ir(StrBuf *sb, IrNode *node, int depth);
int svelte_is_markup_child_kind(IrKind k);
void svelte_gen_element_ir(StrBuf *sb, IrNode *node, int depth, int is_layout);
int svelte_is_ir_script_decl(const IrNode *n);
void svelte_gen_children_ir(StrBuf *sb, IrNode *node, int depth, int is_layout);
void svelte_gen_node_ir(StrBuf *sb, IrNode *node, int depth, int is_layout);
void svelte_sv_collect_binds_ir(IrNode *n, char names[][64], int *count,
                                int max);
int svelte_sv_state_exists_ir(IrNode *def, const char *name);
void svelte_emit_js_lit(StrBuf *sb, const char *val);
void svelte_foreach_state(IrNode *def,
                          void (*fn)(const char *name, const char *init,
                                     void *ud),
                          void *ud);
void svelte_foreach_prop(IrNode *def,
                         void (*fn)(const char *name, const char *deflt,
                                    void *ud),
                         void *ud);
void svelte_props_collect_name(const char *name, const char *deflt, void *ud);
void svelte_props_emit_one(const char *name, const char *deflt, void *ud);
void svelte_state_emit_one(const char *name, const char *init, void *ud);
void svelte_state_emit_setter(const char *name, const char *init, void *ud);
void svelte_svelte_emit_module_imports_ir(SvelteProject *proj, SvelteUnit *u,
                                         IrNode *def, StrBuf *script,
                                         SvModuleNeeds *needs,
                                         ProvideInfo *provides, int *n_provides);
void svelte_svelte_emit_module_props_ir(SvelteUnit *u, IrNode *def,
                                        int is_layout, StrBuf *script);
void svelte_svelte_emit_module_state_ir(IrNode *def, StrBuf *script,
                                        const SvModuleNeeds *needs,
                                        const ProvideInfo *provides,
                                        int n_provides);
char *svelte_gen_svelte_module_ir(SvelteProject *proj, SvelteUnit *u);
char *svelte_gen_contexts_module_ir(SvelteProject *proj);
const char *svelte_route_layout_attr_ir(IrNode *route);
SvelteUnit *svelte_find_layout_for_route_ir(SvelteProject *proj, IrNode *route);
char *svelte_gen_app_svelte_ir(SvelteProject *proj);
int svelte_emit_modules_from_ir(IrProgram *ir, SvelteWriteFn write_fn,
                                void *userdata);
int svelte_emit_modules(Node *root, SvelteWriteFn write_fn, void *userdata);
char *svelte_svelte_generate_impl_ir(IrProgram *ir);
char *svelte_generate_from_ir(IrProgram *ir);
char *svelte_generate(Node *root);
#endif
