#!/usr/bin/env node
/**
 * Stdio LSP smoke for textDocument/references, prepareRename, rename.
 * Usage: node tests/lsp_rename_refs.mjs <cordlang-bin>
 */
import { spawn } from "node:child_process";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(__dirname, "..");
const binArg = process.argv[2] || path.join(root, "cordlang");
const bin = path.resolve(binArg);
const fixture = path.join(root, "tests", "fixtures", "lsp_rename");

function fileUri(p) {
  const abs = path.resolve(p).replace(/\\/g, "/");
  return abs.startsWith("/") ? `file://${abs}` : `file:///${abs}`;
}

function read(rel) {
  return fs.readFileSync(path.join(fixture, rel), "utf8");
}

function encode(obj) {
  const body = JSON.stringify(obj);
  return Buffer.concat([
    Buffer.from(`Content-Length: ${Buffer.byteLength(body)}\r\n\r\n`, "utf8"),
    Buffer.from(body, "utf8"),
  ]);
}

function parseFrames(buf) {
  const frames = [];
  let i = 0;
  while (i < buf.length) {
    const view = buf.slice(i).toString("latin1");
    const m = view.match(/Content-Length:\s*(\d+)/i);
    if (!m || m.index == null) break;
    const n = parseInt(m[1], 10);
    const after = m.index + m[0].length;
    const rest = view.slice(after);
    /* Standard \r\n\r\n, Unix \n\n, or CRT text-mode \r\r\n\r\r\n. */
    const sep = rest.match(/^\r*\n\r*\n/);
    if (!sep) break;
    const bodyStart = i + after + sep[0].length;
    if (buf.length < bodyStart + n) break;
    const json = buf.slice(bodyStart, bodyStart + n).toString("utf8");
    frames.push(JSON.parse(json));
    i = bodyStart + n;
  }
  return frames;
}

function fail(msg) {
  console.error("FAIL: lsp rename/references —", msg);
  process.exit(1);
}

const boxPath = path.join(fixture, "src/components/Box.cord");
const homePath = path.join(fixture, "src/pages/HomePage.cord");
const boxUri = fileUri(boxPath);
const homeUri = fileUri(homePath);
const boxText = read("src/components/Box.cord");
const homeText = read("src/pages/HomePage.cord");

const child = spawn(bin, ["lsp"], {
  stdio: ["pipe", "pipe", "pipe"],
  cwd: fixture,
  windowsHide: true,
});
child.on("error", (err) => {
  fail(`spawn ${bin}: ${err.message}`);
});

let stdout = Buffer.alloc(0);
let stderr = Buffer.alloc(0);
child.stdout.on("data", (d) => {
  stdout = Buffer.concat([stdout, d]);
});
child.stderr.on("data", (d) => {
  stderr = Buffer.concat([stderr, d]);
});

const msgs = [
  {
    jsonrpc: "2.0",
    id: 1,
    method: "initialize",
    params: { capabilities: {}, rootUri: fileUri(fixture) },
  },
  { jsonrpc: "2.0", method: "initialized", params: {} },
  {
    jsonrpc: "2.0",
    method: "textDocument/didOpen",
    params: {
      textDocument: {
        uri: boxUri,
        languageId: "cordlang",
        version: 1,
        text: boxText,
      },
    },
  },
  {
    jsonrpc: "2.0",
    method: "textDocument/didOpen",
    params: {
      textDocument: {
        uri: homeUri,
        languageId: "cordlang",
        version: 1,
        text: homeText,
      },
    },
  },
  {
    jsonrpc: "2.0",
    id: 2,
    method: "textDocument/references",
    params: {
      textDocument: { uri: boxUri },
      position: { line: 0, character: 4 },
      context: { includeDeclaration: true },
    },
  },
  {
    jsonrpc: "2.0",
    id: 3,
    method: "textDocument/references",
    params: {
      textDocument: { uri: boxUri },
      position: { line: 0, character: 4 },
      context: { includeDeclaration: false },
    },
  },
  {
    jsonrpc: "2.0",
    id: 4,
    method: "textDocument/prepareRename",
    params: {
      textDocument: { uri: homeUri },
      position: { line: 2, character: 0 },
    },
  },
  {
    jsonrpc: "2.0",
    id: 5,
    method: "textDocument/prepareRename",
    params: {
      textDocument: { uri: boxUri },
      position: { line: 0, character: 4 },
    },
  },
  {
    jsonrpc: "2.0",
    id: 6,
    method: "textDocument/rename",
    params: {
      textDocument: { uri: boxUri },
      position: { line: 0, character: 4 },
      newName: "Boxy",
    },
  },
  {
    jsonrpc: "2.0",
    id: 7,
    method: "textDocument/rename",
    params: {
      textDocument: { uri: boxUri },
      position: { line: 0, character: 4 },
      newName: "col",
    },
  },
  { jsonrpc: "2.0", id: 8, method: "shutdown", params: null },
  { jsonrpc: "2.0", method: "exit" },
];

