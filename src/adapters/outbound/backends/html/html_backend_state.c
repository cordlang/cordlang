#include "html_backend_internal.h"

#define CL_MAX_STATE 64
static char *g_state_names[CL_MAX_STATE];
static char *g_state_inits[CL_MAX_STATE];
static int g_state_count;

void htmlb_state_reset(void) {
  for (int i = 0; i < g_state_count; i++) {
    free(g_state_names[i]);
    free(g_state_inits[i]);
    g_state_names[i] = NULL;
    g_state_inits[i] = NULL;
  }
  g_state_count = 0;
}

int htmlb_state_has(const char *name) {
  if (!name) return 0;
  for (int i = 0; i < g_state_count; i++) {
    if (g_state_names[i] && strcmp(g_state_names[i], name) == 0) return 1;
  }
  return 0;
}

void htmlb_state_add(const char *name, const char *init) {
  if (!name || !*name || htmlb_state_has(name) || g_state_count >= CL_MAX_STATE) return;
  g_state_names[g_state_count] = strdup(name);
  g_state_inits[g_state_count] = strdup(init && *init ? init : "0");
  g_state_count++;
}

void htmlb_collect_state_from_node(Node *node) {
  if (!node) return;
  if (node->type == NODE_STATE_DECL) {
    if (node->value) {
      htmlb_state_add(node->value, node->value2);
    } else {
      for (size_t i = 0; i < node->children_len; i++) {
        Node *st = node->children[i];
        if (st && st->type == NODE_STATE_DECL && st->value)
          htmlb_state_add(st->value, st->value2);
      }
    }
  }
  for (size_t i = 0; i < node->children_len; i++)
    htmlb_collect_state_from_node(node->children[i]);
}

int htmlb_is_simple_state_expr(const char *expr) {
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
  return htmlb_state_has(buf);
}

const char *htmlb_state_init_for(const char *name) {
  if (!name) return "0";
  for (int i = 0; i < g_state_count; i++) {
    if (g_state_names[i] && strcmp(g_state_names[i], name) == 0)
      return g_state_inits[i] ? g_state_inits[i] : "0";
  }
  return "0";
}

void htmlb_make_setter_name(char *out, size_t out_sz, const char *name) {
  if (!out || out_sz < 4 || !name || !*name) {
    if (out && out_sz) out[0] = '\0';
    return;
  }
  snprintf(out, out_sz, "set%c%s", (char)toupper((unsigned char)name[0]),
           name + 1);
}

int htmlb_is_safe_js_ident(const char *s) {
  if (!s || !*s) return 0;
  if (!(isalpha((unsigned char)s[0]) || s[0] == '_')) return 0;
  for (const char *p = s + 1; *p; p++) {
    if (!(isalnum((unsigned char)*p) || *p == '_')) return 0;
  }
  return 1;
}

int htmlb_is_safe_js_init(const char *s) {
  if (!s || !*s) return 0;
  if (strcmp(s, "true") == 0 || strcmp(s, "false") == 0 ||
      strcmp(s, "null") == 0)
    return 1;
  const char *p = s;
  if (*p == '-' || *p == '+') p++;
  int digits = 0;
  while (*p) {
    if (isdigit((unsigned char)*p))
      digits++;
    else if (*p != '.')
      return 0;
    p++;
  }
  if (digits > 0) return 1;
  /* "..." with no unescaped quotes/backslashes beyond simple content */
  if (s[0] == '"') {
    size_t n = strlen(s);
    if (n >= 2 && s[n - 1] == '"') {
      for (size_t i = 1; i + 1 < n; i++) {
        if (s[i] == '"' || s[i] == '\\' || s[i] == '\n' || s[i] == '\r')
          return 0;
      }
      return 1;
    }
  }
  return 0;
}

int htmlb_href_is_safe(const char *href) {
  if (!href) return 1;
  while (*href && isspace((unsigned char)*href)) href++;
  if (!*href) return 1;
  /* Block javascript:/data:/vbscript: scheme URLs */
  if (strncasecmp(href, "javascript:", 11) == 0) return 0;
  if (strncasecmp(href, "data:", 5) == 0) return 0;
  if (strncasecmp(href, "vbscript:", 9) == 0) return 0;
  return 1;
}

void htmlb_emit_state_runtime_js(StrBuf *sb) {
  if (g_state_count == 0) return;
  htmlb_sb_append(sb, "/* Cordlang preview reactive state (C8) */\n");
  for (int i = 0; i < g_state_count; i++) {
    const char *name = g_state_names[i];
    const char *init = g_state_inits[i] ? g_state_inits[i] : "0";
    if (!htmlb_is_safe_js_ident(name)) continue;
    if (!htmlb_is_safe_js_init(init)) init = "null";
    char setter[128];
    htmlb_make_setter_name(setter, sizeof(setter), name);
    /* Use var so clEvalExpr/handlers resolve state on the global object.
     * Init comes from the parser (number / true / "string" / expr). */
    htmlb_sb_appendf(sb, "var %s = %s;\n", name, init);
    htmlb_sb_appendf(sb,
               "function %s(v){ %s = (typeof v==='function') ? v(%s) : v; "
               "clUpdate(); }\n",
               setter, name, name);
  }
  htmlb_sb_append(sb, "\n");
}

int htmlb_ir_attr_is_true(const char *v) {
  return v && strcmp(v, "true") == 0;
}

void htmlb_collect_state_from_ir(IrNode *node) {
  if (!node) return;
  if (node->kind == IR_STATE) {
    if (node->name && strcmp(node->name, "__states__") == 0) {
      for (size_t i = 0; i < node->n_kids; i++) {
        IrNode *st = node->kids[i];
        if (st && st->kind == IR_STATE && st->name)
          htmlb_state_add(st->name, st->value);
      }
    } else if (node->name) {
      htmlb_state_add(node->name, node->value);
    }
  }
  for (size_t i = 0; i < node->n_kids; i++)
    htmlb_collect_state_from_ir(node->kids[i]);
}
