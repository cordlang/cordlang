#include "adapters/outbound/backends/html/html_backend.h"
#include "adapters/outbound/backends/ir_walk.h"
#include "adapters/outbound/backends/theme_css.h"
#include "application/ports/fs_port.h"
#include "domain/interp.h"
#include "domain/ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

typedef struct {
  char *buf;
  size_t len;
  size_t cap;
} StrBuf;

static void sb_init(StrBuf *sb) {
  sb->cap = 65536;
  sb->len = 0;
  sb->buf = calloc(sb->cap, 1);
}

static void sb_append(StrBuf *sb, const char *s) {
  if (!s) return;
  size_t slen = strlen(s);
  if (sb->len + slen + 1 >= sb->cap) {
    while (sb->len + slen + 1 >= sb->cap) sb->cap *= 2;
    sb->buf = realloc(sb->buf, sb->cap);
  }
  memcpy(sb->buf + sb->len, s, slen);
  sb->len += slen;
  sb->buf[sb->len] = '\0';
}

static void sb_appendf(StrBuf *sb, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int n = vsnprintf(NULL, 0, fmt, args);
  va_end(args);
  if (n < 0) return;

  if (sb->len + (size_t)n + 1 >= sb->cap) {
    while (sb->len + (size_t)n + 1 >= sb->cap) sb->cap *= 2;
    sb->buf = realloc(sb->buf, sb->cap);
  }

  va_start(args, fmt);
  vsnprintf(sb->buf + sb->len, sb->cap - sb->len, fmt, args);
  va_end(args);
  sb->len += (size_t)n;
  sb->buf[sb->len] = '\0';
}

static void sb_indent(StrBuf *sb, int depth) {
  for (int i = 0; i < depth; i++) sb_append(sb, "  ");
}

static void html_escape_append(StrBuf *sb, const char *s) {
  if (!s) return;
  for (const char *p = s; *p; p++) {
    switch (*p) {
      case '&': sb_append(sb, "&amp;"); break;
      case '<': sb_append(sb, "&lt;"); break;
      case '>': sb_append(sb, "&gt;"); break;
      case '"': sb_append(sb, "&quot;"); break;
      case '\'': sb_append(sb, "&#39;"); break;
      default: {
        char c[2] = {*p, 0};
        sb_append(sb, c);
        break;
      }
    }
  }
}

/* Runtime CSS: Tailwind-ish utilities so preview matches cord attrs without Node */
static const char *RUNTIME_CSS =
  "*,*::before,*::after{box-sizing:border-box}\n"
  "html,body{margin:0;padding:0;min-height:100%;"
  "font-family:Inter,ui-sans-serif,system-ui,-apple-system,Segoe UI,Roboto,Helvetica,Arial,sans-serif;"
  "background:var(--color-bg,#f8fafc);color:var(--color-text,#0f172a);-webkit-font-smoothing:antialiased}\n"
  "a{color:inherit;text-decoration:none}\n"
  "a:hover{opacity:.9}\n"
  "img{max-width:100%;display:block}\n"
  "button,input,textarea,select{font:inherit}\n"
  "hr{border:0;border-top:1px solid currentColor;opacity:.2;margin:.5rem 0}\n"
  "aside a{display:block;padding:.5rem .75rem;border-radius:.5rem;color:#e2e8f0;"
  "font-size:.9rem}\n"
  "aside a:hover{background:rgba(255,255,255,.08)}\n"
  "/* Cordlang runtime chrome */\n"
  ".cl-runtime-bar{position:sticky;top:0;z-index:9999;display:flex;align-items:center;"
  "justify-content:space-between;gap:12px;padding:8px 16px;background:#0f172a;color:#e2e8f0;"
  "font-size:13px;border-bottom:1px solid #1e293b}\n"
  ".cl-runtime-bar strong{color:#38bdf8;font-weight:600}\n"
  ".cl-runtime-bar span{opacity:.85}\n"
  ".cl-runtime-badge{display:inline-flex;align-items:center;gap:6px;padding:2px 10px;"
  "border-radius:999px;background:#1e293b;color:#7dd3fc;font-size:11px;letter-spacing:.02em;"
  "text-transform:uppercase}\n"
  ".cl-runtime-dot{width:7px;height:7px;border-radius:50%;background:#22c55e;"
  "box-shadow:0 0 0 3px rgba(34,197,94,.25)}\n"
  "/* Layout */\n"
  ".flex{display:flex}.flex-col{flex-direction:column}.flex-row{flex-direction:row}\n"
  ".flex-wrap{flex-wrap:wrap}.flex-1{flex:1 1 0%}.flex-2{flex:2 1 0%}\n"
  ".grid{display:grid}\n"
  ".items-center{align-items:center}.items-start{align-items:flex-start}\n"
  ".justify-center{justify-content:center}.justify-between{justify-content:space-between}\n"
  ".justify-around{justify-content:space-around}.justify-evenly{justify-content:space-evenly}\n"
  ".min-h-screen{min-height:100vh}.h-screen{height:100vh}\n"
  ".sticky{position:sticky}.top-0{top:0}\n"
  ".overflow-hidden{overflow:hidden}.overflow-y-auto{overflow-y:auto}\n"
  "/* Spacing — denser than stock Tailwind so cord numbers (p=20,gap=24) look sane */\n"
  ".p-0{padding:0}.p-2{padding:.25rem}.p-4{padding:.5rem}.p-6{padding:.75rem}\n"
  ".p-8{padding:.5rem}.p-12{padding:.75rem}.p-16{padding:1rem}.p-20{padding:1.25rem}.p-24{padding:1.5rem}\n"
  ".px-2{padding-left:.5rem;padding-right:.5rem}.px-4{padding-left:1rem;padding-right:1rem}\n"
  ".py-2{padding-top:.5rem;padding-bottom:.5rem}.py-4{padding-top:1rem;padding-bottom:1rem}\n"
  ".gap-0{gap:0}.gap-2{gap:.25rem}.gap-4{gap:.5rem}.gap-8{gap:.5rem}\n"
  ".gap-12{gap:.75rem}.gap-16{gap:1rem}.gap-20{gap:1.25rem}.gap-24{gap:1.5rem}\n"
  ".m-0{margin:0}\n"
  "/* Sizes: icon boxes ~px; sidebar w-64 stays wide */\n"
  ".w-8{width:0.5rem;min-width:0.5rem}.w-12{width:3rem}.w-16{width:4rem}.w-24{width:6rem}\n"
  ".w-32{width:8rem}.w-48{width:3rem;min-width:3rem}.w-56{width:14rem}.w-64{width:16rem;min-width:14rem}\n"
  ".h-8{height:0.5rem;min-height:0.5rem}.h-12{height:3rem}.h-16{height:4rem}.h-24{height:6rem}\n"
  ".h-32{height:8rem}.h-48{height:3rem;min-height:3rem}.h-64{height:16rem}\n"
  ".min-w-0{min-width:0}.w-full{width:100%}\n"
  "/* Text */\n"
  ".font-bold{font-weight:700}.font-medium{font-weight:500}\n"
  ".text-gray-500{color:#6b7280}.text-gray-700{color:#374151}.text-slate-200{color:#e2e8f0}\n"
  ".text-xs{font-size:.75rem}.text-sm{font-size:.875rem}.text-base{font-size:1rem}\n"
  ".text-lg{font-size:1.125rem}.text-xl{font-size:1.25rem}.text-2xl{font-size:1.5rem}\n"
  ".text-3xl{font-size:1.875rem}.text-4xl{font-size:2.25rem}\n"
  ".text-white{color:#fff}.text-primary{color:var(--color-primary,#2563eb)}\n"
  ".text-success{color:var(--color-success,#22c55e)}.text-warning{color:var(--color-warning,#f59e0b)}\n"
  ".text-muted{color:var(--color-muted,#94a3b8)}.text-status{color:var(--color-primary,#2563eb)}\n"
  "/* Colors / surfaces */\n"
  ".bg-white{background:#fff}.bg-gray-50{background:#f9fafb}.bg-gray-100{background:#f3f4f6}\n"
  ".bg-blue-600{background:#2563eb}.bg-primary{background:var(--color-primary,#6366f1)}\n"
  ".bg-success{background:var(--color-success,#22c55e)}.bg-warning{background:var(--color-warning,#f59e0b)}\n"
  ".bg-sidebar{background:var(--color-sidebar,#1e293b)}.bg-card{background:var(--color-card,#fff)}\n"
  ".bg-muted{background:var(--color-muted,#94a3b8)}\n"
  "/* Radius / shadow / border */\n"
  ".rounded-lg{border-radius:.5rem}.rounded-xl{border-radius:.75rem}.rounded-12{border-radius:12px}\n"
  ".rounded-full{border-radius:9999px}.rounded-theme{border-radius:var(--radius,12px)}\n"
  ".shadow-sm{box-shadow:0 1px 2px 0 rgba(0,0,0,.05)}\n"
  ".shadow-md{box-shadow:0 4px 6px -1px rgba(0,0,0,.1),0 2px 4px -2px rgba(0,0,0,.1)}\n"
  ".border{border:1px solid #e2e8f0}.border-b{border-bottom:1px solid #e2e8f0}\n"
  ".border-slate-200{border-color:#e2e8f0}.border-t{border-top:1px solid #e2e8f0}\n"
  ".opacity-20{opacity:.2}.opacity-50{opacity:.5}.opacity-80{opacity:.8}\n"
  "/* Grid cols */\n"
  ".grid-cols-1{grid-template-columns:repeat(1,minmax(0,1fr))}\n"
  ".grid-cols-2{grid-template-columns:repeat(2,minmax(0,1fr))}\n"
  ".grid-cols-3{grid-template-columns:repeat(3,minmax(0,1fr))}\n"
  ".grid-cols-4{grid-template-columns:repeat(4,minmax(0,1fr))}\n"
  ".max-w-640{max-width:40rem}.max-w-md{max-width:28rem}.max-w-lg{max-width:32rem}\n"
  ".max-w-xl{max-width:36rem}.max-w-2xl{max-width:42rem}\n"
  "/* Components */\n"
  ".btn{display:inline-flex;align-items:center;justify-content:center;border-radius:.5rem;"
  "padding:.5rem 1rem;font-weight:500;border:1px solid transparent;cursor:pointer;"
  "transition:background .15s,border-color .15s,color .15s,transform .05s;user-select:none}\n"
  ".btn:active{transform:translateY(1px)}\n"
  ".btn-primary{background:var(--color-primary,#2563eb);color:#fff}.btn-primary:hover{filter:brightness(.92)}\n"
  ".btn-outline{background:#fff;color:#111827;border-color:#d1d5db}.btn-outline:hover{background:#f9fafb}\n"
  ".btn-ghost{background:transparent;color:#374151}.btn-ghost:hover{background:#f3f4f6}\n"
  ".btn-secondary{background:#1f2937;color:#fff}.btn-secondary:hover{background:#111827}\n"
  ".card{border-radius:var(--radius,.75rem);background:var(--color-card,#fff);"
  "box-shadow:0 1px 3px rgba(15,23,42,.08),0 4px 12px rgba(15,23,42,.04);overflow:hidden}\n"
  "input,textarea,select{border:1px solid #d1d5db;border-radius:.5rem;padding:.5rem .75rem;"
  "background:#fff;min-width:12rem}\n"
  "input:focus,textarea:focus,select:focus{outline:2px solid #93c5fd;outline-offset:1px;"
  "border-color:#60a5fa}\n"
  "/* Demo / loop placeholders */\n"
  ".cl-loop{border:1px dashed #cbd5e1;border-radius:.75rem;padding:12px;background:"
  "repeating-linear-gradient(-45deg,#f8fafc,#f8fafc 8px,#f1f5f9 8px,#f1f5f9 16px)}\n"
  ".cl-loop-label{font-size:11px;color:#64748b;margin-bottom:8px;text-transform:uppercase;"
  "letter-spacing:.04em}\n"
  ".cl-toast{position:fixed;bottom:20px;right:20px;z-index:10000;background:#0f172a;color:#f8fafc;"
  "padding:10px 14px;border-radius:10px;font-size:13px;box-shadow:0 10px 25px rgba(0,0,0,.25);"
  "opacity:0;transform:translateY(8px);transition:opacity .2s,transform .2s;pointer-events:none}\n"
  ".cl-toast.show{opacity:1;transform:translateY(0)}\n"
  ".cl-interp{color:#0369a1;background:#e0f2fe;padding:0 .2em;border-radius:4px;"
  "font-family:ui-monospace,SFMono-Regular,Menlo,monospace;font-size:.92em}\n"
  "#app{min-height:calc(100vh - 40px)}\n"
  "@media (max-width:768px){.grid-cols-3,.grid-cols-4{grid-template-columns:1fr}"
  ".w-64{width:100%}.h-screen{height:auto;min-height:100vh}.flex-row{flex-wrap:wrap}}\n";

