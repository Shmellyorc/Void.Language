#include "diagnostic.h"
#include <string.h>
int main(void)
{
    VcDiagnostic diagnostic;
    vc_diagnostic_init(&diagnostic, "path\n\"\\é", (VcSourceSpan){0});
    for (size_t i = 1; i < 32; i++) diagnostic.message[i - 1] = (char)i;
    strcpy(diagnostic.message + 31, "\"\\é界\n");
    vc_diagnostic_add_context(&diagnostic, VC_DIAGNOSTIC_NOTE, diagnostic.path,
        (VcSourceSpan){0}, "multiline\ncontext\t\"\\");
    vc_diagnostic_add_context(&diagnostic, VC_DIAGNOSTIC_HELP, NULL, (VcSourceSpan){0}, "guidance");
    if (!vc_diagnostic_set_format("json")) return 1;
    vc_diagnostic_render(stdout, &diagnostic);
    return 0;
}
