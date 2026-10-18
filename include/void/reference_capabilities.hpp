#pragma once
#include "void/ir.hpp"
#include <compare>

namespace vd::ir {
// Provenance is attached to logical reference leaves, not to physical offsets
// or runtime objects. An origin denotes authority supplied by one formal leaf.
struct ReferenceOrigin {
    unsigned parameter;
    ReferencePath path;
    friend auto operator<=>(const ReferenceOrigin&,const ReferenceOrigin&)=default;
};
struct ReferenceFact {
    bool readonly=false;
    std::set<ReferenceOrigin> origins;
    bool join(const ReferenceFact& other) {
        bool changed=other.readonly && !readonly; readonly|=other.readonly;
        for (const auto& origin:other.origins) changed|=origins.insert(origin).second;
        return changed;
    }
};
using ReferenceFacts=std::map<ReferencePath,ReferenceFact>;
inline bool join_facts(ReferenceFacts& into,const ReferenceFacts& from) {
    bool changed=false;
    for (const auto& [path,fact]:from) {
        auto [entry,inserted]=into.try_emplace(path);
        changed|=inserted; changed|=entry->second.join(fact);
    }
    return changed;
}
inline ReferenceFacts reference_facts(Type type) {
    ReferenceFacts result;
    // Private places carry abstract authority/provenance, not managed identity.
    // reference_paths still excludes them from every actual GC root shape.
    if(type.construction) {result[{}].readonly=type.readonly;return result;}
    auto readonly=readonly_paths(type);
    for (const auto& path:reference_paths(type)) result[path].readonly=readonly.contains(path);
    return result;
}
inline ReferenceFacts formal_facts(Type type,unsigned parameter) {
    auto result=reference_facts(type);
    for (auto& [path,fact]:result) fact.origins.insert({parameter,path});
    return result;
}
inline Type fact_type(Type type,const ReferenceFacts& facts) {
    ReferencePaths paths;
    for (const auto& [path,fact]:facts) if (fact.readonly) paths.insert(path);
    return with_readonly_paths(type,paths);
}
inline ReferenceFacts field_facts(Type owner,const ReferenceFacts& facts,unsigned field) {
    ReferenceFacts result=reference_facts(field_type(owner,field));
    if(array_type(owner) && field==array_element_path) {
        for(const auto& [path,fact]:facts)if(!path.empty() && path[0]==field)result[ReferencePath(path.begin()+1,path.end())].join(fact);
        if(owner.kind==Type::growable_array)if(auto found=facts.find({});found!=facts.end() && found->second.readonly)for(auto& [path,fact]:result)fact.readonly=true;
        return result;
    }
    if (owner.kind==Type::managed_ref) {
        if (auto found=facts.find({});found!=facts.end())
            for (auto& [path,fact]:result) fact.join(found->second);
    } else for (const auto& [path,fact]:facts)
        if (!path.empty() && path[0]==field) result[ReferencePath(path.begin()+1,path.end())].join(fact);
    return result;
}
inline ReferenceFacts replaced_facts(ReferenceFacts owner,unsigned field,const ReferenceFacts& value) {
    for (auto p=owner.begin();p!=owner.end();) {
        if (!p->first.empty() && p->first[0]==field) p=owner.erase(p); else ++p;
    }
    for (const auto& [leaf,fact]:value) { auto path=leaf; path.insert(path.begin(),field); owner.emplace(std::move(path),fact); }
    return owner;
}
struct ReferenceEffects {
    std::set<ReferenceOrigin> mutations,retained,required_mutable;
    ReferenceFacts returned,receiver_after;
};
inline bool join_effects(ReferenceEffects& into,const ReferenceEffects& from) {
    bool changed=false;
    for(auto origin:from.mutations) changed|=into.mutations.insert(origin).second;
    for(auto origin:from.retained) changed|=into.retained.insert(origin).second;
    for(auto origin:from.required_mutable) changed|=into.required_mutable.insert(origin).second;
    changed|=join_facts(into.returned,from.returned);
    changed|=join_facts(into.receiver_after,from.receiver_after);
    return changed;
}
inline ReferenceEffects abstract_interface_effects(const ReferenceEffects& concrete,bool receiver_mutates) {
    ReferenceEffects result;
    auto rebase=[](ReferenceOrigin origin) {if(origin.parameter==0) origin.path.clear();return origin;};
    for(auto origin:concrete.mutations) result.mutations.insert(rebase(origin));
    for(auto origin:concrete.retained) result.retained.insert(rebase(origin));
    for(auto origin:concrete.required_mutable) result.required_mutable.insert(rebase(origin));
    if(receiver_mutates) result.mutations.insert(ReferenceOrigin{0,{}});
    for(const auto& [path,fact]:concrete.returned) {
        auto& out=result.returned[path];out.readonly=fact.readonly;
        for(auto origin:fact.origins) out.origins.insert(rebase(origin));
    }
    return result;
}
inline ReferenceEffects unknown_interface_effects(const Function& requirement) {
    ReferenceEffects result;result.mutations.insert(ReferenceOrigin{0,{}});result.retained.insert(ReferenceOrigin{0,{}});result.required_mutable.insert(ReferenceOrigin{0,{}});
    for(auto path:reference_paths(requirement.result)) {
        auto& fact=result.returned[path];fact.readonly=true;
        for(unsigned n=0;n<requirement.parameters.size();++n) for(auto leaf:reference_paths(requirement.parameters[n])) fact.origins.insert({n,leaf});
    }
    return result;
}
inline std::set<ReferenceOrigin> mutable_requirements(const ReferenceFacts& actual,Type expected,const ReferenceFacts* capacity=nullptr) {
    std::set<ReferenceOrigin> result;
    const auto readonly=readonly_paths(expected);
    for (const auto& [path,fact]:actual) if (!readonly.contains(path)) {
        const ReferenceFact* existing=nullptr;
        if (capacity) if (auto found=capacity->find(path);found!=capacity->end()) existing=&found->second;
        for (const auto& origin:fact.origins)
            if (!existing || !existing->origins.contains(origin)) result.insert(origin);
    }
    return result;
}
inline ReferenceFact instantiate_fact(const ReferenceFact& summary,const std::vector<ReferenceFacts>& arguments) {
    ReferenceFact result; result.readonly=summary.readonly;
    for (const auto& origin:summary.origins) if (origin.parameter<arguments.size()) {
        const auto& actual=arguments[origin.parameter];
        if (auto leaf=actual.find(origin.path);leaf!=actual.end()) result.join(leaf->second);
    }
    return result;
}
inline ReferenceFacts instantiate_result(const ReferenceFacts& summary,const std::vector<ReferenceFacts>& arguments) {
    ReferenceFacts result;
    for (const auto& [path,fact]:summary) result[path]=instantiate_fact(fact,arguments);
    return result;
}
}