/* Base runtime: toast for unknown handlers; setX(...) is eval'd when state exists.
 * State decls + setters are prepended by html_generate when present. */
static const char *RUNTIME_JS_CORE =
  "function clUpdate(){\n"
  "  document.querySelectorAll('[data-bind]').forEach(function(el){\n"
  "    var expr=el.getAttribute('data-bind');\n"
  "    if(!expr) return;\n"
  "    try{\n"
  "      var v=(0,eval)(expr);\n"
  "      el.textContent=v==null?'':String(v);\n"
  "    }catch(e){ /* leave existing text */ }\n"
  "  });\n"
  "}\n"
  "function clPreviewHandler(name, ev){\n"
  "  if(ev && typeof ev.preventDefault==='function') ev.preventDefault();\n"
  "  /* C8: evaluate setCount(...)-style handlers against preview state */\n"
  "  if(name && /^set[A-Za-z_][\\w]*\\s*\\(/.test(name)){\n"
  "    try{ (0,eval)(name); return; }catch(e){\n"
  "      if(window.console) console.warn('[cordlang preview] handler failed', name, e);\n"
  "    }\n"
  "  }\n"
  "  var el=document.getElementById('cl-toast');\n"
  "  if(!el){ el=document.createElement('div'); el.id='cl-toast'; el.className='cl-toast';"
  " document.body.appendChild(el); }\n"
  "  el.textContent='@event -> '+name+' (preview runtime)';\n"
  "  el.classList.add('show');\n"
  "  clearTimeout(el._t);\n"
  "  el._t=setTimeout(function(){ el.classList.remove('show'); }, 1800);\n"
  "  if(window.console) console.log('[cordlang preview]', name, ev&&ev.type);\n"
  "}\n"
  "if(document.readyState==='loading'){\n"
  "  document.addEventListener('DOMContentLoaded', clUpdate);\n"
  "} else { clUpdate(); }\n";

typedef struct {
  const char *style_key;
  const char *tw_prefix;
  int is_value_appended;
} StyleMap;

static StyleMap style_mappings[] = {
  {"bg", "bg-", 1},
  {"text", "text-", 1},
  {"p", "p-", 1},
  {"px", "px-", 1},
  {"py", "py-", 1},
  {"m", "m-", 1},
  {"mx", "mx-", 1},
  {"my", "my-", 1},
  {"gap", "gap-", 1},
  {"rounded", "rounded-", 1},
  {"shadow", "shadow-", 1},
  {"size", "text-", 1},
  {"w", "w-", 1},
  {"h", "h-", 1},
  {"max-w", "max-w-", 1},
  {"z", "z-", 1},
  {"op", "opacity-", 1},
  {"leading", "leading-", 1},
  {"tracking", "tracking-", 1},
  {NULL, NULL, 0},
};

static void gen_style_classes(StrBuf *sb, Node *style_map) {
  for (size_t i = 0; i < style_map->children_len; i++) {
    Node *entry = style_map->children[i];
    if (entry->type != NODE_STYLE_ENTRY) continue;

    const char *key = entry->value ? entry->value : "";
    const char *val = entry->value2 ? entry->value2 : "";

    int mapped = 0;
    for (int s = 0; style_mappings[s].style_key; s++) {
      if (strcmp(key, style_mappings[s].style_key) == 0) {
        sb_append(sb, " ");
        sb_append(sb, style_mappings[s].tw_prefix);
        if (style_mappings[s].is_value_appended) sb_append(sb, val);
        mapped = 1;
        break;
      }
    }

    if (!mapped) {
      if (strcmp(key, "between") == 0) sb_append(sb, " justify-between");
      else if (strcmp(key, "center") == 0) sb_append(sb, " items-center justify-center");
      else if (strcmp(key, "around") == 0) sb_append(sb, " justify-around");
      else if (strcmp(key, "evenly") == 0) sb_append(sb, " justify-evenly");
      else if (strcmp(key, "sticky") == 0) sb_append(sb, " sticky top-0");
      else if (strcmp(key, "bold") == 0) sb_append(sb, " font-bold");
      else if (strcmp(key, "muted") == 0) sb_append(sb, " text-gray-500");
      else if (strcmp(key, "overflow") == 0) {
        sb_append(sb, " overflow-");
        sb_append(sb, val);
      }
    }
  }
}

static const char *tag_to_div_plus_class(const char *tag) {
  return irw_base_class(tag);
}

static const char *html_tag_for(const char *tag) {
  return irw_html_tag(tag);
}

static void gen_node(StrBuf *sb, Node *node, int depth);
static void gen_children(StrBuf *sb, Node *node, int depth);

/* Collected state names for live bindings (C8). Filled before gen. */
#define CL_MAX_STATE 64
static char *g_state_names[CL_MAX_STATE];
static char *g_state_inits[CL_MAX_STATE];
static int g_state_count;

/* Component prop scope stack (HTML preview expands PascalCase components). */
#define CL_MAX_PROPS 32
#define CL_MAX_PROP_DEPTH 16
#define CL_MAX_COMPS 64
typedef struct {
  char names[CL_MAX_PROPS][64];
  char values[CL_MAX_PROPS][256];
  int n;
} PropScope;
static PropScope g_prop_stack[CL_MAX_PROP_DEPTH];
static int g_prop_depth;
static IrNode *g_ir_comps[CL_MAX_COMPS];
static int g_n_ir_comps;
static IrNode *g_ir_layouts[16];
static int g_n_ir_layouts;
static IrNode *g_ir_routes[32];
static int g_n_ir_routes;
static IrNode *g_slot_content; /* page body injected into layout slot */
static int g_expand_depth;

