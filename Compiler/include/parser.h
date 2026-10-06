#ifndef VOIDC_PARSER_H
#define VOIDC_PARSER_H

#include "ast.h"
#include "lexer.h"

#include <stdbool.h>

typedef struct VcParseResult
{
    bool has_error;
    VcDiagnostic diagnostic;
} VcParseResult;

void vc_parse_result_init(VcParseResult *result);
bool vc_parse_source(
    const VcSource *source,
    const VcTokenList *tokens,
    VcAstTree *tree,
    VcParseResult *result);

#endif
