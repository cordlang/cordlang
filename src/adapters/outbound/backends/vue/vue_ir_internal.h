#ifndef CORDLANG_VUE_IR_INTERNAL_H
#define CORDLANG_VUE_IR_INTERNAL_H
#include "adapters/outbound/backends/vue/vue_backend.h"
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
} VuModuleNeeds;












typedef enum { SK_PAGE = 0, SK_COMPONENT = 1, SK_LAYOUT = 2 } VueKind;

#define VU_MAX_UNITS 64
#define VU_MAX_USED 24
#define VU_MAX_ROUTES 64

typedef struct {
  char name[96];
  char dir[16];
  char rel[128];
  char source_path[256];
  IrNode *ir_def; 
  Node *def;      
  VueKind kind;
  int use_link;
  int use_slot;
  int use_context;
  int use_params;
  int use_cord_icon;
  int use_cord_motion;
  int use_cord_chart;
  char used[VU_MAX_USED][96];
  int n_used;
} VueUnit;

typedef struct {
  VueUnit units[VU_MAX_UNITS];
  int n_units;
  IrNode *routes[VU_MAX_ROUTES];
  int n_routes;
  int has_router;
  IrNode *contexts[32]; 
  int n_contexts;
  IrNode *foreigns[32];
  int n_foreigns;
  char lazy_route_names[16][64];
  int n_lazy_routes;
  int truncated;
} VueProject;

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

void vue_sb_init(StrBuf *sb);
void vue_sb_oom(void);
void vue_sb_append(StrBuf *sb, const char *s);
void vue_sb_appendf(StrBuf *sb, const char *fmt, ...);
void vue_sb_indent(StrBuf *sb, int depth);
int vue_looks_like_js_expr(const char *s);
void vue_emit_vue_prop_value(StrBuf *sb, const char *val);
int vue_is_style_attr(const char *name);
void vue_vu_reset_stores(void);
void vue_vu_add_store(const char *name);
int vue_vu_is_store_expr(const char *expr);
int vue_is_vue_directive_attr(const char *name);
void vue_emit_vue_directive(StrBuf *sb, const char *dir, const char *val);
int vue_is_vue_transition_fn(const char *name);
int vue_is_vue_animate_fn(const char *name);
void vue_directive_fn_name(const char *val, char *out, size_t outsz);
void vue_vu_needs_add_trans(VuModuleNeeds *n, const char *fn);
void vue_vu_needs_add_anim(VuModuleNeeds *n, const char *fn);
int vue_attr_is_true(const IrNode *a);
void vue_collect_classes_ir(char *classes, size_t sz, const IrNode *node,
                               const char *base);
int vue_name_ends_with(const char *s, const char *suffix);
int vue_is_route_target_ir(const char *name, IrNode **routes, int n);
void vue_unit_add_used(VueUnit *u, const char *name);
int vue_route_has_lazy_attr_ir(IrNode *route);
int vue_project_is_lazy_route_name(VueProject *proj, const char *name);
int vue_route_is_lazy_ir(VueProject *proj, IrNode *route);
void vue_scan_deps_ir(IrNode *n, VueUnit *u);
void vue_scan_module_needs_ir(IrNode *n, VuModuleNeeds *needs);
void vue_project_add_context_ir(VueProject *p, IrNode *c);
void vue_harvest_contexts_ir(VueProject *p, IrNode *root);
void vue_collect_provides_ir(IrNode *n, ProvideInfo *out, int *count,
                                int max);
VueUnit *vue_find_unit(VueProject *p, const char *name);
void vue_layout_name(const char *orig, char *out, size_t n);
void vue_project_partition_from_ir(VueProject *p, IrNode *root);
void vue_emit_interp_expr(StrBuf *sb, const char *expr);
void vue_gen_text_or_interp_ir(StrBuf *sb, IrNode *node, int depth);
int vue_is_markup_child_kind(IrKind k);
void vue_gen_element_ir(StrBuf *sb, IrNode *node, int depth, int is_layout);
int vue_is_ir_script_decl(const IrNode *n);
void vue_gen_children_ir(StrBuf *sb, IrNode *node, int depth, int is_layout);
void vue_gen_node_ir(StrBuf *sb, IrNode *node, int depth, int is_layout);
void vue_vu_collect_binds_ir(IrNode *n, char names[][64], int *count,
                                int max);
int vue_vu_state_exists_ir(IrNode *def, const char *name);
void vue_emit_js_lit(StrBuf *sb, const char *val);
void vue_foreach_state(IrNode *def,
                          void (*fn)(const char *name, const char *init,
                                     void *ud),
                          void *ud);
void vue_foreach_prop(IrNode *def,
                         void (*fn)(const char *name, const char *deflt,
                                    void *ud),
                         void *ud);
void vue_props_collect_name(const char *name, const char *deflt, void *ud);
void vue_props_emit_one(const char *name, const char *deflt, void *ud);
void vue_state_emit_one(const char *name, const char *init, void *ud);
void vue_state_emit_setter(const char *name, const char *init, void *ud);
void vue_vue_emit_module_imports_ir(VueProject *proj, VueUnit *u,
                                      IrNode *def, StrBuf *script,
                                      VuModuleNeeds *needs,
                                      ProvideInfo *provides, int *n_provides);
void vue_vue_emit_module_props_ir(VueUnit *u, IrNode *def,
                                     int is_layout, StrBuf *script);
void vue_vue_emit_module_state_ir(IrNode *def, StrBuf *script,
                                     const VuModuleNeeds *needs,
                                     const ProvideInfo *provides,
                                     int n_provides);
char *vue_gen_vue_module_ir(VueProject *proj, VueUnit *u);
char *vue_gen_contexts_module_ir(VueProject *proj);
const char *vue_route_layout_attr_ir(IrNode *route);
VueUnit *vue_find_layout_for_route_ir(VueProject *proj, IrNode *route);
char *vue_gen_router_js_ir(VueProject *proj);
char *vue_gen_app_vue_ir(VueProject *proj);
int vue_emit_modules_from_ir(IrProgram *ir, VueWriteFn write_fn,
                                void *userdata);
char *vue_vue_generate_impl_ir(IrProgram *ir);
char *vue_generate_from_ir(IrProgram *ir);
#endif
