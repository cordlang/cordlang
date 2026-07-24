#include "domain/ir_pass.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int attr_is_debug(const IrNode *a) {
  if (!a || a->kind != IR_ATTR || !a->name) return 0;
  if (strcmp(a->name, "debug") == 0) return 1;
  if (strncmp(a->name, "data-debug", 10) == 0) return 1;
  return 0;
}

static void strip_debug_attrs(IrNode *n) {
  if (!n) return;
  size_t w = 0;
  for (size_t i = 0; i < n->n_kids; i++) {
    IrNode *k = n->kids[i];
    if (attr_is_debug(k)) {
      ir_free_node(k);
      continue;
    }
    n->kids[w++] = k;
  }
  n->n_kids = w;
  for (size_t i = 0; i < n->n_kids; i++) strip_debug_attrs(n->kids[i]);
}

static IrProgram *pass_strip_debug(IrProgram *ir) {
  if (ir && ir->root) strip_debug_attrs(ir->root);
  return ir;
}

static const IrPassDesc k_passes[] = {
    {"strip-debug", pass_strip_debug,
     "Remove IR_ATTR named debug or data-debug*"},
    {NULL, NULL, NULL},
};

const IrPassDesc *ir_pass_find(const char *name) {
  if (!name) return NULL;
  for (int i = 0; k_passes[i].name; i++) {
    if (strcmp(k_passes[i].name, name) == 0) return &k_passes[i];
  }
  return NULL;
}

void ir_pass_list(void) {
  fprintf(stderr, "Available IR passes:\n");
  for (int i = 0; k_passes[i].name; i++)
    fprintf(stderr, "  %-16s %s\n", k_passes[i].name,
            k_passes[i].description ? k_passes[i].description : "");
}

IrProgram *ir_pass_apply(IrProgram *ir, const char *const *names, int n_names) {
  if (!ir || !names || n_names <= 0) return ir;
  for (int i = 0; i < n_names; i++) {
    const IrPassDesc *d = ir_pass_find(names[i]);
    if (!d) {
      fprintf(stderr, "Error: unknown IR pass '%s'\n", names[i] ? names[i] : "");
      ir_pass_list();
      return NULL;
    }
    ir = d->fn(ir);
    if (!ir) return NULL;
  }
  return ir;
}

char **ir_pass_parse_list(const char *csv, int *out_n) {
  *out_n = 0;
  if (!csv || !*csv) return NULL;
  char *buf = strdup(csv);
  if (!buf) return NULL;
  char **names = NULL;
  int n = 0, cap = 0;
  char *p = buf;
  while (*p) {
    while (*p && (isspace((unsigned char)*p) || *p == ',')) p++;
    if (!*p) break;
    char *start = p;
    while (*p && *p != ',' && !isspace((unsigned char)*p)) p++;
    char save = *p;
    *p = '\0';
    if (*start) {
      if (n >= cap) {
        cap = cap ? cap * 2 : 4;
        names = realloc(names, (size_t)cap * sizeof(char *));
      }
      names[n++] = strdup(start);
    }
    *p = save;
  }
  free(buf);
  *out_n = n;
  return names;
}

void ir_pass_names_free(char **names, int n) {
  if (!names) return;
  for (int i = 0; i < n; i++) free(names[i]);
  free(names);
}
