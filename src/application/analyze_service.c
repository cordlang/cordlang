#include "application/analyze_service.h"

#include "application/ports/compiler_port.h"
#include "domain/ast.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NAMES 256
#define NAME_LEN 96

typedef struct {
  char names[MAX_NAMES][NAME_LEN];
  int count;
  int used[MAX_NAMES];
  int lines[MAX_NAMES];
  int cols[MAX_NAMES];
  int is_layout[MAX_NAMES];
} CompSet;

typedef struct {
  int n_link_bad;
  int n_layout_bad;
  int n_h1;
  int n_fetch_ui;
  int n_form_pending;
  int n_no_purpose;
} AnalyzeCounts;

static int is_pascal(const char *s) {
  return s && s[0] && isupper((unsigned char)s[0]);
}

static int comp_index(CompSet *s, const char *name) {
  if (!s || !name) return -1;
  for (int i = 0; i < s->count; i++) {
    if (strcmp(s->names[i], name) == 0) return i;
  }
  return -1;
}

static void comp_add(CompSet *s, const char *name, int line, int col,
                     int layout) {
  if (!s || !name || !*name || s->count >= MAX_NAMES) return;
  int idx = comp_index(s, name);
  if (idx >= 0) {
    if (layout) s->is_layout[idx] = 1;
    return;
  }
  strncpy(s->names[s->count], name, NAME_LEN - 1);
  s->names[s->count][NAME_LEN - 1] = '\0';
  s->lines[s->count] = line;
  s->cols[s->count] = col;
  s->used[s->count] = 0;
  s->is_layout[s->count] = layout;
  s->count++;
}

static void comp_mark(CompSet *s, const char *name) {
  int idx = comp_index(s, name);
  if (idx >= 0) s->used[idx] = 1;
}

static int elem_has_attr(Node *el, const char *key) {
  if (!el) return 0;
  for (size_t i = 0; i < el->children_len; i++) {
    Node *c = el->children[i];
    if (!c) continue;
    if ((c->type == NODE_ATTR || c->type == NODE_BOOL_ATTR) && c->value &&
        strcmp(c->value, key) == 0)
      return 1;
  }
  return 0;
}

static int subtree_has_slot(Node *n) {
  if (!n) return 0;
  if (n->type == NODE_SLOT) return 1;
  if (n->type == NODE_ELEMENT && n->value && strcmp(n->value, "slot") == 0)
    return 1;
  for (size_t i = 0; i < n->children_len; i++) {
    if (subtree_has_slot(n->children[i])) return 1;
  }
  return 0;
}

/* Heuristic: loading/error UI for fetch `name` in scope. */
static int subtree_has_fetch_ui(Node *n, const char *fetch_name) {
  if (!n || !fetch_name) return 0;
  char loading[NAME_LEN + 16];
  char err[NAME_LEN + 16];
  snprintf(loading, sizeof(loading), "%sLoading", fetch_name);
  snprintf(err, sizeof(err), "%sError", fetch_name);

  if (n->type == NODE_LOADING) return 1;
  if (n->type == NODE_IF && n->value) {
    if (strstr(n->value, loading) || strstr(n->value, err) ||
        strstr(n->value, "Loading") || strstr(n->value, "Error"))
      return 1;
  }
  if (n->type == NODE_TEXT && n->value) {
    if (strstr(n->value, "Loading") || strstr(n->value, "loading") ||
        strstr(n->value, "Error") || strstr(n->value, "error"))
      return 1;
  }
  for (size_t i = 0; i < n->children_len; i++) {
    if (subtree_has_fetch_ui(n->children[i], fetch_name)) return 1;
  }
  return 0;
}

static void collect(Node *n, CompSet *comps) {
  if (!n) return;
  if (n->type == NODE_COMPONENT_DEF && n->value) {
    int layout = (n->value2 && strcmp(n->value2, "__layout__") == 0) ||
                 strstr(n->value, "Layout") != NULL;
    comp_add(comps, n->value, n->line, n->col, layout);
  }
  if (n->type == NODE_LAZY_DECL && n->value)
    comp_add(comps, n->value, n->line, n->col, 0);
  for (size_t i = 0; i < n->children_len; i++) collect(n->children[i], comps);
}

