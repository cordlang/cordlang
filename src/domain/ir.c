#include "domain/ir.h"
#include "domain/expr.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── helpers ────────────────────────────────────────────── */

static char *xstrdup(const char *s) {
  if (!s) return NULL;
  return strdup(s);
}

static IrNode *ir_node_create(IrKind kind, const char *name, const char *value,
                              const char *value2, int line, int col,
                              const char *file) {
  IrNode *n = calloc(1, sizeof(IrNode));
  if (!n) return NULL;
  n->kind = kind;
  n->name = xstrdup(name);
  n->value = xstrdup(value);
  n->value2 = xstrdup(value2);
  n->line = line;
  n->col = col;
  n->file = xstrdup(file);
  n->origin = NULL;
  n->cap = 4;
  n->kids = calloc(n->cap, sizeof(IrNode *));
  return n;
}

/* Attach weak AST origin (not owned). */
static IrNode *ir_tag(IrNode *n, Node *ast) {
  if (n) n->origin = ast;
  return n;
}

static void ir_add_kid(IrNode *parent, IrNode *child) {
  if (!parent || !child) {
    if (child) {
      /* free orphan if parent missing — handled by ir_node_free later paths */
    }
    return;
  }
  if (parent->n_kids >= parent->cap) {
    parent->cap *= 2;
    parent->kids =
        realloc(parent->kids, parent->cap * sizeof(IrNode *));
  }
  parent->kids[parent->n_kids++] = child;
}

static void ir_node_free(IrNode *n) {
  if (!n) return;
  for (size_t i = 0; i < n->n_kids; i++) ir_node_free(n->kids[i]);
  free(n->kids);
  free(n->name);
  free(n->value);
  free(n->value2);
  free(n->file);
  free(n);
}

const char *ir_kind_name(IrKind k) {
  switch (k) {
    case IR_PROJECT:
      return "PROJECT";
    case IR_COMPONENT:
      return "COMPONENT";
    case IR_LAYOUT:
      return "LAYOUT";
    case IR_ROUTE:
      return "ROUTE";
    case IR_PROP:
      return "PROP";
    case IR_STATE:
      return "STATE";
    case IR_COMPUTED:
      return "COMPUTED";
    case IR_EFFECT:
      return "EFFECT";
    case IR_ELEMENT:
      return "ELEMENT";
    case IR_TEXT:
      return "TEXT";
    case IR_INTERP:
      return "INTERP";
    case IR_IF:
      return "IF";
    case IR_FOR:
      return "FOR";
    case IR_EVENT:
      return "EVENT";
    case IR_ATTR:
      return "ATTR";
    case IR_SLOT:
      return "SLOT";
    case IR_FETCH:
      return "FETCH";
    case IR_HOOK:
      return "HOOK";
    case IR_MODULE_USE:
      return "MODULE_USE";
    case IR_AWAIT:
      return "AWAIT";
    case IR_SNIPPET:
      return "SNIPPET";
    case IR_STORE:
      return "STORE";
    case IR_RENDER:
      return "RENDER";
    default:
      return "UNKNOWN";
  }
}

/* Try to normalize an expression; fall back to original if parse fails. */
static char *maybe_normalize(const char *src) {
  if (!src) return NULL;
  char *n = expr_normalize(src);
  if (n) return n;
  return xstrdup(src);
}

/* ── AST → IR conversion ────────────────────────────────── */

static IrNode *convert_node(const Node *node, const char *file);
static void convert_children(const Node *node, IrNode *parent, const char *file);

static IrNode *make_hook(const char *hook_name, const Node *node,
                         const char *file) {
  IrNode *h =
      ir_node_create(IR_HOOK, hook_name, node->value, node->value2, node->line,
                     node->col, file);
  convert_children(node, h, file);
  return ir_tag(h, (Node *)node);
}

