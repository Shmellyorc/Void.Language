#include "void/source.hpp"
#include <algorithm>
#include <stdexcept>
namespace vd {
SourceFile::SourceFile(std::string path, std::string text)
    : path_(std::move(path)), text_(std::move(text)), lines_{0} {
    for (std::size_t i = 0; i < text_.size(); ++i)
        if (text_[i] == '\n') lines_.push_back(i + 1);
}
SourceLocation SourceFile::location(std::size_t offset) const {
    if (offset > text_.size()) throw std::out_of_range("source offset");
    auto line = std::upper_bound(lines_.begin(), lines_.end(), offset) - lines_.begin() - 1;
    return {path_, static_cast<unsigned>(line + 1), static_cast<unsigned>(offset - lines_[line] + 1)};
}
std::string SourceFile::diagnostic(std::size_t offset, std::string_view message) const {
    auto pos = location(offset);
    auto begin = lines_[pos.line - 1];
    auto end = text_.find_first_of("\r\n", begin);
    auto line = text_.substr(begin, end == std::string::npos ? end : end - begin);
    return path_ + ":" + std::to_string(pos.line) + ":" + std::to_string(pos.column) +
        ": error: " + std::string(message) + "\n" + line + "\n" + std::string(pos.column - 1, ' ') + "^\n";
}
}
