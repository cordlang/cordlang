/**
 * Static checks for the VS Code extension package.
 * Does NOT load extension.js — the `vscode` module only exists in the editor host.
 */
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const pkgPath = path.join(root, "package.json");
const pkg = JSON.parse(fs.readFileSync(pkgPath, "utf8"));

function die(msg) {
  console.error("FAIL:", msg);
  process.exit(1);
}

if (pkg.name !== "cordlang") die(`expected name cordlang, got ${pkg.name}`);
if (!pkg.main) die("missing main");
if (!pkg.engines?.vscode) die("missing engines.vscode");
if (!pkg.contributes?.languages) die("missing contributes.languages");

const langs = pkg.contributes.languages.map((l) => l.id);
if (!langs.includes("cordlang")) die("cordlang language id missing");

const mainAbs = path.join(root, pkg.main);
if (!fs.existsSync(mainAbs)) die(`main not found: ${pkg.main}`);

const mustExist = [
  "language-configuration.json",
  "snippets/cordlang.json",
  "README.md",
];
for (const rel of mustExist) {
  if (!fs.existsSync(path.join(root, rel))) die(`missing asset: ${rel}`);
}

// Snippet path from package.json must resolve
for (const s of pkg.contributes.snippets || []) {
  const p = path.join(root, s.path);
  if (!fs.existsSync(p)) die(`snippet path missing: ${s.path}`);
}

// Language configuration path
for (const lang of pkg.contributes.languages) {
  if (lang.configuration) {
    const p = path.join(root, lang.configuration);
    if (!fs.existsSync(p)) die(`language config missing: ${lang.configuration}`);
  }
}

// Dependency used by extension.js
const lc = path.join(root, "node_modules", "vscode-languageclient");
if (!fs.existsSync(lc)) {
  die("vscode-languageclient not installed (run npm ci first)");
}

console.log("package check ok", pkg.version);