static IrNode *convert_node(const Node *node, const char *file) {
  if (!node) return NULL;

  switch (node->type) {
    case NODE_ROOT: {
      IrNode *p =
          ir_node_create(IR_PROJECT, NULL, NULL, NULL, node->line, node->col,
                         file);
      convert_children(node, p, file);
      return ir_tag(p, (Node *)node);
    }

    case NODE_COMPONENT_DEF: {
      int is_layout =
          node->value2 && strcmp(node->value2, "__layout__") == 0;
      IrNode *c = ir_node_create(is_layout ? IR_LAYOUT : IR_COMPONENT,
                                 node->value, NULL, is_layout ? NULL : node->value2,
                                 node->line, node->col, file);
      convert_children(node, c, file);
      return ir_tag(c, (Node *)node);
    }

    case NODE_ROUTE: {
      /* name=path, value=target component/path */
      IrNode *r = ir_node_create(IR_ROUTE, node->value, node->value2, NULL,
                                 node->line, node->col, file);
      convert_children(node, r, file);
      return ir_tag(r, (Node *)node);
    }

    case NODE_ELEMENT: {
      IrNode *el = ir_node_create(IR_ELEMENT, node->value, NULL, NULL, node->line,
                                  node->col, file);
      convert_children(node, el, file);
      return ir_tag(el, (Node *)node);
    }

    case NODE_PROPS_DECL: {
      /* Expand props block into individual IR_PROP kids under a synthetic parent
         — attach each prop as IR_PROP. Caller of convert_children uses this
         by expanding inline when parent is component. Here return a bag of props
         as multiple nodes via a temporary: we create a dummy and let caller
         reparent. Simpler: emit first prop as this node and siblings via kids.
         Actually: props container has TEXT children (name=value default). */
      IrNode *bag =
          ir_node_create(IR_PROP, NULL, NULL, NULL, node->line, node->col, file);
      /* mark bag name as "__props__" so dump can flatten or show group */
      free(bag->name);
      bag->name = xstrdup("__props__");
      for (size_t i = 0; i < node->children_len; i++) {
        Node *ch = node->children[i];
        if (!ch) continue;
        /* prop name in value, default in value2 (stored as TEXT nodes) */
        IrNode *pr = ir_tag(ir_node_create(IR_PROP, ch->value, ch->value2, NULL,
                                           ch->line, ch->col, file),
                            ch);
        ir_add_kid(bag, pr);
      }
      return ir_tag(bag, (Node *)node);
    }

    case NODE_STATE_DECL: {
      /* Container (value==NULL) with STATE children, or leaf state. */
      if (!node->value && node->children_len > 0) {
        IrNode *bag =
            ir_node_create(IR_STATE, "__states__", NULL, NULL, node->line,
                           node->col, file);
        for (size_t i = 0; i < node->children_len; i++) {
          Node *ch = node->children[i];
          if (!ch) continue;
          if (ch->type == NODE_STATE_DECL) {
            IrNode *st = ir_tag(ir_node_create(IR_STATE, ch->value, ch->value2,
                                               NULL, ch->line, ch->col, file),
                                ch);
            ir_add_kid(bag, st);
          } else {
            IrNode *sub = convert_node(ch, file);
            if (sub) ir_add_kid(bag, sub);
          }
        }
        return ir_tag(bag, (Node *)node);
      }
      return ir_tag(ir_node_create(IR_STATE, node->value, node->value2, NULL,
                                   node->line, node->col, file),
                    (Node *)node);
    }

    case NODE_COMPUTED_DECL: {
      char *expr = maybe_normalize(node->value2);
      IrNode *c = ir_node_create(IR_COMPUTED, node->value, expr, NULL, node->line,
                                 node->col, file);
      free(expr);
      return ir_tag(c, (Node *)node);
    }

    case NODE_EFFECT_DECL: {
      char *body = maybe_normalize(node->value2);
      IrNode *e = ir_node_create(IR_EFFECT, "effect", node->value /* deps */, body,
                                 node->line, node->col, file);
      free(body);
      convert_children(node, e, file);
      return ir_tag(e, (Node *)node);
    }

    case NODE_LAYOUT_EFFECT: {
      char *body = maybe_normalize(node->value2);
      IrNode *e =
          ir_node_create(IR_EFFECT, "layoutEffect", node->value, body, node->line,
                         node->col, file);
      free(body);
      convert_children(node, e, file);
      return ir_tag(e, (Node *)node);
    }

    case NODE_INSERTION_EFFECT: {
      char *body = maybe_normalize(node->value2);
      IrNode *e = ir_node_create(IR_EFFECT, "insertionEffect", node->value, body,
                                 node->line, node->col, file);
      free(body);
      convert_children(node, e, file);
      return ir_tag(e, (Node *)node);
    }

    case NODE_IF: {
      char *cond = maybe_normalize(node->value);
      IrNode *n =
          ir_node_create(IR_IF, NULL, cond, NULL, node->line, node->col, file);
      free(cond);
      convert_children(node, n, file);
      return ir_tag(n, (Node *)node);
    }

    case NODE_FOR: {
      char *list = maybe_normalize(node->value2);
      IrNode *n = ir_node_create(IR_FOR, node->value, list, NULL, node->line,
                                 node->col, file);
      free(list);
      convert_children(node, n, file);
      return ir_tag(n, (Node *)node);
    }

    case NODE_TEXT:
      return ir_tag(ir_node_create(IR_TEXT, NULL, node->value, node->value2,
                                   node->line, node->col, file),
                    (Node *)node);

    case NODE_STRING:
      return ir_tag(ir_node_create(IR_TEXT, NULL, node->value, NULL, node->line,
                                   node->col, file),
                    (Node *)node);

    case NODE_INTERPOLATION: {
      char *expr = node->value ? maybe_normalize(node->value) : NULL;
      IrNode *n = ir_node_create(IR_INTERP, NULL, expr, NULL, node->line, node->col,
                                 file);
      free(expr);
      convert_children(node, n, file);
      return ir_tag(n, (Node *)node);
    }

    case NODE_SLOT: {
      IrNode *n =
          ir_node_create(IR_SLOT, node->value, node->value2, NULL, node->line,
                         node->col, file);
      convert_children(node, n, file);
      return ir_tag(n, (Node *)node);
    }

    case NODE_EVENT: {
      char *handler = maybe_normalize(node->value2);
      IrNode *n = ir_node_create(IR_EVENT, node->value, handler, NULL, node->line,
                                 node->col, file);
      free(handler);
      return ir_tag(n, (Node *)node);
    }

    case NODE_ATTR:
      return ir_tag(ir_node_create(IR_ATTR, node->value, node->value2, NULL,
                                   node->line, node->col, file),
                    (Node *)node);

    case NODE_BOOL_ATTR:
      return ir_tag(ir_node_create(IR_ATTR, node->value, "true", NULL, node->line,
                                   node->col, file),
                    (Node *)node);

    case NODE_STYLE_MAP: {
      IrNode *n =
          ir_node_create(IR_ATTR, "style", NULL, NULL, node->line, node->col, file);
      convert_children(node, n, file);
      return ir_tag(n, (Node *)node);
    }

    case NODE_STYLE_ENTRY:
      return ir_tag(ir_node_create(IR_ATTR, node->value, node->value2, NULL,
                                   node->line, node->col, file),
                    (Node *)node);

    case NODE_FETCH_DECL: {
      IrNode *n = ir_node_create(IR_FETCH, node->value, node->value2, NULL,
                                 node->line, node->col, file);
      convert_children(node, n, file);
      return ir_tag(n, (Node *)node);
    }

    case NODE_USE:
      return ir_tag(ir_node_create(IR_MODULE_USE, node->value2 /* alias */,
                                   node->value /* path */, NULL, node->line,
                                   node->col, file),
                    (Node *)node);

    case NODE_AWAIT: {
      IrNode *n = ir_node_create(IR_AWAIT, node->value2 /* then name */,
                                 node->value /* promise */, NULL, node->line,
                                 node->col, file);
      convert_children(node, n, file);
      return ir_tag(n, (Node *)node);
    }
    case NODE_SNIPPET: {
      IrNode *n = ir_node_create(IR_SNIPPET, node->value, node->value2, NULL,
                                 node->line, node->col, file);
      convert_children(node, n, file);
      return ir_tag(n, (Node *)node);
    }
    case NODE_STORE_DECL:
      return ir_tag(ir_node_create(IR_STORE, node->value, node->value2, NULL,
                                   node->line, node->col, file),
                    (Node *)node);
    case NODE_RENDER:
      return ir_tag(ir_node_create(IR_RENDER, node->value, node->value2, NULL,
                                   node->line, node->col, file),
                    (Node *)node);

    /* Hooks → IR_HOOK with kind in name */
    case NODE_REF_DECL:
      return make_hook("ref", node, file);
    case NODE_CONTEXT_DECL:
      return make_hook("context", node, file);
    case NODE_CONTEXT_USE:
      return make_hook("ctx", node, file);
    case NODE_REDUCER_DECL:
      return make_hook("reducer", node, file);
    case NODE_PARAMS_DECL:
      return make_hook("params", node, file);
    case NODE_NAVIGATE_DECL:
      return make_hook("navigate", node, file);
    case NODE_CALLBACK_DECL:
      return make_hook("callback", node, file);
    case NODE_ID_DECL:
      return make_hook("id", node, file);
    case NODE_TRANSITION_DECL:
      return make_hook("transition", node, file);
    case NODE_DEFERRED_DECL:
      return make_hook("deferred", node, file);
    case NODE_ACTION_DECL:
      return make_hook("action", node, file);
    case NODE_EFFECT_EVENT:
      return make_hook("effectEvent", node, file);
    case NODE_EXTERNAL_STORE:
      return make_hook("externalStore", node, file);
    case NODE_IMPERATIVE_HANDLE:
      return make_hook("imperativeHandle", node, file);
    case NODE_LAZY_DECL:
      return make_hook("lazy", node, file);
    case NODE_PORTAL:
      return make_hook("portal", node, file);
    case NODE_ERROR_BOUNDARY:
      return make_hook("errorBoundary", node, file);
    case NODE_SUSPENSE:
      return make_hook("suspense", node, file);
    case NODE_LOADING:
      return make_hook("loading", node, file);
    case NODE_EMPTY:
      return make_hook("empty", node, file);
    case NODE_HEAD:
      return make_hook("head", node, file);
    case NODE_THEME:
      return make_hook("theme", node, file);

    default: {
      char kindbuf[32];
      snprintf(kindbuf, sizeof(kindbuf), "ast_%d", (int)node->type);
      return make_hook(kindbuf, node, file);
    }
  }
}

