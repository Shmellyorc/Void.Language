#include "void/tooling.hpp"
#include "void/path.hpp"

#include <algorithm>
#include <stdexcept>
#include <tuple>

namespace vd {
namespace {
using Site = std::pair<std::string,std::size_t>;
Site site(const SourceSpan& span) {
    return {span.module_identity,span.begin};
}
}

std::vector<AnalyzedDeclaration> ToolingIndex::rename_family(const std::string& identity) const {
    std::map<std::string,std::vector<std::string>> relationships;
    const auto connect=[&](const auto& a,const auto& b) {
        relationships[a].push_back(b);
        relationships[b].push_back(a);
    };
    for(const auto& [a,b]:analysis_->rename_relationships())connect(a,b);
    // A single authored property name defines both accessors.
    std::map<Site,std::string> properties;
    for(const auto& record:declarations())if(!record.builtin && record.editor_symbol_kind==7) {
        auto [previous,inserted]=properties.emplace(site(record.source),record.identity);
        if(!inserted)connect(previous->second,record.identity);
    }
    std::vector<std::string> pending{identity};
    std::set<std::string> visited;
    std::vector<AnalyzedDeclaration> result;
    while(!pending.empty()) {
        auto member=std::move(pending.back());pending.pop_back();
        if(!visited.insert(member).second)continue;
        auto record=declaration(member);
        if(!record)throw std::invalid_argument("A related member has no resolved source declaration");
        result.push_back(std::move(*record));
        if(auto neighbors=relationships.find(member);neighbors!=relationships.end())
            pending.insert(pending.end(),neighbors->second.begin(),neighbors->second.end());
    }
    return result;
}

bool ToolingIndex::unresolved_rename_use(const std::string& spelling) const {
    const auto found=analysis_->unresolved_template_names().find(spelling);
    if(found==analysis_->unresolved_template_names().end())return false;
    std::set<Site> bound;
    for(const auto& record:declarations())if(!record.builtin)bound.insert(site(record.source));
    for(const auto& ref:references())if(!ref.implicit && !ref.navigation_only && !ref.semantic_only)
        bound.insert(site(ref.source));
    return std::any_of(found->second.begin(),found->second.end(),[&](const auto& span) {
        // Dependency templates cannot resolve names from the consuming project.
        return !span.module_identity.starts_with("/@stdlib/") && !package_source(span.module_identity) &&
            !bound.contains(site(span));
    });
}

void ToolingIndex::validate_rename(const ToolingIndex& edited,
    const std::function<std::size_t(const std::string&,std::size_t)>& translate) const {
    // Validate declaration identity by authored location, including two
    // independently resolved accessors sharing the same written name.
    using Key=std::tuple<std::string,std::size_t,AnalyzedDeclaration::Kind>;
    std::map<Key,std::vector<const AnalyzedDeclaration*>> by_site;
    for(const auto& d:edited.declarations())if(!d.builtin)
        by_site[{d.module,d.source.begin,d.kind}].push_back(&d);
    std::map<Key,std::size_t> consumed;
    std::map<std::string,std::string> identities;
    for(const auto& d:declarations()) {
        if(d.builtin) {
            const auto next=edited.declaration(d.identity);
            if(!next || !next->builtin || next->type!=d.type || next->parameters!=d.parameters)
                throw std::invalid_argument("Rename changes a compiler-owned declaration");
            identities.emplace(d.identity,d.identity);
            continue;
        }
        Key key{d.module,translate(d.module,d.source.begin),d.kind};
        auto next=by_site.find(key);
        const auto index=consumed[key]++;
        if(next==by_site.end() || index>=next->second.size())
            throw std::invalid_argument("Rename changes declaration structure");
        identities.emplace(d.identity,next->second[index]->identity);
    }
    if(identities.size()!=declarations().size() || edited.declarations().size()!=declarations().size())
        throw std::invalid_argument("Rename changes declaration structure");

    using Binding=std::tuple<std::string,std::size_t,std::string>;
    std::set<Binding> expected,actual;
    for(const auto& ref:references())if(!ref.implicit) {
        const auto target=identities.find(ref.to);
        if(target==identities.end())throw std::invalid_argument("An existing reference has an uncertain target");
        expected.emplace(ref.source.module_identity,translate(ref.source.module_identity,ref.source.begin),target->second);
    }
    for(const auto& ref:edited.references())if(!ref.implicit)
        actual.emplace(ref.source.module_identity,ref.source.begin,ref.to);
    if(expected!=actual)throw std::invalid_argument("Rename would rebind a written reference");

    std::set<std::pair<std::string,std::string>> relationships;
    for(const auto& [left,right]:analysis_->rename_relationships()) {
        if(!identities.contains(left) || !identities.contains(right))
            throw std::invalid_argument("An existing member relationship has an uncertain target");
        auto a=identities.at(left),b=identities.at(right);
        if(b<a)std::swap(a,b);
        relationships.emplace(std::move(a),std::move(b));
    }
    if(relationships!=edited.analysis_->rename_relationships())
        throw std::invalid_argument("Rename would change a virtual or interface member relationship");
}
}
