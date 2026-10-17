#include "application/lsp_internal.h"

 void handle_hover(const char *id, const char *msg) {
  char uri[1024];
  int line = 0, character = 0;
  const char *td = strstr(msg, "\"textDocument\"");
  if (td) json_get_str(td, "uri", uri, sizeof(uri));
  const char *pos = strstr(msg, "\"position\"");
  if (pos) {
    json_get_int(pos, "line", &line);
    json_get_int(pos, "character", &character);
  }
  LspDoc *d = doc_find(uri);
  char name[128];
  ident_at(d ? d->text : NULL, line, character, name, sizeof(name));
  if (!name[0]) {
    respond_ok(id, "null");
    return;
  }

  char md[1024];
  md[0] = '\0';
  if (cord_is_builtin_tag(name)) {
    snprintf(md, sizeof(md),
             "**`%s`** — built-in Cordlang tag\\n\\nLayout/control tag. Attrs: "
             "style (`gap`, `p`, …) + DOM (`class`, `to`, `bind`, …). See "
             "`docs/schema/attrs.json`.",
             name);
  } else if (cord_is_style_attr(name)) {
    snprintf(md, sizeof(md),
             "**`%s`** — style attribute\\n\\nMaps to utility classes / CSS. "
             "Schema v1.0.",
             name);
  } else if (cord_is_dom_attr(name) || strncmp(name, "aria-", 5) == 0 ||
             strncmp(name, "data-", 5) == 0) {
    snprintf(md, sizeof(md),
             "**`%s`** — DOM / Cord attribute\\n\\nKnown by `cordlang check`.",
             name);
  } else if (strcmp(name, "state") == 0) {
    snprintf(md, sizeof(md),
             "**`state`** — local reactive state\\n\\n`state count=0` → "
             "`setCount(...)`. Not `useState`.");
  } else if (strcmp(name, "props") == 0) {
    snprintf(md, sizeof(md),
             "**`props`** — component props\\n\\n`props title: string = \"\"`. "
             "Types: string|number|boolean|any.");
  } else if (strcmp(name, "computed") == 0 || strcmp(name, "def") == 0 ||
             strcmp(name, "route") == 0 || strcmp(name, "layout") == 0 ||
             strcmp(name, "theme") == 0 || strcmp(name, "fetch") == 0) {
    snprintf(md, sizeof(md),
             "**`%s`** — Cordlang keyword\\n\\nSee `docs/SPEC.md` (v1.0).",
             name);
  } else if (d && (d->text || d->path)) {
    /* Try prop type from component under cursor via buffer or disk */
    CompileResult r = {0};
    if (d->text)
      r = compiler_parse_source(d->text, strlen(d->text));
    else if (d->path)
      r = compiler_parse_project(d->path);
    if (r.ok && r.ast && r.ast->root) {
      Node *root = r.ast->root;
      for (size_t i = 0; i < root->children_len; i++) {
        Node *c = root->children[i];
        if (!c || c->type != NODE_COMPONENT_DEF) continue;
        for (size_t j = 0; j < c->children_len; j++) {
          Node *props = c->children[j];
          if (!props || props->type != NODE_PROPS_DECL) continue;
          for (size_t pi = 0; pi < props->children_len; pi++) {
            Node *ch = props->children[pi];
            if (!ch || !ch->value) continue;
            if (strcmp(ch->value, name) != 0) continue;
            const char *ty = "any";
            for (size_t k = 0; k < ch->children_len; k++) {
              Node *a = ch->children[k];
              if (a && a->type == NODE_ATTR && a->value &&
                  strcmp(a->value, "type") == 0 && a->value2)
                ty = a->value2;
            }
            snprintf(md, sizeof(md),
                     "**props `%s`**: `%s`\\n\\nValidated by `cordlang check`.",
                     name, ty);
            break;
          }
          if (md[0]) break;
        }
      }
    }
    compiler_result_free(&r);
  }
  if (!md[0]) {
    snprintf(md, sizeof(md), "`%s`", name);
  }
  char *esc = json_escape_dup(md);
  char result[2048];
  snprintf(result, sizeof(result),
           "{\"contents\":{\"kind\":\"markdown\",\"value\":\"%s\"}}",
           esc ? esc : "");
  free(esc);
  respond_ok(id, result);
}

/* Extract 'Name' from messages like: JSX attribute 'className' is not Cordlang */
