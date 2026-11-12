#include "void/tooling.hpp"
#include "void/path.hpp"
#include <algorithm>
#include <iterator>
#include <set>
#include <stdexcept>
#include <tuple>
namespace vd {
ToolingIndex::ToolingIndex(std::shared_ptr<const ValidatedAnalysis> analysis) : analysis_(std::move(analysis)) {
    if (!analysis_) throw std::invalid_argument("tooling index requires successful analysis");
    const auto declarations = this->declarations();
    for (std::size_t i=0; i<declarations.size(); ++i) {
        declarations_.emplace(declarations[i].identity,i);
        modules_[declarations[i].module].push_back(i);
    }
    for (auto& [module, indices] : modules_) {
        (void)module;
        std::sort(indices.begin(),indices.end(),[&](auto a,auto b) { return declarations[a].identity < declarations[b].identity; });
    }
    const auto references = this->references();
    for (std::size_t i=0; i<references.size(); ++i) {
        if (!references[i].implicit) {
            incoming_[references[i].to].push_back(i);
            positions_[references[i].source.module_identity].push_back(i);
        }
    }
    auto less = [&](auto a,auto b) {
        const auto& x=references[a]; const auto& y=references[b];
        return std::tie(x.source.module_identity,x.source.begin,x.source.end,x.from,x.to,x.spelling) <
               std::tie(y.source.module_identity,y.source.begin,y.source.end,y.from,y.to,y.spelling);
    };
    for (auto* index : {&incoming_,&positions_}) for (auto& [key,indices] : *index) {
        (void)key; std::stable_sort(indices.begin(),indices.end(),less);
    }
}
std::optional<AnalyzedDeclaration> ToolingIndex::declaration(const std::string& identity) const {
    auto found=declarations_.find(identity);
    if (found==declarations_.end()) return {};
    return declarations()[found->second];
}
std::vector<AnalyzedDeclaration> ToolingIndex::definitions_at(const std::string& module,std::size_t offset) const {
    std::vector<AnalyzedDeclaration> result;
    auto found=positions_.find(module);
    if(found==positions_.end())return result;
    const auto& indices=found->second;
    auto next=std::upper_bound(indices.begin(),indices.end(),offset,[&](auto at,auto i){return at<references()[i].source.begin;});
    if(next==indices.begin())return result;
    auto begin=references()[*std::prev(next)].source.begin;
    std::set<std::string> seen;
    while(next!=indices.begin()) {
        const auto& ref=references()[*--next];if(ref.source.begin!=begin)break;
        if(offset>=ref.source.end || !seen.insert(ref.to).second)continue;
        if(auto d=declaration(ref.to)){if(!ref.signature.empty())d->signature=ref.signature;result.push_back(std::move(*d));}
    }
    // Selected user hooks are the source definition of a compiler conversion.
    if(std::any_of(result.begin(),result.end(),[](const auto& d){return !d.builtin;}))std::erase_if(result,[](const auto& d){return d.builtin;});
    // A constructor spelling references both its type and its initializer. Keep
    // established initializer navigation, rather than treating those two roles
    // as different generic specializations.
    std::set<std::string> initialized;
    for(const auto& d:result)if(d.kind==AnalyzedDeclaration::Kind::initializer)initialized.insert(d.owner);
    std::erase_if(result,[&](const auto& d){return (d.kind==AnalyzedDeclaration::Kind::value_structure || d.kind==AnalyzedDeclaration::Kind::managed_structure) && initialized.contains(d.identity);});
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.identity<b.identity;});
    return result;
}
std::optional<AnalyzedDeclaration> ToolingIndex::definition_at(const std::string& module,std::size_t offset) const {
    auto selected=definitions_at(module,offset);
    if(selected.size()!=1)return {};
    return std::move(selected.front());
}
std::vector<AnalyzedReference> ToolingIndex::references_to(const std::string& identity) const {
    std::vector<AnalyzedReference> result;
    auto found=incoming_.find(identity);
    if (found!=incoming_.end()) for(auto i:found->second) result.push_back(references()[i]);
    return result;
}
std::vector<AnalyzedDeclaration> ToolingIndex::declarations_in(const std::string& module) const {
    std::vector<AnalyzedDeclaration> result;
    auto found=modules_.find(module);
    if(found!=modules_.end()) for(auto i:found->second) result.push_back(declarations()[i]);
    return result;
}
namespace {
SemanticToken::Kind semantic_kind(const AnalyzedDeclaration& d) {
    using D=AnalyzedDeclaration::Kind;
    using K=SemanticToken::Kind;
    // These editor kinds come from validated enum, accessor and event declarations.
    switch(d.editor_symbol_kind) {
    case 7: return K::property;
    case 10: return K::enumeration;
    case 22: return K::enum_member;
    case 24: return K::event;
    }
    switch(d.kind) {
    case D::import_alias: return K::namespace_;
    case D::value_structure: case D::managed_structure: return d.builtin ? K::type : K::structure;
    case D::interface_type: return K::interface_;
    case D::type_parameter: return K::type_parameter;
    case D::parameter: case D::return_component: return K::parameter;
    case D::value_field: case D::managed_field: return K::property;
    case D::function: return K::function;
    case D::receiver_method: case D::interface_requirement: case D::initializer: return K::method;
    case D::operator_function: return K::operator_;
    case D::global_variable: case D::module_constant: case D::local_constant: case D::local_variable: return K::variable;
    }
    throw std::logic_error("Unknown semantic declaration kind");
}

bool written_name(std::string_view text) {
    return !text.empty() && (text.front()<'0' || text.front()>'9') &&
        text.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")==std::string_view::npos;
}
}

