#include "svelte_scaffold_internal.h"

int svelte_scaffold_write_vite_skeleton(const char *project_dir, const char *out) {
  char lang[32];
  char title[256];
  svelte_scaffold_load_html_meta(project_dir, lang, sizeof(lang), title, sizeof(title));
  char lang_e[64];
  char title_e[512];
  html_escape_to(lang_e, sizeof(lang_e), lang);
  html_escape_to(title_e, sizeof(title_e), title);

  const char *pkg =
      "{\n"
      "  \"name\": \"cordlang-svelte-app\",\n"
      "  \"private\": true,\n"
      "  \"version\": \"0.1.0\",\n"
      "  \"type\": \"module\",\n"
      "  \"scripts\": {\n"
      "    \"dev\": \"vite\",\n"
      "    \"build\": \"vite build\",\n"
      "    \"preview\": \"vite preview\"\n"
      "  },\n"
      "  \"devDependencies\": {\n"
      "    \"@sveltejs/vite-plugin-svelte\": \"^5.0.3\",\n"
      "    \"autoprefixer\": \"^10.4.20\",\n"
      "    \"postcss\": \"^8.4.49\",\n"
      "    \"svelte\": \"^5.16.0\",\n"
      "    \"tailwindcss\": \"^3.4.17\",\n"
      "    \"vite\": \"^6.0.3\"\n"
      "  }\n"
      "}\n";

  const char *vite =
      "import { defineConfig } from 'vite'\n"
      "import { svelte } from '@sveltejs/vite-plugin-svelte'\n"
      "\n"
      "export default defineConfig({\n"
      "  plugins: [svelte()],\n"
      "})\n";

  /* Svelte 5: runes mode is inferred when files use $state/$props/$derived.
   * Do NOT set compilerOptions.runes — some language-tools versions report
   * "Unrecognized option 'runes'" and pin the diagnostic on the first line
   * of every .svelte file (often our <!-- cordlang: source=... --> comment). */
  const char *svelte_cfg =
      "import { vitePreprocess } from '@sveltejs/vite-plugin-svelte'\n"
      "\n"
      "/** @type {import('@sveltejs/vite-plugin-svelte').SvelteConfig} */\n"
      "export default {\n"
      "  preprocess: vitePreprocess(),\n"
      "}\n";

  char html[1024];
  snprintf(html, sizeof(html),
           "<!doctype html>\n"
           "<html lang=\"%s\">\n"
           "  <head>\n"
           "    <meta charset=\"UTF-8\" />\n"
           "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" />\n"
           "    <title>%s</title>\n"
           "  </head>\n"
           "  <body>\n"
           "    <div id=\"app\"></div>\n"
           "    <script type=\"module\" src=\"/src/main.js\"></script>\n"
           "  </body>\n"
           "</html>\n",
           lang_e, title_e);

  const char *main_js =
      "import { mount } from 'svelte'\n"
      "import './app.css'\n"
      "import App from './App.svelte'\n"
      "\n"
      "mount(App, { target: document.getElementById('app') })\n";

  const char *css =
      "/* Theme tokens from Cord `theme` blocks (see theme.css) */\n"
      "@import './theme.css';\n"
      "\n"
      "@tailwind base;\n"
      "@tailwind components;\n"
      "@tailwind utilities;\n"
      "\n"
      "@layer components {\n"
      "  .btn {\n"
      "    @apply inline-flex items-center justify-center rounded-lg px-4 py-2 font-medium transition;\n"
      "    border-radius: var(--radius, 0.5rem);\n"
      "  }\n"
      "  .btn-primary {\n"
      "    @apply text-white;\n"
      "    background-color: var(--color-primary, #2563eb);\n"
      "  }\n"
      "  .btn-primary:hover {\n"
      "    filter: brightness(0.92);\n"
      "  }\n"
      "  .btn-outline {\n"
      "    @apply border border-gray-300 bg-white text-gray-900 hover:bg-gray-50;\n"
      "  }\n"
      "  .btn-ghost {\n"
      "    @apply text-gray-700 hover:bg-gray-100;\n"
      "  }\n"
      "  .card {\n"
      "    @apply bg-white shadow-md overflow-hidden;\n"
      "    border-radius: var(--radius, 0.75rem);\n"
      "  }\n"
      "}\n"
      "\n"
      "body {\n"
      "  @apply m-0 min-h-screen antialiased;\n"
      "  background-color: var(--color-bg, #f9fafb);\n"
      "  color: var(--color-text, #111827);\n"
      "}\n"
      "\n"
      ".text-muted {\n"
      "  color: var(--color-muted, #57534e);\n"
      "}\n"
      "\n"
      "a {\n"
      "  color: var(--color-primary, #2563eb);\n"
      "}\n"
      "a:hover {\n"
      "  text-decoration: underline;\n"
      "}\n";

  /* Same Cord spacing scale as react_scaffold (p=16 → 1rem, not Tailwind 4rem). */
  const char *tailwind =
      "/** @type {import('tailwindcss').Config} */\n"
      "export default {\n"
      "  content: ['./index.html', './src/**/*.{js,svelte}'],\n"
      "  theme: {\n"
      "    extend: {\n"
      "      spacing: {\n"
      "        8: '0.5rem',\n"
      "        12: '0.75rem',\n"
      "        16: '1rem',\n"
      "        24: '1.5rem',\n"
      "        32: '2rem',\n"
      "        40: '2.5rem',\n"
      "        48: '3rem',\n"
      "        64: '4rem',\n"
      "        240: '15rem',\n"
      "      },\n"
      "      width: {\n"
      "        240: '15rem',\n"
      "      },\n"
      "      maxWidth: {\n"
      "        640: '40rem',\n"
      "        720: '45rem',\n"
      "      },\n"
      "      borderRadius: {\n"
      "        8: '8px',\n"
      "        12: '12px',\n"
      "      },\n"
      "    },\n"
      "  },\n"
      "  plugins: [],\n"
      "}\n";

  const char *postcss =
      "export default {\n"
      "  plugins: {\n"
      "    tailwindcss: {},\n"
      "    autoprefixer: {},\n"
      "  },\n"
      "}\n";

  const char *gitignore =
      "node_modules\n"
      "dist\n"
      ".DS_Store\n"
      "*.local\n";

  /* Helps editors resolve Svelte 5 + $props/$state from THIS project,
   * not a global/old language-server schema. */
  const char *jsconfig =
      "{\n"
      "  \"compilerOptions\": {\n"
      "    \"moduleResolution\": \"bundler\",\n"
      "    \"target\": \"ESNext\",\n"
      "    \"module\": \"ESNext\",\n"
      "    \"verbatimModuleSyntax\": true,\n"
      "    \"isolatedModules\": true,\n"
      "    \"resolveJsonModule\": true,\n"
      "    \"sourceMap\": true,\n"
      "    \"strict\": false,\n"
      "    \"skipLibCheck\": true\n"
      "  },\n"
      "  \"include\": [\"src/**/*.js\", \"src/**/*.svelte\"]\n"
      "}\n";

  const char *vscode_ext =
      "{\n"
      "  \"recommendations\": [\"svelte.svelte-vscode\"]\n"
      "}\n";

  const char *vscode_settings =
      "{\n"
      "  \"files.associations\": {\n"
      "    \"*.svelte\": \"svelte\"\n"
      "  },\n"
      "  \"svelte.enable-ts-plugin\": true,\n"
      "  \"svelte.plugin.svelte.compilerWarnings\": {\n"
      "    \"a11y-click-events-have-key-events\": \"ignore\",\n"
      "    \"a11y-no-static-element-interactions\": \"ignore\"\n"
      "  }\n"
      "}\n";

  const char *readme_ide =
      "# Cordlang → Svelte\n"
      "\n"
      "Generated by `cordlang run svelte`.\n"
      "\n"
      "## IDE errors (`Unexpected character '@'`, `runes`, `@render`)\n"
      "\n"
      "These usually mean the **editor** is not using Svelte 5 language tools:\n"
      "\n"
      "1. Install extension **Svelte for VS Code** (`svelte.svelte-vscode`).\n"
      "2. Open **this folder** (`dist/svelte`) as the workspace root, **or**\n"
      "   ensure the extension finds `svelte.config.js` + `node_modules/svelte` here.\n"
      "3. Command palette → **Svelte: Restart Language Server**.\n"
      "4. Do **not** open `.svelte` files as plain HTML.\n"
      "\n"
      "Runtime (`npm run dev` / `bun run dev`) can work even if the IDE squiggles.\n"
      "\n"
      "```bash\n"
      "npm install   # or bun install\n"
      "npm run dev\n"
      "```\n";

  int rc = 0;
  rc |= svelte_scaffold_write_path(out, "package.json", pkg);
  rc |= svelte_scaffold_write_path(out, "vite.config.js", vite);
  rc |= svelte_scaffold_write_path(out, "svelte.config.js", svelte_cfg);
  rc |= svelte_scaffold_write_path(out, "jsconfig.json", jsconfig);
  rc |= svelte_scaffold_write_path(out, "index.html", html);
  rc |= svelte_scaffold_write_path(out, "tailwind.config.js", tailwind);
  rc |= svelte_scaffold_write_path(out, "postcss.config.js", postcss);
  rc |= svelte_scaffold_write_path(out, ".gitignore", gitignore);
  rc |= svelte_scaffold_write_path(out, ".vscode/extensions.json", vscode_ext);
  rc |= svelte_scaffold_write_path(out, ".vscode/settings.json", vscode_settings);
  rc |= svelte_scaffold_write_path(out, "README.md", readme_ide);
  rc |= svelte_scaffold_write_path(out, "src/main.js", main_js);
  rc |= svelte_scaffold_write_path(out, "src/app.css", css);
  /* Stub until scaffold overwrites with real theme tokens */
  rc |= svelte_scaffold_write_path(out, "src/theme.css",
                   "/* No theme declared. */\n:root {}\n");
  return rc;
}
