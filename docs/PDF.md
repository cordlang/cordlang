# Cordlang ↔ PDF (HTML intermedio)

El backend `pdf` emite el **mismo HTML estático** que `email` (helper
`static_html_generate_from_ir`). Cordlang **no** incluye un motor PDF.

## Uso

```bash
cordlang compile src/app.cord --backend pdf
cordlang run pdf
# → dist/pdf/index.html + dist/pdf/README.md
```

## Conversión externa

```bash
weasyprint dist/pdf/index.html dist/pdf/out.pdf
npx playwright pdf dist/pdf/index.html dist/pdf/out.pdf
wkhtmltopdf dist/pdf/index.html dist/pdf/out.pdf
```

`cordlang run pdf --check` es un **soft check**: busca `weasyprint` / `npx` y
muestra un ejemplo; **no falla** si no hay convertidor.

## Subconjunto

Idéntico a [EMAIL.md](./EMAIL.md): snapshot estático, sin JS reactivo.
`state` / `@click` / `route` no son interactivos.

## Archivos

- Puerto: `name=pdf`, `extension=.html`, `needs_node_check=0`
- Scaffold: `dist/pdf/index.html`, `dist/pdf/README.md`
- Golden opcional: `tests/golden/email_static.pdf.txt` (HTML)
- Regresión: `tests/regression/pdf-static/`
