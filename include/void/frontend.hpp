#pragma once
#include "void/ir.hpp"
#include "void/diagnostic.hpp"
#include <filesystem>
#include <functional>
#include <optional>
namespace vd {
// Records are owned by an immutable successful analysis, never by parser storage.
struct AnalyzedModule { std::string identity; SourceFile source; std::vector<std::string> imports; };
struct AnalyzedDeclaration {
    enum class Kind { function, module_constant, local_constant } kind;
    std::string identity, module, visibility;
    SourceSpan source;
    std::vector<ir::Type> parameters;
    ir::Type type = ir::Type::none;
    std::string constant_syntax; // Exact length-delimited tokens, excluding trivia.
    std::optional<std::uint64_t> value = {}; // Module constants only: typed bit pattern.
    std::string owner = {}; // Containing function for local constants; module otherwise.
};
struct AnalyzedReference {
    std::string from, to, spelling;
    SourceSpan source;
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
AnalysisResult analyze_project(const std::filesystem::path& entry);
// One source pipeline shared by checking, JIT and AOT. Errors contain rendered
// source diagnostics and are raised before invoking any backend.
ir::Module compile_source(const SourceFile& source);
struct SourceProgram { SourceFile entry; ir::Module module; };
// Entry parent is the VFS root; no import depends on the caller working directory.
SourceProgram compile_project(const std::filesystem::path& entry);
SourceFile read_source(const std::filesystem::path& path);
void require_source_entry(const SourceFile& source, const ir::Module& module);
}
