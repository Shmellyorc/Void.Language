#include "void/diagnostic.hpp"
#include <algorithm>
#include <stdexcept>
namespace vd {
unsigned utf8_scalar_bytes(std::string_view text, std::size_t i) {
    if (i >= text.size()) return 0;
    auto c = static_cast<unsigned char>(text[i]);
    if (c < 128) return 1;
    unsigned count = c >= 0xc2 && c <= 0xdf ? 2 : c >= 0xe0 && c <= 0xef ? 3 : c >= 0xf0 && c <= 0xf4 ? 4 : 0;
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
SourceFile::SourceFile(std::string path, std::string text)
    : path_(std::move(path)) {
    auto snapshot = std::make_shared<Snapshot>(Snapshot{std::move(text), {0}});
    snapshot_ = snapshot;
    const auto& text_ = snapshot->text;
    auto& lines_ = snapshot->lines;
    for (std::size_t i = 0; i < text_.size(); ++i) {
        if (text_[i] == '\r') {
            if (i + 1 < text_.size() && text_[i + 1] == '\n') ++i;
            lines_.push_back(i + 1);
        } else if (text_[i] == '\n') lines_.push_back(i + 1);
    }
    // Validate scalars, including overlong sequences, surrogates and truncation.
    for (std::size_t i = 0; i < text_.size();) {
        auto count = utf8_scalar_bytes(text_,i);
        if (!count) {
            Diagnostic d{DiagnosticCode::utf8};
            d.description = "Invalid UTF-8 source.";
            d.primary = {std::make_shared<const SourceFile>(*this), {}, i, i + 1};
            d.hint = "Save this file as UTF-8.";
            throw CompilationError({std::move(d)});
        }
        i += count;
    }
}
SourceLocation SourceFile::location(std::size_t offset) const {
    const auto& text_ = snapshot_->text;
    const auto& lines_ = snapshot_->lines;
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
    const auto& text_ = snapshot_->text;
    const auto& lines_ = snapshot_->lines;
    auto pos = location(offset);
    auto begin = lines_[pos.line - 1];
    if (begin == 0 && text_.starts_with("\xef\xbb\xbf")) begin = 3;
    auto end = text_.find_first_of("\r\n", begin);
    auto line = text_.substr(begin, end == std::string::npos ? end : end - begin);
    return line + "\n" + std::string(pos.column - 1, ' ') + "^\n";
}
std::string SourceFile::diagnostic(std::size_t offset, std::string_view message) const {
    Diagnostic d{DiagnosticCode::syntax};
    d.description = message;
    d.primary = {std::make_shared<const SourceFile>(*this), {}, offset, offset};
    return render_diagnostic(d);
}
}

namespace vd {
std::string diagnostic_code(DiagnosticCode code) { return "VOID" + std::to_string(static_cast<int>(code)); }
std::string render_diagnostic(const Diagnostic& d) {
    const auto& span = d.primary;
    const auto& source = *span.source;
    auto position = source.location(span.begin);
    const char* severity = d.severity == Severity::error ? "error" : d.severity == Severity::warning ? "warning" : "info";
    std::string result = position.path + ":" + std::to_string(position.line) + ":" + std::to_string(position.column) +
        ": " + severity + ": " + d.description + "\n    code: " + diagnostic_code(d.code) + "\n";
    // Runtime excerpts deliberately keep their existing ABI and presentation.
    auto excerpt = source.excerpt(span.begin);
    auto newline = excerpt.find('\n');
    auto line = excerpt.substr(0, newline);
    auto gutter = std::to_string(position.line);
    auto width = std::size_t{0};
    for (auto i = span.begin; i < span.end && i < source.text().size(); ++i) {
        auto c = static_cast<unsigned char>(source.text()[i]);
        if (c == '\r' || c == '\n') break;
        if ((c & 0xc0) != 0x80) ++width;
    }
    result += "\n " + gutter + " | " + line + "\n " + std::string(gutter.size(), ' ') + " | " +
        std::string(position.column - 1, ' ') + std::string(std::max(std::size_t{1}, width), '^') + "\n";
    if (!span.module_identity.empty()) result += "    Module: " + span.module_identity + "\n";
    for (const auto& related : d.related) {
        auto location = related.span.source->location(related.span.begin);
        result += "    " + related.description + " at " + location.path + ":" + std::to_string(location.line) + ":" + std::to_string(location.column) + "\n";
        auto nested = d;
        nested.primary = related.span; nested.related.clear(); nested.expected_type.clear(); nested.received_type.clear(); nested.hint.clear();
        // Render related snippets with the same range utility without recursive diagnostics.
        auto snippet = render_diagnostic(nested);
        auto start = snippet.find("\n\n");
        if (start != std::string::npos) result += snippet.substr(start + 1);
    }
    if (!d.expected_type.empty()) result += "    Expected: " + d.expected_type + "\n";
    if (!d.received_type.empty()) result += "    Received: " + d.received_type + "\n";
    if (!d.hint.empty()) result += "    Hint: " + d.hint + "\n";
    return result;
}
namespace {
std::string render_all(const std::vector<Diagnostic>& diagnostics) {
    std::string result;
    for (const auto& d : diagnostics) { if (!result.empty()) result += "\n"; result += render_diagnostic(d); }
    return result;
}
}
CompilationError::CompilationError(std::vector<Diagnostic> diagnostics)
    : std::invalid_argument(render_all(diagnostics)), diagnostics_(std::move(diagnostics)) {}
}
