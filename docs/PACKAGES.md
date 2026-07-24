# Paquetes Cord locales (`cordlang add`)

MVP de packaging **local** (sin registry remoto). Un paquete Cord es una carpeta de archivos `.cord`, opcionalmente con manifiesto.

## Manifiesto

`cordlang.pkg.json` (preferido):

```json
{
  "name": "ui-kit",
  "entry": "Button.cord"
}
```

Si no hay manifiesto, también vale:

- una carpeta con `.cord` (el nombre del paquete = basename de la carpeta), o
- `cordlang.json` con `"name"` (p. ej. templates del repo).

## Uso

Desde la raíz de un proyecto Cordlang:

```bash
# Copia a src/vendor/<name>/
cordlang add ../path/to/ui-kit
cordlang add /abs/path/to/pkg

# Copia a src/lib/<name>/
cordlang add ../path/to/ui-kit --lib

# Nombre corto: busca ./<name> o templates/<name> (subiendo desde cwd)
cordlang add counter
cordlang add counter --lib
```

Después, en `.cord`:

```cord
use vendor/ui-kit/Button
# o
use lib/ui-kit/Button
```

(La resolución exacta de `use` sigue las reglas de módulos por path relativas a `src/`.)

## Qué no hace (aún)

- No hay registry npm-like ni `cordlang publish`.
- No hay versionado semántico de paquetes ni lockfile.
- No enlaza con symlinks; copia el árbol (`fs_copy_tree`).

Ver también: [`TEMPLATES.md`](./TEMPLATES.md), [`VERSIONING.md`](./VERSIONING.md), ROADMAP F3.
