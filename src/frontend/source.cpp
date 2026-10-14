#include "void/diagnostic.hpp"
#include <algorithm>
#include <stdexcept>
namespace vd {
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
    // Escape rejected source bytes only in this presentation. Runtime excerpts,
    // owned snapshots, byte spans and source/LSP coordinates remain unchanged.
    const auto& text = source.text();
    auto begin = source.line_starts()[position.line - 1];
    if (begin == 0 && text.starts_with("\xef\xbb\xbf")) begin = 3;
    std::string line;
    std::size_t column = 0, width = 0;
    constexpr char hex[] = "0123456789ABCDEF";
    for (auto i = begin; i < text.size() && text[i] != '\r' && text[i] != '\n';) {
        auto count = utf8_scalar_bytes(text, i);
        auto rendered_width = count ? 1u : 4u;
        if (i < span.begin) column += rendered_width;
        else if (i < span.end) width += rendered_width;
        if (count) line.append(text, i, count);
        else {
            auto byte = static_cast<unsigned char>(text[i]);
            line += "\\x";
            line += hex[byte >> 4]; line += hex[byte & 15];
        }
        i += count ? count : 1;
    }
    auto gutter = std::to_string(position.line);
    result += "\n " + gutter + " | " + line + "\n " + std::string(gutter.size(), ' ') + " | " +
        std::string(column, ' ') + std::string(std::max(std::size_t{1}, width), '^') + "\n";
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
