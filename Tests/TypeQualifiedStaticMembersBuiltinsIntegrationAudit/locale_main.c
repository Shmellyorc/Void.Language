/* Run the actual VOID integration program under an ambient comma-decimal locale.
   Only the test harness changes locale; the generated converter must not. */
#define main void340_program_main
#include ".void/TypeQualifiedStaticMembersBuiltinsIntegrationAudit.c"
#undef main

int main(void)
{
    const char *candidates[] = {
#ifdef _WIN32
        "German_Germany.1252", "French_France.1252",
#endif
        "de_DE.UTF-8", "de_DE.utf8", "fr_FR.UTF-8", "fr_FR.utf8"
    };
    bool found = false;
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++)
    {
        if (setlocale(LC_NUMERIC, candidates[i]) != NULL &&
            strcmp(localeconv()->decimal_point, ",") == 0)
        {
            found = true;
            break;
        }
    }
    if (!found)
        return 77; /* Explicitly unavailable, never claim a locale execution. */
    char before[256];
    snprintf(before, sizeof(before), "%s", setlocale(LC_NUMERIC, NULL));
    const int result = void340_program_main();
    return result != 0 || strcmp(before, setlocale(LC_NUMERIC, NULL)) != 0;
}
