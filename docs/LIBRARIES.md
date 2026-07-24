# Libraries & capabilities (multi-backend)

Cord keeps **dense `.cord` source** and treats third-party UI as **capabilities**, not as React-only npm APIs.

## Model

| Layer | What | Backend role |
|-------|------|----------------|
| Cord packages | `cordlang add` → `vendor/` / `lib/` | Same `.cord` everywhere |
| **Presets** | `icons`, `motion`, `charts`, `tailwind` in `cordlang.json` | Each scaffold merges the right npm packages + bridges |
| **foreign** | Host widgets with a **per-backend module map** | Emit imports for the active backend |

```bash
cordlang preset list
cordlang preset add icons motion charts
cordlang run react --check    # lucide-react, framer-motion, recharts
cordlang run svelte --check   # lucide-svelte, svelte/transition, …
```

## cordlang.json

```json
{
  "name": "my-app",
  "entry": "src/app.cord",
  "presets": ["tailwind", "icons", "motion"],
  "deps": {
    "css": ["@fontsource/inter"]
  }
}
```

## Surface in `.cord`

```cord
icon name=sparkles size=20
motion fade
  p "Hello"
chart type=bar data=sales

foreign MapView from "react-leaflet" for react
  props center: any, zoom: number

foreign MapView
  props center: any, zoom: number
  react from "react-leaflet"
  svelte from "svelte-leaflet"
```

`cordlang check` errors with `missing-preset` if you use `icon` / `motion` / `chart` without the capability.

## Bridges

SPA scaffolds write thin adapters when presets are enabled:

- React: `src/CordIcon.jsx`, `CordMotion.jsx`, `CordChart.jsx`
- Svelte: `src/CordIcon.svelte`, `CordMotion.svelte`, `CordChart.svelte` (icon bridge is thin MVP; npm peer `@lucide/svelte`)
- Vue: `src/CordIcon.vue`, `CordMotion.vue`, `CordChart.vue`
- Solid: `src/CordIcon.jsx`, `CordMotion.jsx`, `CordChart.jsx`

Do **not** teach models to write `import { motion } from "framer-motion"` in `.cord` — use presets + tags.

## Packages Cord-only

See [PACKAGES.md](./PACKAGES.md). Example kit: `templates/ui-kit`.

## Preview HTML

Native preview does **not** run Framer/Lucide — use `cordlang run react|svelte` for capability UIs.
