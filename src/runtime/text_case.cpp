#include "void/runtime_abi.hpp"
#include "void/utf8.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace {

#include "unicode_case_tables.inc"

struct Scalar {
    std::uint32_t value;
    std::size_t offset;
    unsigned width;
};

template<std::size_t N>
bool member(std::uint32_t value, const Range (&ranges)[N]) {
    const auto* found = std::upper_bound(std::begin(ranges), std::end(ranges), value,
        [](std::uint32_t scalar, const Range& range) { return scalar < range.first; });
    return found != std::begin(ranges) && value < std::prev(found)->limit;
}

template<std::size_t N>
std::string_view mapped(std::uint32_t value, const Mapping (&mappings)[N]) {
    const auto* found = std::lower_bound(std::begin(mappings), std::end(mappings), value,
        [](const Mapping& mapping, std::uint32_t scalar) { return mapping.scalar < scalar; });
    if (found != std::end(mappings) && found->scalar == value) {
        return found->bytes;
    }
    return {};
}

std::string transform(std::string_view text, bool upper) {
    std::vector<Scalar> scalars;
    for (std::size_t offset = 0; offset < text.size();) {
        const auto width = vd::utf8_scalar_bytes(text, offset);
        if (!width) {
            throw std::invalid_argument("Invalid UTF-8 in Unicode casing input");
        }
        const auto lead = static_cast<unsigned char>(text[offset]);
        std::uint32_t value = width == 1 ? lead : lead & (width == 2 ? 31 : width == 3 ? 15 : 7);
        for (unsigned index = 1; index < width; ++index) {
            value = (value << 6) | (static_cast<unsigned char>(text[offset + index]) & 63);
        }
        scalars.push_back({value, offset, width});
        offset += width;
    }

    // Two linear context passes avoid rescanning long case-ignorable sequences.
    // Case_Ignorable takes precedence when the two properties overlap (U+0345).
    std::vector<bool> following(scalars.size());
    bool cased = false;
    for (std::size_t index = scalars.size(); index > 0;) {
        --index;
        following[index] = cased;
        if (!member(scalars[index].value, ignorable_ranges)) {
            cased = member(scalars[index].value, cased_ranges);
        }
    }

    std::string output;
    output.reserve(text.size());
    bool preceding = false;
    for (std::size_t index = 0; index < scalars.size(); ++index) {
        const auto& scalar = scalars[index];
        auto replacement = upper ? mapped(scalar.value, upper_mappings) :
                                   mapped(scalar.value, lower_mappings);
        if (!upper && scalar.value == 0x3A3 && preceding && !following[index]) {
            replacement = "\317\202"; // U+03C2 GREEK SMALL LETTER FINAL SIGMA.
        }
        output.append(replacement.empty() ? text.substr(scalar.offset, scalar.width) : replacement);
        if (!member(scalar.value, ignorable_ranges)) {
            preceding = member(scalar.value, cased_ranges);
        }
    }
    return output;
}

} // namespace

extern "C" void vd_text_case(void** output, void* input, bool upper,
                              vd::RuntimeFailure* failure) noexcept {
    try {
        auto& runtime = vd::Runtime::current();
        // Own input and output before string construction can poll or collect.
        const std::string source(runtime.string_bytes(input));
        const auto result = transform(source, upper);
        runtime.string_into(output, result);
    } catch (const std::bad_alloc&) {
        failure->message = "Unicode casing allocation failed";
    } catch (const std::exception&) {
        failure->message = "Unicode casing failed";
    }
}
