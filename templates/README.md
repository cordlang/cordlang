# Cord-native templates

Plantillas **escritas en `.cord`**, no puertos de themes React.

```bash
cordlang init my-app --template counter
cordlang init my-landing --template landing
```

Ids: `counter`, `landing`, `dashboard`, `form-fetch`, `docs-shell`.

Docs: [`docs/TEMPLATES.md`](../docs/TEMPLATES.md) · paquetes locales: [`docs/PACKAGES.md`](../docs/PACKAGES.md).

| Template | Qué incluye |
|----------|-------------|
| [`counter/`](./counter/) | Componente mínimo state/props tipadas |
| [`landing/`](./landing/) | Landing + CTA + layout |
| [`dashboard/`](./dashboard/) | Shell con nav + área principal |
| [`form-fetch/`](./form-fetch/) | Form + fetch JSON |
| [`docs-shell/`](./docs-shell/) | Docs layout (nav + slot) |

```bash
# Alternativa manual
cp -r templates/landing my-landing
cd my-landing && cordlang check && cordlang run react
```