static void walk(Node *n, CompSet *comps, DiagList *out, const char *file,
                 Node *scope, AnalyzeCounts *c) {
  if (!n || !c) return;

  Node *next_scope = scope;
  if (n->type == NODE_COMPONENT_DEF || n->type == NODE_ROOT)
    next_scope = n;

  if (n->type == NODE_ELEMENT && n->value) {
    if (is_pascal(n->value)) comp_mark(comps, n->value);

    if (strcmp(n->value, "link") == 0) {
      if (!elem_has_attr(n, "to") && !elem_has_attr(n, "href")) {
        diag_emit(out, DIAG_WARN, file, n->line, n->col,
                  "link without to= or href=");
        c->n_link_bad++;
      }
      if (!elem_has_attr(n, "purpose")) {
        diag_emit(out, DIAG_INFO, file, n->line, n->col,
                  "link without purpose= (optional semantic hint)");
        c->n_no_purpose++;
      }
    }

    if (strcmp(n->value, "btn") == 0 || strcmp(n->value, "button") == 0) {
      if (!elem_has_attr(n, "purpose")) {
        diag_emit(out, DIAG_INFO, file, n->line, n->col,
                  "btn without purpose= (optional semantic hint)");
        c->n_no_purpose++;
      }
    }

    if (strcmp(n->value, "form") == 0) {
      /* form may rely on action=formAction from action decl; still prefer pending nearby */
      if (!elem_has_attr(n, "pending") && !elem_has_attr(n, "action")) {
        /* bare form — soft info only via action decl check */
      }
    }

    if (strcmp(n->value, "h1") == 0) c->n_h1++;
  }

  if (n->type == NODE_FETCH_DECL && n->value) {
    Node *search = next_scope ? next_scope : n;
    if (!subtree_has_fetch_ui(search, n->value)) {
      diag_emit_ex(out, DIAG_WARN, file, n->line, n->col, "fetch-ui",
                   "add if nameLoading / if nameError (or Loading text)",
                   "fetch '%s' has no nearby loading/error UI", n->value);
      c->n_fetch_ui++;
    }
  }

  if (n->type == NODE_ACTION_DECL) {
    if (!elem_has_attr(n, "pending")) {
      diag_emit_ex(out, DIAG_WARN, file, n->line, n->col, "form-pending",
                   "add pending=saving (or similar) to action …",
                   "action '%s' without pending=",
                   n->value ? n->value : "?");
      c->n_form_pending++;
    }
  }

  if (n->type == NODE_COMPONENT_DEF && n->value) {
    int idx = comp_index(comps, n->value);
    if (idx >= 0 && comps->is_layout[idx] && !subtree_has_slot(n)) {
      diag_emit(out, DIAG_WARN, file, n->line, n->col,
                "layout-like component '%s' has no slot", n->value);
      c->n_layout_bad++;
    }
  }

  /* route layout=Name marks layout + usage */
  if (n->type == NODE_ROUTE) {
    for (size_t i = 0; i < n->children_len; i++) {
      Node *ch = n->children[i];
      if (ch && ch->type == NODE_ATTR && ch->value &&
          strcmp(ch->value, "layout") == 0 && ch->value2) {
        int idx = comp_index(comps, ch->value2);
        if (idx >= 0) comps->is_layout[idx] = 1;
        comp_mark(comps, ch->value2);
      }
    }
    if (n->value2) comp_mark(comps, n->value2);
  }

  for (size_t i = 0; i < n->children_len; i++)
    walk(n->children[i], comps, out, file, next_scope, c);
}

int analyze_service_run(const char *entry_path, DiagList *out) {
  return analyze_service_run_opts(entry_path, out, 0, NULL);
}

int analyze_service_run_opts(const char *entry_path, DiagList *out, int quiet,
                             int *out_score) {
  if (!out) return 1;
  if (!entry_path || !*entry_path) {
    diag_emit(out, DIAG_ERROR, "<analyze>", 0, 0, "no entry path");
    return 1;
  }

  CompileResult result = compiler_parse_project(entry_path);
  if (!result.ok || !result.ast || !result.ast->root) {
    diag_emit(out, DIAG_ERROR, entry_path, 0, 0, "%s",
              result.error ? result.error : "parse failed");
    compiler_result_free(&result);
    return 1;
  }

  CompSet comps = {0};
  AnalyzeCounts counts = {0};
  int n_unused = 0;

  collect(result.ast->root, &comps);
  walk(result.ast->root, &comps, out, entry_path, result.ast->root, &counts);

  for (int i = 0; i < comps.count; i++) {
    if (!comps.used[i] && !comps.is_layout[i]) {
      if (strcmp(comps.names[i], "App") == 0) continue;
      diag_emit(out, DIAG_INFO, entry_path, comps.lines[i], comps.cols[i],
                "component '%s' is never referenced", comps.names[i]);
      n_unused++;
    }
  }

  if (counts.n_h1 == 0) {
    diag_emit(out, DIAG_INFO, entry_path, 0, 0,
              "no h1 found (a11y heuristic)");
  } else if (counts.n_h1 > 3) {
    diag_emit(out, DIAG_INFO, entry_path, 0, 0,
              "%d h1 elements — consider a single page title", counts.n_h1);
  }

  /* Score weights documented in docs/AI_WORKFLOW.md */
  int score = 100;
  score -= counts.n_link_bad * 15;
  score -= counts.n_layout_bad * 10;
  score -= n_unused * 5;
  score -= counts.n_fetch_ui * 8;
  score -= counts.n_form_pending * 6;
  score -= counts.n_no_purpose * 2;
  if (counts.n_h1 == 0) score -= 5;
  if (counts.n_h1 > 3) score -= 3;
  if (score < 0) score = 0;
  if (score > 100) score = 100;
  if (out_score) *out_score = score;

  if (!quiet) {
    printf("cordlang analyze — deterministic score (no LLM)\n");
    printf("  score: %d/100\n", score);
    printf("  unused components: %d\n", n_unused);
    printf("  link without to/href: %d\n", counts.n_link_bad);
    printf("  layout without slot: %d\n", counts.n_layout_bad);
    printf("  fetch without loading/error UI: %d\n", counts.n_fetch_ui);
    printf("  action without pending: %d\n", counts.n_form_pending);
    printf("  btn/link without purpose: %d\n", counts.n_no_purpose);
    printf("  h1 count: %d\n", counts.n_h1);
    printf("  details: %d warning(s), %d info\n",
           diag_count_level(out, DIAG_WARN), diag_count_level(out, DIAG_INFO));
  }

  compiler_result_free(&result);
  return 0;
}
