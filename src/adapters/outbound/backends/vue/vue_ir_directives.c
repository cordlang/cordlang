#include "adapters/outbound/backends/vue/vue_ir_internal.h"

int vue_is_vue_directive_attr(const char *name) {
  return name && (strcmp(name, "use") == 0 || strcmp(name, "transition") == 0 ||
                  strcmp(name, "in") == 0 || strcmp(name, "out") == 0 ||
                  strcmp(name, "animate") == 0);
}

void vue_emit_vue_directive(StrBuf *sb, const char *dir, const char *val) {
  if (!dir || !val || !*val) return;
  const char *lp = strchr(val, '(');
  if (lp && lp > val) {
    char name[96];
    size_t nl = (size_t)(lp - val);
    if (nl >= sizeof(name)) nl = sizeof(name) - 1;
    memcpy(name, val, nl);
    name[nl] = '\0';
    const char *rp = strrchr(val, ')');
    if (rp && rp > lp + 1) {
      char args[384];
      size_t al = (size_t)(rp - lp - 1);
      if (al >= sizeof(args)) al = sizeof(args) - 1;
      memcpy(args, lp + 1, al);
      args[al] = '\0';
      char *a = args;
      while (*a == ' ') a++;
      size_t alen = strlen(a);
      while (alen > 0 && a[alen - 1] == ' ') a[--alen] = '\0';
      if (*a)
        vue_sb_appendf(sb, " %s:%s={%s}", dir, name, a);
      else
        vue_sb_appendf(sb, " %s:%s", dir, name);
      return;
    }
  }
  vue_sb_appendf(sb, " %s:%s", dir, val);
}

int vue_is_vue_transition_fn(const char *name) {
  return name && (strcmp(name, "fade") == 0 || strcmp(name, "fly") == 0 ||
                  strcmp(name, "slide") == 0 || strcmp(name, "scale") == 0 ||
                  strcmp(name, "blur") == 0 || strcmp(name, "draw") == 0 ||
                  strcmp(name, "crossfade") == 0);
}

int vue_is_vue_animate_fn(const char *name) {
  return name && strcmp(name, "flip") == 0;
}

void vue_directive_fn_name(const char *val, char *out, size_t outsz) {
  if (!val || !out || outsz == 0) {
    if (out && outsz) out[0] = '\0';
    return;
  }
  size_t i = 0;
  while (val[i] && val[i] != '(' && i + 1 < outsz) {
    out[i] = val[i];
    i++;
  }
  out[i] = '\0';
}

void vue_vu_needs_add_trans(VuModuleNeeds *n, const char *fn) {
  if (!n || !fn || !*fn || !vue_is_vue_transition_fn(fn)) return;
  for (int i = 0; i < n->n_transition; i++)
    if (strcmp(n->transition_fns[i], fn) == 0) return;
  if (n->n_transition < 16) {
    snprintf(n->transition_fns[n->n_transition], sizeof(n->transition_fns[0]),
             "%s", fn);
    n->n_transition++;
  }
}

void vue_vu_needs_add_anim(VuModuleNeeds *n, const char *fn) {
  if (!n || !fn || !*fn || !vue_is_vue_animate_fn(fn)) return;
  for (int i = 0; i < n->n_animate; i++)
    if (strcmp(n->animate_fns[i], fn) == 0) return;
  if (n->n_animate < 8) {
    snprintf(n->animate_fns[n->n_animate], sizeof(n->animate_fns[0]), "%s", fn);
    n->n_animate++;
  }
}
