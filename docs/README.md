# Cordlang documentation

Start here for humans and for AI tools.

**Product north star:** vibecode + AI agents with **minimal token spend**. Dense `.cord` → deterministic `check` → **Official** ESM/React/Svelte/Vue. Other backends are experimental/meta. Tiers: [`BACKENDS.md`](./BACKENDS.md). If docs conflict, [`AI_CONTEXT.md`](./AI_CONTEXT.md) / [`AI.md`](./AI.md) win for intent.

| Doc | Audience | Content |
|-----|----------|---------|
| [GUIDE.md](./GUIDE.md) | Everyone | Practical guide **with samples** |
| [CHEATSHEET.md](./CHEATSHEET.md) | Everyone | One-screen syntax |
| [EXAMPLES.md](./EXAMPLES.md) | Everyone | Catalog of `examples/` + `templates/` |
| [AI_CONTEXT.md](./AI_CONTEXT.md) | **AI models** | Compact contract (start here; token-cheap) |
| [AI.md](./AI.md) | **AI models** | Full do / don't contract for LLMs |
| [LANGUAGE.md](./LANGUAGE.md) | Design | Language design notes (ex-`idea.md`) |
| [SPEC.md](./SPEC.md) | Language | Normative syntax & semantics (v0.x) |
| [BACKENDS.md](./BACKENDS.md) | Everyone | **Official / Experimental** tiers |
| [REACT.md](./REACT.md) | Backend map | Cordlang ↔ React |
| [SVELTE.md](./SVELTE.md) | Backend map | Cordlang ↔ Svelte 5 |
| [VUE.md](./VUE.md) | Backend map | Cordlang ↔ Vue 3 (Official) |
| [VUE_PROMOTION.md](./VUE_PROMOTION.md) | History | Vue Candidate → Official checklist (done) |
| [SOLID.md](./SOLID.md) | Backend map | Cordlang ↔ Solid (Experimental) |
| [EMAIL.md](./EMAIL.md) | Backend map | Static email HTML |
| [PDF.md](./PDF.md) | Backend map | HTML → external PDF |
| [NEXT.md](./NEXT.md) | Meta backend | Next.js SPA wrap (not RSC) |
| [SVELTEKIT.md](./SVELTEKIT.md) | Meta backend | SvelteKit SPA wrap (not full SSR) |
| [TEMPLATES.md](./TEMPLATES.md) | Seeds | `init --template` + templates/ |
| [PACKAGES.md](./PACKAGES.md) | Ecosystem | `cordlang add` local packages |
| [VERSIONING.md](./VERSIONING.md) | Policy | Language 1.0 vs CLI semver |
| [PLAYGROUND.md](./PLAYGROUND.md) | Ecosystem | WASM playground MVP |
| [NATIVE.md](./NATIVE.md) | Experimental | Flutter / SwiftUI / Compose contract |
| [IR.md](./IR.md) | Compiler | IR pipeline |
| [ARCHITECTURE.md](./ARCHITECTURE.md) | Contributors | Parser → AST → IR → codegen |
| [ROADMAP.md](./ROADMAP.md) | Planning | Phases A–H, next steps |

## Repo entry points

| Path | Purpose |
|------|---------|
| [../README.md](../README.md) | Install, CLI, status |
| [../AGENTS.md](../AGENTS.md) | Instructions for coding agents |
| [../skills/write-cord/SKILL.md](../skills/write-cord/SKILL.md) | Portable AI skill |
| [../skills/README.md](../skills/README.md) | Skills convention |
| [../.github/copilot-instructions.md](../.github/copilot-instructions.md) | Copilot brief |
| [../templates/](../templates/) | Multi-file starters |
| [../examples/](../examples/) | Single-file samples |

## Learning path

1. [GUIDE.md](./GUIDE.md) §1–3 (install + core samples)  
2. Open `templates/counter/` and `examples/counter.cord`  
3. [CHEATSHEET.md](./CHEATSHEET.md) while coding  
4. [REACT.md](./REACT.md) / [SVELTE.md](./SVELTE.md) when mapping mental models  
5. If you are an LLM: [AI_CONTEXT.md](./AI_CONTEXT.md) → [AI.md](./AI.md) + [AGENTS.md](../AGENTS.md)  
