#include "html_backend_internal.h"

int htmlb_is_decl_only(NodeType t) {
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

void htmlb_collect_classes(char *classes, size_t classes_sz, Node *node, const char *base_class) {
  classes[0] = '\0';
  if (base_class) {
    strncat(classes, base_class, classes_sz - 1);
  }

  int has_between = 0;
  for (size_t i = 0; i < node->children_len; i++) {
    Node *child = node->children[i];
    if (child->type == NODE_BOOL_ATTR && child->value &&
        strcmp(child->value, "between") == 0)
      has_between = 1;
    if (child->type == NODE_ATTR && child->value &&
        strcmp(child->value, "between") == 0)
      has_between = 1;
  }

  for (size_t i = 0; i < node->children_len; i++) {
    Node *child = node->children[i];
    if (child->type == NODE_STYLE_MAP && child->value && strcmp(child->value, "style") == 0) {
      StrBuf style_sb = {0};
      style_sb.buf = malloc(1024);
      style_sb.cap = 1024;
      style_sb.len = 0;
      style_sb.buf[0] = '\0';
      htmlb_gen_style_classes(&style_sb, child);
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
      if (strcmp(child->value, "between") == 0) strncat(classes, " flex justify-between", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "center") == 0)
        strncat(classes,
                has_between ? " flex items-center"
                            : " flex items-center justify-center",
                classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "around") == 0) strncat(classes, " justify-around", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "evenly") == 0) strncat(classes, " justify-evenly", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "bold") == 0) strncat(classes, " font-bold", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "muted") == 0) strncat(classes, " text-muted", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "font-mono") == 0) strncat(classes, " font-mono", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "flex-1") == 0) strncat(classes, " flex-1", classes_sz - strlen(classes) - 1);
      else if (strcmp(child->value, "border") == 0) strncat(classes, " border border-gray-200", classes_sz - strlen(classes) - 1);
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
      } else if (strcmp(child->value, "px") == 0) {
        snprintf(vbuf, sizeof(vbuf), " px-%s", child->value2);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      } else if (strcmp(child->value, "py") == 0) {
        snprintf(vbuf, sizeof(vbuf), " py-%s", child->value2);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      } else if (strcmp(child->value, "m") == 0) {
        snprintf(vbuf, sizeof(vbuf), " m-%s", child->value2);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      } else if (strcmp(child->value, "mx") == 0) {
        snprintf(vbuf, sizeof(vbuf), " mx-%s", child->value2);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      } else if (strcmp(child->value, "my") == 0) {
        snprintf(vbuf, sizeof(vbuf), " my-%s", child->value2);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      } else if (strcmp(child->value, "w") == 0) {
        snprintf(vbuf, sizeof(vbuf), " w-%s", child->value2);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      } else if (strcmp(child->value, "h") == 0) {
        snprintf(vbuf, sizeof(vbuf), " h-%s", child->value2);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      } else if (strcmp(child->value, "min-h") == 0) {
        snprintf(vbuf, sizeof(vbuf), " min-h-%s", child->value2);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      } else if (strcmp(child->value, "border") == 0) {
        snprintf(vbuf, sizeof(vbuf), " border border-%s", child->value2);
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
