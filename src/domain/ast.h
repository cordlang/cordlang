#ifndef CORDLANG_DOMAIN_AST_H
#define CORDLANG_DOMAIN_AST_H

#include <stddef.h>

#define NODE_MAGIC 0xDEADBEEF

typedef enum {
  NODE_ROOT,
  NODE_ELEMENT,
  NODE_TEXT,
  NODE_STRING,
  NODE_ATTR,
  NODE_EVENT,
  NODE_BOOL_ATTR,
  NODE_STYLE_MAP,
  NODE_STYLE_ENTRY,
  NODE_FOR,
  NODE_IF,
  NODE_COMPONENT_DEF,
  NODE_PROPS_DECL,
  NODE_STATE_DECL,
  NODE_COMPUTED_DECL,
  NODE_THEME,
  NODE_ROUTE,
  NODE_SLOT,
  NODE_INTERPOLATION,
  NODE_USE, /* use path [as Alias] — multi-file modules */
  /* React hooks / API surface (see docs/REACT.md) */
  NODE_EFFECT_DECL,     /* useEffect: value=deps, value2=body */
  NODE_REF_DECL,        /* useRef: value=name, value2=initial */
  NODE_CONTEXT_DECL,    /* createContext: value=Name, value2=default */
  NODE_CONTEXT_USE,     /* useContext: value=varName, value2=ContextName */
  NODE_REDUCER_DECL,    /* useReducer: value=stateName, value2=reducerFn */
  NODE_PARAMS_DECL,     /* useParams: value="id,slug" or children */
  NODE_NAVIGATE_DECL,   /* useNavigate: value=varName */
  NODE_CALLBACK_DECL,   /* useCallback: value=name, value2=fn, deps in child */
  NODE_ID_DECL,         /* useId: value=name */
  NODE_TRANSITION_DECL, /* useTransition: value=isPending, value2=start */
  NODE_DEFERRED_DECL,   /* useDeferredValue: value=name, value2=source */
  NODE_ACTION_DECL,     /* useActionState: value=stateName, value2=fn */
  NODE_FETCH_DECL,      /* fetch data = "/api/...": value=name, value2=url */
  NODE_LAZY_DECL,       /* lazy: value=ComponentName, value2=module path */
  NODE_LAYOUT_EFFECT,   /* useLayoutEffect: value=deps, value2=body */
  NODE_PORTAL,          /* createPortal: value=target expr, children=UI */
  NODE_ERROR_BOUNDARY,  /* ErrorBoundary wrapper: children + fallback child */
  NODE_SUSPENSE,        /* <Suspense>: children + fallback */
  NODE_LOADING,         /* loading wrapper (Suspense-like): fallback + children */
  NODE_EMPTY,           /* empty if=cond ... empty-state UI */
  NODE_HEAD,            /* document title / <head>: value=title text */
  /* Phase E — Svelte advanced (append only; do not reorder above) */
  NODE_AWAIT,           /* {#await}: value=promiseExpr, value2=thenName */
  NODE_SNIPPET,         /* {#snippet}: value=name, value2=params */
  NODE_STORE_DECL,      /* writable store: value=name, value2=initial */
  NODE_RENDER,          /* {@render}: value=callExpr e.g. card("Hello") */
  /* Phase D — advanced React hooks (append only; do not reorder above) */
  NODE_INSERTION_EFFECT,  /* useInsertionEffect: value=deps, value2=body */
  NODE_EFFECT_EVENT,      /* useEffectEvent: value=name, value2=fn/body */
  NODE_EXTERNAL_STORE,    /* useSyncExternalStore: value=name, value2=subscribe */
  NODE_IMPERATIVE_HANDLE, /* useImperativeHandle: value=ref, value2=body */
  /* Libraries / capabilities (append only) */
  NODE_FOREIGN, /* foreign Comp: value=Name; attrs react/svelte/vue/solid = module */
} NodeType;

typedef struct Node {
  unsigned int magic;
  NodeType type;
  struct Node **children;
  size_t children_len;
  size_t children_cap;
  char *value;
  char *value2;
  int line;
  int col;
} Node;

Node *node_create(NodeType type, const char *value, int line, int col);
/* Takes ownership of value (malloc'd or NULL). Prefer over node_create(token_str()). */
Node *node_adopt(NodeType type, char *value, int line, int col);
void node_add_child(Node *parent, Node *child);
void node_free(Node *node);
Node *node_clone(const Node *node);

typedef struct {
  Node *root;
  char *source;
} AST;

AST *ast_create(void);
void ast_free(AST *ast);

#endif
