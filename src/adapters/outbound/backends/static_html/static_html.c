#include "static_html_internal.h"

char *static_html_generate_from_ir(IrProgram *ir) {
  if (!ir || !ir->root)
    return strdup("<!DOCTYPE html><html><body></body></html>\n");

  statich_bind_reset();
  statich_collect_binds(ir->root);

  StrBuf body;
  statich_sb_init(&body);
  statich_gen_node(&body, ir->root, 2);

  StrBuf doc;
  statich_sb_init(&doc);
  statich_sb_append(&doc,
            "<!DOCTYPE html>\n"
            "<html lang=\"en\" xmlns=\"http://www.w3.org/1999/xhtml\">\n"
            "<head>\n"
            "  <meta charset=\"UTF-8\" />\n"
            "  <meta name=\"viewport\" content=\"width=device-width, "
            "initial-scale=1.0\" />\n"
            "  <meta http-equiv=\"Content-Type\" content=\"text/html; "
            "charset=UTF-8\" />\n"
            "  <title>Cordlang Static</title>\n"
            "</head>\n"
            "<body style=\"margin:0;padding:0;background:#f3f4f6;"
            "font-family:Arial,Helvetica,sans-serif;color:#111827;\">\n"
            "  <table role=\"presentation\" width=\"100%\" cellpadding=\"0\" "
            "cellspacing=\"0\" border=\"0\" style=\"background:#f3f4f6;"
            "padding:24px 0;\">\n"
            "    <tr>\n"
            "      <td align=\"center\">\n"
            "        <table role=\"presentation\" width=\"600\" "
            "cellpadding=\"0\" cellspacing=\"0\" border=\"0\" "
            "style=\"width:100%;max-width:600px;background:#ffffff;"
            "border-collapse:collapse;\">\n"
            "          <tr>\n"
            "            <td style=\"padding:24px;\">\n");
  statich_sb_append(&doc, body.buf ? body.buf : "");
  statich_sb_append(&doc,
            "            </td>\n"
            "          </tr>\n"
            "        </table>\n"
            "      </td>\n"
            "    </tr>\n"
            "  </table>\n"
            "  <!-- Cordlang static HTML: no reactive JS -->\n"
            "</body>\n"
            "</html>\n");

  free(body.buf);
  statich_bind_reset();
  return doc.buf;
}
