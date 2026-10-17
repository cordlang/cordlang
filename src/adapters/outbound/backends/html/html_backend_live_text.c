#include "html_backend_internal.h"

void htmlb_emit_bound_interp(StrBuf *sb, const char *expr) {
  /* data-bind live text for state vars; static placeholder otherwise */
  htmlb_sb_append(sb, "<span class=\"cl-interp\"");
  if (expr && htmlb_is_simple_state_expr(expr)) {
    htmlb_sb_append(sb, " data-bind=\"");
    htmlb_html_escape_append(sb, expr);
    htmlb_sb_append(sb, "\"");
    /* seed with initial state value for first paint before JS */
    const char *init = htmlb_state_init_for(expr);
    htmlb_sb_append(sb, ">");
    htmlb_html_escape_append(sb, init);
  } else {
    htmlb_sb_append(sb, " title=\"");
    htmlb_html_escape_append(sb, expr ? expr : "");
    htmlb_sb_append(sb, "\">${");
    htmlb_html_escape_append(sb, expr ? expr : "");
    htmlb_sb_append(sb, "}");
  }
  htmlb_sb_append(sb, "</span>");
}

void htmlb_emit_text_with_live_interp(StrBuf *sb, const char *str) {
  if (!str) return;
  if (!interp_has(str)) {
    htmlb_html_escape_append(sb, str);
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
      htmlb_html_escape_append(sb, p);
      break;
    }
    if (hash > p) {
      /* literal prefix */
      for (const char *s = p; s < hash; s++) {
        char c[2] = {*s, 0};
        if (*s == '&')
          htmlb_sb_append(sb, "&amp;");
        else if (*s == '<')
          htmlb_sb_append(sb, "&lt;");
        else if (*s == '>')
          htmlb_sb_append(sb, "&gt;");
        else if (*s == '"')
          htmlb_sb_append(sb, "&quot;");
        else
          htmlb_sb_append(sb, c);
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
      htmlb_html_escape_append(sb, hash);
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
    htmlb_emit_bound_interp(sb, expr);
    p = q + 1;
  }
}
