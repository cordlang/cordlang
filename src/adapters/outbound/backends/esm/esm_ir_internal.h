#ifndef CORDLANG_ESM_IR_INTERNAL_H
#define CORDLANG_ESM_IR_INTERNAL_H
#include "adapters/outbound/backends/esm/esm_backend.h"
#include "adapters/outbound/backends/cord_class.h"
#include "adapters/outbound/backends/theme_css.h"
#include "application/ports/compiler_port.h"
#include "application/ports/fs_port.h"
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
} Sb;













#define ESM_MAX_IMPORTS 128

typedef struct {
  char export_name[96];
  char url[512];
  char mod_path[512]; 
  char basename[96];  
} EsmImport;

#define ESM_MAX_SCOPE 192

typedef struct {
  EsmModuleCtx *mod;
  EsmImport imports[ESM_MAX_IMPORTS];
  int n_imports;
  int is_layout;    
  int uses_children;
  int truncated;
  
  char scope[ESM_MAX_SCOPE][64];
  int n_scope;
  
  struct {
    char export_name[96];
    char mod_path[256];
  } unresolved[32];
  int n_unresolved;
  
  struct {
    char name[96];
    char mod_path[256]; 
  } foreigns[32];
  int n_foreigns;
  
  int self_mount;
} EsmCtx;





























































































































static const BackendPort esm_port = {
    .name = "esm",
    .extension = ".js",
    .needs_node_check = 0,
    .generate_from_ir = esm_generate_from_ir,
    .scaffold_from_ir = NULL,
    .generate = esm_generate,
    .scaffold = NULL,
    .scaffold_from_ast = NULL,
};

void esm_ir_sb_init(Sb *sb);
void esm_ir_sb_grow(Sb *sb, size_t extra);
void esm_ir_sb_add(Sb *sb, const char *s);
void esm_ir_sb_addf(Sb *sb, const char *fmt, ...);
void esm_ir_sb_pad(Sb *sb, int depth);
void esm_ir_scope_add(EsmCtx *c, const char *name);
int esm_ir_scope_has(EsmCtx *c, const char *expr);
void esm_ir_scope_add_list(EsmCtx *c, const char *list);
int esm_ir_is_pascal(const char *s);
int esm_ir_is_safe_js_ident(const char *s);
int esm_ir_is_safe_js_path(const char *s);
void esm_ir_emit_sq_string(Sb *sb, const char *s);
void esm_ir_emit_literal(Sb *sb, const char *val);
void esm_ir_emit_value(Sb *sb, const char *val, int prefer_string, EsmCtx *c);
int esm_ir_handler_needs_arrow(const char *h);
void esm_ir_emit_handler(Sb *sb, const char *event, const char *handler);
void esm_ir_event_prop_name(const char *event, char *out, size_t n);
void esm_ir_path_basename_noext(const char *mod_path, char *out, size_t n);
int esm_ir_url_for_resolved(const char *resolved, const char *project_root,
                            char *out, size_t n);
EsmImport *esm_ir_import_find_by_name(EsmCtx *c, const char *name);
const char *esm_ir_import_unresolved(EsmCtx *c, const char *mod_path,
                                     const char *alias);
const char *esm_ir_import_add(EsmCtx *c, const char *mod_path, const char *alias);
const char *esm_ir_foreign_mod_hint(const IrNode *n);
void esm_ir_layout_export_name_of(const char *orig, char *out, size_t n);
void esm_ir_foreign_add(EsmCtx *c, const char *name, const char *mod);
int esm_ir_same_file_export_owns(IrNode *root, const char *name);
void esm_ir_collect_foreigns(EsmCtx *c, IrNode *root);
int esm_ir_ir_hook_is(const IrNode *n, const char *kind);
int esm_ir_is_decl_node(const IrNode *n);
int esm_ir_is_renderable(const IrNode *n);
int esm_ir_node_has_renderable_kids(IrNode *n);
int esm_ir_bare_attr_is_content(const char *name, int skip_preset_attrs);
int esm_ir_attr_is_dom_literal(const char *name);
int esm_ir_prop_is_stringish(const char *name);
void esm_ir_emit_dom_attr_value(Sb *sb, const char *val, EsmCtx *c);
void esm_ir_gen_props_object(Sb *sb, IrNode *node, const char *tag,
                             const char *dom_tag, const char *classes,
                             int is_component, EsmCtx *c);
void esm_ir_gen_element(Sb *sb, IrNode *node, int depth, EsmCtx *c);
void esm_ir_gen_text(Sb *sb, const char *value, int depth);
void esm_ir_gen_interp(Sb *sb, IrNode *n, int depth);
void esm_ir_gen_for(Sb *sb, IrNode *n, int depth, EsmCtx *c);
void esm_ir_gen_if(Sb *sb, IrNode *n, IrNode *else_node, int depth, EsmCtx *c);
void esm_ir_gen_children_array(Sb *sb, IrNode *parent, int depth, EsmCtx *c);
void esm_ir_gen_node(Sb *sb, IrNode *n, int depth, EsmCtx *c);
void esm_ir_gen_prelude(Sb *sb, IrNode *def, EsmCtx *c);
void esm_ir_gen_component(Sb *sb, IrNode *def, const char *name, int is_layout,
                          EsmCtx *c);
IrNode *esm_ir_find_theme(IrNode *root);
void esm_ir_gen_theme_export(Sb *sb, IrNode *root);
void esm_ir_gen_routes(Sb *sb, IrNode *root, EsmCtx *c);
void esm_ir_gen_imports(Sb *sb, EsmCtx *c);
char *esm_generate_module(IrProgram *ir, EsmModuleCtx *mod);
char *esm_generate_from_ir(IrProgram *ir);
char *esm_generate(Node *root);
const BackendPort *esm_backend_port(void);
#endif
