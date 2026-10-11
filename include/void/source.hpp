#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
namespace vd {
struct SourceLocation { std::string path; unsigned line = 1; unsigned column = 1; };
class SourceFile {
public:
    SourceFile(std::string path, std::string text);
    SourceLocation location(std::size_t offset) const;
    std::string diagnostic(std::size_t offset, std::string_view message) const;
private:
    std::string path_, text_;
    std::vector<std::size_t> lines_;
};
}
