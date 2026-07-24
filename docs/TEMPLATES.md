# Templates Cord-nativos

Ver [`../templates/`](../templates/) — 5 seeds para vibecode / dogfooding.

| Id | Path | Uso |
|----|------|-----|
| counter | `templates/counter/` | Widget state |
| landing | `templates/landing/` | Marketing mínimo |
| dashboard | `templates/dashboard/` | App shell |
| form-fetch | `templates/form-fetch/` | Forms + data |
| docs-shell | `templates/docs-shell/` | Documentación |

## Crear proyecto desde template

```bash
cordlang init my-app --template counter
cordlang init landing-demo --template landing
cd my-app && cordlang check && cordlang run
```

También: `cordlang init my-app -t dashboard`.

El CLI copia `templates/<id>/` al destino (debe poder resolver `templates/` subiendo desde el cwd — típicamente desde un checkout del repo Cordlang).

## Paquetes locales

Para reutilizar una carpeta de `.cord` **dentro** de un proyecto existente:

```bash
cordlang add templates/counter          # → src/vendor/…
cordlang add ../my-pkg --lib            # → src/lib/…
```

Detalle: [`PACKAGES.md`](./PACKAGES.md).

No convertir templates React de internet. Ampliar solo con UI Cord.