std::vector<SemanticToken> ToolingIndex::semantic_tokens(const std::string& module) const {
    using D=AnalyzedDeclaration::Kind;
    using K=SemanticToken::Kind;
    struct Candidate { SemanticToken token; unsigned priority; bool conflict=false; };
    std::map<std::pair<std::size_t,std::size_t>,Candidate> sites;
    auto add=[&](const SourceSpan& site,const AnalyzedDeclaration& d,bool declares,std::string_view spelling={}) {
        if(!site.source || site.begin>=site.end || site.end>site.source->text().size())return;
        const auto text=std::string_view(site.source->text()).substr(site.begin,site.end-site.begin);
        if(!declares && spelling!=text)return;
        const auto kind=semantic_kind(d);
        if(kind==K::operator_ ? text!=d.operator_glyph : !written_name(text))return;
        unsigned modifiers=0;
        if(declares) {
            modifiers|=SemanticToken::declaration;
            if(!d.declaration_only && d.kind!=D::interface_requirement && kind!=K::parameter && kind!=K::type_parameter && kind!=K::namespace_)
                modifiers|=SemanticToken::definition;
        }
        if(d.readonly || d.kind==D::module_constant || d.kind==D::local_constant || kind==K::enum_member)
            modifiers|=SemanticToken::readonly;
        if(d.static_member || kind==K::enum_member)modifiers|=SemanticToken::static_;
        if(d.builtin || d.module.starts_with("/@stdlib/"))modifiers|=SemanticToken::default_library;
        // The written type takes precedence when construction also references its initializer.
        // Authored declarations precede references and compiler-generated editor bindings.
        const unsigned priority=declares ? (d.visibility!="local" ? 3 : 2) :
            (d.kind==D::initializer ? 0 : 1);
        Candidate candidate{{site.begin,site.end,kind,modifiers},priority};
        auto [entry,inserted]=sites.try_emplace({site.begin,site.end},candidate);
        if(inserted)return;
        auto& current=entry->second;
        if(priority>current.priority)current=candidate;
        else if(priority==current.priority) {
            current.conflict|=kind!=current.token.kind;
            current.token.modifiers&=modifiers;
        }
    };
    const auto records=declarations();
    if(auto found=modules_.find(module);found!=modules_.end())
        for(auto i:found->second)if(!records[i].builtin)add(records[i].source,records[i],true);
    const auto uses=references();
    if(auto found=positions_.find(module);found!=positions_.end())for(auto i:found->second) {
        const auto& use=uses[i];
        if(use.navigation_only)continue;
        if(auto binding=declarations_.find(use.to);binding!=declarations_.end()) {
            auto spelling=std::string_view(use.spelling);
            if(auto separator=spelling.find("::");separator!=std::string_view::npos)spelling.remove_prefix(separator+2);
            add(use.source,records[binding->second],false,spelling);
        }
    }
    std::vector<SemanticToken> tokens;
    for(const auto& [key,candidate]:sites) {
        (void)key;
        if(!candidate.conflict && (tokens.empty() || tokens.back().end<=candidate.token.begin))tokens.push_back(candidate.token);
    }
    return tokens;
}

