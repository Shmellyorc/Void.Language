#ifndef VOIDC_MONOMORPH_H
#define VOIDC_MONOMORPH_H

#include "ast.h"

#include <stdbool.h>
#include <stddef.h>

bool vc_monomorphize(VcAstTree **trees, size_t tree_count, char *error, size_t error_size);
bool vc_monomorphize_diagnostics(VcAstTree **trees, const VcSource *const *sources,
    size_t tree_count, VcDiagnostic *diagnostic, char *error, size_t error_size);
bool vc_monomorph_mangle_type_ref(const VcAstTypeRef *type, char *output, size_t output_size);

#endif
