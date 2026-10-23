#pragma once
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace vd::formatting {
// LLVM-independent, owning data only. The Runtime ABI translates exceptions
// before returning to generated code; the compiler uses the same implementation.
class Error : public std::invalid_argument {
public:
    std::size_t offset;
    Error(std::size_t at, const std::string& why);
};
class Builder {
    std::string bytes_;
public:
    void append(std::string_view);
    void repeat(char, std::size_t);
    std::size_t size() const noexcept { return bytes_.size(); }
    std::string finish() && { return std::move(bytes_); }
};
struct Alignment { char direction = 0; std::size_t width = 0; };
struct Plan {
    Alignment alignment;
    enum Kind { default_display, decimal, hexadecimal, binary, scientific } kind = default_display;
    std::size_t minimum_integer = 1, minimum_fraction = 0, maximum_fraction = 0;
    bool grouping = false, positive_sign = false, percent = false, uppercase = false, integer_only = false;
    std::string prefix, suffix;
};
enum class NumberKind { i8, i16, i32, i64, u8, u16, u32, u64, f32, f64 };
struct Number { NumberKind kind; std::uint64_t bits; };
struct Layout { Alignment alignment; std::size_t pattern_offset=0; std::string_view pattern; };
Layout split_layout(std::string_view);
Plan parse_numeric(std::string_view);
Alignment parse_text(std::string_view);
void validate(NumberKind, const Plan&);
std::string text(std::string_view, std::string_view spec = {});
std::string align(std::string_view, Alignment);
std::string number(Number, const Plan& = {});
std::string number(Number, std::string_view);
inline constexpr std::string_view abi_version = "void-format-v4;collections-v1;sum-display-v1;ties-to-even;unicode-scalar-alignment;ryu-72e18e8d14d4392a4a3261f37cde248e4fbedbec";
}
