const vscode = require("vscode");
const { LanguageClient, TransportKind } = require("vscode-languageclient/node");

/** @type {LanguageClient | undefined} */
let client;

function activate(context) {
  const conf = vscode.workspace.getConfiguration("cordlang");
  const bin = conf.get("lsp.path") || "cordlang";

  const serverOptions = {
    command: bin,
    args: ["lsp"],
    transport: TransportKind.stdio,
  };

  const clientOptions = {
    documentSelector: [{ scheme: "file", language: "cordlang" }],
  };

  client = new LanguageClient(
    "cordlang",
    "Cordlang Language Server",
    serverOptions,
    clientOptions
  );

  context.subscriptions.push(client.start());
}

function deactivate() {
  if (!client) return undefined;
  return client.stop();
}

module.exports = { activate, deactivate };
