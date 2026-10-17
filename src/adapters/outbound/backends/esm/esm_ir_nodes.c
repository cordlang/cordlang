#include "adapters/outbound/backends/esm/esm_ir_internal.h"

void esm_ir_gen_for(Sb *sb, IrNode *n, int depth, EsmCtx *c) {
  const char *var = n->name ? n->name : "item";
  const char *list = n->value ? n->value : "items";
  const char *key_expr = NULL;
  for (size_t i = 0; i < n->n_kids; i++) {
    IrNode *a = n->kids[i];
    if (a && a->kind == IR_ATTR && a->name && strcmp(a->name, "key") == 0 &&
        a->value) {
      key_expr = a->value;
      break;
    }
  }
  esm_ir_sb_pad(sb, depth);
  esm_ir_sb_addf(sb, "(%s || []).map((%s, idx) => keyed(", list, var);
  if (key_expr)
    esm_ir_sb_addf(sb, "%s, ", key_expr);
  else
    esm_ir_sb_add(sb, "idx, ");
  int multi = 0;
  int count = 0;
  for (size_t i = 0; i < n->n_kids; i++)
    if (esm_ir_is_renderable(n->kids[i])) count++;
  multi = count != 1;
  if (multi) esm_ir_sb_add(sb, "frag([");
  esm_ir_sb_add(sb, "\n");
  /* The loop variable is only in scope inside the body. */
  int saved_scope = c->n_scope;
  esm_ir_scope_add(c, var);
  esm_ir_scope_add(c, "idx");
  esm_ir_gen_children_array(sb, n, depth + 1, c);
  c->n_scope = saved_scope;
  esm_ir_sb_pad(sb, depth);
  if (multi) esm_ir_sb_add(sb, "])");
  esm_ir_sb_add(sb, "))");
}

void esm_ir_gen_if(Sb *sb, IrNode *n, IrNode *else_node, int depth, EsmCtx *c) {
  const char *cond = n->value ? n->value : "true";
  esm_ir_sb_pad(sb, depth);
  esm_ir_sb_addf(sb, "((%s) ? frag([\n", cond);
  esm_ir_gen_children_array(sb, n, depth + 1, c);
  esm_ir_sb_pad(sb, depth);
  esm_ir_sb_add(sb, "]) : ");
  if (else_node) {
    esm_ir_sb_add(sb, "frag([\n");
    esm_ir_gen_children_array(sb, else_node, depth + 1, c);
    esm_ir_sb_pad(sb, depth);
    esm_ir_sb_add(sb, "]))");
  } else {
    esm_ir_sb_add(sb, "null)");
  }
}

void esm_ir_gen_children_array(Sb *sb, IrNode *parent, int depth, EsmCtx *c) {
  for (size_t i = 0; i < parent->n_kids; i++) {
    IrNode *k = parent->kids[i];
    if (!esm_ir_is_renderable(k)) continue;

    if (k->kind == IR_IF) {
      IrNode *else_node = NULL;
      for (size_t j = i + 1; j < parent->n_kids; j++) {
        IrNode *s = parent->kids[j];
        if (s && s->kind == IR_TEXT && s->value &&
            strcmp(s->value, "__else__") == 0) {
          else_node = s;
          i = j;
          break;
        }
        if (esm_ir_is_renderable(s)) break;
      }
      esm_ir_gen_if(sb, k, else_node, depth, c);
      esm_ir_sb_add(sb, ",\n");
      continue;
    }
    esm_ir_gen_node(sb, k, depth, c);
    esm_ir_sb_add(sb, ",\n");
  }
}