static void prop_reset(void) {
  g_prop_depth = 0;
  g_slot_content = NULL;
  g_expand_depth = 0;
  g_n_ir_comps = g_n_ir_layouts = g_n_ir_routes = 0;
}

static const char *prop_lookup(const char *name) {
  if (!name || !*name || g_prop_depth <= 0) return NULL;
  for (int d = g_prop_depth - 1; d >= 0; d--) {
    PropScope *s = &g_prop_stack[d];
    for (int i = 0; i < s->n; i++) {
      if (strcmp(s->names[i], name) == 0) return s->values[i];
    }
  }
  return NULL;
}

/* Resolve attr value: if it names a prop in scope, use the prop value. */
static const char *resolve_attr_val(const char *val) {
  if (!val || !*val) return val;
  const char *p = prop_lookup(val);
  return p ? p : val;
}

static void prop_push_from_usage(IrNode *usage) {
  if (g_prop_depth >= CL_MAX_PROP_DEPTH || !usage) return;
  PropScope *s = &g_prop_stack[g_prop_depth++];
  s->n = 0;
  for (size_t i = 0; i < usage->n_kids; i++) {
    IrNode *c = usage->kids[i];
    if (!c || c->kind != IR_ATTR || !c->name) continue;
    if (c->name[0] == '_' && c->name[1] == '_') continue;
    if (s->n >= CL_MAX_PROPS) break;
    snprintf(s->names[s->n], sizeof(s->names[0]), "%s", c->name);
    snprintf(s->values[s->n], sizeof(s->values[0]), "%s",
             c->value ? c->value : "");
    s->n++;
  }
}

static void prop_pop(void) {
  if (g_prop_depth > 0) g_prop_depth--;
}

static void index_ir_project(IrNode *root) {
  g_n_ir_comps = g_n_ir_layouts = g_n_ir_routes = 0;
  if (!root) return;
  for (size_t i = 0; i < root->n_kids; i++) {
    IrNode *c = root->kids[i];
    if (!c) continue;
    if (c->kind == IR_COMPONENT && g_n_ir_comps < CL_MAX_COMPS)
      g_ir_comps[g_n_ir_comps++] = c;
    else if (c->kind == IR_LAYOUT && g_n_ir_layouts < 16)
      g_ir_layouts[g_n_ir_layouts++] = c;
    else if (c->kind == IR_ROUTE && g_n_ir_routes < 32)
      g_ir_routes[g_n_ir_routes++] = c;
  }
}

static IrNode *find_ir_component(const char *name) {
  if (!name) return NULL;
  for (int i = 0; i < g_n_ir_comps; i++) {
    if (g_ir_comps[i]->name && strcmp(g_ir_comps[i]->name, name) == 0)
      return g_ir_comps[i];
  }
  for (int i = 0; i < g_n_ir_layouts; i++) {
    if (g_ir_layouts[i]->name && strcmp(g_ir_layouts[i]->name, name) == 0)
      return g_ir_layouts[i];
  }
  return NULL;
}

static int name_is_page_like(const char *n) {
  if (!n) return 0;
  size_t len = strlen(n);
  if (len >= 4 && strcmp(n + len - 4, "Page") == 0) return 1;
  if (strcmp(n, "Dashboard") == 0 || strcmp(n, "Home") == 0 ||
      strcmp(n, "Settings") == 0 || strcmp(n, "App") == 0)
    return 1;
  return 0;
}

static int is_route_target(const char *name) {
  if (!name) return 0;
  for (int i = 0; i < g_n_ir_routes; i++) {
    if (g_ir_routes[i]->value && strcmp(g_ir_routes[i]->value, name) == 0)
      return 1;
  }
  return 0;
}

static void state_reset(void) {
  for (int i = 0; i < g_state_count; i++) {
    free(g_state_names[i]);
    free(g_state_inits[i]);
    g_state_names[i] = NULL;
    g_state_inits[i] = NULL;
  }
  g_state_count = 0;
}

static int state_has(const char *name) {
  if (!name) return 0;
  for (int i = 0; i < g_state_count; i++) {
    if (g_state_names[i] && strcmp(g_state_names[i], name) == 0) return 1;
  }
  return 0;
}

static void state_add(const char *name, const char *init) {
  if (!name || !*name || state_has(name) || g_state_count >= CL_MAX_STATE) return;
  g_state_names[g_state_count] = strdup(name);
  g_state_inits[g_state_count] = strdup(init && *init ? init : "0");
  g_state_count++;
}

static void collect_state_from_node(Node *node) {
  if (!node) return;
  if (node->type == NODE_STATE_DECL) {
    if (node->value) {
      state_add(node->value, node->value2);
    } else {
      for (size_t i = 0; i < node->children_len; i++) {
        Node *st = node->children[i];
        if (st && st->type == NODE_STATE_DECL && st->value)
          state_add(st->value, st->value2);
      }
    }
  }
  for (size_t i = 0; i < node->children_len; i++)
    collect_state_from_node(node->children[i]);
}

/* True if expr is a simple identifier that matches a known state var. */
static int is_simple_state_expr(const char *expr) {
  if (!expr || !*expr) return 0;
  const char *p = expr;
  while (*p && isspace((unsigned char)*p)) p++;
  if (!isalpha((unsigned char)*p) && *p != '_') return 0;
  const char *start = p;
  p++;
  while (*p && (isalnum((unsigned char)*p) || *p == '_')) p++;
  const char *end = p;
  while (*p && isspace((unsigned char)*p)) p++;
  if (*p) return 0; /* not a bare identifier */
  size_t n = (size_t)(end - start);
  char buf[128];
  if (n >= sizeof(buf)) return 0;
  memcpy(buf, start, n);
  buf[n] = '\0';
  return state_has(buf);
}

static const char *state_init_for(const char *name) {
  if (!name) return "0";
  for (int i = 0; i < g_state_count; i++) {
    if (g_state_names[i] && strcmp(g_state_names[i], name) == 0)
      return g_state_inits[i] ? g_state_inits[i] : "0";
  }
  return "0";
}

/* setter name: count → setCount */
static void make_setter_name(char *out, size_t out_sz, const char *name) {
  if (!out || out_sz < 4 || !name || !*name) {
    if (out && out_sz) out[0] = '\0';
    return;
  }
  snprintf(out, out_sz, "set%c%s", (char)toupper((unsigned char)name[0]),
           name + 1);
}

static void emit_state_runtime_js(StrBuf *sb) {
  if (g_state_count == 0) return;
  sb_append(sb, "/* Cordlang preview reactive state (C8) */\n");
  for (int i = 0; i < g_state_count; i++) {
    const char *name = g_state_names[i];
    const char *init = g_state_inits[i] ? g_state_inits[i] : "0";
    char setter[128];
    make_setter_name(setter, sizeof(setter), name);
    /* Use var so eval() handlers resolve state on the global object.
     * Init comes from the parser (number / true / "string" / expr). */
    sb_appendf(sb, "var %s = %s;\n", name, init);
    sb_appendf(sb,
               "function %s(v){ %s = (typeof v==='function') ? v(%s) : v; "
               "clUpdate(); }\n",
               setter, name, name);
  }
  sb_append(sb, "\n");
}

static void emit_bound_interp(StrBuf *sb, const char *expr) {
  /* Component prop (expanded usage): emit real value, no placeholder chrome */
  if (expr) {
    const char *pv = prop_lookup(expr);
    if (pv) {
      html_escape_append(sb, pv);
      return;
    }
  }
  /* data-bind live text for state vars; static placeholder otherwise */
  if (expr && is_simple_state_expr(expr)) {
    sb_append(sb, "<span class=\"cl-interp\" data-bind=\"");
    html_escape_append(sb, expr);
    sb_append(sb, "\"");
    const char *init = state_init_for(expr);
    sb_append(sb, ">");
    html_escape_append(sb, init);
    sb_append(sb, "</span>");
    return;
  }
  /* Unknown binding: still show readable placeholder, not raw ${} as only content */
  sb_append(sb, "<span class=\"cl-interp\" title=\"");
  html_escape_append(sb, expr ? expr : "");
  sb_append(sb, "\">");
  html_escape_append(sb, expr ? expr : "?");
  sb_append(sb, "</span>");
}

/* Mixed text with #{...}: live-bind known state, else static interp spans. */
static void emit_text_with_live_interp(StrBuf *sb, const char *str) {
  if (!str) return;
  if (!interp_has(str)) {
    html_escape_append(sb, str);
    return;
  }
  const char *p = str;
  while (*p) {
    const char *hash = NULL;
    for (const char *s = p; *s; s++) {
      if (s[0] == '#' && s[1] == '{') {
        hash = s;
        break;
      }
    }
    if (!hash) {
      html_escape_append(sb, p);
      break;
    }
    if (hash > p) {
      /* literal prefix */
      for (const char *s = p; s < hash; s++) {
        char c[2] = {*s, 0};
        if (*s == '&')
          sb_append(sb, "&amp;");
        else if (*s == '<')
          sb_append(sb, "&lt;");
        else if (*s == '>')
          sb_append(sb, "&gt;");
        else if (*s == '"')
          sb_append(sb, "&quot;");
        else
          sb_append(sb, c);
      }
    }
    /* find matching } */
    const char *q = hash + 2;
    int depth = 1;
    while (*q) {
      if (*q == '{')
        depth++;
      else if (*q == '}') {
        depth--;
        if (depth == 0) break;
      }
      q++;
    }
    if (!*q) {
      html_escape_append(sb, hash);
      break;
    }
    /* trim expr */
    const char *es = hash + 2;
    const char *ee = q;
    while (es < ee && isspace((unsigned char)*es)) es++;
    while (ee > es && isspace((unsigned char)ee[-1])) ee--;
    char expr[256];
    size_t elen = (size_t)(ee - es);
    if (elen >= sizeof(expr)) elen = sizeof(expr) - 1;
    memcpy(expr, es, elen);
    expr[elen] = '\0';
    emit_bound_interp(sb, expr);
    p = q + 1;
  }
}

