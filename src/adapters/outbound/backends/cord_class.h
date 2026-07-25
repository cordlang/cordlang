#ifndef CORDLANG_CORD_CLASS_H
#define CORDLANG_CORD_CLASS_H

#include "domain/ir.h"
#include <stddef.h>

/*
 * Shared Cord attribute → utility-class lowering.
 *
 * Extracted from the React backend so every target that renders Cord layout
 * attrs (`p=16`, `gap=24`, `center`, `bg=surface`, …) produces the SAME class
 * strings. The React scaffold resolves those classes through Tailwind; the
 * native ESM preview resolves them through a generated base.css. If this logic
 * were duplicated the preview and the production build would drift apart.
 */

/* Attr value that means "flag present" (NULL, "", or "true"). */
int cord_attr_is_true(const char *v);

/* Attrs consumed into the class string instead of reaching the DOM. */
int cord_is_style_attr_name(const char *name);

/* Valueless flags consumed into the class string (center, sticky, bold, …). */
int cord_is_style_bool_name(const char *name);

/* role / aria-* / data-* — always real DOM attrs, never implicit children. */
int cord_is_dom_a11y_or_data_attr(const char *name);

/* Base classes implied by a Cord layout tag (col → "flex flex-col"), or NULL. */
const char *cord_tag_base_class(const char *tag);

/*
 * DOM tag for a Cord tag. Returns NULL for `fragment` (children only).
 * Preset tags keep their component names (CordIcon / CordMotion / CordChart);
 * targets without those presets map them to plain elements themselves.
 */
const char *cord_html_tag_for(const char *tag);

/* Numeric / boolean literal sniffing (shared with value emission). */
int cord_looks_like_number(const char *s);
int cord_looks_like_bool(const char *s);

/*
 * Build the full class string for an element node into `classes`
 * (NUL-terminated, may start with a space). base_class may be NULL.
 */
void cord_collect_classes(char *classes, size_t classes_sz, IrNode *node,
                          const char *base_class);

#endif
