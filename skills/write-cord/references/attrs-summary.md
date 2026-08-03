# Cordlang attrs summary (skill bundle)

Critical surface for agents. Full schema: https://raw.githubusercontent.com/cordlang/cordlang/main/docs/schema/attrs.json (upstream `docs/schema/attrs.json`).

## Prop types

`string` \| `number` \| `boolean` \| `any`

## Layout tags

`col` `row` `stack` `page` `card` `grid` `group` `fragment` `section`

## Control / HTML-ish tags

`btn` `button` `link` `input` `textarea` `select` `checkbox` `radio` `form` `label` `img` `icon` `span` `p` `h1`–`h6` `ul` `ol` `li` `nav` `header` `footer` `main` `aside` `article` `div` `a` `table`… `slot`

## Special tags

`provide` `portal` `suspense` `loading` `errorBoundary` `empty` `if` `else` `for` `await` `snippet` `render` `motion` `chart` `foreign`

## Style attrs (common)

`variant` `size` `color` `gap` `cols` `p` `m` `px` `py` `mx` `my` `bg` `shadow` `rounded` `max-w` `min-h` `overflow` `w` `h` `center` `between` `bold` `muted` `sticky` `primary` `outline` `ghost` `border` `flex-1` `font-mono` `leading` `tracking` `elevate` `density`

- `type` scale: `display` \| `title` \| `body` \| `caption` \| `code`
- Responsive prefixes: `sm:` `md:` `lg:`
- Density: `compact` \| `comfortable` \| `spacious`

## DOM / binding attrs (common)

`id` `name` `type` `value` `placeholder` `href` `to` `src` `alt` `class` `style` `bind` `key` `use` `transition` `action` `disabled` `required` `aria-*` `purpose` `importance`

## Semantic vocabulary

| Attr | Values |
|------|--------|
| `purpose` | `navigation` `content` `action` `form` `status` `decoration` `landmark` |
| `importance` | `primary` `secondary` `tertiary` `optional` `critical` |

## Events

Prefix `@` — e.g. `@click=setCount(count+1)`. Not JSX `onClick=`.

## Forbidden in `.cord`

**Attrs:** `className` `onClick` `onChange` `onSubmit` `onInput` `htmlFor` `defaultValue` `tabIndex` `dangerouslySetInnerHTML` (and other React DOM names).

**As tags/hooks:** `useState` `useEffect` `useMemo` `useCallback` `useRef` `useContext` `Link` `Fragment` — use Cord decls (`state`, `effect`, `link`, …).

## Check codes (AI traps)

| code | Meaning |
|------|---------|
| `jsx-attr` | Forbidden JSX/React attribute |
| `jsx-hook` | Hook / JSX component as Cord tag |
| `jsx-tag` | Angle-bracket JSX in `.cord` |
| `jsx-map` | `.map(` — use `for … key=` |
| `bad-interp` | `{expr}` — use `#{expr}` |
| `semantic-vocab` | `purpose`/`importance` outside vocabulary |

## Notes

- PascalCase tags are components; arbitrary props allowed.
- Interpolation is `#{expr}`; escape literal with `\#{`.
- `aria-*` and `data-*` are always known; `img` without `alt` warns.
- Capabilities: declare `presets` in `cordlang.json` — do not `import` framework libs in `.cord`.
