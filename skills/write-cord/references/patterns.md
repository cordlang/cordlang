# Cordlang patterns (for write-cord skill)

## Counter

```cord
def Counter
  state count=0
  props label="Counter"
  col gap=16 p=24 center
    h1 "#{label}" size=2xl bold purpose=content importance=primary
    span "#{count}" size=4xl bold purpose=status
    row gap=8 center
      btn "-" @click=setCount(count - 1) variant=outline purpose=action
      btn "+" @click=setCount(count + 1) variant=primary purpose=action importance=primary
```

## Theme + routes + layout

```cord
# app.cord
theme app
  primary: "#2563eb"
  bg: "#fff"
  text: "#111"
  muted: "#6b7280"
  radius: 12

use layouts/default
route / => pages/HomePage
route /about => pages/AboutPage
```

```cord
# layouts/default.cord
layout default
  header
    row between p=16
      link "Home" to=/ purpose=navigation importance=primary
      link "About" to=/about purpose=navigation
  main p=24
    slot
```

## Fetch list

```cord
def Shop
  fetch products = "/api/products.json"
  col gap=16 p=24
    if productsLoading
      p "Loading…" muted purpose=status
    if productsError
      p "Error" muted purpose=status
    for p in products key=p.id
      row gap=8
        span "#{p.name}" bold purpose=content
```

## Form

```cord
def Contact
  action form = submitForm init=null pending=saving
  col gap=16 p=24
    if saving
      p "Sending…" muted purpose=status
    form action=formAction purpose=form
      input email bind=email name=email placeholder="you@example.com"
      btn "Send" variant=primary purpose=action importance=primary
```

## Context

```cord
context Theme = "light"
def Shell
  state theme="dark"
  provide Theme value=theme
    slot
def Child
  ctx theme = Theme
  p "Theme: #{theme}"
```

## Semantic metadata

Optional attrs (vocabulary in `docs/schema/attrs.json`):

- `purpose`: navigation | content | action | form | status | decoration | landmark
- `importance`: primary | secondary | tertiary | optional | critical
