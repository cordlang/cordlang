# theme-css-sanitize

**Symptom:** theme token values like `red; } body { …` were interpolated raw into `theme.css`.

**Fix:** `theme_css.c` `emit_css_var` rejects unsafe keys/values (`;`, `{`, `}`, quotes, controls) and emits a skip comment instead.

Covered by unit-style logic in `emit_css_var` + goldens that still emit normal themes.
