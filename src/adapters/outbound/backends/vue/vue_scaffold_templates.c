#include "vue_scaffold_internal.h"

int vue_scaffold_write_vite_skeleton(const char *project_dir, const char *out) {
  char lang[32];
  char title[256];
  vue_scaffold_load_html_meta(project_dir, lang, sizeof(lang), title, sizeof(title));
  char lang_e[64];
  char title_e[512];
  html_escape_to(lang_e, sizeof(lang_e), lang);
  html_escape_to(title_e, sizeof(title_e), title);

  const char *pkg =
      "{\n"
      "  \"name\": \"cordlang-vue-app\",\n"
      "  \"private\": true,\n"
      "  \"version\": \"0.1.0\",\n"
      "  \"type\": \"module\",\n"
      "  \"scripts\": {\n"
      "    \"dev\": \"vite\",\n"
      "    \"build\": \"vite build\",\n"
      "    \"preview\": \"vite preview\"\n"
      "  },\n"
      "  \"dependencies\": {\n"
      "    \"vue\": \"^3.5.13\",\n"
      "    \"vue-router\": \"^4.5.0\"\n"
      "  },\n"
      "  \"devDependencies\": {\n"
      "    \"@vitejs/plugin-vue\": \"^5.2.1\",\n"
      "    \"autoprefixer\": \"^10.4.20\",\n"
      "    \"postcss\": \"^8.4.49\",\n"
      "    \"tailwindcss\": \"^3.4.17\",\n"
      "    \"vite\": \"^6.0.3\"\n"
      "  }\n"
      "}\n";

  const char *vite =
      "import { defineConfig } from 'vite'\n"
      "import vue from '@vitejs/plugin-vue'\n"
      "\n"
      "export default defineConfig({\n"
      "  plugins: [vue()],\n"
      "})\n";

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
      "import { createApp } from 'vue'\n"
      "import './app.css'\n"
      "import App from './App.vue'\n"
      "import { router } from './router.js'\n"
      "\n"
      "const app = createApp(App)\n"
      "if (router) app.use(router)\n"
      "app.mount('#app')\n";

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
      "  content: ['./index.html', './src/**/*.{js,vue}'],\n"
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

  int rc = 0;
  rc |= vue_scaffold_write_path(out, "package.json", pkg);
  rc |= vue_scaffold_write_path(out, "vite.config.js", vite);
    rc |= vue_scaffold_write_path(out, "index.html", html);
  rc |= vue_scaffold_write_path(out, "tailwind.config.js", tailwind);
  rc |= vue_scaffold_write_path(out, "postcss.config.js", postcss);
  rc |= vue_scaffold_write_path(out, ".gitignore", gitignore);
  rc |= vue_scaffold_write_path(out, "src/main.js", main_js);
  rc |= vue_scaffold_write_path(out, "src/app.css", css);
  /* Stub until scaffold_from_ast overwrites with real theme tokens */
  rc |= vue_scaffold_write_path(out, "src/theme.css",
                   "/* No theme declared. */\n:root {}\n");
  return rc;
}
