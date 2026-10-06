#ifndef VOIDC_DIAGNOSTIC_H
#define VOIDC_DIAGNOSTIC_H
#include "lexer.h"
#include <stdio.h>
typedef enum VcDiagnosticCode
{
#define VC_DIAGNOSTIC_CODE(name, code, meaning) VC_DIAG_##name,
#include "diagnostic_codes.def"
#undef VC_DIAGNOSTIC_CODE
    VC_DIAG_CODE_COUNT
} VcDiagnosticCode;
const char *vc_diagnostic_code_name(VcDiagnosticCode code);
void vc_diagnostic_set_code(VcDiagnostic *diagnostic, VcDiagnosticCode code);
void vc_diagnostic_init(VcDiagnostic *diagnostic, const char *path, VcSourceSpan span);
bool vc_diagnostic_add_context(VcDiagnostic *diagnostic, VcDiagnosticSeverity severity,
    const char *path, VcSourceSpan span, const char *message);
bool vc_diagnostic_add_source_context(VcDiagnostic *diagnostic, VcDiagnosticSeverity severity,
    const VcSource *source, VcSourceSpan span, const char *message);
void vc_diagnostic_render(FILE *output, const VcDiagnostic *diagnostic);
/* CLI sink is opt-in; library/query callers retain their existing error contract. */
void vc_diagnostic_enable_terminal(void);
bool vc_diagnostic_set_format(const char *format);
bool vc_diagnostic_report(const VcDiagnostic *diagnostic);
void vc_diagnostic_report_error(const char *message);
#endif
