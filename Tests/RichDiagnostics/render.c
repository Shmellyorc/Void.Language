#include "diagnostic.h"
#include <stdlib.h>
int main(int argc, char **argv)
{
    if (argc != 4) return 2;
    VcSourceSpan span = {{strtoul(argv[2], NULL, 10), 1, 1}, {strtoul(argv[3], NULL, 10), 1, 1}};
    VcDiagnostic diagnostic;
    vc_diagnostic_init(&diagnostic, argv[1], span);
    snprintf(diagnostic.message, sizeof(diagnostic.message), "range");
    vc_diagnostic_render(stdout, &diagnostic);
    return 0;
}
