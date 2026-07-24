# escape-hash-brace

**Symptom:** `\#{…}` was treated as interpolation instead of literal text.

**Expect:** escaped sequence stays literal; bare `#{…}` still interpolates.
