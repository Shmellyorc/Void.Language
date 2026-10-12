#pragma once
#include "void/source.hpp"
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace vd {
enum class Severity { error, warning, information };
// Assigned IDs are a public compiler contract; see docs/DIAGNOSTICS.md.
enum class DiagnosticCode {
    utf8 = 1001, indentation = 1002, character = 1003, numeric_literal = 1004,
    import_quote = 1005, syntax = 1101, identifier = 1102, unsupported_type = 1103,
    while_loop = 1104, unsupported_iteration = 1105, initializer = 1106, nesting = 1107,
    type_mismatch = 2001, unknown_identifier = 2002, unknown_function = 2003,
    argument_count = 2004, duplicate = 2005, missing_return = 2006,
    readonly = 2007, not_callable = 2008, invalid_value = 2009, loop_control = 2010,
    constant_call = 2011, constant_cycle = 2012, zero_divisor = 2013,
    integer_range = 2014, numeric_conversion = 2015, numeric_operator = 2016, numeric_representability = 2017, receiver = 2018, unknown_method = 2019, initializer_deferred = 2020, import_path = 3001, import_read = 3002,
    visibility = 3003, ambiguous = 3004, alias_conflict = 3005, import_context = 3006,
    entry = 4001, limit = 9001
};
struct SourceSpan {
    std::shared_ptr<const SourceFile> source;
    std::string module_identity;
    std::size_t begin = 0, end = 0; // Half-open byte offsets; insertion spans may be empty.
};
struct RelatedLocation { SourceSpan span; std::string description; };
struct Diagnostic {
    explicit Diagnostic(DiagnosticCode assigned_code) : code(assigned_code) {}
    DiagnosticCode code;
    Severity severity = Severity::error;
    std::string description;
    SourceSpan primary;
    std::vector<RelatedLocation> related;
    std::string expected_type, received_type, hint;
};
std::string diagnostic_code(DiagnosticCode code);
std::string render_diagnostic(const Diagnostic& diagnostic);
// Both APIs expose identical structured errors. No IR is returned on failure.
class CompilationError : public std::invalid_argument {
    std::vector<Diagnostic> diagnostics_;
public:
    explicit CompilationError(std::vector<Diagnostic> diagnostics);
    const std::vector<Diagnostic>& diagnostics() const { return diagnostics_; }
};
}
