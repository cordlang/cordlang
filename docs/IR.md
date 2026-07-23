# Cordlang Intermediate Representation (IR)

El **IR canónico** es la capa post-AST que **todos los backends consumen**.

```
.cord source
    │
    ▼
  Lexer → Parser → AST (domain/ast.h)
    │
    ▼
  ir_from_ast()  →  IrProgram  (domain/ir.h)
    │
    ├── ir_dump()                 →  cordlang compile --ir
    ├── generate_from_ir()        →  React / Svelte / HTML
    └── scaffold_from_ir()        →  dist/react | dist/svelte | dist/preview
```

## Pipeline en el CLI

`compile_service` y `run_service` hacen siempre:

1. `compiler_parse_project` → AST multi-archivo  
2. `ir_from_ast` → `IrProgram`  
3. `backend->generate_from_ir` / `scaffold_from_ir`

Las APIs legacy `generate(Node*)` / `scaffold_from_ast` siguen existiendo y
**rebajan a IR** antes de emitir, para tests y compat.

## API

```c
#include "domain/ir.h"

IrProgram *ir_from_ast(Node *ast_root, const char *entry_file);
void ir_free(IrProgram *p);
char *ir_dump(const IrProgram *p);   /* caller frees */
const char *ir_kind_name(IrKind k);

/* Walk helpers */
IrNode *ir_find_child(const IrNode *n, IrKind kind);
IrNode *ir_find_hook(const IrNode *n, const char *hook_kind);
const char *ir_attr(const IrNode *n, const char *attr_name);
size_t ir_count_kind(const IrNode *n, IrKind kind);
Node *ir_origin(const IrNode *n);
Node *ir_project_origin(const IrProgram *p);
```

### Backend port (IR-first)

```c
typedef struct BackendPort {
  const char *name;
  const char *extension;
  char *(*generate_from_ir)(IrProgram *ir);           /* preferred */
  int (*scaffold_from_ir)(const char *dir, IrProgram *ir);
  char *(*generate)(Node *root);                      /* legacy → IR */
  int (*scaffold)(const char *dir, const char *blob);
  int (*scaffold_from_ast)(const char *dir, Node *root);
} BackendPort;
```

### Origin pointers

Cada `IrNode` guarda `origin` → puntero **débil** al AST (debug / theme residual).

| Fase | Qué hace |
|------|----------|
| **IR-1** ✅ | Puerto y services solo hablan IR |
| **IR-2** ✅ | React (`react_ir.c`) y Svelte: body/modules caminan **solo `IrNode`** |
| Residual | (cerrado G1/G2) theme + HTML preview en path IR-puro |

### Node kinds

| Kind | Campos típicos |
|------|----------------|
| `IR_PROJECT` | root; `file` |
| `IR_COMPONENT` / `IR_LAYOUT` | `name` |
| `IR_ROUTE` | `name`=path, `value`=target |
| `IR_PROP` / `IR_STATE` / `IR_COMPUTED` | name + value |
| `IR_EFFECT` | name=effect\|layoutEffect\|insertionEffect |
| `IR_ELEMENT` / `IR_TEXT` / `IR_INTERP` | markup |
| `IR_IF` / `IR_FOR` | cond / item+list |
| `IR_EVENT` / `IR_ATTR` / `IR_SLOT` | UI surface |
| `IR_FETCH` | data loading |
| `IR_HOOK` | kind en `name` (`ref`, `ctx`, `action`, `portal`, …) |
| `IR_MODULE_USE` | multi-file |
| `IR_AWAIT` / `IR_SNIPPET` / `IR_STORE` / `IR_RENDER` | Svelte E surface |

## Expr mini-parser (C2)

Al bajar eventos / computed / if / for, `ir_from_ast` llama `expr_normalize`.

## CLI

```bash
cordlang compile file.cord --ir          # dump IR
cordlang compile file.cord --backend react   # IR → React
cordlang compile file.cord --backend svelte  # IR → Svelte
cordlang run react                       # parse → IR → scaffold_from_ir
```

## Definition of Done

### IR-1 ✅
- [x] Services: AST → IR → backend  
- [x] React / Svelte / HTML: `generate_from_ir` + `scaffold_from_ir`  
- [x] Origin weak pointers en cada `IrNode`  

### IR-2 ✅
- [x] React: `react_ir.c` — `project_partition_from_ir`, `gen_ir_node`, `gen_component_fn_ir`, App/contexts  
- [x] Svelte: partition + script/markup walkers sobre `IrNode` (sin origin en body)  
- [x] Goldens **18/18**  
- [ ] Theme CSS 100% desde IR (hoy scaffold puede usar origin solo para theme)  
- [ ] HTML preview 100% IR (opcional)
