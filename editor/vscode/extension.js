const vscode = require("vscode");
const { LanguageClient, TransportKind } = require("vscode-languageclient/node");
const fs = require("fs");
const path = require("path");
const { spawn } = require("child_process");
const os = require("os");

/** @type {LanguageClient | undefined} */
let client;
/** @type {vscode.DiagnosticCollection | undefined} */
let diagCollection;
/** @type {vscode.StatusBarItem | undefined} */
let status;
/** @type {Map<string, NodeJS.Timeout>} */
const debounce = new Map();
/** @type {string} */
let cordBin = "cordlang";
/** @type {boolean} */
let lspReady = false;

/**
 * @param {string | undefined} configured
 * @returns {Promise<string>}
 */
async function resolveCordlangBin(configured) {
  const candidates = [];
  if (configured && configured.trim()) candidates.push(configured.trim());

  const folders = vscode.workspace.workspaceFolders || [];
  for (const folder of folders) {
    const root = folder.uri.fsPath;
    candidates.push(
      path.join(root, "cordlang.exe"),
      path.join(root, "cordlang"),
      path.join(root, "cordlang_r5.exe")
    );
  }

  candidates.push("cordlang.exe", "cordlang");

  for (const c of candidates) {
    if (!c) continue;
    if (c === "cordlang" || c === "cordlang.exe") {
      /* PATH — probe with --version */
      const ok = await probeBin(c);
      if (ok) return c;
      continue;
    }
    try {
      if (fs.existsSync(c)) return c;
    } catch {
      /* ignore */
    }
  }
  return configured && configured.trim() ? configured.trim() : "cordlang";
}

/**
 * @param {string} bin
 * @returns {Promise<boolean>}
 */
function probeBin(bin) {
  return new Promise((resolve) => {
    const child = spawn(bin, ["--version"], {
      shell: process.platform === "win32",
      windowsHide: true,
    });
    let settled = false;
    const done = (v) => {
      if (settled) return;
      settled = true;
      resolve(v);
    };
    child.on("error", () => done(false));
    child.on("exit", (code) => done(code === 0));
    setTimeout(() => {
      try {
        child.kill();
      } catch {
        /* ignore */
      }
      done(false);
    }, 2500);
  });
}

/**
 * @param {string} level
 * @returns {vscode.DiagnosticSeverity}
 */
function severityOf(level) {
  if (level === "error") return vscode.DiagnosticSeverity.Error;
  if (level === "warning") return vscode.DiagnosticSeverity.Warning;
  return vscode.DiagnosticSeverity.Information;
}

/**
 * Run `cordlang check --json` on buffer text (temp file).
 * @param {vscode.TextDocument} doc
 */
async function runCheckDiagnostics(doc) {
  if (!diagCollection) return;
  if (doc.languageId !== "cordlang") return;

  const tmp = path.join(
    os.tmpdir(),
    `cordlang-diag-${process.pid}-${Date.now()}.cord`
  );
  try {
    fs.writeFileSync(tmp, doc.getText(), "utf8");
  } catch {
    return;
  }

  const args = ["check", "--json", tmp];
  /** @type {string} */
  let stdout = "";
  /** @type {string} */
  let stderr = "";

  await new Promise((resolve) => {
    const child = spawn(cordBin, args, {
      shell: process.platform === "win32",
      windowsHide: true,
    });
    child.stdout.on("data", (d) => {
      stdout += d.toString("utf8");
    });
    child.stderr.on("data", (d) => {
      stderr += d.toString("utf8");
    });
    child.on("error", () => resolve(undefined));
    child.on("exit", () => resolve(undefined));
    setTimeout(() => {
      try {
        child.kill();
      } catch {
        /* ignore */
      }
      resolve(undefined);
    }, 8000);
  });

  try {
    fs.unlinkSync(tmp);
  } catch {
    /* ignore */
  }

  /** @type {any[]} */
  let items = [];
  const trimmed = stdout.trim();
  if (trimmed.startsWith("[")) {
    try {
      items = JSON.parse(trimmed);
    } catch {
      items = [];
    }
  }

  const diags = [];
  for (const it of items) {
    if (!it || typeof it.message !== "string") continue;
    const line = Math.max(0, (it.line | 0) - 1);
    const col = Math.max(0, (it.col | 0) - 1);
    const lineText = doc.lineAt(Math.min(line, doc.lineCount - 1)).text;
    const endCol = Math.min(lineText.length, Math.max(col + 1, col + 8));
    const range = new vscode.Range(line, col, line, endCol);
    const d = new vscode.Diagnostic(
      range,
      it.hint ? `${it.message} — ${it.hint}` : it.message,
      severityOf(it.level)
    );
    d.source = "cordlang";
    if (it.code) d.code = it.code;
    diags.push(d);
  }

  /* If JSON empty but stderr has parse line, surface one diagnostic. */
  if (diags.length === 0 && /unterminated string|error:/i.test(stderr)) {
    const m = stderr.match(/:(\d+):(\d+):\s*(?:error|warning):\s*(.+)/i);
    if (m) {
      const line = Math.max(0, parseInt(m[1], 10) - 1);
      const col = Math.max(0, parseInt(m[2], 10) - 1);
      diags.push(
        new vscode.Diagnostic(
          new vscode.Range(line, col, line, col + 1),
          m[3].trim(),
          vscode.DiagnosticSeverity.Error
        )
      );
    }
  }

  diagCollection.set(doc.uri, diags);
}

