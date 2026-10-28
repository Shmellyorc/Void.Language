#include "void/native.hpp"
#include "void/utf8.hpp"
#include <stdexcept>
namespace vd {
std::string native_utf8(std::string_view text) {
    if (!valid_utf8(text)) throw std::invalid_argument("invalid UTF-8 native text");
    if (text.find('\0') != std::string_view::npos) throw std::invalid_argument("native text cannot contain NUL");
    return std::string(text);
}
}