static void convert_children(const Node *node, IrNode *parent,
                             const char *file) {
  if (!node || !parent) return;
  for (size_t i = 0; i < node->children_len; i++) {
    IrNode *kid = convert_node(node->children[i], file);
    if (kid) ir_add_kid(parent, kid);
  }
}

IrProgram *ir_from_ast(Node *ast_root, const char *entry_file) {
  if (!ast_root) return NULL;
  IrProgram *p = calloc(1, sizeof(IrProgram));
  if (!p) return NULL;
  p->entry_file = entry_file;
  p->origin_ast = NULL; /* caller may set if useful */
  p->root = convert_node(ast_root, entry_file);
  if (!p->root) {
    free(p);
    return NULL;
  }
  /* Ensure root is IR_PROJECT even if non-ROOT was passed */
  if (p->root->kind != IR_PROJECT && ast_root->type != NODE_ROOT) {
    IrNode *proj =
        ir_tag(ir_node_create(IR_PROJECT, NULL, NULL, NULL, 0, 0, entry_file),
               ast_root);
    ir_add_kid(proj, p->root);
    p->root = proj;
  }
  return p;
}

void ir_free(IrProgram *p) {
  if (!p) return;
  ir_node_free(p->root);
  free(p);
}

/* ── walk helpers ───────────────────────────────────────── */