/* Skip pure declaration nodes when rendering preview UI */
static int is_decl_only(NodeType t) {
  return t == NODE_STATE_DECL || t == NODE_PROPS_DECL || t == NODE_COMPUTED_DECL ||
         t == NODE_THEME || t == NODE_ROUTE || t == NODE_USE ||
         t == NODE_EFFECT_DECL || t == NODE_LAYOUT_EFFECT || t == NODE_REF_DECL ||
         t == NODE_CONTEXT_DECL || t == NODE_CONTEXT_USE || t == NODE_REDUCER_DECL ||
         t == NODE_PARAMS_DECL || t == NODE_NAVIGATE_DECL || t == NODE_CALLBACK_DECL ||
         t == NODE_ID_DECL || t == NODE_TRANSITION_DECL || t == NODE_DEFERRED_DECL ||
         t == NODE_ACTION_DECL || t == NODE_FETCH_DECL || t == NODE_LAZY_DECL ||
         t == NODE_STORE_DECL || t == NODE_SNIPPET || t == NODE_RENDER ||
         t == NODE_AWAIT || t == NODE_INSERTION_EFFECT || t == NODE_EFFECT_EVENT ||
         t == NODE_EXTERNAL_STORE || t == NODE_IMPERATIVE_HANDLE ||
         t == NODE_ATTR || t == NODE_EVENT || t == NODE_BOOL_ATTR ||
         t == NODE_STYLE_MAP || t == NODE_STYLE_ENTRY;
}

static void collect_classes(char *classes, size_t classes_sz, Node *node, const char *base_class) {
  classes[0] = '\0';
  if (base_class) {
    strncat(classes, base_class, classes_sz - 1);
  }

  for (size_t i = 0; i < node->children_len; i++) {
    Node *child = node->children[i];
    if (child->type == NODE_STYLE_MAP && child->value && strcmp(child->value, "style") == 0) {
      StrBuf style_sb = {0};
      style_sb.buf = malloc(1024);
      style_sb.cap = 1024;
      style_sb.len = 0;
      style_sb.buf[0] = '\0';
      gen_style_classes(&style_sb, child);
      if (style_sb.len > 0) {
        size_t room = classes_sz - strlen(classes) - 1;
        strncat(classes, style_sb.buf, room);
      }
      free(style_sb.buf);
    }
  }

  for (size_t i = 0; i < node->children_len; i++) {
    Node *child = node->children[i];
    char vbuf[96];
    if (child->type == NODE_BOOL_ATTR && child->value) {
      if (strcmp(child->value, "between") == 0) strncat(classes, " justify-between", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "center") == 0) strncat(classes, " items-center justify-center", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "around") == 0) strncat(classes, " justify-around", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "evenly") == 0) strncat(classes, " justify-evenly", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "bold") == 0) strncat(classes, " font-bold", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "muted") == 0) strncat(classes, " text-gray-500", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "sticky") == 0) strncat(classes, " sticky top-0", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "primary") == 0) strncat(classes, " btn-primary", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "outline") == 0) strncat(classes, " btn-outline", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "ghost") == 0) strncat(classes, " btn-ghost", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "secondary") == 0) strncat(classes, " btn-secondary", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "xs") == 0) strncat(classes, " text-xs", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "sm") == 0) strncat(classes, " text-sm", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "lg") == 0) strncat(classes, " text-lg", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "xl") == 0) strncat(classes, " text-xl", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "2xl") == 0) strncat(classes, " text-2xl", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "3xl") == 0) strncat(classes, " text-3xl", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "4xl") == 0) strncat(classes, " text-4xl", classes_sz - strlen(classes) - 1);
    }

    if (child->type == NODE_ATTR && child->value && child->value2) {
      if (strcmp(child->value, "variant") == 0) {
        snprintf(vbuf, sizeof(vbuf), " btn-%s", child->value2);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      } else if (strcmp(child->value, "size") == 0) {
        snprintf(vbuf, sizeof(vbuf), " text-%s", child->value2);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      } else if (strcmp(child->value, "color") == 0) {
        if (strcmp(child->value2, "primary") == 0)
          strncat(classes, " text-primary", classes_sz - strlen(classes) - 1);
        else {
          snprintf(vbuf, sizeof(vbuf), " text-%s", child->value2);
          strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
        }
      } else if (strcmp(child->value, "gap") == 0) {
        snprintf(vbuf, sizeof(vbuf), " gap-%s", child->value2);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      } else if (strcmp(child->value, "cols") == 0) {
        snprintf(vbuf, sizeof(vbuf), " grid-cols-%s", child->value2);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      } else if (strcmp(child->value, "p") == 0) {
        snprintf(vbuf, sizeof(vbuf), " p-%s", child->value2);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      } else if (strcmp(child->value, "bg") == 0) {
        snprintf(vbuf, sizeof(vbuf), " bg-%s", child->value2);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      } else if (strcmp(child->value, "shadow") == 0) {
        snprintf(vbuf, sizeof(vbuf), " shadow-%s", child->value2);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      } else if (strcmp(child->value, "rounded") == 0) {
        snprintf(vbuf, sizeof(vbuf), " rounded-%s", child->value2);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      } else if (strcmp(child->value, "max-w") == 0) {
        snprintf(vbuf, sizeof(vbuf), " max-w-%s", child->value2);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      }
    }
  }
}

static int is_style_attr(const char *name) {
  return strcmp(name, "variant") == 0 || strcmp(name, "size") == 0 ||
         strcmp(name, "color") == 0 || strcmp(name, "gap") == 0 ||
         strcmp(name, "cols") == 0 || strcmp(name, "p") == 0 ||
         strcmp(name, "bg") == 0 || strcmp(name, "shadow") == 0 ||
         strcmp(name, "rounded") == 0 || strcmp(name, "max-w") == 0 ||
         strcmp(name, "overflow") == 0 || strcmp(name, "fit") == 0 ||
         strcmp(name, "aspect") == 0 || strcmp(name, "lines") == 0;
}

static void gen_element(StrBuf *sb, Node *node, int depth) {
  const char *tag = node->value ? node->value : "div";
  const char *html_tag = html_tag_for(tag);
  const char *base_class = tag_to_div_plus_class(tag);

  if (!html_tag) {
    gen_children(sb, node, depth);
    return;
  }

  sb_indent(sb, depth);

  int self_closing = (strcmp(html_tag, "img") == 0 || strcmp(html_tag, "input") == 0);

  sb_appendf(sb, "<%s", html_tag);

  char classes[2048];
  collect_classes(classes, sizeof(classes), node, base_class);
  {
    char *cls = classes;
    while (*cls == ' ') cls++;
    if (*cls) {
      sb_append(sb, " class=\"");
      html_escape_append(sb, cls);
      sb_append(sb, "\"");
    }
  }

  /* Regular HTML attributes */
  for (size_t i = 0; i < node->children_len; i++) {
    Node *child = node->children[i];
    if (child->type == NODE_ATTR && child->value && !is_style_attr(child->value)) {
      const char *attr_name = child->value;
      if (strcmp(attr_name, "to") == 0) attr_name = "href";
      if (strcmp(attr_name, "src") == 0 || strcmp(attr_name, "alt") == 0 ||
          strcmp(attr_name, "href") == 0 || strcmp(attr_name, "placeholder") == 0 ||
          strcmp(attr_name, "type") == 0 || strcmp(attr_name, "rows") == 0 ||
          strcmp(attr_name, "name") == 0 || strcmp(attr_name, "value") == 0 ||
          strcmp(attr_name, "id") == 0) {
        sb_appendf(sb, " %s=\"", attr_name);
        html_escape_append(sb, child->value2 ? child->value2 : "");
        sb_append(sb, "\"");
      }
    }
  }

  /* checkbox / radio type */
  if (strcmp(tag, "checkbox") == 0) sb_append(sb, " type=\"checkbox\"");
  if (strcmp(tag, "radio") == 0) sb_append(sb, " type=\"radio\"");
  if (strcmp(tag, "input") == 0) {
    int has_type = 0;
    for (size_t i = 0; i < node->children_len; i++) {
      Node *c = node->children[i];
      if (c->type == NODE_ATTR && c->value && strcmp(c->value, "type") == 0) has_type = 1;
      /* shorthand: input text / input email as first child text? handled as attrs elsewhere */
      if (c->type == NODE_BOOL_ATTR && c->value) {
        if (strcmp(c->value, "text") == 0 || strcmp(c->value, "email") == 0 ||
            strcmp(c->value, "password") == 0 || strcmp(c->value, "search") == 0 ||
            strcmp(c->value, "number") == 0) {
          if (!has_type) {
            sb_appendf(sb, " type=\"%s\"", c->value);
            has_type = 1;
          }
        }
      }
    }
  }

  for (size_t i = 0; i < node->children_len; i++) {
    Node *child = node->children[i];
    if (child->type == NODE_BOOL_ATTR && child->value) {
      if (strcmp(child->value, "required") == 0 || strcmp(child->value, "disabled") == 0 ||
          strcmp(child->value, "readonly") == 0 || strcmp(child->value, "checked") == 0) {
        sb_appendf(sb, " %s", child->value);
      }
    }
  }

  /* Events → lightweight preview handlers */
  for (size_t i = 0; i < node->children_len; i++) {
    Node *child = node->children[i];
    if (child->type == NODE_EVENT && child->value && child->value2) {
      const char *event = child->value;
      const char *handler = child->value2;
      /* escape single quotes in handler name for JS string */
      sb_appendf(sb, " on%s=\"clPreviewHandler('", event);
      for (const char *p = handler; *p; p++) {
        if (*p == '\'' || *p == '\\') sb_append(sb, "\\");
        char c[2] = {*p, 0};
        if (*p != '\n' && *p != '\r') sb_append(sb, c);
      }
      sb_append(sb, "', event)\"");
    }
  }

  if (self_closing) {
    sb_append(sb, " />\n");
    return;
  }

  sb_append(sb, ">\n");
  gen_children(sb, node, depth + 1);
  sb_indent(sb, depth);
  sb_appendf(sb, "</%s>\n", html_tag);
}

