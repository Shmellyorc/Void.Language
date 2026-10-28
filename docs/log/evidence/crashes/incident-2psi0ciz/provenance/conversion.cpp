#include "void/runtime_abi.hpp"

#include <bit>
#include <charconv>
#include <cmath>
#include <limits>
#include <string_view>
#include <type_traits>

namespace {

// Grammar is checked before range, so trailing junk never becomes a range error.
bool decimal(std::string_view text, bool floating) {
    std::size_t position = 0;
    if (position < text.size() && (text[position] == '+' || text[position] == '-')) {
        ++position;
    }
    auto digits = [&] {
        const auto first = position;
        while (position < text.size() && text[position] >= '0' && text[position] <= '9') {
            ++position;
        }
        return position != first;
    };
    bool significant = digits();
    if (floating && position < text.size() && text[position] == '.') {
        ++position;
        significant = digits() || significant;
    }
    if (!significant) {
        return false;
    }
    if (floating && position < text.size() && (text[position] == 'e' || text[position] == 'E')) {
        ++position;
        if (position < text.size() && (text[position] == '+' || text[position] == '-')) {
            ++position;
        }
        if (!digits()) {
            return false;
        }
    }
    return position == text.size();
}

template<class T>
unsigned number(std::string_view text, std::uint64_t& bits) {
    constexpr bool floating = std::is_floating_point_v<T>;
    if (!decimal(text, floating)) {
        return 1;
    }
    const bool negative = text.front() == '-';
    if constexpr (std::is_unsigned_v<T>) {
        if (negative) {
            return 2;
        }
    }
    if (text.front() == '+') {
        text.remove_prefix(1);
    }
    T value{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec == std::errc::result_out_of_range) {
        return 2;
    }
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
        return 1;
    }
    if constexpr (floating) {
        // from_chars reports overflow and underflow; also defend zero rounding
        // on standard libraries that accept extremely small nonzero inputs.
        if (!std::isfinite(value)) {
            return 2;
        }
        if (value == 0) {
            auto significand = text.substr(0, text.find_first_of("eE"));
            if (significand.find_first_of("123456789") != std::string_view::npos) {
                return 2;
            }
        }
        if constexpr (sizeof(T) == 4) {
            bits = std::bit_cast<std::uint32_t>(value);
        } else {
            bits = std::bit_cast<std::uint64_t>(value);
        }
    } else {
        bits = static_cast<std::make_unsigned_t<T>>(value);
    }
    return 0;
}

} // namespace

extern "C" unsigned vd_convert_parse(std::uint64_t* bits, void* source, unsigned encoding,
                                      const char* const* cases, std::size_t count,
                                      vd::RuntimeFailure* failure) noexcept {
    try {
        auto text = vd::Runtime::current().string_bytes(source);
        using vd::ScalarEncoding;
        if (encoding == 11) {
            for (std::size_t index = 0; index < count; ++index) {
                std::string_view qualified(cases[index]);
                const auto separator = qualified.rfind('.');
                if (text == qualified || text == qualified.substr(separator + 1)) {
                    *bits = index;
                    return 0;
                }
            }
            return 1;
        }
        switch (static_cast<ScalarEncoding>(encoding)) {
        case ScalarEncoding::i8:
            return number<std::int8_t>(text, *bits);
        case ScalarEncoding::i16:
            return number<std::int16_t>(text, *bits);
        case ScalarEncoding::i32:
            return number<std::int32_t>(text, *bits);
        case ScalarEncoding::i64:
            return number<std::int64_t>(text, *bits);
        case ScalarEncoding::u8:
            return number<std::uint8_t>(text, *bits);
        case ScalarEncoding::u16:
            return number<std::uint16_t>(text, *bits);
        case ScalarEncoding::u32:
            return number<std::uint32_t>(text, *bits);
        case ScalarEncoding::u64:
            return number<std::uint64_t>(text, *bits);
        case ScalarEncoding::f32:
            return number<float>(text, *bits);
        case ScalarEncoding::f64:
            return number<double>(text, *bits);
        case ScalarEncoding::boolean:
            if (text == "true" || text == "false") {
                *bits = text == "true";
                return 0;
            }
            return 1;
        }
        failure->message = "Invalid scalar parse encoding";
    } catch (const std::exception&) {
        failure->message = "Invalid conversion input or Runtime state";
    }
    return 1;
}
