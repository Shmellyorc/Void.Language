#pragma once
#include "void/frontend.hpp"
#include <map>
#include <iterator>
#include <span>
namespace vd {
struct SemanticToken {
    enum class Kind { namespace_, type, structure, interface_, enumeration, type_parameter,
                      parameter, variable, property, enum_member, event, function, method, operator_, count };
    enum Modifier : unsigned { declaration = 1, definition = 2, readonly = 4, static_ = 8, default_library = 16 };
    std::size_t begin, end;
    Kind kind;
    unsigned modifiers;
};
// Owns the successful analysis. Query results are copies with owning source spans.
class ToolingIndex {
    std::shared_ptr<const ValidatedAnalysis> analysis_;
    std::map<std::string,std::size_t> declarations_;
    std::map<std::string,std::vector<std::size_t>> modules_, incoming_, positions_;
public:
    // Ordinary records precede editor records, exactly as in the former copied
    // vectors. Views and their iterators retain the immutable backing analysis.
    template<class T> class Records {
        friend class ToolingIndex;
        std::shared_ptr<const ValidatedAnalysis> owner_;
        std::span<const T> first_,second_;
        Records(std::shared_ptr<const ValidatedAnalysis> owner,const std::vector<T>& first,const std::vector<T>& second)
            :owner_(std::move(owner)),first_(first),second_(second) {}
    public:
        std::size_t size() const {return first_.size()+second_.size();}
        bool empty() const {return !size();}
        const T& operator[](std::size_t n) const {return n<first_.size()?first_[n]:second_[n-first_.size()];}
        class iterator {
            friend class Records;
            std::shared_ptr<const ValidatedAnalysis> owner_;
            const T* current_=nullptr;
            const T* limit_=nullptr;
            const T* next_=nullptr;
            const T* next_limit_=nullptr;
            iterator(const Records& view,bool at_end):owner_(view.owner_) {
                auto end=[](auto span){return span.empty()?span.data():span.data()+span.size();};
                if(at_end){current_=view.second_.empty()?end(view.first_):end(view.second_);return;}
                if(view.first_.empty()) {
                    current_=view.second_.empty()?view.first_.data():view.second_.data();
                    limit_=view.second_.empty()?current_:end(view.second_);
                } else {
                    current_=view.first_.data();limit_=end(view.first_);
                    if(!view.second_.empty()){next_=view.second_.data();next_limit_=end(view.second_);}
                }
            }
        public:
            using iterator_category=std::forward_iterator_tag;
            using value_type=T;
            using difference_type=std::ptrdiff_t;
            using pointer=const T*;
            using reference=const T&;
            iterator()=default;
            reference operator*() const {return *current_;}
            pointer operator->() const {return current_;}
            iterator& operator++(){
                ++current_;
                if(current_==limit_ && next_){current_=next_;limit_=next_limit_;next_=nullptr;}
                return *this;
            }
            iterator operator++(int){auto old=*this;++*this;return old;}
            bool operator==(const iterator& other) const {return current_==other.current_;}
        };
        iterator begin() const {return iterator(*this,false);}
        iterator end() const {return iterator(*this,true);}
    };
    auto declarations() const {return Records<AnalyzedDeclaration>(analysis_,analysis_->declarations(),analysis_->editor_declarations());}
    auto references() const {return Records<AnalyzedReference>(analysis_,analysis_->references(),analysis_->editor_references());}
    explicit ToolingIndex(std::shared_ptr<const ValidatedAnalysis> analysis);
    std::optional<AnalyzedDeclaration> declaration(const std::string& identity) const;
    std::vector<AnalyzedDeclaration> definitions_at(const std::string& module, std::size_t byte_offset) const;
    std::optional<AnalyzedDeclaration> definition_at(const std::string& module, std::size_t byte_offset) const;
    std::vector<AnalyzedReference> references_to(const std::string& identity) const;
    std::vector<AnalyzedDeclaration> declarations_in(const std::string& module) const;
    std::vector<SemanticToken> semantic_tokens(const std::string& module) const;
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
    std::optional<std::filesystem::path> project_root_, project_request_;
    std::optional<std::string> entry_override_;
    ManifestOverrides manifests_;
    EditorDiscovery discovery_;
    std::map<std::string, DocumentRevision> documents_;
    std::uint64_t revision_ = 0;
public:
    std::uint64_t revision() const { return revision_; }
    const auto& documents() const { return documents_; }
    const auto& overrides() const { return overrides_; }
    const auto& project_root() const { return project_root_; }
    const auto& entry_override() const { return entry_override_; }
    const auto& manifests() const { return manifests_; }
    const auto& discovery() const { return discovery_; }
    bool same_revision(const ProjectSnapshot& other) const { return identity_ && identity_ == other.identity_; }
};
struct EditorDocumentAnalysis {
    AnalysisResult result;
    std::optional<ToolingIndex> index;
};
struct EditorRecoveryAnalysis {
    std::shared_ptr<const ValidatedAnalysis> program;
    ToolingIndex index;
};
class SessionAnalysis {
    friend class ToolingSession;
    ProjectSnapshot snapshot_;
    AnalysisResult result_;
    std::shared_ptr<const ToolingIndex> index_;
    std::optional<ProjectAnalysisResult> project_;
    std::shared_ptr<const ValidatedAnalysis> semantic_;
    std::map<std::string, EditorDocumentAnalysis> editor_documents_;
    std::map<std::string, EditorRecoveryAnalysis> recovery_;
public:
    const ProjectSnapshot& snapshot() const { return snapshot_; }
    const AnalysisResult& result() const { return result_; }
    const auto& index() const { return index_; } // Explicit historical access.
    const auto& project() const { return project_; }
    const auto& semantic_analysis() const { return semantic_; }
    const auto& editor_documents() const { return editor_documents_; }
    const EditorRecoveryAnalysis* recovery_for(const std::string& document) const {
        auto found=recovery_.find(document);
        return found==recovery_.end()?nullptr:&found->second;
    }
    const ToolingIndex* index_for(const std::string& document) const {
        if(auto found=editor_documents_.find(document);found!=editor_documents_.end())
            return found->second.index?&*found->second.index:nullptr;
        return index_.get();
    }
    bool succeeded() const {return project_?project_->succeeded():result_.succeeded();}
};
struct ProjectSession {std::filesystem::path root; std::optional<std::string> entry = {};};
// Single-threaded orchestration. Selecting, running and accepting are separate so
// a future scheduler can deliver results out of order without publishing stale data.
class ToolingSession {
    ProjectSnapshot current_;
    std::vector<std::string> changed_;
    std::shared_ptr<const SessionAnalysis> latest_, last_success_;
    InvalidationPlan accepted_plan_;
    std::string selected_context_;
    void change(const std::string&, DocumentRevision, std::optional<SourceFile>, bool close);
    bool manifest_document(const std::string&) const;
    bool overridden(const std::string&) const;
    void snapshot_discovery();
public:
    explicit ToolingSession(std::filesystem::path entry);
    explicit ToolingSession(ProjectSession);
    ToolingSession(const ToolingSession&) = delete;
    ToolingSession& operator=(const ToolingSession&) = delete;
    void open(const std::string&, DocumentRevision, std::string text);
    void replace(const std::string&, DocumentRevision, std::string text);
    void unavailable(const std::string&, DocumentRevision);
    void close(const std::string&, DocumentRevision);
    // Explicit disk refresh invalidates freshness even if bytes have not changed.
    void refresh();
    void reselect(std::optional<std::string> entry = {});
    bool requires_reselection() const;
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