static void gen_children(StrBuf *sb, Node *node, int depth) {
  for (size_t i = 0; i < node->children_len; i++) {
    Node *child = node->children[i];
    if (child->type == NODE_FOR) {
      const char *var = child->value ? child->value : "item";
      const char *list = child->value2 ? child->value2 : "items";
      sb_indent(sb, depth);
      sb_append(sb, "<div class=\"cl-loop\">\n");
      sb_indent(sb, depth + 1);
      sb_appendf(sb, "<div class=\"cl-loop-label\">for %s in %s — preview (2 samples)</div>\n",
                 var, list);
      /* Render body twice as demo samples */
      for (int sample = 0; sample < 2; sample++) {
        for (size_t j = 0; j < child->children_len; j++) {
          gen_node(sb, child->children[j], depth + 1);
        }
      }
      sb_indent(sb, depth);
      sb_append(sb, "</div>\n");
    } else if (child->type == NODE_IF) {
      const char *cond = child->value ? child->value : "true";
      sb_indent(sb, depth);
      sb_appendf(sb, "<!-- if %s (preview shows true branch) -->\n", cond);
      for (size_t j = 0; j < child->children_len; j++) {
        gen_node(sb, child->children[j], depth);
      }
      /* skip paired else marker + body for now (preview shows true branch) */
      if (i + 1 < node->children_len) {
        Node *next = node->children[i + 1];
        if (next->type == NODE_TEXT && next->value && strcmp(next->value, "__else__") == 0) {
          i++; /* skip marker; leave else body unrendered in preview */
          /* skip until non-else-related? keep simple: only skip marker */
        }
      }
    } else if (child->type == NODE_INTERPOLATION) {
      sb_indent(sb, depth);
      if (child->value && child->children_len == 0) {
        emit_bound_interp(sb, child->value);
        sb_append(sb, "\n");
      } else if (child->children_len > 0) {
        for (size_t j = 0; j < child->children_len; j++) {
          Node *c = child->children[j];
          if (c->type == NODE_TEXT && c->value) {
            html_escape_append(sb, c->value);
          } else if (c->type == NODE_INTERPOLATION && c->value) {
            emit_bound_interp(sb, c->value);
          }
        }
        sb_append(sb, "\n");
      }
    } else if (child->type == NODE_TEXT) {
      if (child->value && strcmp(child->value, "__else__") == 0) continue;
      sb_indent(sb, depth);
      if (child->value && interp_has(child->value)) {
        /* Prefer live data-bind spans for known state inside mixed text */
        emit_text_with_live_interp(sb, child->value);
      } else {
        html_escape_append(sb, child->value ? child->value : "");
      }
      sb_append(sb, "\n");
    } else if (child->type == NODE_STRING) {
      sb_indent(sb, depth);
      if (child->value && interp_has(child->value)) {
        emit_text_with_live_interp(sb, child->value);
      } else {
        html_escape_append(sb, child->value ? child->value : "");
      }
      sb_append(sb, "\n");
    } else if (!is_decl_only(child->type)) {
      gen_node(sb, child, depth);
    }
  }
}

static void gen_node(StrBuf *sb, Node *node, int depth) {
  if (!node) return;
  if (is_decl_only(node->type)) return;
  switch (node->type) {
    case NODE_ROOT:
      gen_children(sb, node, depth);
      break;
    case NODE_COMPONENT_DEF:
      /* Render component body (state/props skipped via is_decl_only) */
      gen_children(sb, node, depth);
      break;
    case NODE_ELEMENT:
      gen_element(sb, node, depth);
      break;
    case NODE_INTERPOLATION:
      sb_indent(sb, depth);
      if (node->value && node->children_len == 0) {
        emit_bound_interp(sb, node->value);
        sb_append(sb, "\n");
      } else {
        gen_children(sb, node, depth);
      }
      break;
    case NODE_TEXT:
    case NODE_STRING:
      if (node->value && strcmp(node->value, "__else__") != 0) {
        sb_indent(sb, depth);
        if (interp_has(node->value)) {
          emit_text_with_live_interp(sb, node->value);
        } else {
          html_escape_append(sb, node->value);
        }
        sb_append(sb, "\n");
      }
      break;
    default:
      gen_children(sb, node, depth);
      break;
  }
}

static char *html_generate_impl(Node *root) {
  state_reset();
  collect_state_from_node(root);

  StrBuf body;
  sb_init(&body);
  gen_node(&body, root, 2);

  StrBuf doc;
  sb_init(&doc);
  sb_append(&doc,
    "<!DOCTYPE html>\n"
    "<html lang=\"en\">\n"
    "<head>\n"
    "  <meta charset=\"UTF-8\" />\n"
    "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" />\n"
    "  <title>Cordlang Runtime Preview</title>\n"
    "  <style>\n");
  sb_append(&doc, RUNTIME_CSS);
  sb_append(&doc,
    "  </style>\n"
    "</head>\n"
    "<body>\n"
    "  <div class=\"cl-runtime-bar\">\n"
    "    <div><strong>Cordlang</strong> <span>native runtime preview</span></div>\n"
    "    <div class=\"cl-runtime-badge\"><span class=\"cl-runtime-dot\"></span> live · no React/Node</div>\n"
    "  </div>\n"
    "  <div id=\"app\">\n");
  sb_append(&doc, body.buf ? body.buf : "");
  sb_append(&doc,
    "  </div>\n"
    "  <div id=\"cl-toast\" class=\"cl-toast\"></div>\n"
    "  <script>\n");
  emit_state_runtime_js(&doc);
  sb_append(&doc, RUNTIME_JS_CORE);
  sb_append(&doc,
    "  </script>\n"
    "</body>\n"
    "</html>\n");

  free(body.buf);
  state_reset();
  return doc.buf;
}

/* ── Pure IR walk (G2) — no origin/AST for body or state ─────────── */

static void gen_ir_node(StrBuf *sb, IrNode *node, int depth);
static void gen_ir_children(StrBuf *sb, IrNode *node, int depth);

static int ir_attr_is_true(const char *v) {
  return v && strcmp(v, "true") == 0;
}

static void collect_state_from_ir(IrNode *node) {
  if (!node) return;
  if (node->kind == IR_STATE) {
    if (node->name && strcmp(node->name, "__states__") == 0) {
      for (size_t i = 0; i < node->n_kids; i++) {
        IrNode *st = node->kids[i];
        if (st && st->kind == IR_STATE && st->name)
          state_add(st->name, st->value);
      }
    } else if (node->name) {
      state_add(node->name, node->value);
    }
  }
  for (size_t i = 0; i < node->n_kids; i++)
    collect_state_from_ir(node->kids[i]);
}

/* Declarative IR nodes skipped when rendering UI siblings. */
static int is_ir_decl_only(const IrNode *n) {
  if (!n) return 1;
  switch (n->kind) {
    case IR_STATE:
    case IR_PROP:
    case IR_COMPUTED:
    case IR_EFFECT:
    case IR_FETCH:
    case IR_MODULE_USE:
    case IR_STORE:
    case IR_SNIPPET:
    case IR_RENDER:
    case IR_AWAIT:
    case IR_ROUTE:
    case IR_ATTR:
    case IR_EVENT:
      return 1;
    case IR_HOOK:
      /* Structural hooks render; pure declarations skip */
      if (!n->name) return 1;
      if (strcmp(n->name, "portal") == 0 || strcmp(n->name, "empty") == 0 ||
          strcmp(n->name, "suspense") == 0 || strcmp(n->name, "loading") == 0 ||
          strcmp(n->name, "errorBoundary") == 0)
        return 0;
      return 1;
    default:
      return 0;
  }
}

