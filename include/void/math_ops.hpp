#pragma once
#include <array>
#include <cstdint>
#include <string_view>

namespace vd::math {
// Closed scalar compiler boundary. LLVM intrinsic identities remain backend-only.
inline constexpr std::string_view source_module = "/@stdlib/math.void";
enum class Kind : std::int64_t {
    identity, floor, sqrt, sin, cos, tan, asin, acos, atan, atan2, pow,
    exp, exp2, log, log2
};
struct Operation { std::string_view name; Kind kind; unsigned arity; };
inline constexpr std::array operations = {
    Operation{"_math_float", Kind::identity, 1},
    Operation{"_math_floor", Kind::floor, 1},
    Operation{"_math_sqrt", Kind::sqrt, 1},
    Operation{"_math_sin", Kind::sin, 1},
    Operation{"_math_cos", Kind::cos, 1},
    Operation{"_math_tan", Kind::tan, 1},
    Operation{"_math_asin", Kind::asin, 1},
    Operation{"_math_acos", Kind::acos, 1},
    Operation{"_math_atan", Kind::atan, 1},
    Operation{"_math_atan2", Kind::atan2, 2},
    Operation{"_math_pow", Kind::pow, 2},
    Operation{"_math_exp", Kind::exp, 1},
    Operation{"_math_exp2", Kind::exp2, 1},
    Operation{"_math_log", Kind::log, 1},
    Operation{"_math_log2", Kind::log2, 1}
};
inline constexpr const Operation* operation(std::int64_t kind) {
    for (const auto& candidate : operations)
        if (static_cast<std::int64_t>(candidate.kind) == kind) return &candidate;
    return nullptr;
}
inline constexpr const Operation* operation(std::string_view name) {
    for (const auto& candidate : operations)
        if (candidate.name == name) return &candidate;
    return nullptr;
}
// Only these ordinary public generics carry the closed primitive contract.
// Private source helpers retain the language's normal generic semantics.
inline constexpr bool numeric_function(std::string_view name) {
    return name == "abs" || name == "min" || name == "max" ||
           name == "clamp" || name == "sign";
}
inline constexpr bool floating_function(std::string_view name) {
    return name == "is_nan" || name == "is_infinite" || name == "is_finite" ||
           name == "floor" || name == "ceil" || name == "trunc" || name == "round" ||
           name == "lerp" || name == "inverse_lerp" || name == "sin" ||
           name == "cos" || name == "tan" || name == "asin" || name == "acos" ||
           name == "atan" || name == "atan2" || name == "sqrt" || name == "pow" ||
           name == "exp" || name == "exp2" || name == "log" || name == "log2" ||
           name == "radians" || name == "degrees";
}
}
