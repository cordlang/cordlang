#include "static_html_internal.h"

#define SH_MAX_BIND 96
static char *g_bind_names[SH_MAX_BIND];
static char *g_bind_vals[SH_MAX_BIND];
static int g_bind_count;
static unsigned g_spa_mask;

void statich_bind_reset(void) {
  for (int i = 0; i < g_bind_count; i++) {
    free(g_bind_names[i]);
    free(g_bind_vals[i]);
    g_bind_names[i] = NULL;
    g_bind_vals[i] = NULL;
  }
  g_bind_count = 0;
  g_spa_mask = 0;
}

const char *statich_bind_get(const char *name) {
  if (!name) return NULL;
  for (int i = 0; i < g_bind_count; i++) {
    if (g_bind_names[i] && strcmp(g_bind_names[i], name) == 0)
      return g_bind_vals[i];
  }
  return NULL;
}

void statich_bind_add(const char *name, const char *val) {
  if (!name || !*name || g_bind_count >= SH_MAX_BIND) return;
  if (statich_bind_get(name)) return;
  g_bind_names[g_bind_count] = strdup(name);
  g_bind_vals[g_bind_count] = strdup(val ? val : "");
  g_bind_count++;
}

void statich_strip_quotes(char *s) {
  if (!s || !*s) return;
  size_t n = strlen(s);
  if (n >= 2 && ((s[0] == '"' && s[n - 1] == '"') ||
                 (s[0] == '\'' && s[n - 1] == '\''))) {
    memmove(s, s + 1, n - 2);
    s[n - 2] = '\0';
  }
}

void statich_collect_binds(IrNode *node) {
  if (!node) return;
  if (node->kind == IR_STATE) {
    g_spa_mask |= 1u;
    if (node->name && strcmp(node->name, "__states__") == 0) {
      for (size_t i = 0; i < node->n_kids; i++) {
        IrNode *st = node->kids[i];
        if (st && st->kind == IR_STATE && st->name) {
          char *v = strdup(st->value ? st->value : "0");
          statich_strip_quotes(v);
          statich_bind_add(st->name, v);
          free(v);
        }
      }
    } else if (node->name) {
      char *v = strdup(node->value ? node->value : "0");
      statich_strip_quotes(v);
      statich_bind_add(node->name, v);
      free(v);
    }
  } else if (node->kind == IR_PROP) {
    if (node->name && strcmp(node->name, "__props__") == 0) {
      for (size_t i = 0; i < node->n_kids; i++) {
        IrNode *p = node->kids[i];
        if (p && p->kind == IR_PROP && p->name) {
          char *v = strdup(p->value ? p->value : "");
          statich_strip_quotes(v);
          statich_bind_add(p->name, v);
          free(v);
        }
      }
    } else if (node->name) {
      char *v = strdup(node->value ? node->value : "");
      statich_strip_quotes(v);
      statich_bind_add(node->name, v);
      free(v);
    }
  } else if (node->kind == IR_EVENT) {
    g_spa_mask |= 2u;
  } else if (node->kind == IR_ROUTE) {
    g_spa_mask |= 4u;
  }
  for (size_t i = 0; i < node->n_kids; i++) statich_collect_binds(node->kids[i]);
}

unsigned static_html_scan_spa_hooks(IrProgram *ir) {
  if (!ir || !ir->root) return 0;
  statich_bind_reset();
  statich_collect_binds(ir->root);
  unsigned m = g_spa_mask;
  statich_bind_reset();
  return m;
}
