#ifndef CORDLANG_PREVIEW_SERVER_H
#define CORDLANG_PREVIEW_SERVER_H

#include <stddef.h>

/* Serve a single HTML document over HTTP and open the browser.
 * Blocks until the user presses Ctrl+C (or the process is killed).
 * Returns 0 on clean shutdown, non-zero on error.
 */
int preview_server_serve(const char *html, size_t html_len, int port);

/* Open the default browser to the given URL. Best-effort. */
void preview_open_browser(const char *url);

#endif
