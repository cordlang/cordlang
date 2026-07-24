# strip-debug-pass

**Symptom:** need an opt-in IR transform that drops debug attrs before codegen.

**Expect:** with `--pass strip-debug`, `debug` / `data-debug*` attrs do not appear in emit.
