#include "diagnostic.h"
#include <assert.h>
#include <string.h>
int main(void)
{
    VcDiagnostic diagnostic;
    VcSourceSpan span = {{0, 1, 1}, {1, 1, 2}};
    vc_diagnostic_init(&diagnostic, "test.void", span);
    assert(diagnostic.severity == VC_DIAGNOSTIC_ERROR);
    assert(diagnostic.context_count == 0);
    assert(!vc_diagnostic_add_context(&diagnostic, VC_DIAGNOSTIC_WARNING, NULL, span, "invalid context severity"));
    assert(diagnostic.context_count == 0);
    snprintf(diagnostic.message, sizeof(diagnostic.message), "primary");
    assert(vc_diagnostic_add_context(&diagnostic, VC_DIAGNOSTIC_NOTE, NULL, span, "context"));
    assert(vc_diagnostic_add_context(&diagnostic, VC_DIAGNOSTIC_HELP, NULL, span, "guidance"));
    for (size_t i = 2; i < VC_DIAGNOSTIC_CONTEXT_LIMIT; i++)
        assert(vc_diagnostic_add_context(&diagnostic, VC_DIAGNOSTIC_NOTE, NULL, span, "detail"));
    assert(!vc_diagnostic_add_context(&diagnostic, VC_DIAGNOSTIC_NOTE, NULL, span, "overflow"));
    VcDiagnostic copy = diagnostic;
    strcpy(diagnostic.context[0].message, "changed");
    assert(strcmp(copy.context[0].message, "context") == 0);
    vc_diagnostic_render(stdout, &copy);
    return 0;
}
