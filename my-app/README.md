# my-app — Cordlang multi-file demo

Demo multi-archivo del compilador. Generados (`dist/`, `node_modules/`) **no** van al git.

```bash
# desde la raíz del repo (tras build.bat)
cd my-app
..\cordlang.exe run
..\cordlang.exe run react
..\cordlang.exe run svelte --check
..\cordlang.exe check
..\cordlang.exe symbols
```

Docs del lenguaje: [../README.md](../README.md) · [../docs/ROADMAP.md](../docs/ROADMAP.md)

## Estructura

```
src/
  app.cord                 # solo layout + routes
  layouts/
    default.cord           # shell (header / slot / footer)
  pages/
    HomePage.cord
    CounterPage.cord
    ShopPage.cord
    AboutPage.cord
  components/
    Counter.cord
    ProductCard.cord
```

## Sistema de módulos

```cord
# app.cord — rutas cargan páginas por path
use layouts/default
route / => pages/HomePage
route /counter => pages/CounterPage

# pages/CounterPage.cord — importa componentes
use ../components/Counter
col
  Counter label="Live"
```

| Directiva | Efecto |
|-----------|--------|
| `use path` | Carga `.cord` y mergea defs |
| `use path as Name` | Alias del export |
| `import Name from path` | Igual, estilo ES |
| `route /x => pages/Foo` | Carga el módulo y enruta a `Foo` |

Si un archivo no tiene `def`, el body se envuelve con el nombre del archivo.

## Comandos

```bash
..\cordlang.exe run              # preview HTML
..\cordlang.exe run react        # export React multi-file
..\cordlang.exe compile src/app.cord --ast
```