for (const m of msgs) child.stdin.write(encode(m));
child.stdin.end();

const timeout = setTimeout(() => {
  child.kill("SIGKILL");
  fail("timed out waiting for LSP");
}, 15000);

child.on("close", (code) => {
  clearTimeout(timeout);
  const frames = parseFrames(stdout);
  const byId = new Map();
  for (const f of frames) {
    if (f && f.id != null) byId.set(f.id, f);
  }

  const init = byId.get(1);
  if (!init || !init.result || !init.result.capabilities) {
    const preview = stdout.toString("utf8").slice(0, 400);
    fail(
      `initialize missing capabilities (frames=${frames.length} exit=${code} stdout=${JSON.stringify(preview)} stderr=${JSON.stringify(stderr.toString("utf8").slice(0, 200))})`
    );
  }
  const caps = init.result.capabilities;
  if (caps.referencesProvider !== true)
    fail("initialize missing referencesProvider");
  const rp = caps.renameProvider;
  if (!(rp === true || (rp && rp.prepareProvider === true)))
    fail("initialize missing renameProvider.prepareProvider");

  const refsDecl = byId.get(2);
  const locsDecl = refsDecl && refsDecl.result;
  if (!Array.isArray(locsDecl) || locsDecl.length < 2)
    fail(`references includeDeclaration=true expected ≥2 locations, got ${JSON.stringify(locsDecl)}`);
  const urisDecl = locsDecl.map((l) => l.uri || "");
  if (!urisDecl.some((u) => u.includes("Box.cord")))
    fail("references(true) missing Box.cord declaration");
  if (!urisDecl.some((u) => u.includes("HomePage.cord")))
    fail("references(true) missing HomePage.cord usage");

  const refsNo = byId.get(3);
  const locsNo = refsNo && refsNo.result;
  if (!Array.isArray(locsNo) || locsNo.length < 1)
    fail(`references includeDeclaration=false expected ≥1 usage, got ${JSON.stringify(locsNo)}`);
  const hasDecl = locsNo.some(
    (l) =>
      (l.uri || "").includes("Box.cord") &&
      l.range &&
      l.range.start &&
      l.range.start.line === 0 &&
      l.range.start.character === 4
  );
  if (hasDecl)
    fail("references(false) included the declaration");
  if (!locsNo.some((l) => (l.uri || "").includes("HomePage.cord")))
    fail("references(false) missing HomePage.cord usage");

  const prepBad = byId.get(4);
  if (!prepBad || !prepBad.error)
    fail("prepareRename on non-symbol should return an error");

  const prepOk = byId.get(5);
  if (!prepOk || !prepOk.result || !prepOk.result.range)
    fail("prepareRename on def Box should return a range");
  if (prepOk.result.range.start.character !== 4)
    fail("prepareRename range should start at character 4");

  const rename = byId.get(6);
  const edit = rename && rename.result;
  const blob = JSON.stringify(edit || {});
  if (!edit) fail("rename missing WorkspaceEdit");
  if (!blob.includes("Boxy")) fail("rename edit missing newText Boxy");
  if (!blob.includes("Box.cord")) fail("rename edit missing Box.cord");
  if (!blob.includes("HomePage.cord")) fail("rename edit missing HomePage.cord");
  const boxyCount = (blob.match(/"newText":"Boxy"/g) || []).length;
  if (boxyCount < 2)
    fail(`rename should update def + usages (newText count=${boxyCount})`);

  const renameBad = byId.get(7);
  if (!renameBad || !renameBad.error)
    fail("rename to reserved name 'col' should return an error");

  if (code !== 0 && code !== null)
    fail(`lsp exit code ${code}; stderr=${stderr.toString("utf8")}`);

  process.exit(0);
});
