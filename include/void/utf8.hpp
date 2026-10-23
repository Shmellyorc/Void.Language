#pragma once
#include <cstddef>
#include <string_view>
namespace vd {
// Shared scalar validation, independent of the frontend, LLVM and runtime.
inline unsigned utf8_sequence_bytes(unsigned char c) {
    return c < 128 ? 1 : c >= 0xc2 && c <= 0xdf ? 2 : c >= 0xe0 && c <= 0xef ? 3 : c >= 0xf0 && c <= 0xf4 ? 4 : 0;
}
inline unsigned utf8_scalar_bytes(std::string_view text, std::size_t i) {
    if (i >= text.size()) return 0;
    auto c = static_cast<unsigned char>(text[i]);
    if (c < 128) return 1;
    unsigned count = utf8_sequence_bytes(c);
    if (!count || count > text.size()-i) return 0;
    unsigned scalar = c & (count == 2 ? 31 : count == 3 ? 15 : 7);
    for (unsigned n=1;n<count;++n) {
        auto next=static_cast<unsigned char>(text[i+n]);
        if ((next & 0xc0) != 0x80) return 0;
        scalar = (scalar << 6) | (next & 63);
    }
    if ((scalar >= 0xd800 && scalar <= 0xdfff) || scalar > 0x10ffff ||
        (count == 3 && scalar < 0x800) || (count == 4 && scalar < 0x10000)) return 0;
    return count;
}
inline bool valid_utf8(std::string_view text) {
    for (std::size_t i=0;i<text.size();) {
        auto count=utf8_scalar_bytes(text,i); if (!count) return false; i+=count;
    }
    return true;
}
}
