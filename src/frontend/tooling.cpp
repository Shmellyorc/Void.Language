#include "void/tooling.hpp"
#include <algorithm>
#include <iterator>
#include <set>
#include <stdexcept>
#include <tuple>
namespace vd {
ToolingIndex::ToolingIndex(std::shared_ptr<const ValidatedAnalysis> analysis) : analysis_(std::move(analysis)) {
    if (!analysis_) throw std::invalid_argument("tooling index requires successful analysis");
    const auto& declarations = analysis_->declarations();
    for (std::size_t i=0; i<declarations.size(); ++i) {
        declarations_.emplace(declarations[i].identity,i);
        modules_[declarations[i].module].push_back(i);
    }
    for (auto& [module, indices] : modules_) {
        (void)module;
        std::sort(indices.begin(),indices.end(),[&](auto a,auto b) { return declarations[a].identity < declarations[b].identity; });
    }
    const auto& references = analysis_->references();
    for (std::size_t i=0; i<references.size(); ++i) {
        incoming_[references[i].to].push_back(i);
        positions_[references[i].source.module_identity].push_back(i);
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
    return analysis_->declarations()[found->second];
}
std::optional<AnalyzedDeclaration> ToolingIndex::definition_at(const std::string& module,std::size_t offset) const {
    auto found=positions_.find(module);
    if (found==positions_.end()) return {};
    const auto& references=analysis_->references();
    const auto& indices=found->second;
    auto next=std::upper_bound(indices.begin(),indices.end(),offset,[&](auto at,auto i) { return at < references[i].source.begin; });
    if (next==indices.begin()) return {};
    const auto& r=references[*std::prev(next)];
    if (offset>=r.source.end) return {};
    return declaration(r.to);
}
std::vector<AnalyzedReference> ToolingIndex::references_to(const std::string& identity) const {
    std::vector<AnalyzedReference> result;
    auto found=incoming_.find(identity);
    if (found!=incoming_.end()) for(auto i:found->second) result.push_back(analysis_->references()[i]);
    return result;
}
std::vector<AnalyzedDeclaration> ToolingIndex::declarations_in(const std::string& module) const {
    std::vector<AnalyzedDeclaration> result;
    auto found=modules_.find(module);
    if(found!=modules_.end()) for(auto i:found->second) result.push_back(analysis_->declarations()[i]);
    return result;
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
            if(d.kind==AnalyzedDeclaration::Kind::local_constant) references[d.identity].insert(d.owner);
        }
        for(const auto& r:analysis.references()) {
            references[r.to].insert(r.from); bindings[r.from].push_back(r.to);
        }
    }
};
bool metadata_equal(const AnalyzedDeclaration& a,const AnalyzedDeclaration& b) {
    return a.kind==b.kind && a.module==b.module && a.owner==b.owner && a.visibility==b.visibility &&
        a.parameters==b.parameters && a.type==b.type && a.constant_syntax==b.constant_syntax && a.value==b.value;
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
