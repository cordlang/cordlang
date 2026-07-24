# Cordlang ↔ Email HTML

Static, email-client-safe HTML from the canonical IR. **No reactive JS.**

## Uso

```bash
cordlang compile src/app.cord --backend email
cordlang run email
# → dist/email/index.html
```

`cordlang check --backend email` **no existe**: el checker semántico es
independiente del backend. Para email, las limitaciones son de **codegen**:

| Feature SPA | Email |
|-------------|--------|
| `state` / `bind` | Snapshot del valor inicial en el markup |
| `@click` / events | Ignorados (CTA como `<a href="#">` estático) |
| `route` | No hay SPA router; se omite la navegación |
| CSS classes / Tailwind | Inline `style=` + tablas `role="presentation"` |

Si el IR contiene `IR_STATE` / `IR_EVENT` / `IR_ROUTE`, `compile`/`run`
pueden imprimir un `note:` en stderr (aviso, no error).

## Subconjunto

- Layout `col` / `row` / `page` / `stack` → tablas anidadas
- `card`, tipografía, `btn` (como enlace), `link`, `img`, `h1`–`h3`, `p`
- Interpolaciones `#{x}` resueltas con init de `state`/`props` cuando existe
- `if` → rama true estática; `for` → 1 muestra comentada

## Archivos

- Puerto: `name=email`, `extension=.html`, `needs_node_check=0`
- Emit compartido: `backends/static_html/static_html.c`
- Scaffold: `dist/email/index.html`

## Tests

- Fixture: `tests/fixtures/email_static.cord`
- Golden: `tests/golden/email_static.email.txt`
- Regresión: `tests/regression/email-static/`

**Nota:** email **no** entra en el loop principal `BACKENDS=(react svelte vue solid)`
para no exigir goldens de todos los fixtures SPA; se cubre con fixture dedicado + regresión.
