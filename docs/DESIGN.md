# Cord design system

Dense `.cord` should produce **professional UI**, not only valid compile output.

## Theme tokens

```cord
theme brand
  primary: "#0f766e"
  bg: "#fafaf9"
  text: "#1c1917"
  muted: "#78716c"
  surface: "#ffffff"
  border: "#e7e5e4"
  radius: 12
  font: "IBM Plex Sans"
```

`cordlang run` / `run react|svelte|…` writes `theme.css` with:

- `--color-*` from color keys (plus semantic fallbacks: `surface`, `surface-2`, `border`, `on-primary`)
- **Native fonts:** `font` / `font-sans` / `font-mono` / `font-display` → `--font-*` **and** `@font-face` (WOFF2 cached under `~/.cordlang/cache/fonts`, copied to `public/fonts` or served as `/@cord/fonts/…`). No Google Fonts `<link>` in the shell.
- Type scale: `.type-display|title|body|caption|code`
- Elevation: `.elevate-0` … `.elevate-4`
- Density: `.density-compact|comfortable|spacious`
- Motion: `--duration-fast|normal|slow`, `--ease-standard`
- `.cord-section` max-width container

Without any `font*` key, the UI uses `system-ui` (zero font bytes). Declaring `font: "Inter"` downloads once (needs `curl` + network), then every project reuses the disk cache.

## Typography

```cord
h1 "Headline" type=display
p "Supporting copy" type=body
span "Meta" type=caption
```

Values: `display` | `title` | `body` | `caption` | `code`.  
(HTML `type=` on inputs is unchanged when the value is not a scale name.)

## Elevation & density

```cord
card elevate=2 p=24
stack density=spacious gap=16
```

## Composition

- `section` — page band with `.cord-section` (centered max-width)
- `stack` — column flex (same as `col`) + optional `density=`

## Responsive prefixes

```cord
section md:p=48 p=24
row md:gap=24 gap=12
grid cols=1 md:cols=3
```

Prefixes: `sm:`, `md:`, `lg:` → Tailwind responsive classes.

## Motion budget

Prefer presets (`motion fade`) with 1–2 motions per viewport. See [LIBRARIES.md](./LIBRARIES.md).

## Anti-patterns

- Inventing Framer/Lucide imports inside `.cord`
- Hero full of cards/stats strips
- Ignoring `theme` and hardcoding one-off colors in every page
