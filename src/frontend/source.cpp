#include "void/source.hpp"
#include <algorithm>
#include <stdexcept>
namespace vd {
SourceFile::SourceFile(std::string path, std::string text)
    : path_(std::move(path)), text_(std::move(text)), lines_{0} {
    for (std::size_t i = 0; i < text_.size(); ++i) {
        if (text_[i] == '\r') {
            if (i + 1 < text_.size() && text_[i + 1] == '\n') ++i;
            lines_.push_back(i + 1);
        } else if (text_[i] == '\n') lines_.push_back(i + 1);
    }
    // Validate scalars, including overlong sequences, surrogates and truncation.
    for (std::size_t i = 0; i < text_.size();) {
        auto c = static_cast<unsigned char>(text_[i]);
        if (c < 128) { ++i; continue; }
        unsigned count = c >= 0xc2 && c <= 0xdf ? 2 : c >= 0xe0 && c <= 0xef ? 3 : c >= 0xf0 && c <= 0xf4 ? 4 : 0;
        bool valid = count && i + count <= text_.size();
        unsigned scalar = c & (count == 2 ? 31 : count == 3 ? 15 : 7);
        for (unsigned n = 1; valid && n < count; ++n) {
            auto next = static_cast<unsigned char>(text_[i + n]);
            valid = (next & 0xc0) == 0x80;
            scalar = (scalar << 6) | (next & 63);
        }
        valid = valid && !(scalar >= 0xd800 && scalar <= 0xdfff) && scalar <= 0x10ffff &&
            (count != 3 || scalar >= 0x800) && (count != 4 || scalar >= 0x10000);
        if (!valid) throw std::invalid_argument(diagnostic(i, "Invalid UTF-8 source. Hint: save this file as UTF-8."));
        i += count;
    }
}
SourceLocation SourceFile::location(std::size_t offset) const {
    if (offset > text_.size()) throw std::out_of_range("source offset");
    auto line = std::upper_bound(lines_.begin(), lines_.end(), offset) - lines_.begin() - 1;
    unsigned column = 1;
    auto begin = lines_[line];
    if (begin == 0 && text_.starts_with("\xef\xbb\xbf")) begin = 3;
    for (std::size_t i = begin; i < offset; ++i)
        if ((static_cast<unsigned char>(text_[i]) & 0xc0) != 0x80) ++column;
    return {path_, static_cast<unsigned>(line + 1), column};
}
std::string SourceFile::excerpt(std::size_t offset) const {
    auto pos = location(offset);
    auto begin = lines_[pos.line - 1];
    if (begin == 0 && text_.starts_with("\xef\xbb\xbf")) begin = 3;
    auto end = text_.find_first_of("\r\n", begin);
    auto line = text_.substr(begin, end == std::string::npos ? end : end - begin);
    return line + "\n" + std::string(pos.column - 1, ' ') + "^\n";
}
std::string SourceFile::diagnostic(std::size_t offset, std::string_view message) const {
    auto pos = location(offset);
    return path_ + ":" + std::to_string(pos.line) + ":" + std::to_string(pos.column) +
        ": error: " + std::string(message) + "\n" + excerpt(offset);
}
}