void esm_ir_gen_node(Sb *sb, IrNode *n, int depth, EsmCtx *c) {
  if (!n) return;
  switch (n->kind) {
    case IR_ELEMENT:
      esm_ir_gen_element(sb, n, depth, c);
      break;
    case IR_SLOT:
      esm_ir_sb_pad(sb, depth);
      esm_ir_sb_add(sb, "props.children");
      c->uses_children = 1;
      break;
    case IR_TEXT:
      esm_ir_gen_text(sb, n->value, depth);
      break;
    case IR_INTERP:
      esm_ir_gen_interp(sb, n, depth);
      break;
    case IR_FOR:
      esm_ir_gen_for(sb, n, depth, c);
      break;
    case IR_IF:
      esm_ir_gen_if(sb, n, NULL, depth, c);
      break;
    case IR_HOOK:
      /* Structural hooks we can express without a framework. */
      if (esm_ir_ir_hook_is(n, "empty")) {
        const char *cond = n->value ? n->value : "true";
        esm_ir_sb_pad(sb, depth);
        esm_ir_sb_addf(sb, "((%s) ? frag([\n", cond);
        esm_ir_gen_children_array(sb, n, depth + 1, c);
        esm_ir_sb_pad(sb, depth);
        esm_ir_sb_add(sb, "]) : null)");
      } else if (esm_ir_ir_hook_is(n, "errorBoundary")) {
        /* Local boundary: catch render throws; optional __fallback__ child. */
        esm_ir_sb_pad(sb, depth);
        esm_ir_sb_add(sb, "h(ErrorBoundary, { fallback: ");
        {
          int has_fb = 0;
          for (size_t i = 0; i < n->n_kids; i++) {
            IrNode *k = n->kids[i];
            if (k && k->kind == IR_ELEMENT && k->name &&
                strcmp(k->name, "__fallback__") == 0) {
              esm_ir_sb_add(sb, "frag([\n");
              esm_ir_gen_children_array(sb, k, depth + 1, c);
              esm_ir_sb_pad(sb, depth);
              esm_ir_sb_add(sb, "])");
              has_fb = 1;
              break;
            }
          }
          if (!has_fb)
            esm_ir_sb_add(sb, "h('div', { class: 'cord-runtime-error' }, 'Error')");
        }
        esm_ir_sb_add(sb, " }, frag([\n");
        for (size_t i = 0; i < n->n_kids; i++) {
          IrNode *k = n->kids[i];
          if (!esm_ir_is_renderable(k)) continue;
          if (k->kind == IR_ELEMENT && k->name &&
              strcmp(k->name, "__fallback__") == 0)
            continue;
          esm_ir_gen_node(sb, k, depth + 1, c);
          esm_ir_sb_add(sb, ",\n");
        }
        esm_ir_sb_pad(sb, depth);
        esm_ir_sb_add(sb, "]))");
      } else if (esm_ir_ir_hook_is(n, "portal")) {
        esm_ir_sb_pad(sb, depth);
        esm_ir_sb_add(sb, "h(Portal, { target: 'body' }, frag([\n");
        for (size_t i = 0; i < n->n_kids; i++) {
          IrNode *k = n->kids[i];
          if (!esm_ir_is_renderable(k)) continue;
          if (k->kind == IR_ELEMENT && k->name &&
              strcmp(k->name, "__fallback__") == 0)
            continue;
          esm_ir_gen_node(sb, k, depth + 1, c);
          esm_ir_sb_add(sb, ",\n");
        }
        esm_ir_sb_pad(sb, depth);
        esm_ir_sb_add(sb, "]))");
      } else if (esm_ir_ir_hook_is(n, "suspense") || esm_ir_ir_hook_is(n, "loading")) {
        /* Show __fallback__ while any $.resource in tree is loading — best-effort:
         * emit Suspense wrapper that swaps on props.loading if present. */
        esm_ir_sb_pad(sb, depth);
        esm_ir_sb_add(sb, "h(Suspense, { fallback: ");
        {
          int has_fb = 0;
          for (size_t i = 0; i < n->n_kids; i++) {
            IrNode *k = n->kids[i];
            if (k && k->kind == IR_ELEMENT && k->name &&
                strcmp(k->name, "__fallback__") == 0) {
              esm_ir_sb_add(sb, "frag([\n");
              esm_ir_gen_children_array(sb, k, depth + 1, c);
              esm_ir_sb_pad(sb, depth);
              esm_ir_sb_add(sb, "])");
              has_fb = 1;
              break;
            }
          }
          if (!has_fb) esm_ir_sb_add(sb, "null");
        }
        esm_ir_sb_add(sb, " }, frag([\n");
        for (size_t i = 0; i < n->n_kids; i++) {
          IrNode *k = n->kids[i];
          if (!esm_ir_is_renderable(k)) continue;
          if (k->kind == IR_ELEMENT && k->name &&
              strcmp(k->name, "__fallback__") == 0)
            continue;
          esm_ir_gen_node(sb, k, depth + 1, c);
          esm_ir_sb_add(sb, ",\n");
        }
        esm_ir_sb_pad(sb, depth);
        esm_ir_sb_add(sb, "]))");
      } else {
        esm_ir_sb_pad(sb, depth);
        esm_ir_sb_add(sb, "null");
      }
      break;
    case IR_FOREIGN:
      /* Declarations only; stubs are emitted once in gen_imports. */
      esm_ir_sb_pad(sb, depth);
      esm_ir_sb_add(sb, "null");
      break;
    default:
      esm_ir_sb_pad(sb, depth);
      esm_ir_sb_add(sb, "frag([\n");
      esm_ir_gen_children_array(sb, n, depth + 1, c);
      esm_ir_sb_pad(sb, depth);
      esm_ir_sb_add(sb, "])");
      break;
  }
}