IrNode *ir_find_child(const IrNode *n, IrKind kind) {
  if (!n) return NULL;
  for (size_t i = 0; i < n->n_kids; i++)
    if (n->kids[i] && n->kids[i]->kind == kind) return n->kids[i];
  return NULL;
}

IrNode *ir_find_hook(const IrNode *n, const char *hook_kind) {
  if (!n || !hook_kind) return NULL;
  for (size_t i = 0; i < n->n_kids; i++) {
    IrNode *k = n->kids[i];
    if (k && k->kind == IR_HOOK && k->name && strcmp(k->name, hook_kind) == 0)
      return k;
  }
  return NULL;
}

const char *ir_attr(const IrNode *n, const char *attr_name) {
  if (!n || !attr_name) return NULL;
  for (size_t i = 0; i < n->n_kids; i++) {
    IrNode *k = n->kids[i];
    if (k && k->kind == IR_ATTR && k->name && strcmp(k->name, attr_name) == 0)
      return k->value;
  }
  return NULL;
}

size_t ir_count_kind(const IrNode *n, IrKind kind) {
  size_t c = 0;
  if (!n) return 0;
  for (size_t i = 0; i < n->n_kids; i++)
    if (n->kids[i] && n->kids[i]->kind == kind) c++;
  return c;
}

Node *ir_origin(const IrNode *n) { return n ? n->origin : NULL; }

Node *ir_project_origin(const IrProgram *p) {
  if (!p || !p->root) return NULL;
  if (p->root->origin) return p->root->origin;
  /* Fallback: first kid origin */
  for (size_t i = 0; i < p->root->n_kids; i++) {
    if (p->root->kids[i] && p->root->kids[i]->origin)
      return p->root->kids[i]->origin;
  }
  return NULL;
}

/* ── dump ───────────────────────────────────────────────── */

typedef struct {
  char *data;
  size_t len;
  size_t cap;
} DumpBuf;

static void db_init(DumpBuf *b) {
  b->cap = 256;
  b->len = 0;
  b->data = malloc(b->cap);
  if (b->data) b->data[0] = '\0';
}

