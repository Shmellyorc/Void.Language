#ifndef VOIDC_ASYNC_LOWER_H
#define VOIDC_ASYNC_LOWER_H

#include "ast.h"
#include "semantic.h"

#include <stdbool.h>
#include <stddef.h>

bool vc_has_async_methods(VcAstTree **trees, size_t tree_count);
bool vc_lower_async_lambdas(VcAstTree **trees, size_t tree_count,
    const VcSemanticModel *semantic, char *error, size_t error_size);
bool vc_lower_async_methods(VcAstTree **trees, size_t tree_count,
    const VcSemanticModel *semantic, const VcSemanticModel *generated_semantic,
    char *error, size_t error_size);

#endif
