# Cordlang Playground

In-browser compiler (WASM). Build artifacts with:

```powershell
powershell -ExecutionPolicy Bypass -File playground\build_wasm.ps1
```

```bash
bash playground/build_wasm.sh
# or: make wasm
```

Native smoke (no Docker / emcc):

```powershell
powershell -ExecutionPolicy Bypass -File playground\smoke_native.ps1
```

```bash
bash playground/smoke_native.sh
```

Then serve this folder over HTTP:

```bash
npx --yes serve playground
```

Docs: [`docs/PLAYGROUND.md`](../docs/PLAYGROUND.md).
