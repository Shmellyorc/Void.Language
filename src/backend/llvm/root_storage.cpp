#include "root_storage.hpp"
#include "void/numeric.hpp"
#include "void/reference_capabilities.hpp"
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DerivedTypes.h>
#include <algorithm>
#include <iterator>

namespace vd::llvm_backend {
// Reuse the validated construction-view ABI for a bounded local helper, rather
// than passing a stack address through an ordinary managed-reference root.
// Original symbols/signatures remain intact for all other calls and reloads.
namespace {
// Root-copy retirement needs an explicit root-transfer contract in addition to
// freshness. A factory can return fresh storage while also invoking a retaining
// native/callback target; that is not permission to shorten the caller's homes.
bool allocation_root_transfer(ir::Op op) {
    using O=ir::Op;
    switch(op) {
    case O::argument: case O::constant: case O::load: case O::store:
    case O::gc_construct: case O::gc_allocate: case O::gc_read: case O::gc_write:
    case O::gc_poll: case O::gc_collect:
    case O::add: case O::subtract: case O::multiply: case O::divide: case O::remainder:
    case O::less: case O::less_equal: case O::greater: case O::greater_equal:
    case O::equal: case O::not_equal: case O::logical_not: case O::negate: case O::convert:
        return true;
    default:return false;
    }
}
}

// A fresh result is current-heap storage on every successful return. This
// bounded straight-line check reuses typed IR; parameter/field/unknown-call
// results are deliberately not an ownership promise.
std::map<std::string,ElisionFactory> allocation_heap_factories(const ir::Module& input,
        const ir::InstructionTypes& types,const std::map<std::string,ir::ReferenceEffects>& effects) {
    std::map<std::string,ElisionFactory> result;
    for(unsigned fn=0;fn<input.functions.size();++fn) {
        const auto& f=input.functions[fn];
        if(f.visibility!="private" || f.module_identity.empty() || f.asynchronous || f.worker || f.receiver ||
           f.initializer || f.closure || f.cleanup_entry || f.blocks.size()!=1 ||
           f.blocks[0].instructions.size()>64 || f.result.kind!=ir::Type::managed_ref || f.result.construction)
            continue;
        const auto summary=effects.find(f.name);
        // Outlined defaults are already typed but have no formal-effect
        // summary here. They are not a fresh-result contract for callers.
        if(summary==effects.end())continue;
        bool formal=false;
        for(const auto& [path,fact]:summary->second.returned)formal|=!fact.origins.empty();
        if(formal || f.blocks[0].end.kind!=ir::Terminator::Kind::return_value)continue;
        std::vector<bool> locals(f.locals.size()),values;
        bool root_transfer=true;
        for(const auto& i:f.blocks[0].instructions) {
            root_transfer &= allocation_root_transfer(i.op) && !i.receiver;
            if(i.op==ir::Op::gc_write && !(numeric::number(types[fn][0][i.b]) || types[fn][0][i.b]==ir::Type::boolean))
                root_transfer=false;
            bool heap=i.op==ir::Op::gc_construct || i.op==ir::Op::gc_allocate || i.op==ir::Op::global_cell ||
                (i.op==ir::Op::constant && types[fn][0][values.size()].kind==ir::Type::managed_ref && i.immediate==0);
            if(i.op==ir::Op::load)heap=locals[i.a];
            if(i.op==ir::Op::store)locals[i.a]=values[i.b];
            values.push_back(heap);
        }
        if(values[f.blocks[0].end.value])result.emplace(f.name,ElisionFactory{f.module_identity,root_transfer});
    }
    return result;
}
std::map<std::string,ElisionHelper> allocation_elision_helpers(const ir::Module& input,
        const ir::InstructionTypes& types,const std::map<std::string,ir::ReferenceEffects>& effects) {
    std::map<std::string,ElisionHelper> result;
    std::set<std::string> names;
    for(const auto& f:input.functions)names.insert(f.name);
    for(unsigned fn=0;fn<input.functions.size();++fn) {
        const auto& f=input.functions[fn];
        if(f.visibility!="private" || f.module_identity.empty() || f.asynchronous || f.worker ||
           f.receiver || f.initializer || f.closure || f.cleanup_entry || !f.virtual_slot.empty() ||
           !f.construction_origins.empty() || f.blocks.empty() || f.blocks.size()>8)
            continue;
        if(!(numeric::number(f.result) || f.result==ir::Type::boolean || f.result==ir::Type::none))continue;
        std::optional<unsigned> parameter;
        bool safe=true;
        for(unsigned n=0;n<f.parameters.size();++n) {
            auto t=f.parameters[n];
            if(t.kind==ir::Type::managed_ref && !t.nullable && !t.construction && t.definition &&
               !t.definition->native_resource && !t.definition->task_result && !t.definition->callable &&
               !t.definition->interface_type && t.definition->base.expired()) {
                if(parameter)safe=false;else parameter=n;
            } else if(!(numeric::number(t) || t==ir::Type::boolean))safe=false;
        }
        if(!safe || !parameter || *parameter>=f.locals.size() || f.locals[*parameter]!=f.parameters[*parameter])continue;
        // A pre-existing construction context need not have the same body in
        // hand-built validated IR. Do not redirect to an unproven namesake.
        const auto specialized=ir::construction_name(f.name,{*parameter});
        if(names.contains(specialized))continue;
        for(auto t:f.locals)
            if(!(numeric::number(t) || t==ir::Type::boolean || (t.kind==ir::Type::managed_ref && !t.construction)))safe=false;
        unsigned instructions=0;
        for(const auto& block:f.blocks)instructions+=block.instructions.size();
        if(instructions>64)continue;
        const auto& summary=effects.at(f.name);
        for(auto origin:summary.retained)if(origin.parameter==*parameter)safe=false;
        for(const auto& [path,fact]:summary.returned)for(auto origin:fact.origins)
            if(origin.parameter==*parameter)safe=false;
        std::set<unsigned> aliases{*parameter},unborrowed_stores,unborrowed_loads;
        for(unsigned bn=0;bn<f.blocks.size();++bn) {
            const auto& block=f.blocks[bn];
            std::vector<bool> borrowed_values(block.instructions.size());
            if(block.end.kind!=ir::Terminator::Kind::return_value && block.end.kind!=ir::Terminator::Kind::return_void &&
               block.end.kind!=ir::Terminator::Kind::jump && block.end.kind!=ir::Terminator::Kind::branch){safe=false;break;}
            if(block.end.kind==ir::Terminator::Kind::branch &&
               types[fn][bn][block.end.value].kind==ir::Type::managed_ref){safe=false;break;}
            for(unsigned id=0;id<block.instructions.size();++id) {
                const auto& i=block.instructions[id];
                if(i.receiver || !i.arguments.empty()) {safe=false;break;}
                using O=ir::Op;
                if(i.op==O::argument && i.a==*parameter)borrowed_values[id]=true;
                if(i.op==O::load && aliases.contains(i.a))borrowed_values[id]=true;
                if(i.op==O::load && !borrowed_values[id])unborrowed_loads.insert(i.a);
                if(i.op==O::store) {
                    if(!borrowed_values[i.b])unborrowed_stores.insert(i.a);
                    if(borrowed_values[i.b]) {
                        if(f.locals[i.a]!=f.parameters[*parameter])safe=false;
                        aliases.insert(i.a);
                    } else if(aliases.contains(i.a))safe=false;
                }
                switch(i.op) {
                case O::argument: case O::constant: case O::load: case O::store:
                case O::gc_read: case O::gc_poll: case O::gc_collect:
                case O::add: case O::subtract: case O::multiply: case O::divide: case O::remainder:
                case O::less: case O::less_equal: case O::greater: case O::greater_equal:
                case O::equal: case O::not_equal: case O::logical_not: case O::negate: case O::convert:
                    break;
                case O::gc_write:
                    if(!(numeric::number(types[fn][bn][i.b]) || types[fn][bn][i.b]==ir::Type::boolean))safe=false;
                    break;
                default:safe=false;break;
                }
                // Identity/null/type inspection of the borrowed pointer is outside
                // this initial helper proof, even when a summary reports no escape.
                auto pointer=[&](unsigned value){return value<id && types[fn][bn][value].kind==ir::Type::managed_ref;};
                if(i.op!=O::gc_read && i.op!=O::gc_write && i.op!=O::store && i.op!=O::load &&
                   i.op!=O::argument && i.op!=O::constant && i.op!=O::gc_poll && i.op!=O::gc_collect &&
                   (pointer(i.a) || pointer(i.b)))safe=false;
            }
            for(unsigned id=0;id<block.instructions.size();++id) {
                const auto& i=block.instructions[id];
                if(i.op==ir::Op::store && aliases.contains(i.a) && !borrowed_values[i.b])safe=false;
                if(i.op==ir::Op::load && aliases.contains(i.a) && !borrowed_values[id])safe=false;
            }
        }
        // An alias discovered in a later block cannot retroactively bless an
        // earlier nonformal store/load. Construction-origin validation below
        // independently checks the selected clone across the original CFG.
        for(auto slot:aliases)if(unborrowed_stores.contains(slot) || unborrowed_loads.contains(slot))safe=false;
        if(safe)result.emplace(f.name,ElisionHelper{*parameter,f.module_identity,std::move(aliases)});
    }
    return result;
}
// The proof owns both heap-elision eligibility and omission of pointer homes.
BlockAllocationElision block_allocation_elision(const ir::Function &f,
                                                const std::vector<std::vector<ir::Type>> &instruction_types,
                                                const llvm::DataLayout &layout,
                                                const std::function<llvm::Type *(ir::Type)> &type,
                                                const std::map<std::string,ElisionHelper>& helpers,
                                                const std::map<std::string,ElisionFactory>& factories) {
    using O = ir::Op;
    BlockAllocationElision result{std::vector<std::set<unsigned>>(f.blocks.size()), {}, {}, {}, {}, false};
    if (f.asynchronous || f.receiver || f.initializer || f.cleanup_entry)
        return result;
    struct Use { unsigned block, index; enum Kind { field, store, call, alias, escape } kind; };
    using Position = std::pair<unsigned,unsigned>;
    std::vector<std::vector<std::vector<Use>>> uses(f.blocks.size());
    std::vector<std::vector<Position>> local_uses(f.locals.size());
    std::vector<std::vector<unsigned>> predecessors(f.blocks.size());
    bool retire_copies=!f.worker && !f.closure && f.virtual_slot.empty();
    auto nullable_alias=[](ir::Type a,ir::Type b) {
        if(a.kind!=ir::Type::managed_ref || b.kind!=ir::Type::managed_ref ||
           a.construction || b.construction || a.readonly!=b.readonly ||
           a.readonly_references!=b.readonly_references)return false;
        a.nullable=b.nullable=false;return a==b;
    };
    // Index once. Candidate walks visit their alias uses, rather than rescanning
    // every instruction in the function for each locally constructed object.
    for (unsigned bn=0;bn<f.blocks.size();++bn) {
        const auto& block=f.blocks[bn]; uses[bn].resize(block.instructions.size());
        auto use=[&](unsigned value,unsigned index,Use::Kind kind) {
            if(value<uses[bn].size() && instruction_types[bn][value].kind==ir::Type::managed_ref)
                uses[bn][value].push_back({bn,index,kind});
        };
        for(unsigned id=0;id<block.instructions.size();++id) {
            const auto& i=block.instructions[id];
            if(i.op==O::local_cleanup || i.op==O::cleanup_call || i.op==O::gc_release)
                return result;
            if(i.receiver)local_uses[i.receiver->slot].push_back({bn,id});
            const auto helper=helpers.find(i.callee);
            const bool borrowed=i.op==O::call && !i.receiver && helper!=helpers.end() && helper->second.module==f.module_identity;
            // Earlier copy retirement is bounded to synchronous typed operations
            // with known root transfer. Unknown/public/native/callback/async and
            // lifecycle operations retain the original conservative homes.
            const auto factory=factories.find(i.callee);
            const bool factory_transfer=i.op==O::call && !i.receiver && factory!=factories.end() &&
                factory->second.module==f.module_identity && factory->second.root_transfer;
            if(!(allocation_root_transfer(i.op) || borrowed || factory_transfer) || i.receiver)
                retire_copies=false;
            // Unlike construction, a field write has no new publication-time
            // child-heap check. Keep conservative homes for reference writes.
            if(i.op==O::gc_write && !(numeric::number(instruction_types[bn][i.b]) || instruction_types[bn][i.b]==ir::Type::boolean))
                retire_copies=false;
            for(unsigned arg=0;arg<i.arguments.size();++arg)
                use(i.arguments[arg],id,borrowed && arg==helper->second.parameter?Use::call:Use::escape);
            switch(i.op) {
            case O::load: local_uses[i.a].push_back({bn,id}); break;
            case O::store: local_uses[i.a].push_back({bn,id}); use(i.b,id,Use::store); break;
            case O::call:
                if(i.arguments.empty())use(i.a,id,borrowed && helper->second.parameter==0?Use::call:Use::escape);
                break;
            case O::gc_read: use(i.a,id,Use::field); break;
            case O::gc_write: use(i.a,id,Use::field); use(i.b,id,Use::escape); break;
            case O::convert:
                use(i.a,id,nullable_alias(instruction_types[bn][i.a],instruction_types[bn][id])?Use::alias:Use::escape);break;
            case O::negate: case O::logical_not: case O::bitcast:
                use(i.a,id,Use::escape); break;
            case O::constant: case O::argument: case O::stage_begin:
            case O::gc_construct: case O::gc_allocate: case O::global_cell:
            case O::gc_poll: case O::gc_collect: case O::string_literal:
            case O::function_null: case O::dict_new: break;
            default:
                // Unknown operand conventions can only reduce eligibility.
                use(i.a,id,Use::escape); use(i.b,id,Use::escape); break;
            }
        }
        const auto& end=block.end;
        if(end.kind==ir::Terminator::Kind::return_value || end.kind==ir::Terminator::Kind::branch)
            use(end.value,~0u,Use::escape);
        if(end.kind==ir::Terminator::Kind::jump || end.kind==ir::Terminator::Kind::branch)
            predecessors[end.yes].push_back(bn);
        if(end.kind==ir::Terminator::Kind::branch)predecessors[end.no].push_back(bn);
    }
    // Propagate unproven heap affinity through the SAME indexed alias uses.
    // No incoming pointer becomes trusted merely because its type is managed.
    // Heap construction validates child ownership; elision may omit that check
    // only when every initial/replacement child is already proved current-heap
    // (or null). Any construction used as a child remains heap-backed because
    // field publication is an escape in the alias proof below.
    // Heap affinity alone is insufficient to trust fields (allocation and
    // initialization may be separate). Limit immutable-field provenance to a
    // completed gc_construct, directly or through locals whose every stored
    // origin is one of those constructions (including proved local aliases).
    // Build this fact once from the existing local index, never per candidate.
    std::vector<bool> constructed_locals(f.locals.size()),completed_candidate(f.locals.size());
    std::vector<unsigned> dependencies(f.locals.size()),completed;
    std::vector<std::vector<unsigned>> completed_users(f.locals.size());
    for(unsigned slot=0;slot<f.locals.size();++slot) {
        bool stored=false,possible=slot>=f.parameters.size();
        for(auto [bn,id]:local_uses[slot]) {
            const auto& i=f.blocks[bn].instructions[id];
            if(i.receiver)possible=false;
            if(i.op!=O::store)continue;
            stored=true;const auto& value=f.blocks[bn].instructions[i.b];
            if(value.op==O::gc_construct)continue;
            if(value.op==O::load) {
                ++dependencies[slot];completed_users[value.a].push_back(slot);
            } else possible=false;
        }
        completed_candidate[slot]=stored && possible;
        if(completed_candidate[slot] && !dependencies[slot]) {
            constructed_locals[slot]=true;completed.push_back(slot);
        }
    }
    // Least fixed point rooted only in completed constructions. An incoming
    // parameter, factory, uncertain merge or self-supporting alias cycle can
    // never certify its own fields. Visit each indexed store dependency once.
    for(unsigned cursor=0;cursor<completed.size();++cursor)
        for(auto slot:completed_users[completed[cursor]])
            if(!--dependencies[slot] && completed_candidate[slot]) {
                constructed_locals[slot]=true;completed.push_back(slot);
            }
    std::vector<std::vector<bool>> unproven(f.blocks.size());
    std::vector<bool> unproven_locals(f.locals.size());
    std::vector<Position> uncertain;
    for(unsigned bn=0;bn<f.blocks.size();++bn) {
        unproven[bn].resize(f.blocks[bn].instructions.size());
        for(unsigned id=0;id<f.blocks[bn].instructions.size();++id) {
            const auto& i=f.blocks[bn].instructions[id];
            if(instruction_types[bn][id].kind!=ir::Type::managed_ref)continue;
            const auto factory=factories.find(i.callee);
            // A successful construction validates immutable child bindings.
            // The owner's current-heap proof is propagated below; managed type
            // alone (including an incoming readonly owner) grants no affinity.
            bool immutable_child=false;
            if(i.op==O::gc_read && instruction_types[bn][i.a].kind==ir::Type::managed_ref &&
               instruction_types[bn][i.a].definition->fields.at(i.immediate).readonly) {
                const auto& owner=f.blocks[bn].instructions[i.a];
                immutable_child=owner.op==O::gc_construct ||
                    (owner.op==O::load && constructed_locals[owner.a]);
            }
            const bool heap=immutable_child || i.op==O::load || i.op==O::gc_construct || i.op==O::gc_allocate || i.op==O::global_cell ||
                (i.op==O::constant && i.immediate==0) ||
                (i.op==O::call && !i.receiver && factory!=factories.end() && factory->second.module==f.module_identity);
            if(!heap){unproven[bn][id]=true;uncertain.push_back({bn,id});}
        }
    }
    for(std::size_t next=0;next<uncertain.size();++next) {
        auto [block,id]=uncertain[next];
        for(const auto& use:uses[block][id]) {
            // Only field/store uses propagate heap affinity through an instruction.
            // Return/branch escapes carry ~0u, not an instruction index.
            if(use.kind!=Use::field && use.kind!=Use::store)continue;
            const auto& access=f.blocks[use.block].instructions[use.index];
            if(use.kind==Use::field && access.op==O::gc_read &&
               instruction_types[use.block][use.index].kind==ir::Type::managed_ref &&
               !unproven[use.block][use.index]) {
                unproven[use.block][use.index]=true;uncertain.push_back({use.block,use.index});
            }
            if(use.kind!=Use::store)continue;
            const auto slot=f.blocks[use.block].instructions[use.index].a;
            if(unproven_locals[slot])continue;
            unproven_locals[slot]=true;
            for(auto [lb,li]:local_uses[slot])if(f.blocks[lb].instructions[li].op==O::load && !unproven[lb][li]) {
                unproven[lb][li]=true;uncertain.push_back({lb,li});
            }
        }
    }
    // Per-function scratch for the SAME must-alias proof. Small alias sets use
    // one word per block; no candidate allocates a tree node for every state.
    std::vector<std::uint64_t> must_in,must_out;
    std::vector<unsigned> must_work,alias_bits;
    std::vector<bool> must_queued;
    unsigned elided_bytes=0;
    for(unsigned bn=0;bn<f.blocks.size();++bn)for(unsigned origin=0;origin<f.blocks[bn].instructions.size();++origin) {
        const auto& construct=f.blocks[bn].instructions[origin];
        auto t=instruction_types[bn][origin];
        if(construct.op!=O::gc_construct || t.kind!=ir::Type::managed_ref || !t.definition ||
           t.construction || t.definition->native_resource || t.definition->task_result ||
           !t.definition->base.expired() || t.definition->callable || t.definition->interface_type)
            continue;
        bool supported=true,has_references=false;
        for(unsigned field=0;field<t.definition->fields.size();++field) {
            ir::Type ft=t.definition->fields[field].type;
            const bool child=ft.kind==ir::Type::managed_ref && !ft.construction && ft.definition &&
                !ft.definition->native_resource && !ft.definition->task_result && !ft.definition->callable &&
                !ft.definition->interface_type;
            if(!(numeric::integer(ft) || numeric::floating(ft) || ft==ir::Type::boolean || child))supported=false;
            has_references|=child;
            if(child && unproven[bn][construct.arguments[field]])supported=false;
        }
        const auto bytes=layout.getTypeAllocSize(type(ir::Type(t.definition))).getFixedValue();
        if(!supported || !bytes || bytes>256 || elided_bytes+bytes>16384)continue;
        std::set<Position> values{{bn,origin}};
        std::set<unsigned> locals;
        std::set<Position> calls;
        std::vector<Position> pending{{bn,origin}};
        bool safe=true,block_lifetime=true,nullable_flow=false;
        std::vector<Position> lifetime_uses;
        unsigned final_use=origin;
        auto lifetime_use=[&](unsigned block,unsigned id) {
            lifetime_uses.push_back({block,id});
            if(block!=bn || id<origin)block_lifetime=false;
            else final_use=std::max(final_use,id);
        };
        for(std::size_t next=0;next<pending.size() && safe;++next) {
            auto [block,id]=pending[next];
            lifetime_use(block,id);
            for(const auto& use:uses[block][id]) {
                lifetime_use(use.block,use.index);
                if(use.kind==Use::escape){safe=false;break;}
                if(use.kind==Use::alias) {
                    nullable_flow=true;
                    if(values.insert({use.block,use.index}).second)pending.push_back({use.block,use.index});
                    continue;
                }
                if(use.kind==Use::field) {
                    const auto& access=f.blocks[use.block].instructions[use.index];
                    if(access.op==O::gc_write && instruction_types[use.block][access.b].kind==ir::Type::managed_ref &&
                       unproven[use.block][access.b]){safe=false;break;}
                }
                if(use.kind==Use::call){calls.insert({use.block,use.index});continue;}
                if(use.kind!=Use::store)continue;
                const auto slot=f.blocks[use.block].instructions[use.index].a;
                if(f.locals[slot]!=t) {
                    if(!nullable_alias(f.locals[slot],t)){safe=false;break;}
                    nullable_flow=true;
                }
                if(!locals.insert(slot).second)continue;
                for(auto [lb,li]:local_uses[slot]) {
                    const auto& i=f.blocks[lb].instructions[li];
                    if(i.receiver){safe=false;break;}
                    if(i.op==O::load && values.insert({lb,li}).second)pending.push_back({lb,li});
                }
            }
        }
        if(!safe)continue;
        // One origin per local. A rebound local could still expose an older
        // loop instance or another object's identity; keep that case on heap.
        std::vector<std::vector<Position>> events(f.blocks.size());
        for(auto slot:locals)for(auto p:local_uses[slot]) {
            const auto& i=f.blocks[p.first].instructions[p.second];
            lifetime_use(p.first,p.second);
            if(i.op==O::store && !values.contains({p.first,i.b}))safe=false;
            events[p.first].push_back(p);
        }
        if(!safe)continue;
        std::optional<Position> joined_retirement;
        if(has_references && !block_lifetime && !f.worker && !f.closure && f.virtual_slot.empty()) {
            const auto& fork=f.blocks[bn].end;
            if(fork.kind==ir::Terminator::Kind::branch && fork.yes!=fork.no && fork.yes!=bn && fork.no!=bn) {
                std::set<unsigned> region{bn,fork.yes,fork.no};
                // A direct arm, or ONE nested diamond with a direct exit.
                // Vertices are distinct and internal edges acyclic. Existing
                // must-alias validation still rejects unsafe incoming paths.
                auto arm_exit=[&](unsigned arm)->std::optional<unsigned> {
                    const auto& end=f.blocks[arm].end;
                    if(end.kind==ir::Terminator::Kind::jump)return end.yes;
                    if(end.kind!=ir::Terminator::Kind::branch || end.yes==end.no ||
                       region.contains(end.yes) || region.contains(end.no))return {};
                    const auto& left=f.blocks[end.yes].end;const auto& right=f.blocks[end.no].end;
                    if(left.kind!=ir::Terminator::Kind::jump || right.kind!=ir::Terminator::Kind::jump ||
                       left.yes!=right.yes || left.yes==end.yes || left.yes==end.no || region.contains(left.yes))return {};
                    const auto& tail=f.blocks[left.yes].end;
                    if(tail.kind!=ir::Terminator::Kind::jump)return {};
                    region.insert(end.yes);region.insert(end.no);region.insert(left.yes);
                    return tail.yes;
                };
                const auto left=arm_exit(fork.yes),right=arm_exit(fork.no);
                if(left && right && *left==*right && !region.contains(*left) && region.size()<=6) {
                    auto join=*left;region.insert(join);bool bounded=true;
                    // One empty common join may forward directly to an existing
                    // nonempty retirement block. Keep BOTH entry checkpoints;
                    // clear after its first instruction, never across an edge.
                    if(f.blocks[join].instructions.empty()) {
                        const auto& end=f.blocks[join].end;
                        if(end.kind!=ir::Terminator::Kind::jump || region.contains(end.yes) ||
                           f.blocks[end.yes].instructions.empty())bounded=false;
                        else {join=end.yes;region.insert(join);}
                    }
                    unsigned last=0;
                    for(auto [block,id]:lifetime_uses) {
                        if(block==bn){if(id<origin)bounded=false;}
                        else if(!region.contains(block))bounded=false;
                        if(block==join)last=std::max(last,id);
                    }
                    if(bounded)joined_retirement=Position{join,last};
                }
            }
        }
        // Nullable/cast flow stays within a bounded reconverging region (at most eight blocks).
        // Existing epoch/must-alias validation below still rejects old instances,
        // rebinding, pre-birth loads and every escaping owner use.
        if(nullable_flow && (!has_references || !joined_retirement))continue;
        if(has_references) {
            const auto& instructions=f.blocks[bn].instructions;
            const bool owning_local=origin+1<instructions.size() && instructions[origin+1].op==O::store &&
                instructions[origin+1].b==origin;
            // Existing CFG owners retain their accepted lifetime. New direct
            // temporaries must have no local publication and an exact bounded
            // lifetime in their construction block; SSA cannot cross IR edges.
            if(!owning_local && (!locals.empty() || !block_lifetime) && !joined_retirement)continue;
        }
        events[bn].push_back({bn,origin});
        for(auto& block:events) {std::sort(block.begin(),block.end());block.erase(std::unique(block.begin(),block.end()),block.end());}
        // Must-dataflow over the indexed alias events. Reset at every birth;
        // loads before a rebinding store fail, including loop-carried aliases.
        // With no local aliases the lattice is empty: birth is its only event,
        // so the accepted proof is vacuous. Larger sets keep the original solver.
        if(!locals.empty() && locals.size()<=64) {
            alias_bits.resize(f.locals.size());unsigned bit=0;
            for(auto slot:locals)alias_bits[slot]=bit++;
            const auto all=locals.size()==64?~std::uint64_t{0}:(std::uint64_t{1}<<locals.size())-1;
            must_in.resize(f.blocks.size());must_out.assign(f.blocks.size(),all);
            must_queued.assign(f.blocks.size(),true);must_work.clear();must_work.reserve(f.blocks.size());
            // Reverse insertion visits source order initially. Thereafter only
            // a changed predecessor output schedules a successor, once at a time.
            for(unsigned block=f.blocks.size();block-->0;)must_work.push_back(block);
            auto enqueue=[&](unsigned block) {
                if(!must_queued[block]){must_queued[block]=true;must_work.push_back(block);}
            };
            while(!must_work.empty()) {
                const auto block=must_work.back();must_work.pop_back();must_queued[block]=false;
                auto state=(block==0 || predecessors[block].empty())?std::uint64_t{0}:all;
                for(auto pred:predecessors[block])state&=must_out[pred];
                must_in[block]=state;
                for(auto [eb,id]:events[block]) {
                    if(eb==bn && id==origin)state=0;
                    else if(f.blocks[eb].instructions[id].op==O::store)
                        state|=std::uint64_t{1}<<alias_bits[f.blocks[eb].instructions[id].a];
                }
                if(must_out[block]!=state) {
                    must_out[block]=state;const auto& end=f.blocks[block].end;
                    if(end.kind==ir::Terminator::Kind::jump || end.kind==ir::Terminator::Kind::branch)enqueue(end.yes);
                    if(end.kind==ir::Terminator::Kind::branch)enqueue(end.no);
                }
            }
            for(unsigned block=0;block<f.blocks.size() && safe;++block) {
                auto state=must_in[block];
                for(auto [eb,id]:events[block]) {
                    const auto& i=f.blocks[eb].instructions[id];
                    if(eb==bn && id==origin)state=0;
                    else if(i.op==O::store)state|=std::uint64_t{1}<<alias_bits[i.a];
                    else if(i.op==O::load && !(state&(std::uint64_t{1}<<alias_bits[i.a]))){safe=false;break;}
                }
            }
        } else if(!locals.empty()) {
            std::vector<std::set<unsigned>> incoming(f.blocks.size(),locals),outgoing(f.blocks.size(),locals);
            bool changed;
            do {
                changed=false;
                for(unsigned block=0;block<f.blocks.size();++block) {
                    auto state=(block==0 || predecessors[block].empty())?std::set<unsigned>{}:locals;
                    for(auto pred:predecessors[block])std::erase_if(state,[&](unsigned slot){return !outgoing[pred].contains(slot);});
                    if(incoming[block]!=state){incoming[block]=state;changed=true;}
                    for(auto [eb,id]:events[block]) {
                        if(eb==bn && id==origin)state.clear();
                        else if(f.blocks[eb].instructions[id].op==O::store)state.insert(f.blocks[eb].instructions[id].a);
                    }
                    if(outgoing[block]!=state){outgoing[block]=std::move(state);changed=true;}
                }
            }while(changed);
            for(unsigned block=0;block<f.blocks.size() && safe;++block) {
                auto state=incoming[block];
                for(auto [eb,id]:events[block]) {
                    const auto& i=f.blocks[eb].instructions[id];
                    if(eb==bn && id==origin)state.clear();
                    else if(i.op==O::store)state.insert(i.a);
                    else if(i.op==O::load && !state.contains(i.a)){safe=false;break;}
                }
            }
        }
        if(safe) {
            for(auto [block,id]:values)result.values[block].insert(id);
            result.locals.insert(locals.begin(),locals.end());
            result.calls.insert(calls.begin(),calls.end());
            if(has_references && block_lifetime)
                result.retirements[{bn,final_use}].payloads.push_back({bn,origin});
            else if(has_references && joined_retirement)
                result.retirements[*joined_retirement].payloads.push_back({bn,origin});
            elided_bytes+=bytes;
        }
    }
    if(retire_copies) {
        auto ordinary=[](ir::Type t) {
            return t.kind==ir::Type::managed_ref && !t.construction && t.definition &&
                !t.definition->native_resource && !t.definition->task_result && !t.definition->callable &&
                !t.definition->interface_type && t.definition->base.expired();
        };
        // SSA values are block-local. At final use, every surviving copy has
        // already reached its own existing root home (including a child read).
        // Returned values retain their home through frame exit.
        for(unsigned bn=0;bn<f.blocks.size();++bn)for(unsigned id=0;id<uses[bn].size();++id) {
            if(!ordinary(instruction_types[bn][id]) || result.values[bn].contains(id))continue;
            unsigned last=id;
            for(const auto& use:uses[bn][id])last=std::max(last,use.index);
            if(last!=~0u)result.retirements[{bn,last}].values.push_back(id);
        }
        // Only block-local homes with every store before every load can retire.
        // Thus each loop instance overwrites the home before any new load; a
        // loop-carried read-before-store, receiver place or CFG alias falls back.
        for(unsigned slot=f.parameters.size();slot<f.locals.size();++slot) {
            if(!ordinary(f.locals[slot]) || result.locals.contains(slot) || local_uses[slot].empty())continue;
            const auto bn=local_uses[slot].front().first;
            bool safe=true,stored=false;unsigned last_store=0,first_load=~0u,last=0;
            for(auto [block,id]:local_uses[slot]) {
                const auto& i=f.blocks[block].instructions[id];
                if(block!=bn || i.receiver){safe=false;break;}
                if(i.op==O::store){stored=true;last_store=std::max(last_store,id);}
                else if(i.op==O::load)first_load=std::min(first_load,id);
                else {safe=false;break;}
                last=std::max(last,id);
            }
            if(safe && stored && last_store<first_load)
                result.retirements[{bn,last}].locals.push_back(slot);
        }
    }
    // Retain the G39 payload/copy lifetime safety floor. Optional copies
    // elsewhere need a reachable collection driver before function exit.
    // Polls service external requests; they do not initiate collection.
    const bool payload_lifetime=std::any_of(result.retirements.begin(),result.retirements.end(),
        [](const auto& event){return !event.second.payloads.empty();});
    auto collection_driver=[&](unsigned bn,unsigned id) {
        const auto& i=f.blocks[bn].instructions[id];
        const auto factory=factories.find(i.callee);
        return i.op==O::gc_collect || i.op==O::gc_allocate ||
            (i.op==O::gc_construct && !result.values[bn].contains(id)) ||
            (i.op==O::call && factory!=factories.end() &&
             factory->second.module==f.module_identity && factory->second.root_transfer);
    };
    // One reverse reachability walk over the existing CFG. In particular,
    // a later loop instance's allocation can observe this instance's roots.
    // Lack of a same-block driver alone does not make its clears redundant.
    std::vector<bool> reaches_driver(f.blocks.size()),successor_driver(f.blocks.size());
    std::vector<unsigned> pending;
    for(unsigned bn=0;bn<f.blocks.size();++bn)
        for(unsigned id=0;id<f.blocks[bn].instructions.size();++id)
            if(collection_driver(bn,id)) {reaches_driver[bn]=true;pending.push_back(bn);break;}
    for(unsigned cursor=0;cursor<pending.size();++cursor)
        for(auto pred:predecessors[pending[cursor]])if(!reaches_driver[pred]) {
            reaches_driver[pred]=true;pending.push_back(pred);
        }
    for(unsigned bn=0;bn<f.blocks.size();++bn)if(reaches_driver[bn])
        for(auto pred:predecessors[bn])successor_driver[pred]=true;
    for(unsigned bn=0;bn<f.blocks.size();++bn) {
        const auto& instructions=f.blocks[bn].instructions;
        bool driver=successor_driver[bn];
        for(unsigned id=instructions.size();id-->0;) {
            if(auto event=result.retirements.find({bn,id});event!=result.retirements.end() &&
               !payload_lifetime && !driver) {
                event->second.values.clear();event->second.locals.clear();
            }
            driver |= collection_driver(bn,id);
        }
        // Batch only across intervals with no observable safepoint. Calls and
        // polls remain barriers, including polls which can service another
        // mutator's collection. Never move across an implicit block-entry poll,
        // a root-home overwrite on a CFG edge, or a mandatory payload clear.
        std::vector<unsigned> values,locals;
        auto flush=[&](unsigned id) {
            if(values.empty() && locals.empty())return;
            auto& target=result.retirements[{bn,id}];
            target.values.insert(target.values.end(),values.begin(),values.end());
            target.locals.insert(target.locals.end(),locals.begin(),locals.end());
            values.clear();locals.clear();
        };
        for(unsigned id=0;id<instructions.size();++id) {
            const auto op=instructions[id].op;
            const bool barrier=op==O::call || op==O::gc_poll || op==O::gc_collect ||
                op==O::gc_allocate || (op==O::gc_construct && !result.values[bn].contains(id));
            if(barrier && id)flush(id-1);
            if(auto event=result.retirements.find({bn,id});event!=result.retirements.end()) {
                values.insert(values.end(),event->second.values.begin(),event->second.values.end());
                locals.insert(locals.end(),event->second.locals.begin(),event->second.locals.end());
                event->second.values.clear();event->second.locals.clear();
                if(!event->second.payloads.empty())flush(id);
            }
        }
        if(!instructions.empty())flush(instructions.size()-1);
    }
    result.precise_copies=retire_copies;
    result.predecessors=std::move(predecessors);
    std::erase_if(result.retirements,[](const auto& event) {
        const auto& r=event.second;return r.payloads.empty() && r.values.empty() && r.locals.empty();
    });
    return result;
}
}