namespace {
using Keys=std::set<std::string>;
using Edges=std::map<std::string,Keys>;
// Iterative finite-graph traversal; every identity is enqueued at most once.
Keys closure(Keys reached,const Edges& edges) {
    std::vector<std::string> work(reached.begin(),reached.end());
    for(std::size_t n=0;n<work.size();++n) {
        auto found=edges.find(work[n]);
        if(found!=edges.end()) for(const auto& key:found->second) if(reached.insert(key).second) work.push_back(key);
    }
    return reached;
}
std::vector<std::string> list(const Keys& keys) { return {keys.begin(),keys.end()}; }
struct Graph {
    std::map<std::string,const AnalyzedModule*> modules;
    std::map<std::string,const AnalyzedDeclaration*> declarations;
    std::map<std::string,std::vector<std::string>> bindings;
    Edges imports, references;
    explicit Graph(const ValidatedAnalysis& analysis) {
        for(const auto& m:analysis.modules()) {
            modules.emplace(m.identity,&m);
            for(const auto& target:m.imports) imports[target].insert(m.identity);
        }
        for(const auto& d:analysis.declarations()) {
            declarations.emplace(d.identity,&d);
            if(d.kind==AnalyzedDeclaration::Kind::local_constant || d.kind==AnalyzedDeclaration::Kind::value_field || d.kind==AnalyzedDeclaration::Kind::managed_field || d.kind==AnalyzedDeclaration::Kind::interface_requirement || d.kind==AnalyzedDeclaration::Kind::receiver_method || d.kind==AnalyzedDeclaration::Kind::initializer || d.kind==AnalyzedDeclaration::Kind::operator_function) references[d.identity].insert(d.owner);
        }
        for(const auto& r:analysis.references()) {
            references[r.to].insert(r.from); bindings[r.from].push_back(r.binding_contract.empty()?r.to:r.binding_contract);
        }
    }
};
bool metadata_equal(const AnalyzedDeclaration& a,const AnalyzedDeclaration& b) {
    auto same_type=[](auto x,auto y) { return x==y && x.readonly==y.readonly && x.readonly_references==y.readonly_references; };
    return a.operator_glyph==b.operator_glyph && a.readonly==b.readonly && a.base_identity==b.base_identity && a.virtual_slot==b.virtual_slot && a.override_target==b.override_target && a.using_field==b.using_field && a.kind==b.kind && a.module==b.module && a.owner==b.owner && a.visibility==b.visibility &&
        a.parameters.size()==b.parameters.size() && std::equal(a.parameters.begin(),a.parameters.end(),b.parameters.begin(),same_type) && same_type(a.type,b.type) && a.constant_syntax==b.constant_syntax && a.value==b.value && same_type(a.receiver,b.receiver) && a.receiver_mutates==b.receiver_mutates && a.initialization_contract==b.initialization_contract && a.capability_contract==b.capability_contract && a.construction_parameters==b.construction_parameters && a.required_initialization_fields==b.required_initialization_fields;
}
void merge(Edges& into,const Edges& from) {
    for(const auto& [key,values]:from) into[key].insert(values.begin(),values.end());
}
}
InvalidationPlan plan_invalidation(const ValidatedAnalysis& before,const std::vector<std::string>& hints,const ValidatedAnalysis* after) {
    InvalidationPlan plan;
    Graph old(before);
    Keys changed(hints.begin(),hints.end()), unknown, added, removed, declaration_changes, binding_changes;
    auto imports=old.imports, references=old.references;
    auto declarations=old.declarations;
    Keys namespace_changes;
    if(after) {
        Graph fresh(*after);
        merge(imports,fresh.imports); merge(references,fresh.references);
        declarations.insert(fresh.declarations.begin(),fresh.declarations.end());
        for(const auto& [id,module]:old.modules) {
            auto next=fresh.modules.find(id);
            if(next==fresh.modules.end()) { changed.insert(id); removed.insert(id); namespace_changes.insert(id); }
            else {
                if(module->source.text()!=next->second->source.text()) changed.insert(id);
                if(module->imports!=next->second->imports) { plan.graph_changed=true; changed.insert(id); namespace_changes.insert(id); }
            }
        }
        for(const auto& [id,module]:fresh.modules) {
            (void)module;
            if(!old.modules.contains(id)) { changed.insert(id); added.insert(id); namespace_changes.insert(id); }
        }
        for(const auto& [id,d]:declarations) {
            auto previous=old.declarations.find(id), next=fresh.declarations.find(id);
            if(previous==old.declarations.end() || next==fresh.declarations.end() || !metadata_equal(*previous->second,*next->second)) {
                declaration_changes.insert(id); namespace_changes.insert(d->module);
            }
        }
        Keys owners;
        for(const auto& [id,binding]:old.bindings) { (void)binding; owners.insert(id); }
        for(const auto& [id,binding]:fresh.bindings) { (void)binding; owners.insert(id); }
        for(const auto& id:owners) if(old.bindings[id]!=fresh.bindings[id]) binding_changes.insert(id);
        plan.graph_changed |= !added.empty() || !removed.empty() || before.entry_identity()!=after->entry_identity();
        plan.has_validated_comparison=true;
        plan.requires_graph_discovery=false; plan.requires_full_reanalysis=false;
        // Reuse the authoritative exact compiler contract, never an approximation.
        plan.source_compatibility=before.compatibility_contract()==after->compatibility_contract() ?
            SourceCompatibility::exact_match : SourceCompatibility::restart_required;
        for(const auto& id:changed) if(!old.modules.contains(id) && !fresh.modules.contains(id)) unknown.insert(id);
    } else {
        namespace_changes=changed;
        for(const auto& id:changed) if(!old.modules.contains(id)) unknown.insert(id);
    }
    // Unknown names cannot tell which old importer might acquire a new dependency.
    // Root declarations can affect implicit project-wide lookup without an import.
    Keys module_seeds=changed;
    bool broad=!unknown.empty() || namespace_changes.contains(before.entry_identity()) ||
       (after && namespace_changes.contains(after->entry_identity()));
    if(broad) {
        for(const auto& [id,module]:old.modules) { (void)module; module_seeds.insert(id); }
        if(after) for(const auto& m:after->modules()) module_seeds.insert(m.identity);
    }
    Keys affected, dependent;
    for(const auto& [id,d]:declarations) if(changed.contains(d->module)) affected.insert(id);
    auto namespace_modules=closure(namespace_changes,imports);
    if(broad) namespace_modules.insert(module_seeds.begin(),module_seeds.end());
    for(const auto& [id,d]:declarations) if(namespace_modules.contains(d->module)) dependent.insert(id);
    dependent.insert(declaration_changes.begin(),declaration_changes.end());
    dependent.insert(binding_changes.begin(),binding_changes.end());
    dependent.insert(affected.begin(),affected.end());
    dependent=closure(std::move(dependent),references);
    auto modules=closure(std::move(module_seeds),imports);
    for(const auto& id:dependent) if(declarations.contains(id)) modules.insert(declarations.at(id)->module);
    for(const auto& id:affected) dependent.erase(id);
    for(const auto& id:changed) modules.erase(id);
    plan.directly_changed_modules=list(changed); plan.added_modules=list(added); plan.removed_modules=list(removed); plan.unknown_modules=list(unknown);
    plan.affected_declarations=list(affected); plan.dependent_declarations=list(dependent); plan.dependent_modules=list(modules);
    plan.changed_declarations=list(declaration_changes); plan.changed_bindings=list(binding_changes);
    // A hint absent from both snapshots has no fresh validated representation.
    if(!unknown.empty()) { plan.requires_graph_discovery=true; plan.requires_full_reanalysis=true; plan.has_validated_comparison=false; plan.source_compatibility=SourceCompatibility::unknown; }
    return plan;
}
}

