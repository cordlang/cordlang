#ifndef CORDLANG_DOMAIN_VERSION_H
#define CORDLANG_DOMAIN_VERSION_H

/* CLI / LSP version (semver). Language surface freeze is documented separately
 * in docs/VERSIONING.md and docs/SPEC.md (language 1.0).
 * Release workflow may override with -DCORDLANG_VERSION=\"…\" from the git tag. */
#ifndef CORDLANG_VERSION
#define CORDLANG_VERSION "0.0.013"
#endif

#endif
