#pragma once
#include "void/utf8.hpp"
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
namespace vd {
struct SourceLocation { std::string path; unsigned line = 1; unsigned column = 1; };
class SourceFile {
public:
    SourceFile(std::string path, std::string text);
    const auto& line_starts() const { return snapshot_->lines; }
    SourceLocation location(std::size_t offset) const;
    const std::string& text() const { return snapshot_->text; }
    const std::string& path() const { return path_; }
    std::string excerpt(std::size_t offset) const;
    std::string diagnostic(std::size_t offset, std::string_view message) const;
private:
    struct Snapshot { std::string text; std::vector<std::size_t> lines; };
    std::string path_;
    std::shared_ptr<const Snapshot> snapshot_;
};
}
