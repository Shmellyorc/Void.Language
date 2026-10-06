#include "diagnostic.h"
#include <assert.h>
#include <string.h>
int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    char text[] = "original snapshot\n";
    VcSource source = {argv[1], text, sizeof(text) - 1};
    VcSourceSpan span = {{0, 1, 1}, {8, 1, 9}};
    VcDiagnostic diagnostic;
    vc_diagnostic_init(&diagnostic, source.path, span);
    diagnostic.source = &source;
    strcpy(diagnostic.message, "control\033message");
    for (size_t i = 0; i < VC_DIAGNOSTIC_CONTEXT_LIMIT; i++)
        assert(vc_diagnostic_add_source_context(&diagnostic, VC_DIAGNOSTIC_NOTE,
            &source, span, "snapshot context"));
    assert(!vc_diagnostic_add_context(&diagnostic, VC_DIAGNOSTIC_NOTE, NULL, span, "overflow"));
    vc_diagnostic_render(stdout, &diagnostic);
    assert(vc_diagnostic_set_format("json"));
    diagnostic.message[0] = (char)0xff;
    diagnostic.message[1] = 0;
    vc_diagnostic_render(stdout, &diagnostic);
    return 0;
}
