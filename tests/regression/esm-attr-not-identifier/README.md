# esm-attr-not-identifier

**Symptom.** Templates crashed at render with `ReferenceError: action is not defined`
(`templates/landing`), `content is not defined` (`templates/counter`,
`templates/form-fetch`), `button is not defined` and `forma is not defined`
(docs site).

**Cause.** Cord attribute values are bare words, so `purpose=action` is
indistinguishable in shape from `count`. The emitter treated any lowercase
identifier-looking value as a JS expression.

**Fix.** The ESM emitter tracks the identifiers actually declared in the
component (props, state, computed, refs, fetch results, route params, `for`
variables) and only emits an expression when the name is in scope. Known DOM
attributes (`type`, `id`, `src`, …) additionally require a dotted path.

This pin locks the three shapes together: a bare word attr (`type=button`,
`purpose=action`, `id=forma`) must be a string, while a declared name
(`label`, `count`) must stay an expression.

**Note.** The React backend still emits `purpose={action}` for this input —
the same latent bug, tracked separately. This case pins the `esm` output only.
