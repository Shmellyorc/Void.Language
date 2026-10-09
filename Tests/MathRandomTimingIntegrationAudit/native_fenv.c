#include <stdbool.h>
#include <fenv.h>
bool audit_rounding_mode(int mode)
{
    const int modes[] = { FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO };
    return mode >= 0 && mode < 4 && fesetround(modes[mode]) == 0;
}