static void gen_style_classes_ir(StrBuf *sb, IrNode *style_map) {
  if (!style_map) return;
  for (size_t i = 0; i < style_map->n_kids; i++) {
    IrNode *entry = style_map->kids[i];
    if (!entry || entry->kind != IR_ATTR) continue;

    const char *key = entry->name ? entry->name : "";
    const char *val = entry->value ? entry->value : "";

    int mapped = 0;
    for (int s = 0; style_mappings[s].style_key; s++) {
      if (strcmp(key, style_mappings[s].style_key) == 0) {
        sb_append(sb, " ");
        sb_append(sb, style_mappings[s].tw_prefix);
        if (style_mappings[s].is_value_appended) sb_append(sb, val);
        mapped = 1;
        break;
      }
    }

    if (!mapped) {
      if (strcmp(key, "between") == 0)
        sb_append(sb, " justify-between");
      else if (strcmp(key, "center") == 0)
        sb_append(sb, " items-center justify-center");
      else if (strcmp(key, "around") == 0)
        sb_append(sb, " justify-around");
      else if (strcmp(key, "evenly") == 0)
        sb_append(sb, " justify-evenly");
      else if (strcmp(key, "sticky") == 0)
        sb_append(sb, " sticky top-0");
      else if (strcmp(key, "bold") == 0)
        sb_append(sb, " font-bold");
      else if (strcmp(key, "muted") == 0)
        sb_append(sb, " text-gray-500");
      else if (strcmp(key, "overflow") == 0) {
        sb_append(sb, " overflow-");
        sb_append(sb, val);
      }
    }
  }
}

static void cl_cat(char *classes, size_t classes_sz, const char *piece) {
  if (!piece || !*piece) return;
  size_t room = classes_sz - strlen(classes) - 1;
  if (room > 0) strncat(classes, piece, room);
}

static void collect_classes_ir(char *classes, size_t classes_sz, IrNode *node,
                               const char *base_class) {
  classes[0] = '\0';
  if (base_class) cl_cat(classes, classes_sz, base_class);

  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *child = node->kids[i];
    if (child && child->kind == IR_ATTR && child->name &&
        strcmp(child->name, "style") == 0) {
      StrBuf style_sb = {0};
      style_sb.buf = malloc(1024);
      style_sb.cap = 1024;
      style_sb.len = 0;
      style_sb.buf[0] = '\0';
      gen_style_classes_ir(&style_sb, child);
      if (style_sb.len > 0) cl_cat(classes, classes_sz, style_sb.buf);
      free(style_sb.buf);
    }
  }

  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *child = node->kids[i];
    char vbuf[128];
    if (!child || child->kind != IR_ATTR || !child->name) continue;
    if (child->name[0] == '_' && child->name[1] == '_') continue;

    const char *k = child->name;
    const char *raw = child->value ? child->value : "";
    const char *v = resolve_attr_val(raw);

    /* Bool-like attrs: IR_ATTR name with value "true" (or empty) */
    if (ir_attr_is_true(raw) || raw[0] == '\0') {
      if (strcmp(k, "between") == 0)
        cl_cat(classes, classes_sz, " justify-between");
      else if (strcmp(k, "center") == 0)
        cl_cat(classes, classes_sz, " items-center justify-center");
      else if (strcmp(k, "around") == 0)
        cl_cat(classes, classes_sz, " justify-around");
      else if (strcmp(k, "evenly") == 0)
        cl_cat(classes, classes_sz, " justify-evenly");
      else if (strcmp(k, "bold") == 0)
        cl_cat(classes, classes_sz, " font-bold");
      else if (strcmp(k, "muted") == 0)
        cl_cat(classes, classes_sz, " text-gray-500");
      else if (strcmp(k, "sticky") == 0)
        cl_cat(classes, classes_sz, " sticky top-0");
      else if (strcmp(k, "wrap") == 0)
        cl_cat(classes, classes_sz, " flex-wrap");
      else if (strcmp(k, "col") == 0)
        cl_cat(classes, classes_sz, " flex flex-col");
      else if (strcmp(k, "row") == 0)
        cl_cat(classes, classes_sz, " flex flex-row");
      else if (strcmp(k, "h-screen") == 0)
        cl_cat(classes, classes_sz, " h-screen");
      else if (strcmp(k, "min-h-screen") == 0)
        cl_cat(classes, classes_sz, " min-h-screen");
      else if (strcmp(k, "primary") == 0)
        cl_cat(classes, classes_sz, " btn-primary");
      else if (strcmp(k, "outline") == 0)
        cl_cat(classes, classes_sz, " btn-outline");
      else if (strcmp(k, "ghost") == 0)
        cl_cat(classes, classes_sz, " btn-ghost");
      else if (strcmp(k, "secondary") == 0)
        cl_cat(classes, classes_sz, " btn-secondary");
      else if (strcmp(k, "xs") == 0)
        cl_cat(classes, classes_sz, " text-xs");
      else if (strcmp(k, "sm") == 0)
        cl_cat(classes, classes_sz, " text-sm");
      else if (strcmp(k, "lg") == 0)
        cl_cat(classes, classes_sz, " text-lg");
      else if (strcmp(k, "xl") == 0)
        cl_cat(classes, classes_sz, " text-xl");
      else if (strcmp(k, "2xl") == 0)
        cl_cat(classes, classes_sz, " text-2xl");
      else if (strcmp(k, "3xl") == 0)
        cl_cat(classes, classes_sz, " text-3xl");
      else if (strcmp(k, "4xl") == 0)
        cl_cat(classes, classes_sz, " text-4xl");
      continue;
    }

    if (!v || !*v) continue;

    if (strcmp(k, "variant") == 0) {
      snprintf(vbuf, sizeof(vbuf), " btn-%s", v);
      cl_cat(classes, classes_sz, vbuf);
    } else if (strcmp(k, "size") == 0) {
      snprintf(vbuf, sizeof(vbuf), " text-%s", v);
      cl_cat(classes, classes_sz, vbuf);
    } else if (strcmp(k, "color") == 0) {
      snprintf(vbuf, sizeof(vbuf), " text-%s", v);
      cl_cat(classes, classes_sz, vbuf);
    } else if (strcmp(k, "gap") == 0) {
      snprintf(vbuf, sizeof(vbuf), " gap-%s", v);
      cl_cat(classes, classes_sz, vbuf);
    } else if (strcmp(k, "cols") == 0) {
      snprintf(vbuf, sizeof(vbuf), " grid-cols-%s", v);
      cl_cat(classes, classes_sz, vbuf);
    } else if (strcmp(k, "p") == 0) {
      snprintf(vbuf, sizeof(vbuf), " p-%s", v);
      cl_cat(classes, classes_sz, vbuf);
    } else if (strcmp(k, "px") == 0) {
      snprintf(vbuf, sizeof(vbuf), " px-%s", v);
      cl_cat(classes, classes_sz, vbuf);
    } else if (strcmp(k, "py") == 0) {
      snprintf(vbuf, sizeof(vbuf), " py-%s", v);
      cl_cat(classes, classes_sz, vbuf);
    } else if (strcmp(k, "bg") == 0) {
      snprintf(vbuf, sizeof(vbuf), " bg-%s", v);
      cl_cat(classes, classes_sz, vbuf);
    } else if (strcmp(k, "shadow") == 0) {
      snprintf(vbuf, sizeof(vbuf), " shadow-%s", v);
      cl_cat(classes, classes_sz, vbuf);
    } else if (strcmp(k, "rounded") == 0) {
      snprintf(vbuf, sizeof(vbuf), " rounded-%s", v);
      cl_cat(classes, classes_sz, vbuf);
    } else if (strcmp(k, "max-w") == 0) {
      snprintf(vbuf, sizeof(vbuf), " max-w-%s", v);
      cl_cat(classes, classes_sz, vbuf);
    } else if (strcmp(k, "w") == 0) {
      snprintf(vbuf, sizeof(vbuf), " w-%s", v);
      cl_cat(classes, classes_sz, vbuf);
    } else if (strcmp(k, "h") == 0) {
      snprintf(vbuf, sizeof(vbuf), " h-%s", v);
      cl_cat(classes, classes_sz, vbuf);
    } else if (strcmp(k, "flex") == 0) {
      snprintf(vbuf, sizeof(vbuf), " flex-%s", v);
      cl_cat(classes, classes_sz, vbuf);
    } else if (strcmp(k, "justify") == 0) {
      snprintf(vbuf, sizeof(vbuf), " justify-%s", v);
      cl_cat(classes, classes_sz, vbuf);
    } else if (strcmp(k, "items") == 0) {
      snprintf(vbuf, sizeof(vbuf), " items-%s", v);
      cl_cat(classes, classes_sz, vbuf);
    } else if (strcmp(k, "overflow") == 0) {
      snprintf(vbuf, sizeof(vbuf), " overflow-%s", v);
      cl_cat(classes, classes_sz, vbuf);
    } else if (strcmp(k, "overflow-y") == 0) {
      snprintf(vbuf, sizeof(vbuf), " overflow-y-%s", v);
      cl_cat(classes, classes_sz, vbuf);
    } else if (strcmp(k, "opacity") == 0) {
      snprintf(vbuf, sizeof(vbuf), " opacity-%s", v);
      cl_cat(classes, classes_sz, vbuf);
    } else if (strcmp(k, "border") == 0) {
      snprintf(vbuf, sizeof(vbuf), " border border-%s", v);
      cl_cat(classes, classes_sz, vbuf);
    } else if (strcmp(k, "border-b") == 0) {
      cl_cat(classes, classes_sz, " border-b");
    }
  }
}

