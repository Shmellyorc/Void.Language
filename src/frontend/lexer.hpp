#pragma once

#include "void/conditional.hpp"
#include "void/frontend.hpp"
#include <set>

namespace vd::frontend {
enum class TokenKind { invalid, name, number, path, string, symbol, newline, indent, dedent, end };
struct Token { TokenKind kind; std::string text; std::size_t offset; std::string decoded = {}; std::optional<std::size_t> source_width = {}; bool format_spec = false; };
using Code = DiagnosticCode;

// A poisoned dependency has already produced a primary error. It is never a value.
struct Poison {};

class Diagnostics;

// A second editor-only parse may recognize a small number of safe holes in
// one open source. The ordinary analysis still owns all reported diagnostics.
struct EditorRecovery {
    std::string_view module;
    std::shared_ptr<const ValidatedAnalysis> analysis;
    bool repaired = false;
    bool member_probe = false;
    unsigned repairs = 0;
    bool presentation = false;
    bool available() const { return repairs < 8; }
    void record() { ++repairs; repaired = true; }
};

struct Indentation {
    std::size_t end, width = 0;
    char style = 0;
    std::size_t mixed = std::string::npos;
};

inline bool letter(char c) { return (c >= 'a' && c <= 'z') || c == '_'; }
inline bool digit(char c) { return c >= '0' && c <= '9'; }
extern const std::set<std::string> reserved;

SourceSpan span(const SourceFile&, const Token&);
[[noreturn]] void error(const SourceFile&, const Token&, Code, const std::string& message,
                        const std::string& hint = {}, std::vector<RelatedLocation> related = {},
                        std::string expected = {}, std::string received = {});

Indentation indentation_at(std::string_view, std::size_t start);
std::vector<Token> lex(const SourceFile&, Diagnostics&, bool expand_interpolation = true,
                       EditorRecovery* = nullptr, const BuildConditions* = nullptr,
                       std::vector<ConditionalRegion>* = nullptr);
}
