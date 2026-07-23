# Cordlang patterns (for write-cord skill)

## Counter

```cord
def Counter
  state count=0
  props label="Counter"
  col gap=16 p=24 center
    h1 "#{label}" size=2xl bold
    span "#{count}" size=4xl bold
    row gap=8 center
      btn "-" @click=setCount(count - 1) variant=outline
      btn "+" @click=setCount(count + 1) variant=primary
```

## Fetch list

```cord
def Shop
  fetch products = "/api/products.json"
  col gap=16 p=24
    if productsLoading
      p "Loading…" muted
    if productsError
      p "Error" muted
    for p in products key=p.id
      row gap=8
        span "#{p.name}" bold
```

## Form

```cord
def Contact
  action form = submitForm init=null pending=saving
  col gap=16 p=24
    if saving
      p "Sending…" muted
    form action=formAction
      input email bind=email name=email placeholder="you@example.com"
      btn "Send" variant=primary
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