/**
 * @param {vscode.TextDocument} doc
 * @param {number} ms
 */
function scheduleCheck(doc, ms) {
  if (doc.languageId !== "cordlang") return;
  /* Prefer LSP diagnostics when the language server is up. */
  if (lspReady) return;
  const conf = vscode.workspace.getConfiguration("cordlang");
  if (conf.get("diagnostics.fallback") === false) return;
  const key = doc.uri.toString();
  const prev = debounce.get(key);
  if (prev) clearTimeout(prev);
  debounce.set(
    key,
    setTimeout(() => {
      debounce.delete(key);
      runCheckDiagnostics(doc).catch(() => {});
    }, ms)
  );
}

/**
 * @param {vscode.ExtensionContext} context
 */
async function startLanguageClient(context) {
  const conf = vscode.workspace.getConfiguration("cordlang");
  cordBin = await resolveCordlangBin(conf.get("lsp.path") || "cordlang");

  const serverOptions = {
    command: cordBin,
    args: ["lsp"],
    transport: TransportKind.stdio,
    options: {
      env: process.env,
    },
  };

  const clientOptions = {
    documentSelector: [
      { scheme: "file", language: "cordlang" },
      { scheme: "untitled", language: "cordlang" },
    ],
    synchronize: {
      fileEvents: vscode.workspace.createFileSystemWatcher("**/*.cord"),
    },
    outputChannelName: "Cordlang Language Server",
  };

  client = new LanguageClient(
    "cordlang",
    "Cordlang Language Server",
    serverOptions,
    clientOptions
  );

  try {
    await client.start();
    lspReady = true;
    if (status) {
      status.text = "$(check) Cordlang LSP";
      status.tooltip = `Using ${cordBin}`;
      status.backgroundColor = undefined;
    }
    /* Clear fallback diags — LSP owns them now. */
    if (diagCollection) diagCollection.clear();
  } catch (err) {
    lspReady = false;
    const msg = err && err.message ? err.message : String(err);
    if (status) {
      status.text = "$(warning) Cordlang check";
      status.tooltip = `LSP failed (${msg}). Using cordlang check --json fallback.\nBinary: ${cordBin}`;
      status.backgroundColor = new vscode.ThemeColor(
        "statusBarItem.warningBackground"
      );
    }
    vscode.window.setStatusBarMessage(
      `Cordlang: LSP unavailable — diagnostics via check (${cordBin})`,
      6000
    );
    /* Fallback for already-open docs. */
    for (const doc of vscode.workspace.textDocuments) {
      scheduleCheck(doc, 100);
    }
  }

  if (client) context.subscriptions.push({ dispose: () => client.stop() });
}

/**
 * @param {vscode.ExtensionContext} context
 */
function activate(context) {
  status = vscode.window.createStatusBarItem(
    vscode.StatusBarAlignment.Right,
    90
  );
  status.text = "$(sync~spin) Cordlang";
  status.tooltip = "Cordlang language support";
  status.show();
  context.subscriptions.push(status);

  diagCollection = vscode.languages.createDiagnosticCollection("cordlang");
  context.subscriptions.push(diagCollection);

  context.subscriptions.push(
    vscode.workspace.onDidOpenTextDocument((doc) => scheduleCheck(doc, 200)),
    vscode.workspace.onDidChangeTextDocument((e) =>
      scheduleCheck(e.document, 400)
    ),
    vscode.workspace.onDidSaveTextDocument((doc) => scheduleCheck(doc, 50)),
    vscode.workspace.onDidCloseTextDocument((doc) => {
      if (diagCollection) diagCollection.delete(doc.uri);
      const t = debounce.get(doc.uri.toString());
      if (t) clearTimeout(t);
      debounce.delete(doc.uri.toString());
    })
  );

  startLanguageClient(context).catch(() => {});
}

function deactivate() {
  lspReady = false;
  for (const t of debounce.values()) clearTimeout(t);
  debounce.clear();
  if (!client) return undefined;
  return client.stop();
}

module.exports = { activate, deactivate };