static void db_grow(DumpBuf *b, size_t need) {
  if (!b->data) return;
  if (b->len + need + 1 <= b->cap) return;
  while (b->len + need + 1 > b->cap) b->cap *= 2;
  char *nd = realloc(b->data, b->cap);
  if (nd) b->data = nd;
}

static void db_puts(DumpBuf *b, const char *s) {
  if (!s || !b->data) return;
  size_t n = strlen(s);
  db_grow(b, n);
  memcpy(b->data + b->len, s, n);
  b->len += n;
  b->data[b->len] = '\0';
}

static void db_printf(DumpBuf *b, const char *fmt, ...) {
  char tmp[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(tmp, sizeof(tmp), fmt, ap);
  va_end(ap);
  db_puts(b, tmp);
}

static void dump_node(DumpBuf *b, const IrNode *n, int depth) {
  if (!n) return;
  for (int i = 0; i < depth; i++) db_puts(b, "  ");

  db_puts(b, ir_kind_name(n->kind));

  switch (n->kind) {
    case IR_PROJECT:
      if (n->file) db_printf(b, " file=%s", n->file);
      break;
    case IR_COMPONENT:
    case IR_LAYOUT:
      if (n->name) db_printf(b, " %s", n->name);
      break;
    case IR_ROUTE:
      db_printf(b, " %s => %s", n->name ? n->name : "?",
                n->value ? n->value : "?");
      break;
    case IR_PROP:
      if (n->name && strcmp(n->name, "__props__") == 0)
        db_puts(b, "s");
      else {
        if (n->name) db_printf(b, " %s", n->name);
        if (n->value) db_printf(b, " = %s", n->value);
      }
      break;
    case IR_STATE:
      if (n->name && strcmp(n->name, "__states__") == 0)
        db_puts(b, "s");
      else {
        if (n->name) db_printf(b, " %s", n->name);
        if (n->value) db_printf(b, " = %s", n->value);
      }
      break;
    case IR_COMPUTED:
      if (n->name) db_printf(b, " %s", n->name);
      if (n->value) db_printf(b, " = %s", n->value);
      break;
    case IR_EFFECT:
      if (n->name) db_printf(b, " %s", n->name);
      if (n->value) db_printf(b, " deps=[%s]", n->value);
      if (n->value2) db_printf(b, " body=%s", n->value2);
      break;
    case IR_ELEMENT:
      if (n->name) db_printf(b, " <%s>", n->name);
      break;
    case IR_TEXT:
      db_printf(b, " \"%s\"", n->value ? n->value : "");
      break;
    case IR_INTERP:
      if (n->value)
        db_printf(b, " #{%s}", n->value);
      else
        db_puts(b, " (template)");
      break;
    case IR_IF:
      db_printf(b, " %s", n->value ? n->value : "?");
      break;
    case IR_FOR:
      db_printf(b, " %s in %s", n->name ? n->name : "?",
                n->value ? n->value : "?");
      break;
    case IR_EVENT:
      db_printf(b, " @%s = %s", n->name ? n->name : "?",
                n->value ? n->value : "?");
      break;
    case IR_ATTR:
      db_printf(b, " %s = %s", n->name ? n->name : "?",
                n->value ? n->value : "");
      break;
    case IR_SLOT:
      if (n->name) db_printf(b, " %s", n->name);
      break;
    case IR_FETCH:
      db_printf(b, " %s = %s", n->name ? n->name : "?",
                n->value ? n->value : "?");
      break;
    case IR_HOOK:
      db_printf(b, " %s", n->name ? n->name : "?");
      if (n->value) db_printf(b, " %s", n->value);
      if (n->value2) db_printf(b, " = %s", n->value2);
      break;
    case IR_MODULE_USE:
      db_printf(b, " %s", n->value ? n->value : "?");
      if (n->name) db_printf(b, " as %s", n->name);
      break;
    default:
      break;
  }

  if (n->line > 0) db_printf(b, "  @%d:%d", n->line, n->col);
  db_puts(b, "\n");

  for (size_t i = 0; i < n->n_kids; i++) dump_node(b, n->kids[i], depth + 1);
}

char *ir_dump(const IrProgram *p) {
  DumpBuf b;
  db_init(&b);
  if (!b.data) return NULL;
  if (!p || !p->root) {
    db_puts(&b, "(empty IR)\n");
    return b.data;
  }
  dump_node(&b, p->root, 0);
  return b.data;
}
