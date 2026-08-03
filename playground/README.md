# Cordlang Playground

In-browser compiler (WASM). Build artifacts with:

```powershell
powershell -ExecutionPolicy Bypass -File playground\build_wasm.ps1
```

Then serve this folder over HTTP:

```bash
npx --yes serve playground
```

Docs: [`docs/PLAYGROUND.md`](../docs/PLAYGROUND.md).