static void gen_ir_element(StrBuf *sb, IrNode *node, int depth) {
  const char *tag = node->name ? node->name : "div";

  /* slot element → inject current page/layout slot content */
  if (strcmp(tag, "slot") == 0) {
    if (g_slot_content) {
      gen_ir_children(sb, g_slot_content, depth);
    } else {
      sb_indent(sb, depth);
      sb_append(sb, "<!-- slot -->\n");
    }
    return;
  }

  /* PascalCase → expand component definition with props as scope */
  if (irw_is_pascal(tag)) {
    IrNode *def = find_ir_component(tag);
    if (def && g_expand_depth < 32) {
      g_expand_depth++;
      prop_push_from_usage(node);
      gen_ir_children(sb, def, depth);
      prop_pop();
      g_expand_depth--;
      return;
    }
    /* Unknown component: render kids only (avoid empty custom tags) */
    sb_indent(sb, depth);
    sb_appendf(sb, "<!-- component %s not found -->\n", tag);
    gen_ir_children(sb, node, depth);
    return;
  }

  const char *html_tag = html_tag_for(tag);
  const char *base_class = tag_to_div_plus_class(tag);

  if (!html_tag) {
    gen_ir_children(sb, node, depth);
    return;
  }

  sb_indent(sb, depth);

  int self_closing = irw_is_void_html(html_tag);

  sb_appendf(sb, "<%s", html_tag);

  char classes[2048];
  collect_classes_ir(classes, sizeof(classes), node, base_class);
  {
    char *cls = classes;
    while (*cls == ' ') cls++;
    if (*cls) {
      sb_append(sb, " class=\"");
      html_escape_append(sb, cls);
      sb_append(sb, "\"");
    }
  }

  /* Regular HTML attributes */
  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *child = node->kids[i];
    if (child && child->kind == IR_ATTR && child->name &&
        !is_style_attr(child->name) && !ir_attr_is_true(child->value) &&
        strcmp(child->name, "style") != 0 &&
        strcmp(child->name, "__file__") != 0) {
      const char *attr_name = child->name;
      if (strcmp(attr_name, "to") == 0) attr_name = "href";
      /* Skip layout/style-ish attrs already mapped to classes */
      if (strcmp(attr_name, "w") == 0 || strcmp(attr_name, "h") == 0 ||
          strcmp(attr_name, "flex") == 0 || strcmp(attr_name, "justify") == 0 ||
          strcmp(attr_name, "items") == 0 || strcmp(attr_name, "opacity") == 0 ||
          strcmp(attr_name, "overflow-y") == 0 ||
          strcmp(attr_name, "border") == 0 ||
          strcmp(attr_name, "border-b") == 0 ||
          strcmp(attr_name, "icon") == 0)
        continue;
      if (strcmp(attr_name, "src") == 0 || strcmp(attr_name, "alt") == 0 ||
          strcmp(attr_name, "href") == 0 ||
          strcmp(attr_name, "placeholder") == 0 ||
          strcmp(attr_name, "type") == 0 || strcmp(attr_name, "rows") == 0 ||
          strcmp(attr_name, "name") == 0 || strcmp(attr_name, "value") == 0 ||
          strcmp(attr_name, "id") == 0) {
        sb_appendf(sb, " %s=\"", attr_name);
        html_escape_append(sb, child->value ? child->value : "");
        sb_append(sb, "\"");
      }
    }
  }

  if (strcmp(tag, "checkbox") == 0) sb_append(sb, " type=\"checkbox\"");
  if (strcmp(tag, "radio") == 0) sb_append(sb, " type=\"radio\"");
  if (strcmp(tag, "input") == 0) {
    int has_type = 0;
    for (size_t i = 0; i < node->n_kids; i++) {
      IrNode *c = node->kids[i];
      if (c && c->kind == IR_ATTR && c->name && strcmp(c->name, "type") == 0)
        has_type = 1;
      if (c && c->kind == IR_ATTR && c->name && ir_attr_is_true(c->value)) {
        if (strcmp(c->name, "text") == 0 || strcmp(c->name, "email") == 0 ||
            strcmp(c->name, "password") == 0 ||
            strcmp(c->name, "search") == 0 || strcmp(c->name, "number") == 0) {
          if (!has_type) {
            sb_appendf(sb, " type=\"%s\"", c->name);
            has_type = 1;
          }
        }
      }
    }
  }

  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *child = node->kids[i];
    if (child && child->kind == IR_ATTR && child->name &&
        ir_attr_is_true(child->value)) {
      if (strcmp(child->name, "required") == 0 ||
          strcmp(child->name, "disabled") == 0 ||
          strcmp(child->name, "readonly") == 0 ||
          strcmp(child->name, "checked") == 0) {
        sb_appendf(sb, " %s", child->name);
      }
    }
  }

  /* Events → clPreviewHandler (setCount etc.) */
  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *child = node->kids[i];
    if (child && child->kind == IR_EVENT && child->name && child->value) {
      const char *event = child->name;
      const char *handler = child->value;
      sb_appendf(sb, " on%s=\"clPreviewHandler('", event);
      for (const char *p = handler; *p; p++) {
        if (*p == '\'' || *p == '\\') sb_append(sb, "\\");
        char c[2] = {*p, 0};
        if (*p != '\n' && *p != '\r') sb_append(sb, c);
      }
      sb_append(sb, "', event)\"");
    }
  }

  if (self_closing) {
    sb_append(sb, " />\n");
    return;
  }

  sb_append(sb, ">\n");
  gen_ir_children(sb, node, depth + 1);
  sb_indent(sb, depth);
  sb_appendf(sb, "</%s>\n", html_tag);
}

static void gen_ir_children(StrBuf *sb, IrNode *node, int depth) {
  if (!node) return;
  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *child = node->kids[i];
    if (!child) continue;

    if (child->kind == IR_FOR) {
      const char *var = child->name ? child->name : "item";
      const char *list = child->value ? child->value : "items";
      sb_indent(sb, depth);
      sb_append(sb, "<div class=\"cl-loop\">\n");
      sb_indent(sb, depth + 1);
      sb_appendf(sb,
                 "<div class=\"cl-loop-label\">for %s in %s — preview (2 "
                 "samples)</div>\n",
                 var, list);
      for (int sample = 0; sample < 2; sample++) {
        for (size_t j = 0; j < child->n_kids; j++)
          gen_ir_node(sb, child->kids[j], depth + 1);
      }
      sb_indent(sb, depth);
      sb_append(sb, "</div>\n");
    } else if (child->kind == IR_IF) {
      const char *cond = child->value ? child->value : "true";
      sb_indent(sb, depth);
      sb_appendf(sb, "<!-- if %s (preview shows true branch) -->\n", cond);
      for (size_t j = 0; j < child->n_kids; j++) {
        IrNode *kc = child->kids[j];
        if (kc && kc->kind == IR_TEXT && kc->value &&
            strcmp(kc->value, "__else__") == 0)
          break; /* stop at else marker — true branch only */
        gen_ir_node(sb, kc, depth);
      }
      /* skip paired else marker sibling if present under parent */
      if (i + 1 < node->n_kids) {
        IrNode *next = node->kids[i + 1];
        if (next && next->kind == IR_TEXT && next->value &&
            strcmp(next->value, "__else__") == 0) {
          i++; /* skip marker; leave else body unrendered */
        }
      }
    } else if (child->kind == IR_INTERP) {
      sb_indent(sb, depth);
      if (child->value && child->n_kids == 0) {
        emit_bound_interp(sb, child->value);
        sb_append(sb, "\n");
      } else if (child->n_kids > 0) {
        for (size_t j = 0; j < child->n_kids; j++) {
          IrNode *c = child->kids[j];
          if (!c) continue;
          if (c->kind == IR_TEXT && c->value) {
            html_escape_append(sb, c->value);
          } else if (c->kind == IR_INTERP && c->value) {
            emit_bound_interp(sb, c->value);
          }
        }
        sb_append(sb, "\n");
      } else {
        emit_bound_interp(sb, child->value);
        sb_append(sb, "\n");
      }
    } else if (child->kind == IR_TEXT) {
      if (child->value && strcmp(child->value, "__else__") == 0) continue;
      sb_indent(sb, depth);
      if (child->value && interp_has(child->value)) {
        emit_text_with_live_interp(sb, child->value);
      } else {
        html_escape_append(sb, child->value ? child->value : "");
      }
      sb_append(sb, "\n");
    } else if (child->kind == IR_SLOT) {
      if (g_slot_content) {
        gen_ir_children(sb, g_slot_content, depth);
      } else {
        sb_indent(sb, depth);
        sb_append(sb, "<!-- slot");
        if (child->name) sb_appendf(sb, " %s", child->name);
        sb_append(sb, " -->\n");
        gen_ir_children(sb, child, depth);
      }
    } else if (!is_ir_decl_only(child)) {
      gen_ir_node(sb, child, depth);
    }
  }
}

