#pragma once
#include <cerrno>
#include <cfenv>
#include <cstdlib>
#include <locale.h>
#ifdef __APPLE__
#include <xlocale.h>
#endif
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace vd::detail {
// Called only after VOID's decimal grammar validation. Parse f32 directly to
// avoid double rounding. ERANGE alone cannot reject a representable subnormal;
// the caller classifies nonfinite results and nonzero inputs rounded to zero.
template<class T> T decimal_float(std::string_view text) {
    struct Environment {
        int saved_errno = errno;
        fenv_t saved;
        Environment() {
            if (feholdexcept(&saved)) throw std::runtime_error("Cannot save floating environment");
            if (fesetround(FE_TONEAREST)) {
                fesetenv(&saved);
                throw std::runtime_error("Cannot select decimal rounding");
            }
        }
        ~Environment() { fesetenv(&saved); errno = saved_errno; }
    } environment;
    struct Locale {
        locale_t value = newlocale(LC_NUMERIC_MASK, "C", nullptr);
        Locale() { if (!value) throw std::runtime_error("Cannot create decimal locale"); }
        ~Locale() { freelocale(value); }
    };
    static const Locale locale;
    const std::string terminated(text);
    char* end = nullptr;
    T value;
    if constexpr (std::is_same_v<T, float>) value = strtof_l(terminated.c_str(), &end, locale.value);
    else value = strtod_l(terminated.c_str(), &end, locale.value);
    if (end != terminated.c_str() + terminated.size()) throw std::invalid_argument("Invalid decimal input");
    return value;
}
}
