#include "adapters/outbound/backends/solid/solid_ir_internal.h"

void solid_ir_gen_component_state_hooks_solid(StrBuf *sb, IrNode *def, GenCtx *ctx) {
  /* state bags + leaf states */
  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (c->kind != IR_STATE) continue;
    if (c->name && strcmp(c->name, "__states__") == 0) {
      for (size_t j = 0; j < c->n_kids; j++) {
        IrNode *st = c->kids[j];
        if (!st->name) continue;
        char setter[128];
        snprintf(setter, sizeof(setter), "set%c%s",
                 (char)toupper((unsigned char)st->name[0]), st->name + 1);
        solid_ir_sb_appendf(sb, "  const [%s, %s] = createSignal(", st->name, setter);
        solid_ir_emit_js_literal(sb, st->value ? st->value : "null");
        solid_ir_sb_append(sb, ");\n");
        solid_ir_solid_track_signal(ctx, st->name);
        ctx->use_state = 1;
      }
    } else if (c->name) {
      char setter[128];
      snprintf(setter, sizeof(setter), "set%c%s",
               (char)toupper((unsigned char)c->name[0]), c->name + 1);
      solid_ir_sb_appendf(sb, "  const [%s, %s] = createSignal(", c->name, setter);
      solid_ir_emit_js_literal(sb, c->value ? c->value : "null");
      solid_ir_sb_append(sb, ");\n");
      solid_ir_solid_track_signal(ctx, c->name);
      ctx->use_state = 1;
    }
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (!solid_ir_ir_hook_is(c, "reducer") || !c->value) continue;
    const char *init = "undefined";
    for (size_t j = 0; j < c->n_kids; j++) {
      if (c->kids[j]->kind == IR_ATTR && c->kids[j]->name &&
          strcmp(c->kids[j]->name, "init") == 0 && c->kids[j]->value)
        init = c->kids[j]->value;
    }
    solid_ir_sb_appendf(sb, "  const [%s, dispatch] = /* reducer */(%s, %s);\n", c->value,
               c->value2 ? c->value2 : "reducer", init);
    ctx->use_reducer = 1;
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (solid_ir_ir_hook_is(c, "ctx") && c->value && c->value2) {
      solid_ir_sb_appendf(sb, "  const %s = useContext(%s);\n", c->value, c->value2);
      ctx->use_context = 1;
    }
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (solid_ir_ir_hook_is(c, "ref") && c->value) {
      solid_ir_sb_appendf(sb, "  const %s = /* ref */(", c->value);
      if (c->value2)
        solid_ir_emit_js_literal(sb, c->value2);
      else
        solid_ir_sb_append(sb, "null");
      solid_ir_sb_append(sb, ");\n");
      ctx->use_ref = 1;
    }
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (solid_ir_ir_hook_is(c, "params")) {
      solid_ir_sb_append(sb, "  const { ");
      solid_ir_sb_append(sb, c->value ? c->value : "id");
      solid_ir_sb_append(sb, " } = useParams();\n");
      ctx->use_params = 1;
    }
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (solid_ir_ir_hook_is(c, "navigate")) {
      solid_ir_sb_appendf(sb, "  const %s = useNavigate();\n",
                 c->value ? c->value : "navigate");
      ctx->use_navigate = 1;
    }
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (solid_ir_ir_hook_is(c, "id") && c->value) {
      solid_ir_sb_appendf(sb, "  const %s = /* id */();\n", c->value);
      ctx->use_id = 1;
    }
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (solid_ir_ir_hook_is(c, "transition")) {
      solid_ir_sb_appendf(sb, "  const [%s, %s] = /* transition */();\n",
                 c->value ? c->value : "isPending",
                 c->value2 ? c->value2 : "startTransition");
      ctx->use_transition = 1;
    }
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (solid_ir_ir_hook_is(c, "deferred") && c->value) {
      solid_ir_sb_appendf(sb, "  const %s = useDeferredValue(%s);\n", c->value,
                 c->value2 ? c->value2 : c->value);
      ctx->use_deferred = 1;
    }
  }
}

void solid_ir_gen_component_fn_ir(StrBuf *sb, IrNode *def, const char *name,
                                GenCtx *ctx) {
  IrNode *props_decl = NULL;
  int has_children_slot = 0;

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (c->kind == IR_PROP && c->name && strcmp(c->name, "__props__") == 0 &&
        !props_decl)
      props_decl = c;
    else if (c->kind == IR_PROP && !c->name && !props_decl)
      props_decl = c;
    if (c->kind == IR_SLOT && !ctx->is_layout) has_children_slot = 1;
    if (c->kind == IR_ELEMENT && c->name && strcmp(c->name, "slot") == 0 &&
        !ctx->is_layout)
      has_children_slot = 1;
    if (c->kind == IR_ATTR && c->name && strcmp(c->name, "forwardRef") == 0 &&
        solid_ir_attr_is_true(c->value))
      ctx->use_forward_ref = 1;
  }
  if (has_children_slot) ctx->use_children = 1;

  solid_ir_sb_appendf(sb, "function %s(", name);
  solid_ir_sb_append(sb, "{ ");
  int prop_n = 0;
  if (props_decl) {
    for (size_t i = 0; i < props_decl->n_kids; i++) {
      IrNode *pr = props_decl->kids[i];
      if (!pr->name) continue;
      if (prop_n++) solid_ir_sb_append(sb, ", ");
      solid_ir_sb_append(sb, pr->name);
      if (pr->value) {
        solid_ir_sb_append(sb, " = ");
        solid_ir_emit_js_literal(sb, pr->value);
      }
    }
  }
  if (ctx->use_children || has_children_slot) {
    if (prop_n++) solid_ir_sb_append(sb, ", ");
    solid_ir_sb_append(sb, "children");
  }
  solid_ir_sb_append(sb, " }");
  if (ctx->use_forward_ref) solid_ir_sb_append(sb, ", ref");
  solid_ir_sb_append(sb, ") {\n");

  solid_ir_gen_component_state_hooks_solid(sb, def, ctx);
  solid_ir_gen_component_derived_hooks_solid(sb, def, ctx);

  solid_ir_sb_append(sb, "  return (\n");
  solid_ir_sb_append(sb, "    <>\n");
  int body_count = 0;
  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (solid_ir_is_decl_ir(c)) continue;
    solid_ir_gen_ir_node(sb, c, 3, ctx);
    body_count++;
  }
  if (body_count == 0) solid_ir_sb_append(sb, "      null\n");
  solid_ir_sb_append(sb, "    </>\n");
  solid_ir_sb_append(sb, "  );\n");
  solid_ir_sb_append(sb, "}\n");
}
