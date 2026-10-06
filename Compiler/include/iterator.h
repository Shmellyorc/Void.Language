#ifndef VOIDC_ITERATOR_H
#define VOIDC_ITERATOR_H

#include "ast.h"
#include "semantic.h"

#include <stdbool.h>
#include <stddef.h>

bool vc_has_iterators(VcAstTree **trees, size_t tree_count);
bool vc_has_async_iterators(VcAstTree **trees, size_t tree_count);
bool vc_lower_iterators(VcAstTree **trees, size_t tree_count,
    const VcSemanticModel *semantic, char *error, size_t error_size);

#endif
