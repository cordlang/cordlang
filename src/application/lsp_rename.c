#include "application/lsp_internal.h"

 void handle_prepare_rename(const char *id, const char *msg) {
  char uri[1024];
  int line = 0, character = 0;
  parse_pos_uri(msg, uri, sizeof(uri), &line, &character);
  LspDoc *d = doc_find(uri);
  if (d) doc_ensure_text(d);
  char name[128];
  int sc = 0, ec = 0;
  if (!ident_range(d ? d->text : NULL, line, character, name, sizeof(name), &sc,
                   &ec) ||
      !d || !d->path ||
      !symbols_name_is_component(d->path, name, lsp_overlay, NULL)) {
    respond_err(id, -32602, "not a renamable component");
    return;
  }
  char *ph = json_escape_dup(name);
  char result[512];
  snprintf(result, sizeof(result),
           "{\"range\":{\"start\":{\"line\":%d,\"character\":%d},"
           "\"end\":{\"line\":%d,\"character\":%d}},\"placeholder\":\"%s\"}",
           line, sc, line, ec, ph ? ph : "");
  free(ph);
  respond_ok(id, result);
}

 char *renamed_file_path(const char *old_path, const char *new_name) {
  if (!old_path || !new_name) return NULL;
  char *dir = fs_dirname(old_path);
  if (!dir) return NULL;
  char fname[256];
  snprintf(fname, sizeof(fname), "%s.cord", new_name);
  char *joined = fs_join(dir, fname);
  free(dir);
  return joined;
}

 int basename_stem_eq(const char *path, const char *name) {
  char *base = fs_basename(path);
  if (!base) return 0;
  size_t n = strlen(base);
  if (n > 5 && strcmp(base + n - 5, ".cord") == 0) base[n - 5] = '\0';
  int eq = name && strcmp(base, name) == 0;
  free(base);
  return eq;
}

 void handle_rename(const char *id, const char *msg) {
  char name[128];
  LspDoc *d = NULL;
  if (!cursor_component(id, msg, name, sizeof(name), &d, 1)) return;

  char new_name[128];
  new_name[0] = '\0';
  json_get_str(msg, "newName", new_name, sizeof(new_name));
  char err[256];
  int rc = symbols_rename_check(d->path, name, new_name, lsp_overlay, NULL, err,
                                sizeof(err));
  if (rc != 0) {
    respond_err(id, -32602, err[0] ? err : "invalid component name");
    return;
  }

  const char *extra[MAX_DOCS];
  int n_extra = lsp_extra_paths(extra, MAX_DOCS);
  SymbolLocList locs;
  symbols_collect_refs(d->path, name, 1, lsp_overlay, NULL, extra, n_extra,
                       &locs);
  if (locs.len > 1)
    qsort(locs.items, locs.len, sizeof(SymbolLoc), loc_cmp);

  int rename_file = locs.decl_path && basename_stem_eq(locs.decl_path, name) &&
                    strcmp(name, new_name) != 0;

  char *new_text_esc = json_escape_dup(new_name);
  size_t cap = 4096, len = 0;
  char *out = malloc(cap);
  if (!out) {
    free(new_text_esc);
    symbol_loc_list_free(&locs);
    respond_err(id, -32603, "out of memory");
    return;
  }

  if (rename_file) {
    strcpy(out, "{\"documentChanges\":[");
    len = strlen(out);
  } else {
    strcpy(out, "{\"changes\":{");
    len = strlen(out);
  }

  /* Group consecutive locs by path. */
  size_t i = 0;
  int file_n = 0;
  while (i < locs.len) {
    const char *path = locs.items[i].path;
    size_t j = i;
    while (j < locs.len && locs.items[j].path &&
           strcmp(locs.items[j].path, path) == 0)
      j++;

    char file_uri[1024];
    abs_uri(path, file_uri, sizeof(file_uri));
    char *uri_esc = json_escape_dup(file_uri);

    size_t chunk_cap = 2048, chunk_len = 0;
    char *chunk = malloc(chunk_cap);
    if (!chunk) {
      free(uri_esc);
      break;
    }
    chunk[0] = '\0';
    int edit_n = 0;
    for (size_t k = i; k < j; k++) {
      SymbolLoc *it = &locs.items[k];
      if (it->is_path && !rename_file) continue;
      int sl = it->line > 0 ? it->line - 1 : 0;
      int sc = it->col > 0 ? it->col - 1 : 0;
      char item[512];
      snprintf(item, sizeof(item),
               "%s{\"range\":{\"start\":{\"line\":%d,\"character\":%d},"
               "\"end\":{\"line\":%d,\"character\":%d}},\"newText\":\"%s\"}",
               edit_n ? "," : "", sl, sc, sl, sc + it->length,
               new_text_esc ? new_text_esc : "");
      size_t il = strlen(item);
      if (chunk_len + il + 1 >= chunk_cap) {
        chunk_cap = (chunk_len + il + 1) * 2;
        char *nc = realloc(chunk, chunk_cap);
        if (!nc) break;
        chunk = nc;
      }
      memcpy(chunk + chunk_len, item, il);
      chunk_len += il;
      chunk[chunk_len] = '\0';
      edit_n++;
    }

    if (edit_n > 0) {
      char header[1536];
      if (rename_file)
        snprintf(header, sizeof(header),
                 "%s{\"textDocument\":{\"uri\":\"%s\",\"version\":null},"
                 "\"edits\":[",
                 file_n ? "," : "", uri_esc ? uri_esc : "");
      else
        snprintf(header, sizeof(header), "%s\"%s\":[", file_n ? "," : "",
                 uri_esc ? uri_esc : "");
      size_t need = strlen(header) + chunk_len + 8;
      if (len + need >= cap) {
        while (len + need >= cap) cap *= 2;
        char *nbuf = realloc(out, cap);
        if (!nbuf) {
          free(uri_esc);
          free(chunk);
          break;
        }
        out = nbuf;
      }
      memcpy(out + len, header, strlen(header));
      len += strlen(header);
      memcpy(out + len, chunk, chunk_len);
      len += chunk_len;
      out[len++] = ']';
      if (rename_file) out[len++] = '}';
      out[len] = '\0';
      file_n++;
    }
    free(uri_esc);
    free(chunk);
    i = j;
  }

  if (rename_file && locs.decl_path) {
    char old_uri[1024], new_uri[1024];
    abs_uri(locs.decl_path, old_uri, sizeof(old_uri));
    char *np = renamed_file_path(locs.decl_path, new_name);
    if (np) abs_uri(np, new_uri, sizeof(new_uri));
    else new_uri[0] = '\0';
    free(np);
    char *old_esc = json_escape_dup(old_uri);
    char *new_esc = json_escape_dup(new_uri);
    char item[2048];
    snprintf(item, sizeof(item),
             "%s{\"kind\":\"rename\",\"oldUri\":\"%s\",\"newUri\":\"%s\"}",
             file_n ? "," : "", old_esc ? old_esc : "",
             new_esc ? new_esc : "");
    free(old_esc);
    free(new_esc);
    size_t il = strlen(item);
    if (len + il + 4 >= cap) {
      cap = len + il + 4;
      char *nbuf = realloc(out, cap);
      if (nbuf) out = nbuf;
    }
    if (len + il + 4 <= cap) {
      memcpy(out + len, item, il);
      len += il;
      out[len] = '\0';
    }
  }

  const char *tail = rename_file ? "]}" : "}}";
  size_t tl = strlen(tail);
  if (len + tl + 1 >= cap) {
    cap = len + tl + 1;
    char *nbuf = realloc(out, cap);
    if (nbuf) out = nbuf;
  }
  if (len + tl + 1 <= cap) memcpy(out + len, tail, tl + 1);

  respond_ok(id, out);
  free(out);
  free(new_text_esc);
  symbol_loc_list_free(&locs);
}

