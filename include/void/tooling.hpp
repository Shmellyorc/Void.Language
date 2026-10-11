#pragma once
#include "void/frontend.hpp"
#include <map>
namespace vd {
// Owns the successful analysis. Query results are copies with owning source spans.
class ToolingIndex {
    std::shared_ptr<const ValidatedAnalysis> analysis_;
    std::map<std::string,std::size_t> declarations_;
    std::map<std::string,std::vector<std::size_t>> modules_, incoming_, positions_;
public:
    explicit ToolingIndex(std::shared_ptr<const ValidatedAnalysis> analysis);
    std::optional<AnalyzedDeclaration> declaration(const std::string& identity) const;
    std::optional<AnalyzedDeclaration> definition_at(const std::string& module, std::size_t byte_offset) const;
    std::vector<AnalyzedReference> references_to(const std::string& identity) const;
    std::vector<AnalyzedDeclaration> declarations_in(const std::string& module) const;
};
// Source contract only; never authorization to publish code or a host ABI verdict.
enum class SourceCompatibility { unknown, exact_match, restart_required };
struct InvalidationPlan {
    std::vector<std::string> directly_changed_modules;
    std::vector<std::string> added_modules, removed_modules, unknown_modules;
    std::vector<std::string> affected_declarations; // Seeds in directly changed modules.
    std::vector<std::string> dependent_declarations, dependent_modules;
    std::vector<std::string> changed_declarations, changed_bindings; // Confirmed metadata differences only.
    bool graph_changed = false; // Confirmed only with two validated snapshots.
    bool requires_graph_discovery = true, requires_full_reanalysis = true;
    bool has_validated_comparison = false;
    SourceCompatibility source_compatibility = SourceCompatibility::unknown;
    bool operator==(const InvalidationPlan&) const = default;
};
// VFS identities are exact keys, never filesystem paths. No disk reads, compilation,
// publication or mutation. A null after includes missing/invalid candidate graphs.
InvalidationPlan plan_invalidation(const ValidatedAnalysis& before,
    const std::vector<std::string>& changed_modules, const ValidatedAnalysis* after = nullptr);
}
