# ia-classname-trap

**Symptom:** JSX `className=` slipped into `.cord` and passed unnoticed.

**Expect:** `cordlang check` exits non-zero and mentions `className`.
