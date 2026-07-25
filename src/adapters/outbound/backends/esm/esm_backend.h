#ifndef CORDLANG_ESM_BACKEND_H
#define CORDLANG_ESM_BACKEND_H

#include "application/ports/backend_port.h"
#include "domain/ir.h"

/*
 * Native ES-module backend.
 *
 * Every .cord file lowers to ONE standalone ES module whose imports point at
 * other .cord URLs. The browser then walks the real module graph — the same
 * shape Vite gives .vue / .svelte, where the dev server compiles each request
 * on the fly and answers with `Content-Type: text/javascript`. Nothing is
 * bundled and nothing is flattened into a single document.
 *
 * This is deliberately NOT the html backend: that one pre-renders one static
 * document and re-binds it with data-bind attributes over window globals, so
 * components, per-component state and routing all collapse.
 */

/* Module kind, decided by the dev server from the request path. */
typedef enum {
  ESM_MOD_ENTRY,     /* the project entry: routes + theme + layouts */
  ESM_MOD_COMPONENT, /* any other .cord: single default-exported component */
} EsmModuleKind;

typedef struct {
  EsmModuleKind kind;
  /* Export name for a component module (HomePage, DefaultLayout, …). */
  const char *export_name;
  /* Source file, relative to project root, for the header comment. */
  const char *source_rel;
  /* Absolute path of the file being compiled (module ref resolution base). */
  const char *abs_path;
  /* Project root, used to jail module resolution and build /src/... URLs. */
  const char *project_root;
} EsmModuleCtx;

/*
 * Compile a single parsed .cord module (IR of ONE file, dependencies NOT
 * inlined) into ES module source. Never NULL; caller frees.
 */
char *esm_generate_module(IrProgram *ir, const EsmModuleCtx *ctx);

/* BackendPort surface: whole-project single-file emit (compile --backend esm). */
char *esm_generate_from_ir(IrProgram *ir);
char *esm_generate(Node *root);

const BackendPort *esm_backend_port(void);

/* ── static assets served by the dev server ─────────────── */

/* The runtime: h/diff/hooks/router/mount. Static string, do not free. */
const char *esm_runtime_js(void);

/* Reload client (served at /@cord/client and /@cord/hmr.js). Static, do not free. */
const char *esm_hmr_client_js(void);

/* Error-overlay module body for a failed compile. Caller frees. */
char *esm_error_module(const char *message);

/*
 * Utility CSS for exactly the classes this project emits (a tiny JIT over the
 * Cord attr scale) plus the shared base/reset. Caller frees.
 */
char *esm_base_css(IrProgram *ir);

/* Vite-style index.html: script src=entry + /@cord/client (entry self-mounts). */
char *esm_index_html(const char *lang, const char *title, const char *entry_url,
                     int has_site_css, int has_site_js, int has_favicon,
                     int has_logo_svg);

#endif