namespace vd {
ToolingSession::ToolingSession(std::filesystem::path entry) {
    current_.entry_ = std::filesystem::absolute(entry);
    refresh();
}
ToolingSession::ToolingSession(ProjectSession project) {
    // Pins location only. The compiler owns physical/policy admission, including
    // diagnostics for malformed/no-entry projects; no source discovery here.
    current_.project_request_=std::filesystem::absolute(project.root);
    current_.project_root_=std::filesystem::canonical(project.root);
    current_.entry_override_=std::move(project.entry);refresh();
}
bool ToolingSession::manifest_document(const std::string& id) const {
    return current_.project_root_ && id.ends_with("/void.project.toml");
}
bool ToolingSession::overridden(const std::string& id) const {
    return manifest_document(id)?current_.manifests_.contains(id):current_.overrides_.contains(id);
}
void ToolingSession::reselect(std::optional<std::string> entry) {
    if(!current_.project_root_)throw std::invalid_argument("Project reselection requires manifest mode.");
    refresh();current_.entry_override_=std::move(entry);selected_context_.clear();
}
bool ToolingSession::matches_selection(const ProjectAnalysis& project) const {
    return selected_context_.empty() || selected_context_==project.context_key();
}
bool ToolingSession::requires_reselection() const {
    if(!current_.project_root_)return false;
    auto current=current_analysis();
    if(!current || !current->project() || !current->project()->project)return true;
    return !matches_selection(*current->project()->project);
}
void ToolingSession::snapshot_discovery() {
    current_.discovery_.clear();
    if(!last_success_ || !last_success_->project())return;
    const auto& project=*last_success_->project()->project;
    if(!project.entry())return;
    const auto entry="/"+path_utf8(project.entry()->lexically_relative(project.root()));
    for(const auto& [id,source]:current_.overrides_)
        if(source && id!=entry)current_.discovery_.insert(id);
}
void ToolingSession::refresh() {
    if (current_.revision_ == UINT64_MAX) throw std::overflow_error("Project revision exhausted");
    if (last_success_) for (const auto& m : last_success_->semantic_analysis()->modules())
        if (std::find(changed_.begin(),changed_.end(),m.identity) == changed_.end()) changed_.push_back(m.identity);
    snapshot_discovery();
    current_.identity_ = std::make_shared<const ProjectSnapshot::Identity>();
    ++current_.revision_;
}
void ToolingSession::change(const std::string& id, DocumentRevision revision, std::optional<SourceFile> source, bool closing) {
    if(manifest_document(id))require_manifest_identity(id);else require_module_identity(id);
    auto previous = current_.documents_.find(id);
    if (!revision || (previous != current_.documents_.end() && revision <= previous->second))
        throw std::invalid_argument("Document revision must strictly increase, including close/delete/reopen");
    if (current_.revision_ == UINT64_MAX) throw std::overflow_error("Project revision exhausted");
    current_.documents_[id] = revision;
    auto& overrides=manifest_document(id)?current_.manifests_:current_.overrides_;
    if (closing) overrides.erase(id);
    else overrides.insert_or_assign(id, std::move(source));
    if (std::find(changed_.begin(),changed_.end(),id) == changed_.end()) changed_.push_back(id);
    snapshot_discovery();
    current_.identity_ = std::make_shared<const ProjectSnapshot::Identity>();
    ++current_.revision_;
}
void ToolingSession::open(const std::string& id, DocumentRevision revision, std::string text) {
    if (overridden(id)) throw std::invalid_argument("Document already open or unavailable; replace or close it first");
    change(id, revision, SourceFile(id,std::move(text)), false);
}
void ToolingSession::replace(const std::string& id, DocumentRevision revision, std::string text) {
    if (!overridden(id)) throw std::invalid_argument("Document must be open or unavailable before replacement");
    change(id, revision, SourceFile(id,std::move(text)), false);
}
void ToolingSession::unavailable(const std::string& id, DocumentRevision revision) { change(id,revision,{},false); }
void ToolingSession::close(const std::string& id, DocumentRevision revision) {
    if (!overridden(id)) throw std::invalid_argument("No override to close");
    change(id,revision,{},true);
}
std::shared_ptr<const SessionAnalysis> ToolingSession::analyze(ProjectSnapshot snapshot) {
    if (!snapshot.identity_) throw std::invalid_argument("Select a project snapshot before analysis");
    auto result = std::make_shared<SessionAnalysis>();
    result->snapshot_ = std::move(snapshot);
    if(result->snapshot_.project_root_) {
        result->project_=analyze_editor_project(*result->snapshot_.project_request_,result->snapshot_.entry_override_,result->snapshot_.overrides_,result->snapshot_.manifests_,result->snapshot_.discovery_);
        if(result->project_->manifest) {
            std::vector<std::string> documents;for(const auto& [id,source]:result->snapshot_.overrides_)if(source)documents.push_back(id);
            auto manifests=result->snapshot_.manifests_;manifests.insert_or_assign("/void.project.toml",result->project_->manifest->source());
            auto failures=check_project_documents(*result->snapshot_.project_request_,result->snapshot_.overrides_,manifests,documents);
            if(!failures.empty()) {
                result->project_->diagnostics.insert(result->project_->diagnostics.end(),failures.begin(),failures.end());result->project_->project.reset();
                auto state=std::make_shared<ProjectState>(*result->project_->state);state->validated=false;result->project_->state=std::move(state);
            }
        }
        result->result_.diagnostics=result->project_->diagnostics;
        if(auto project=result->project_->project){result->result_.program=project->program();result->semantic_=project->semantic_analysis();}
    } else {result->result_=analyze_project(result->snapshot_.entry_,result->snapshot_.overrides_);result->semantic_=result->result_.program;}
    if (result->semantic_) result->index_ = std::make_shared<ToolingIndex>(result->semantic_);
    const auto& selected=result->snapshot_;
    const auto* state=result->project_?result->project_->state.get():nullptr;
    auto recover=[&](const std::vector<Diagnostic>& diagnostics,const std::string* document) {
        std::string module;
        for(const auto& d:diagnostics) {
            if(d.severity!=Severity::error)continue;
            if(d.code!=DiagnosticCode::syntax && d.code!=DiagnosticCode::identifier && !d.editor_enum_recovery)return;
            if(module.empty())module=d.primary.module_identity;
            if(module.empty() || module!=d.primary.module_identity || (document && module!=*document))return;
        }
        auto open=selected.overrides_.find(module);
        if(open==selected.overrides_.end() || !open->second)return;
        auto program=recover_editor_analysis(state?*selected.project_request_:selected.entry_,selected.entry_override_,
            selected.overrides_,state?&selected.manifests_:nullptr,selected.discovery_,module,document!=nullptr);
        if(program)result->recovery_.emplace(module,EditorRecoveryAnalysis{program,ToolingIndex(program)});
    };
    if(!result->semantic_ && (!state || state->manifest))recover(result->result_.diagnostics,nullptr);
    if(!state || state->manifest)for(const auto& [id,source]:selected.overrides_) {
        if(!source)continue;
        if(result->recovery_.contains(id))continue;
        if(state && (state->execution.contains(id) ||
            (state->entry && state->root/utf8_path(id).relative_path()==*state->entry)))continue;
        if(!state && id=="/"+path_utf8(selected.entry_.filename()))continue;
        if(result->semantic_ && std::any_of(result->semantic_->modules().begin(),result->semantic_->modules().end(),
            [&](const auto& module){return module.identity==id;}))continue;
        auto& document=result->editor_documents_[id];
        document.result=analyze_editor_document(state?*selected.project_request_:selected.entry_,selected.overrides_,id,
            state?&selected.manifests_:nullptr);
        if(document.result.program)document.index.emplace(document.result.program);
        else recover(document.result.diagnostics,&id);
    }
    return result;
}
bool ToolingSession::accept(std::shared_ptr<const SessionAnalysis> result) {
    if (!result || !is_current(*result) || (latest_ && latest_->snapshot().same_revision(result->snapshot()))) return false;
    auto hints=changed_;
    if(current_.project_root_ && last_success_ && result->semantic_analysis()) {
        // A completed compiler selection has resolved D, TOML and membership.
        // Feed the source invalidator only its old/new semantic graph domain.
        auto present=[&](const std::string& id){for(const auto* analysis:{last_success_->semantic_analysis().get(),result->semantic_analysis().get()})for(const auto& module:analysis->modules())if(module.identity==id)return true;return false;};
        std::erase_if(hints,[&](const auto& id){return !present(id);});
    }
    accepted_plan_ = last_success_ ? plan_invalidation(*last_success_->semantic_analysis(), hints, result->semantic_analysis().get()) : InvalidationPlan{};
    latest_ = std::move(result);
    if (latest_->succeeded()) {
        if(latest_->project() && selected_context_.empty())selected_context_=latest_->project()->project->context_key();
        last_success_ = latest_; changed_.clear();
    }
    return true;
}
std::shared_ptr<const SessionAnalysis> ToolingSession::current_analysis() const {
    return latest_ && is_current(*latest_) ? latest_ : nullptr;
}
std::shared_ptr<const ToolingIndex> ToolingSession::current_index() const {
    auto current = current_analysis(); return current ? current->index() : nullptr;
}
InvalidationPlan ToolingSession::invalidation() const {
    auto fresh = current_analysis();
    if (fresh) return accepted_plan_;
    if (!last_success_) return {};
    return plan_invalidation(*last_success_->semantic_analysis(), changed_);
}
}
