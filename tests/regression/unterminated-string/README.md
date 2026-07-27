# unterminated-string

**Symptom:** `btn "Primary…` without a closing `"` was lexed as a normal
string (content until EOF), so the preview rendered the broken text and never
showed the compile overlay.

**Expect:** lexer/parser error mentioning `unterminated string`; `check` exits
non-zero.
