# Cord-native templates

Plantillas **escritas en `.cord`**, no puertos de themes React. Copiar a un proyecto o usar como referencia.

`cord add <template>` llega cuando el formato de paquete sea estable (Horizonte B). Hasta entonces: este directorio + [`docs/TEMPLATES.md`](../docs/TEMPLATES.md).

| Template | Qué incluye |
|----------|-------------|
| [`counter/`](./counter/) | Componente mínimo state/props tipadas |
| [`landing/`](./landing/) | Landing + CTA + layout |
| [`dashboard/`](./dashboard/) | Shell con nav + área principal |
| [`form-fetch/`](./form-fetch/) | Form + fetch JSON |
| [`docs-shell/`](./docs-shell/) | Docs layout (nav + slot) |

```bash
cp -r templates/landing my-landing
cd my-landing && cordlang check && cordlang run react
```
