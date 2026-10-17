#include "html_backend_internal.h"

int htmlb_is_ir_decl_only(const IrNode *n) {
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

void htmlb_collect_classes_ir(char *classes, size_t classes_sz, IrNode *node,
                               const char *base_class) {
  classes[0] = '\0';
  if (base_class) {
    strncat(classes, base_class, classes_sz - 1);
  }

  int has_between = 0;
  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *child = node->kids[i];
    if (child && child->kind == IR_ATTR && child->name &&
        strcmp(child->name, "between") == 0)
      has_between = 1;
  }

  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *child = node->kids[i];
    if (child && child->kind == IR_ATTR && child->name &&
        strcmp(child->name, "style") == 0) {
      StrBuf style_sb = {0};
      style_sb.buf = malloc(1024);
      style_sb.cap = 1024;
      style_sb.len = 0;
      style_sb.buf[0] = '\0';
      htmlb_gen_style_classes_ir(&style_sb, child);
      if (style_sb.len > 0) {
        size_t room = classes_sz - strlen(classes) - 1;
        strncat(classes, style_sb.buf, room);
      }
      free(style_sb.buf);
    }
  }

  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *child = node->kids[i];
    char vbuf[96];
    if (!child || child->kind != IR_ATTR || !child->name) continue;

    /* Bool-like attrs: IR_ATTR name with value "true" */
    if (htmlb_ir_attr_is_true(child->value)) {
      if (strcmp(child->name, "between") == 0)
        strncat(classes, " flex justify-between", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->name, "center") == 0)
        strncat(classes,
                has_between ? " flex items-center"
                            : " flex items-center justify-center",
                classes_sz - strlen(classes) - 1);
      else if (strcmp(child->name, "around") == 0)
        strncat(classes, " justify-around", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->name, "evenly") == 0)
        strncat(classes, " justify-evenly", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->name, "bold") == 0)
        strncat(classes, " font-bold", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->name, "muted") == 0)
        strncat(classes, " text-muted", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->name, "font-mono") == 0)
        strncat(classes, " font-mono", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->name, "flex-1") == 0)
        strncat(classes, " flex-1", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->name, "border") == 0)
        strncat(classes, " border border-gray-200",
                classes_sz - strlen(classes) - 1);
      else if (strcmp(child->name, "sticky") == 0)
        strncat(classes, " sticky top-0", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->name, "primary") == 0)
        strncat(classes, " btn-primary", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->name, "outline") == 0)
        strncat(classes, " btn-outline", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->name, "ghost") == 0)
        strncat(classes, " btn-ghost", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->name, "secondary") == 0)
        strncat(classes, " btn-secondary", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->name, "xs") == 0)
        strncat(classes, " text-xs", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->name, "sm") == 0)
        strncat(classes, " text-sm", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->name, "lg") == 0)
        strncat(classes, " text-lg", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->name, "xl") == 0)
        strncat(classes, " text-xl", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->name, "2xl") == 0)
        strncat(classes, " text-2xl", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->name, "3xl") == 0)
        strncat(classes, " text-3xl", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->name, "4xl") == 0)
        strncat(classes, " text-4xl", classes_sz - strlen(classes) - 1);
      continue;
    }

    if (!child->value) continue;

    if (strcmp(child->name, "variant") == 0) {
      snprintf(vbuf, sizeof(vbuf), " btn-%s", child->value);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(child->name, "size") == 0) {
      snprintf(vbuf, sizeof(vbuf), " text-%s", child->value);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(child->name, "color") == 0) {
      if (strcmp(child->value, "primary") == 0)
        strncat(classes, " text-primary", classes_sz - strlen(classes) - 1);
      else {
        snprintf(vbuf, sizeof(vbuf), " text-%s", child->value);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      }
    } else if (strcmp(child->name, "gap") == 0) {
      snprintf(vbuf, sizeof(vbuf), " gap-%s", child->value);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(child->name, "cols") == 0) {
      snprintf(vbuf, sizeof(vbuf), " grid-cols-%s", child->value);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(child->name, "p") == 0) {
      snprintf(vbuf, sizeof(vbuf), " p-%s", child->value);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(child->name, "px") == 0) {
      snprintf(vbuf, sizeof(vbuf), " px-%s", child->value);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(child->name, "py") == 0) {
      snprintf(vbuf, sizeof(vbuf), " py-%s", child->value);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(child->name, "m") == 0) {
      snprintf(vbuf, sizeof(vbuf), " m-%s", child->value);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(child->name, "mx") == 0) {
      snprintf(vbuf, sizeof(vbuf), " mx-%s", child->value);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(child->name, "my") == 0) {
      snprintf(vbuf, sizeof(vbuf), " my-%s", child->value);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(child->name, "w") == 0) {
      snprintf(vbuf, sizeof(vbuf), " w-%s", child->value);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(child->name, "h") == 0) {
      snprintf(vbuf, sizeof(vbuf), " h-%s", child->value);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(child->name, "min-h") == 0) {
      snprintf(vbuf, sizeof(vbuf), " min-h-%s", child->value);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(child->name, "border") == 0) {
      snprintf(vbuf, sizeof(vbuf), " border border-%s", child->value);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(child->name, "bg") == 0) {
      snprintf(vbuf, sizeof(vbuf), " bg-%s", child->value);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(child->name, "shadow") == 0) {
      snprintf(vbuf, sizeof(vbuf), " shadow-%s", child->value);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(child->name, "rounded") == 0) {
      snprintf(vbuf, sizeof(vbuf), " rounded-%s", child->value);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(child->name, "max-w") == 0) {
      snprintf(vbuf, sizeof(vbuf), " max-w-%s", child->value);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    }
  }
}
