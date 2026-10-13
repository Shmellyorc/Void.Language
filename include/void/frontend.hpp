#pragma once
#include "void/ir.hpp"
#include "void/diagnostic.hpp"
#include <filesystem>
#include <functional>
#include <optional>
#include <map>
namespace vd {
// Records are owned by an immutable successful analysis, never by parser storage.
struct AnalyzedModule { std::string identity; SourceFile source; std::vector<std::string> imports; };
struct AnalyzedParameter {
    std::string name; ir::Type type; SourceSpan source;
    std::optional<SourceSpan> default_source = {};
};
struct AnalyzedDeclaration {
    enum class Kind { function, module_constant, local_constant, value_structure, value_field, receiver_method, initializer, managed_structure, managed_field } kind;
    std::string identity, module, visibility;
    SourceSpan source;
    std::vector<ir::Type> parameters;
    ir::Type type = ir::Type::none;
    std::string constant_syntax; // Exact length-delimited tokens, excluding trivia.
    std::optional<std::uint64_t> value = {}; // Module constants only: typed bit pattern.
    std::string owner = {}; // Containing declaration, or module.
    ir::Type receiver = ir::Type::none;
    bool receiver_mutates = false;
    bool using_field = false; // Stored composition field; does not affect native layout.
    std::string initialization_contract = {};
    std::vector<AnalyzedParameter> initializer_parameters = {};
    std::string capability_contract = {};
    // Private passing contexts, indexed over receiver then ordinary parameters.
    // Required fields are unavailable at managed initializer entry, but proved
    // ready on every successful exit before complete-object publication.
    std::vector<std::vector<unsigned>> construction_parameters = {};
    std::vector<std::string> required_initialization_fields = {};
};
struct AnalyzedReference {
    std::string from, to, spelling;
    SourceSpan source;
    bool implicit = false; // Dependency edge without a written use site.
};
class ValidatedAnalysis : public std::enable_shared_from_this<ValidatedAnalysis> {
    friend struct AnalysisAccess;
    std::string entry_, contract_, constants_;
    std::vector<AnalyzedModule> modules_;
    std::vector<AnalyzedDeclaration> declarations_;
    std::vector<AnalyzedReference> references_;
    std::function<ir::Module()> lower_;
    ValidatedAnalysis() = default;
public:
    ValidatedAnalysis(const ValidatedAnalysis&) = delete;
    ValidatedAnalysis& operator=(const ValidatedAnalysis&) = delete;
    const std::string& entry_identity() const { return entry_; }
    const auto& modules() const { return modules_; }
    const auto& declarations() const { return declarations_; }
    const auto& references() const { return references_; }
    const std::string& compatibility_contract() const { return contract_; }
    const std::string& constant_contract() const { return constants_; }
    // Lowers the retained validated tree; no filesystem reads or reanalysis.
    ir::Module lower() const;
};
struct AnalysisResult {
    std::shared_ptr<const ValidatedAnalysis> program;
    std::vector<Diagnostic> diagnostics;
    bool succeeded() const { return bool(program); }
};
AnalysisResult analyze_source(const SourceFile& source);
// Exact canonical VFS keys. nullopt is an explicit unavailable/deleted source.
using SourceOverrides = std::map<std::string, std::optional<SourceFile>>;
void require_module_identity(const std::string& identity);
AnalysisResult analyze_project(const std::filesystem::path& entry);
AnalysisResult analyze_project(const std::filesystem::path& entry, const SourceOverrides& overrides);
// One source pipeline shared by checking, JIT and AOT. Errors contain rendered
// source diagnostics and are raised before invoking any backend.
ir::Module compile_source(const SourceFile& source);
struct SourceProgram { SourceFile entry; ir::Module module; };
// Entry parent is the VFS root; no import depends on the caller working directory.
SourceProgram compile_project(const std::filesystem::path& entry);
SourceFile read_source(const std::filesystem::path& path);
void require_source_entry(const SourceFile& source, const ir::Module& module);
}
