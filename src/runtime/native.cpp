#include "void/native.hpp"
#include <cstdint>
#include <stdexcept>
namespace vd {
std::string native_utf8(std::string_view text) {
    for (std::size_t i = 0; i < text.size();) {
        auto c = static_cast<unsigned char>(text[i++]);
        if (!c) throw std::invalid_argument("native text cannot contain NUL");
        if (c < 0x80) continue;
        unsigned remaining;
        std::uint32_t value, minimum;
        if (c >= 0xc2 && c <= 0xdf) { remaining = 1; value = c & 31; minimum = 0x80; }
        else if (c >= 0xe0 && c <= 0xef) { remaining = 2; value = c & 15; minimum = 0x800; }
        else if (c >= 0xf0 && c <= 0xf4) { remaining = 3; value = c & 7; minimum = 0x10000; }
        else throw std::invalid_argument("invalid UTF-8 native text");
        if (text.size() - i < remaining) throw std::invalid_argument("truncated UTF-8 native text");
        while (remaining--) {
            c = static_cast<unsigned char>(text[i++]);
            if ((c & 0xc0) != 0x80) throw std::invalid_argument("invalid UTF-8 continuation");
            value = (value << 6) | (c & 63);
        }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
            throw std::invalid_argument("invalid UTF-8 scalar");
    }
    return std::string(text);
}
}
