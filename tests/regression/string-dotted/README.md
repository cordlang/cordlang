# string-dotted

**Symptom:** dotted filenames / escaped quotes inside strings corrupted codegen.

**Expect:** `"build.bat"` and `\"hi\"` remain string literals in React/Svelte emit.
