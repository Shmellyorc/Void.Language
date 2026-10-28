#pragma once
#include "void/source.hpp"
#include "void/path.hpp"
#include <filesystem>
#include <optional>
#include <cstdint>
namespace vd::lsp {
struct Position {
    std::uint32_t line = 0, character = 0;
    bool operator==(const Position&) const = default;
};
// Retains the exact source bytes. No conversion via human diagnostic columns.
class Positions {
    SourceFile source_;
public:
    explicit Positions(SourceFile source) : source_(std::move(source)) {}
    std::optional<Position> position(std::size_t byte_offset) const;
    // LSP clamps characters beyond EOL; interior surrogate positions reject.
    std::optional<std::size_t> offset(Position) const;
};
std::string file_uri(const std::filesystem::path& absolute_path);
std::filesystem::path uri_path(const std::string& uri);
}