/* Compose layout + first route page; expand components with props. */
static void gen_ir_project_preview(StrBuf *sb, IrNode *root, int depth) {
  index_ir_project(root);

  IrNode *page = NULL;
  if (g_n_ir_routes > 0 && g_ir_routes[0]->value)
    page = find_ir_component(g_ir_routes[0]->value);

  if (!page) {
    for (int i = 0; i < g_n_ir_comps; i++) {
      if (is_route_target(g_ir_comps[i]->name) ||
          name_is_page_like(g_ir_comps[i]->name)) {
        page = g_ir_comps[i];
        break;
      }
    }
  }
  if (!page) {
    /* Prefer last page-like; else last component (pages often load last) */
    for (int i = g_n_ir_comps - 1; i >= 0; i--) {
      if (name_is_page_like(g_ir_comps[i]->name)) {
        page = g_ir_comps[i];
        break;
      }
    }
  }

  IrNode *layout = g_n_ir_layouts > 0 ? g_ir_layouts[0] : NULL;

  if (layout) {
    g_slot_content = page;
    gen_ir_children(sb, layout, depth);
    g_slot_content = NULL;
  } else if (page) {
    gen_ir_children(sb, page, depth);
  } else {
    /* Single-file / no multi-module: emit markup kids only (not defs) */
    for (size_t i = 0; i < root->n_kids; i++) {
      IrNode *c = root->kids[i];
      if (!c) continue;
      if (c->kind == IR_COMPONENT || c->kind == IR_LAYOUT ||
          c->kind == IR_ROUTE || is_ir_decl_only(c))
        continue;
      gen_ir_node(sb, c, depth);
    }
    /* If everything was a single component body without routes */
    if (g_n_ir_comps == 1 && g_n_ir_layouts == 0)
      gen_ir_children(sb, g_ir_comps[0], depth);
  }
}

static void gen_ir_node(StrBuf *sb, IrNode *node, int depth) {
  if (!node) return;
  if (is_ir_decl_only(node) && node->kind != IR_HOOK) return;

  switch (node->kind) {
    case IR_PROJECT:
      gen_ir_project_preview(sb, node, depth);
      break;
    case IR_COMPONENT:
    case IR_LAYOUT:
      gen_ir_children(sb, node, depth);
      break;
    case IR_ELEMENT:
      gen_ir_element(sb, node, depth);
      break;
    case IR_INTERP:
      sb_indent(sb, depth);
      if (node->value && node->n_kids == 0) {
        emit_bound_interp(sb, node->value);
        sb_append(sb, "\n");
      } else {
        gen_ir_children(sb, node, depth);
      }
      break;
    case IR_TEXT:
      if (node->value && strcmp(node->value, "__else__") != 0) {
        sb_indent(sb, depth);
        if (interp_has(node->value)) {
          emit_text_with_live_interp(sb, node->value);
        } else {
          html_escape_append(sb, node->value);
        }
        sb_append(sb, "\n");
      }
      break;
    case IR_SLOT:
      if (g_slot_content) {
        gen_ir_children(sb, g_slot_content, depth);
      } else {
        sb_indent(sb, depth);
        sb_append(sb, "<!-- slot");
        if (node->name) sb_appendf(sb, " %s", node->name);
        sb_append(sb, " -->\n");
        gen_ir_children(sb, node, depth);
      }
      break;
    case IR_IF:
    case IR_FOR: {
      /* Top-level: wrap via gen_ir_children sibling logic */
      IrNode fake;
      memset(&fake, 0, sizeof(fake));
      IrNode *kids[1] = {node};
      fake.kids = kids;
      fake.n_kids = 1;
      gen_ir_children(sb, &fake, depth);
      break;
    }
    case IR_HOOK:
      if (node->name &&
          (strcmp(node->name, "portal") == 0 ||
           strcmp(node->name, "empty") == 0 ||
           strcmp(node->name, "suspense") == 0 ||
           strcmp(node->name, "loading") == 0 ||
           strcmp(node->name, "errorBoundary") == 0)) {
        sb_indent(sb, depth);
        if (strcmp(node->name, "portal") == 0) {
          sb_appendf(sb, "<!-- portal %s -->\n",
                     node->value ? node->value : "document.body");
        } else if (strcmp(node->name, "empty") == 0) {
          sb_appendf(sb, "<!-- empty when %s -->\n",
                     node->value ? node->value : "?");
        } else {
          sb_appendf(sb, "<!-- %s -->\n", node->name);
        }
        for (size_t i = 0; i < node->n_kids; i++) {
          IrNode *c = node->kids[i];
          if (!c || c->kind == IR_ATTR) continue;
          if (c->kind == IR_ELEMENT && c->name &&
              strcmp(c->name, "__fallback__") == 0)
            continue;
          gen_ir_node(sb, c, depth);
        }
      }
      break;
    default:
      gen_ir_children(sb, node, depth);
      break;
  }
}

/* Build full HTML document walking ir->root only (no origin/AST). */
static char *html_generate_from_ir_program(IrProgram *ir) {
  IrNode *root = ir && ir->root ? ir->root : NULL;
  state_reset();
  prop_reset();
  if (root) {
    index_ir_project(root);
    collect_state_from_ir(root);
  }

  StrBuf body;
  sb_init(&body);
  if (root) gen_ir_node(&body, root, 2);

  StrBuf doc;
  sb_init(&doc);
  sb_append(&doc,
            "<!DOCTYPE html>\n"
            "<html lang=\"en\">\n"
            "<head>\n"
            "  <meta charset=\"UTF-8\" />\n"
            "  <meta name=\"viewport\" content=\"width=device-width, "
            "initial-scale=1.0\" />\n"
            "  <title>Cordlang Runtime Preview</title>\n"
            "  <style>\n");
  sb_append(&doc, RUNTIME_CSS);
  {
    char *theme = theme_css_generate_from_ir(ir);
    if (theme) {
      sb_append(&doc, "\n/* Theme from .cord */\n");
      sb_append(&doc, theme);
      free(theme);
    }
  }
  sb_append(&doc,
            "  </style>\n"
            "</head>\n"
            "<body>\n"
            "  <div class=\"cl-runtime-bar\">\n"
            "    <div><strong>Cordlang</strong> <span>native runtime "
            "preview</span></div>\n"
            "    <div class=\"cl-runtime-badge\"><span "
            "class=\"cl-runtime-dot\"></span> live · no React/Node</div>\n"
            "  </div>\n"
            "  <div id=\"app\">\n");
  sb_append(&doc, body.buf ? body.buf : "");
  sb_append(&doc,
            "  </div>\n"
            "  <div id=\"cl-toast\" class=\"cl-toast\"></div>\n"
            "  <script>\n");
  emit_state_runtime_js(&doc);
  sb_append(&doc, RUNTIME_JS_CORE);
  sb_append(&doc,
            "  </script>\n"
            "</body>\n"
            "</html>\n");

  free(body.buf);
  state_reset();
  prop_reset();
  return doc.buf;
}

char *html_generate_from_ir(IrProgram *ir) {
  /* Walk ir->root only — never ir_project_origin / AST for body or state. */
  if (!ir || !ir->root)
    return strdup("<!DOCTYPE html><html><body>empty</body></html>\n");
  return html_generate_from_ir_program(ir);
}

char *html_generate(Node *root) {
  if (!root)
    return strdup("<!DOCTYPE html><html><body>empty</body></html>\n");
  IrProgram *ir = ir_from_ast(root, NULL);
  if (!ir) return html_generate_impl(root);
  char *out = html_generate_from_ir(ir);
  ir_free(ir);
  return out;
}

int html_scaffold_from_ir(const char *project_dir, IrProgram *ir) {
  char *html = html_generate_from_ir(ir);
  if (!html) return -1;
  int rc = html_scaffold(project_dir, html);
  free(html);
  return rc;
}

static int write_path(const char *dir, const char *rel, const char *content) {
  char *path = fs_join(dir, rel);
  if (!path) return -1;
  int rc = fs_write_file(path, content);
  free(path);
  return rc;
}

int html_scaffold(const char *project_dir, const char *html_doc) {
  char *out = fs_join(project_dir, "dist/preview");
  if (!out) return -1;
  if (fs_mkdir_p(out) != 0) {
    free(out);
    return -1;
  }
  int rc = write_path(out, "index.html", html_doc ? html_doc : "<!DOCTYPE html><html><body>empty</body></html>\n");
  if (rc == 0) {
    printf("Preview written to: dist/preview/index.html\n");
  } else {
    fprintf(stderr, "Error: failed writing preview HTML\n");
  }
  free(out);
  return rc;
}

static const BackendPort html_port = {
    .name = "html",
    .extension = ".html",
    .generate_from_ir = html_generate_from_ir,
    .scaffold_from_ir = html_scaffold_from_ir,
    .generate = html_generate,
    .scaffold = html_scaffold,
    .scaffold_from_ast = NULL,
};

const BackendPort *html_backend_port(void) { return &html_port; }
