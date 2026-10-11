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

namespace vd {
using DocumentRevision = std::uint64_t;
// Identity is token ownership, not a finite hash or a filesystem timestamp.
class ProjectSnapshot {
    friend class ToolingSession;
    struct Identity {};
    std::shared_ptr<const Identity> identity_;
    std::filesystem::path entry_;
    SourceOverrides overrides_;
    std::map<std::string, DocumentRevision> documents_;
    std::uint64_t revision_ = 0;
public:
    std::uint64_t revision() const { return revision_; }
    const auto& documents() const { return documents_; }
    const auto& overrides() const { return overrides_; }
    bool same_revision(const ProjectSnapshot& other) const { return identity_ && identity_ == other.identity_; }
};
class SessionAnalysis {
    friend class ToolingSession;
    ProjectSnapshot snapshot_;
    AnalysisResult result_;
    std::shared_ptr<const ToolingIndex> index_;
public:
    const ProjectSnapshot& snapshot() const { return snapshot_; }
    const AnalysisResult& result() const { return result_; }
    const auto& index() const { return index_; } // Explicit historical access.
};
// Single-threaded orchestration. Selecting, running and accepting are separate so
// a future scheduler can deliver results out of order without publishing stale data.
class ToolingSession {
    ProjectSnapshot current_;
    std::vector<std::string> changed_;
    std::shared_ptr<const SessionAnalysis> latest_, last_success_;
    InvalidationPlan accepted_plan_;
    void change(const std::string&, DocumentRevision, std::optional<SourceFile>, bool close);
public:
    explicit ToolingSession(std::filesystem::path entry);
    ToolingSession(const ToolingSession&) = delete;
    ToolingSession& operator=(const ToolingSession&) = delete;
    void open(const std::string&, DocumentRevision, std::string text);
    void replace(const std::string&, DocumentRevision, std::string text);
    void unavailable(const std::string&, DocumentRevision);
    void close(const std::string&, DocumentRevision);
    // Explicit disk refresh invalidates freshness even if bytes have not changed.
    void refresh();
    ProjectSnapshot select() const { return current_; }
    static std::shared_ptr<const SessionAnalysis> analyze(ProjectSnapshot);
    bool accept(std::shared_ptr<const SessionAnalysis>);
    bool is_current(const SessionAnalysis& result) const { return current_.same_revision(result.snapshot()); }
    std::shared_ptr<const SessionAnalysis> current_analysis() const;
    std::shared_ptr<const ToolingIndex> current_index() const;
    const auto& latest_accepted() const { return latest_; }
    const auto& last_success() const { return last_success_; }
    InvalidationPlan invalidation() const;
};
}
