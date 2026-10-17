#include "application/lsp_internal.h"
#include "application/lsp_service.h"
#include "application/check_service.h"
#include "application/fmt_service.h"
#include "application/ports/fs_port.h"
#include "domain/version.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

int g_shutdown = 0;
 void handle_message(const char *msg) {
  const char *method = json_method(msg);
  char id[64];
  int has_id = json_id(msg, id, sizeof(id));

  if (!method) return;

  if (strcmp(method, "initialize") == 0) {
    char caps[512];
    snprintf(caps, sizeof(caps),
             "{\"capabilities\":{"
             "\"textDocumentSync\":1,"
             "\"documentSymbolProvider\":true,"
             "\"definitionProvider\":true,"
             "\"referencesProvider\":true,"
             "\"renameProvider\":{\"prepareProvider\":true},"
             "\"completionProvider\":{\"triggerCharacters\":[\" \",\"=\",\"@\"]},"
             "\"hoverProvider\":true,"
             "\"codeActionProvider\":true,"
             "\"documentFormattingProvider\":true"
             "},\"serverInfo\":{\"name\":\"cordlang\",\"version\":\"%s\"}}",
             CORDLANG_VERSION);
    respond_ok(has_id ? id : "0", caps);
    return;
  }
  if (strcmp(method, "initialized") == 0) return;
  if (strcmp(method, "shutdown") == 0) {
    g_shutdown = 1;
    docs_free();
    if (has_id) respond_ok(id, "null");
    return;
  }
  if (strcmp(method, "exit") == 0) {
    exit(g_shutdown ? 0 : 1);
  }
  if (strcmp(method, "textDocument/didOpen") == 0 ||
      strcmp(method, "textDocument/didChange") == 0 ||
      strcmp(method, "textDocument/didSave") == 0) {
    char uri[1024];
    uri[0] = '\0';
    const char *td = strstr(msg, "\"textDocument\"");
    if (td) json_get_str(td, "uri", uri, sizeof(uri));
    char *text = NULL;
    /* didOpen/didChange may include text — parse on heap (never 4MB stack). */
    const char *tp = strstr(msg, "\"text\"");
    if (tp) text = json_get_str_dup(tp, "text");
    LspDoc *d = doc_upsert(uri, text);
    free(text);
    if (d) publish_diagnostics(uri, d);
    return;
  }
  if (strcmp(method, "textDocument/documentSymbol") == 0 && has_id) {
    handle_document_symbol(id, msg);
    return;
  }
  if (strcmp(method, "textDocument/definition") == 0 && has_id) {
    handle_definition(id, msg);
    return;
  }
  if (strcmp(method, "textDocument/references") == 0 && has_id) {
    handle_references(id, msg);
    return;
  }
  if (strcmp(method, "textDocument/prepareRename") == 0 && has_id) {
    handle_prepare_rename(id, msg);
    return;
  }
  if (strcmp(method, "textDocument/rename") == 0 && has_id) {
    handle_rename(id, msg);
    return;
  }
  if (strcmp(method, "textDocument/completion") == 0 && has_id) {
    handle_completion(id, msg);
    return;
  }
  if (strcmp(method, "textDocument/hover") == 0 && has_id) {
    handle_hover(id, msg);
    return;
  }
  if (strcmp(method, "textDocument/codeAction") == 0 && has_id) {
    handle_code_action(id, msg);
    return;
  }
  if (strcmp(method, "textDocument/formatting") == 0 && has_id) {
    handle_formatting(id, msg);
    return;
  }
  if (has_id) respond_ok(id, "null");
}

int lsp_service_run(void) {
#ifdef _WIN32
  /* LSP is a byte protocol. CRT text mode translates \n ↔ \r\n and
   * desyncs Content-Length against what editors / test harnesses send. */
  _setmode(_fileno(stdin), _O_BINARY);
  _setmode(_fileno(stdout), _O_BINARY);
#endif
  char *msg = malloc(MAX_MSG);
  if (!msg) return 1;

  for (;;) {
    char header[256];
    size_t content_len = 0;
    /* Read headers */
    for (;;) {
      if (!fgets(header, sizeof(header), stdin)) {
        free(msg);
        return 0;
      }
      if (header[0] == '\r' || header[0] == '\n') break;
      if (strncmp(header, "Content-Length:", 15) == 0)
        content_len = (size_t)strtoul(header + 15, NULL, 10);
    }
    if (content_len == 0 || content_len >= MAX_MSG) continue;
    size_t got = fread(msg, 1, content_len, stdin);
    if (got != content_len) {
      free(msg);
      return 1;
    }
    msg[content_len] = '\0';
    handle_message(msg);
  }
}
