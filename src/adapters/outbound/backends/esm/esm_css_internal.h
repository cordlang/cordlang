#ifndef CORDLANG_ESM_CSS_INTERNAL_H
#define CORDLANG_ESM_CSS_INTERNAL_H
#include "adapters/outbound/backends/esm/esm_backend.h"
#include "adapters/outbound/backends/cord_class.h"
#include "adapters/outbound/backends/theme_css.h"
#include "domain/ir.h"
#include <stddef.h>
typedef struct { char *buf; size_t len, cap; } Cb;
#define CLS_MAX 3072
#define CLS_LEN 128
typedef struct { char names[CLS_MAX][CLS_LEN]; int count; } ClassSet;
extern const char *esm_base_css_main;
extern const char *esm_base_css_chart;
extern const char *esm_base_css_portal;
void cb_init(Cb *c);
void cb_add(Cb *c, const char *s);
void cb_addf(Cb *c, const char *fmt, ...);
int cls_has(ClassSet *s, const char *n);
void cls_add(ClassSet *s, const char *n);
void cls_add_split(ClassSet *s, const char *blob);
void collect_from_ir(IrNode *n, ClassSet *set);
int all_digits(const char *s);
int size_value(const char *v, char *out, size_t n);
int starts(const char *s, const char *pfx, const char **rest);
void sel_escape(const char *cls, char *out, size_t n);
int decls_for_colors(const char *cls, char *out, size_t outn);
int decls_for_spacing(const char *cls, char *out, size_t outn);
int decls_for_sizing(const char *cls, char *out, size_t outn);
int decls_for_radius(const char *cls, char *out, size_t outn);
int decls_for_display(const char *cls, char *out, size_t outn);
int decls_for_keywords(const char *cls, char *out, size_t outn);
int esm_css_decls_for(const char *cls, char *out, size_t outn);
#endif
