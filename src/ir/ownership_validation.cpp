#include "validation.hpp"
#include "void/c_primitive.hpp"
#include "void/numeric.hpp"
#include <algorithm>
#include <stdexcept>

namespace vd::ir {
// Independent must-dataflow proof for private construction Places. No source
// type checker or runtime pending-object state is hidden behind this validator.
void validate_construction(const Function& f,const Module& module,const std::vector<std::vector<Type>>& types) {
    auto fail=[&](const std::string& message) { throw std::invalid_argument("VOID IR: "+message+" in "+f.name); };
    std::map<std::string,const Function*> initializers,functions;
    for (const auto& target:module.functions) {
        functions.emplace(target.name,&target);
        if (target.initializer && target.parameters[0].construction) initializers.emplace(Type::key(target.parameters[0]),&target);
    }
    std::set<unsigned> owned;
    for (const auto& block:f.blocks) for (const auto& i:block.instructions) {
        if (i.op==Op::stage_begin) {
            if (!owned.insert(i.a).second || !initializers.contains(Type::key(i.type))) fail("invalid or duplicate staging owner");
        }
        if (i.op==Op::gc_construct && initializers.contains(Type::key(i.type)) && !i.receiver) fail("custom construction omitted initialization");
        if (i.op==Op::gc_allocate && initializers.contains(Type::key(i.type))) fail("legacy allocation bypasses custom initialization");
    }
    if (!(f.initializer && fallible_initializer_result(f.result)) && f.construction_origins.empty() && owned.empty() && std::none_of(f.parameters.begin(),f.parameters.end(),[](auto t){return t.construction;}) && std::none_of(f.locals.begin(),f.locals.end(),[](auto t){return t.construction;})) return;
    for (unsigned parameter=0;parameter<f.parameters.size();++parameter) if (f.parameters[parameter].construction) {
        if (parameter>=f.locals.size() || f.locals[parameter]!=f.parameters[parameter] || !f.construction_origins.contains(parameter) || f.construction_origins.at(parameter)!=parameter || owned.contains(parameter)) fail("invalid construction formal storage");
    }
    for (auto owner:owned) if (!f.construction_origins.contains(owner) || f.construction_origins.at(owner)!=owner) fail("staging owner has a different origin");
    for (unsigned slot=0;slot<f.locals.size();++slot)
        if (f.locals[slot].construction!=f.construction_origins.contains(slot)) fail("missing construction origin metadata");
    for (auto [slot,origin]:f.construction_origins) {
        if (slot>=f.locals.size() || origin>=f.locals.size() || !f.locals[origin].construction || !f.construction_origins.contains(origin) || f.construction_origins.at(origin)!=origin || !([](Type a,Type b){a.construction=b.construction=false;return subtype(b,a);})(f.locals[slot],f.locals[origin])) fail("invalid construction alias origin");
        if (!owned.contains(origin) && (origin>=f.parameters.size() || !f.parameters[origin].construction)) fail("unowned construction storage");
    }
    enum Phase { inactive=1, preparing=2, initialized=4, committed=8, awaiting=16, rejected=32 };
    struct Region { unsigned phase=inactive; std::set<unsigned> ready; unsigned base_phase=2; bool operator==(const Region&) const=default; };
    struct State { std::map<unsigned,Region> regions; std::set<unsigned> bound, checked_returns; bool operator==(const State&) const=default; };
    State entry;
    auto fields=[&](unsigned root)->const auto& { return f.locals.at(root).definition->fields; };
    for (auto [slot,origin]:f.construction_origins) {
        auto& region=entry.regions[origin];
        if (owned.contains(origin)) continue;
        region.phase=f.initializer ? preparing : initialized;
        region.base_phase=(!f.initializer || f.parameters[0].definition->base.expired()) ? 2u : 1u;
        for (unsigned n=0;n<fields(origin).size();++n) {
            if (!f.initializer) region.ready.insert(n);
        }
    }
    if (f.receiver && f.parameters[0].construction) entry.bound.insert(0);
    auto intersection=[](auto a,const auto& b) { std::erase_if(a,[&](const auto& value){return !b.contains(value);}); return a; };
    auto join=[&](State a,const State& b) {
        a.bound=intersection(a.bound,b.bound);
        a.checked_returns=intersection(a.checked_returns,b.checked_returns);
        for (auto& [root,region]:a.regions) { region.phase|=b.regions.at(root).phase; region.base_phase|=b.regions.at(root).base_phase; region.ready=intersection(region.ready,b.regions.at(root).ready); }
        return a;
    };
    std::vector<std::optional<State>> inputs(f.blocks.size()); inputs[0]=entry;
    std::vector<unsigned> pending{0};
    for (std::size_t next=0;next<pending.size();++next) {
        auto block=pending[next]; auto state=*inputs[block];
        std::vector<std::optional<unsigned>> origins;
        std::vector<bool> checked_values;
        std::vector<std::optional<std::pair<unsigned,bool>>> checks;

        auto active=[&](unsigned root) {
            if (state.regions.at(root).phase & (inactive|committed|rejected)) fail("construction view used outside its active scope");
        };
        auto require_all=[&](unsigned root) {
            active(root);
            if (state.regions.at(root).base_phase!=2 || state.regions.at(root).ready.size()!=fields(root).size()) fail("uninitialized construction field at call/exit/commit");
        };
        auto value_origin=[&](unsigned value) {
            if (value>=origins.size() || !origins[value]) fail("missing construction operand origin");
            auto root=*origins[value]; active(root); return root;
        };
        auto place_origin=[&](const Place& place) {
            auto found=f.construction_origins.find(place.slot);
            if (found==f.construction_origins.end() || !state.bound.contains(place.slot)) fail("unbound construction place");
            active(found->second); return found->second;
        };
        for (unsigned index=0;index<f.blocks[block].instructions.size();++index) {
            const auto& i=f.blocks[block].instructions[index]; std::optional<unsigned> origin;
            bool checked=false;std::optional<std::pair<unsigned,bool>> decision;
            if(i.op==Op::load)checked=state.checked_returns.contains(i.a);
            if(i.op==Op::store) {
                if(checked_values.at(i.b))state.checked_returns.insert(i.a);else state.checked_returns.erase(i.a);
            }
            if(i.op==Op::initializer_return) {
                const auto& returned=f.blocks[block].instructions.at(i.a);
                const bool failure=returned.op==Op::sum && returned.immediate==1;
                if(f.parameters[0].construction && !failure)require_all(0);
                checked=true;
            }

            if (i.op==Op::stage_begin) {
                if (f.construction_origins.at(i.a)!=i.a || state.regions.at(i.a).phase & (preparing|initialized|awaiting|rejected)) fail("staging restarted before commit");
                state.regions.at(i.a)={preparing,{},f.locals[i.a].definition->base.expired() ? 2u : 1u};
                std::erase_if(state.bound,[&](auto slot){return f.construction_origins.at(slot)==i.a;}); origin=i.a;
            } else if (i.op==Op::base_init) {
                auto root=place_origin(*i.receiver); auto& region=state.regions.at(root);
                if(root!=0 || owned.contains(root) || region.base_phase!=1 || region.phase!=preparing || !region.ready.empty()) fail("base initialization out of order or repeated");
                auto base=f.parameters[0].definition->base.lock();
                if(!i.callee.empty() && fallible_initializer_result(functions.at(i.callee)->result))region.base_phase=3;
                else {for(unsigned n=0;n<base->fields.size();++n) region.ready.insert(n);region.base_phase=2;}

            } else if(i.op==Op::stage_check) {
                auto root=place_origin(*i.receiver);
                const auto& invocation=f.blocks[block].instructions.at(i.a);
                const bool base=invocation.op==Op::base_init;
                if((invocation.op!=Op::call && !base) || !invocation.receiver || (invocation.receiver->slot!=i.receiver->slot || invocation.receiver->fields!=i.receiver->fields) ||
                   (!base && (!invocation.initialization || state.regions.at(root).phase!=awaiting)) ||
                   (base && state.regions.at(root).base_phase!=3))fail("construction result check lacks its pending initializer call");
                decision=std::pair{root,base};
            } else if(i.op==Op::stage_abort) {
                const auto root=i.receiver->slot;
                if(!owned.contains(root) || !state.bound.contains(root) || state.regions.at(root).phase!=rejected)
                    fail("construction disposal lacks proven initialization failure");
                state.regions.at(root)={inactive,{},2};
                std::erase_if(state.bound,[&](auto slot){return f.construction_origins.at(slot)==root;});
            } else if (i.op==Op::stage_prefix) {
                origin=value_origin(i.a);
                if(state.regions.at(*origin).base_phase!=2) fail("base projection before base initialization");
            } else if (i.op==Op::argument && types[block][index].construction) {
                origin=i.a; active(*origin);
            } else if (i.op==Op::load && types[block][index].construction) {
                if (!state.bound.contains(i.a)) fail("unbound construction view load");
                origin=f.construction_origins.at(i.a); active(*origin);
            } else if (i.op==Op::store && f.locals[i.a].construction) {
                auto root=value_origin(i.b);
                if (root!=f.construction_origins.at(i.a)) fail("construction alias rebound to another scope");
                state.bound.insert(i.a);
            } else if ((i.op==Op::gc_read || i.op==Op::gc_write) && types[block][i.a].construction) {
                auto root=value_origin(i.a); auto field=static_cast<unsigned>(i.immediate);
                if(state.regions.at(root).base_phase!=2) fail("derived storage accessed before base initialization");
                if(state.regions.at(root).phase & (awaiting|rejected))fail("construction field accessed before successful initialization");
                if (i.op==Op::gc_read && !state.regions.at(root).ready.contains(field)) fail("uninitialized construction field read");
                if (i.op==Op::gc_write) {
                    if (owned.contains(root) && state.regions.at(root).phase!=preparing) fail("construction fields changed after initializer completion");
                    state.regions.at(root).ready.insert(field);
                }
            } else if (i.op==Op::call || i.op==Op::virtual_call) {
                auto found=functions.find(i.callee);
                if (found!=functions.end()) {
                    const auto& target=*found->second;
                    std::vector<std::pair<unsigned,Type>> roots;
                    if (i.receiver && f.locals.at(i.receiver->slot).construction) {
                        auto root=place_origin(*i.receiver);
                        if (i.receiver->fields.empty()) roots.emplace_back(root,f.locals.at(i.receiver->slot));
                        else {
                            if (!state.regions.at(root).ready.contains(i.receiver->fields.front())) fail("uninitialized projected receiver");
                        }
                    }
                    auto arguments=i.arguments;
                    if (!target.receiver && arguments.empty() && target.parameters.size()==1) arguments.push_back(i.a);
                    for (auto arg:arguments) if (types[block][arg].construction) roots.emplace_back(value_origin(arg),types[block][arg]);
                    if(i.op==Op::virtual_call && !roots.empty()) fail("virtual dispatch on unfinished construction receiver");
                    if (i.initialization && target.parameters[0].construction) {
                        if (roots.size()!=1 || !i.receiver || !i.receiver->fields.empty()) fail("invalid initializer construction operands");
                        auto root=roots[0].first;
                        if (!owned.contains(root) || state.regions.at(root).phase!=preparing || Type::key(f.locals[root])!=Type::key(target.parameters[0])) fail("initializer called twice or on borrowed storage");
                        if(fallible_initializer_result(target.result))state.regions.at(root).phase=awaiting;
                        else {
                            for (unsigned field=0;field<fields(root).size();++field)state.regions.at(root).ready.insert(field);
                            state.regions.at(root).phase=initialized;state.regions.at(root).base_phase=2;
                        }
                    } else for (auto [root,view]:roots) {
                        active(root);
                        if(owned.contains(root) && state.regions.at(root).phase!=initialized) fail("construction helper before initializer completion");
                        if(state.regions.at(root).base_phase!=2) fail("helper called before base initialization");
                        for(unsigned field=0;field<view.definition->fields.size();++field) if(!state.regions.at(root).ready.contains(field)) fail("uninitialized projected construction field at helper call");
                    }
                }
            } else if (i.op==Op::gc_construct && i.receiver) {
                auto root=place_origin(*i.receiver); require_all(root);
                if (!owned.contains(root) || state.regions.at(root).phase!=initialized) fail("construction commit before initializer completion");
                if(Type::key(f.locals.at(root))!=Type::key(i.type)) fail("construction prefix cannot publish a separate identity");
                for (unsigned field=0;field<i.arguments.size();++field) {
                    const auto& read=f.blocks[block].instructions.at(i.arguments[field]);
                    if (read.op!=Op::gc_read || read.immediate!=field || value_origin(read.a)!=root) fail("commit fields do not come from the initialized storage");
                }
                state.regions.at(root).phase=committed;
            }
            origins.push_back(origin);checked_values.push_back(checked);checks.push_back(decision);
        }
        const auto& end=f.blocks[block].end;
        if (end.kind==Terminator::Kind::return_value || end.kind==Terminator::Kind::return_void) {
            if(f.initializer && fallible_initializer_result(f.result)) {
                if(end.kind!=Terminator::Kind::return_value || !checked_values.at(end.value))fail("fallible initializer exit lacks checked result/readiness");
            } else if (f.initializer && f.parameters[0].construction) require_all(0);
            for (auto root:owned) if (state.regions.at(root).phase & (preparing|initialized|awaiting|rejected)) fail("uncommitted construction on successful exit");
        }
        if(end.kind==Terminator::Kind::yield)
            for(auto root:owned)if(state.regions.at(root).phase & (preparing|initialized|awaiting|rejected))
                fail("unfinished construction at generator yield");
        auto propagate=[&](unsigned target) {
            auto merged=inputs[target] ? join(*inputs[target],state) : state;
            if (!inputs[target] || merged!=*inputs[target]) { inputs[target]=std::move(merged); pending.push_back(target); }
        };
        if (end.kind==Terminator::Kind::jump || end.kind==Terminator::Kind::yield) propagate(end.yes);
        if (end.kind==Terminator::Kind::branch) {
            if(checks.at(end.value)) {
                auto [root,base]=*checks.at(end.value);auto before=state;
                if(base) {
                    auto& region=state.regions.at(root);region.base_phase=2;
                    for(unsigned n=0;n<f.parameters[0].definition->base.lock()->fields.size();++n)region.ready.insert(n);
                } else {
                    state.regions.at(root).phase=initialized;state.regions.at(root).base_phase=2;
                    for(unsigned n=0;n<fields(root).size();++n)state.regions.at(root).ready.insert(n);
                }
                propagate(end.yes);state=before;
                if(base)state.regions.at(root).base_phase=4;else state.regions.at(root).phase=rejected;
                propagate(end.no);
            } else {propagate(end.yes);propagate(end.no);}
        }
    }
}

// Bounded scalar proof inside the shared IR, independent of source annotations.
// Instruction values are block-local. Only initialized local homes cross edges.
void validate_fixed_ownership(const Function& f,const Module& m,const std::vector<std::vector<Type>>& types) {
    auto fail=[&](const std::string& why) {throw std::invalid_argument("VOID IR: fixed ownership: "+why+" in "+f.name);};
    auto carries=[](const Function& fn) {
        if(array_type(fn.result)) return true;
        return std::any_of(fn.parameters.begin(),fn.parameters.end(),[](auto t){return array_type(t);});
    };
    // A legacy value operation must not enter the private address-home ABI.
    for(const auto& b:f.blocks) for(const auto& i:b.instructions) if(i.op==Op::call)
        for(const auto& target:m.functions) if(target.name==i.callee && carries(target) && target.fixed_ownership!=f.fixed_ownership)
            fail("mixed legacy/private call");
    if(!f.fixed_ownership) return;
    if(!m.coordinated || f.receiver || f.initializer || !f.virtual_slot.empty()) fail("unsupported runtime/receiver contract");
    auto supported=[&](Type t) {
        if(t.kind==Type::aggregate || (array_type(t) && !scalar_fixed_array(t)) || reference_value(t) || t.kind==Type::prepared_receiver || t.kind==Type::string)
            fail("non-scalar carrier awaits a later proof");
    };
    for(auto t:f.parameters) supported(t);
    for(auto t:f.locals) supported(t);
    supported(f.result);
    std::set<unsigned> all;
    for(unsigned n=0;n<f.locals.size();++n) if(array_type(f.locals[n])) all.insert(n);
    std::vector<std::vector<unsigned>> pred(f.blocks.size());
    std::set<unsigned> reachable;
    std::vector<unsigned> pending{0};
    while(!pending.empty()) {
        auto id=pending.back();pending.pop_back();if(!reachable.insert(id).second) continue;
        auto edge=[&](unsigned to) {if(to==0) fail("entry reexecution could repeat argument transfer");pred[to].push_back(id);pending.push_back(to);};
        auto end=f.blocks[id].end;
        if(end.kind==Terminator::Kind::jump || end.kind==Terminator::Kind::yield) edge(end.yes);
        if(end.kind==Terminator::Kind::branch) {edge(end.yes);edge(end.no);}
    }
    std::vector<std::set<unsigned>> in(f.blocks.size(),all),out(f.blocks.size(),all);
    bool changed;
    do {
        changed=false;
        for(auto id:reachable) {
            auto state=id==0 ? std::set<unsigned>{} : all;
            for(auto p:pred[id]) std::erase_if(state,[&](auto s){return !out[p].contains(s);});
            if(in[id]!=state) {in[id]=state;changed=true;}
            for(auto& i:f.blocks[id].instructions) if(i.op==Op::store && all.contains(i.a)) state.insert(i.a);
            if(out[id]!=state) {out[id]=state;changed=true;}
        }
    } while(changed);
    std::set<unsigned> arguments;
    for(unsigned block=0;block<f.blocks.size();++block) {
        const auto& b=f.blocks[block];
        auto initialized=in[block];
        enum class Value { ordinary, borrowed, fresh, consumed, stale };
        std::vector<Value> values;
        std::set<unsigned> selections;
        auto use=[&](unsigned id,bool consume=false) {
            if(!array_type(types[block].at(id))) return;
            if(values[id]==Value::consumed || values[id]==Value::stale) fail("use after transfer or invalidated borrow");
            if(consume) {
                if(values[id]!=Value::fresh) fail("shallow mutable alias requires explicit snapshot");
                values[id]=Value::consumed;
            }
        };
        auto invalidate=[&] {for(auto& value:values) if(value==Value::borrowed) value=Value::stale;};
        for(unsigned n=0;n<b.instructions.size();++n) {
            const auto& i=b.instructions[n];Value value=Value::ordinary;
            supported(types[block][n]);
            switch(i.op) {
            case Op::argument:
                if(array_type(types[block][n])) {
                    if(block || !arguments.insert(i.a).second) fail("repeated argument transfer");
                    value=Value::fresh;
                }
                break;
            case Op::fixed_array: value=Value::fresh;break;
            case Op::load:
                if(all.contains(i.a)) {
                    if(!initialized.contains(i.a)) fail("uninitialized fixed home");
                    value=Value::borrowed;
                }
                break;
            case Op::fixed_snapshot: use(i.a);value=Value::fresh;break;
            case Op::array_get: use(i.a);break;
            case Op::array_set: fail("functional array_set requires a separate copy plan");break;
            case Op::store:
                if(all.contains(i.a)) {use(i.b,true);initialized.insert(i.a);invalidate();}
                break;
            case Op::call: {
                auto args=i.arguments;
                const auto target=std::find_if(m.functions.begin(),m.functions.end(),[&](const auto& t){return t.name==i.callee;});
                if(target!=m.functions.end() && args.empty() && target->parameters.size()==1) args.push_back(i.a);
                for(auto arg:args) use(arg,true);
                invalidate();
                if(array_type(types[block][n])) value=Value::fresh;
                break;
            }
            case Op::fixed_select:
                if(!initialized.contains(i.receiver->slot)) fail("uninitialized selected home");
                selections.insert(n);break;
            case Op::fixed_commit:
                if(!selections.erase(i.a)) fail("repeated selected-place commit");
                invalidate();break;
            default: break;
            }
            values.push_back(value);
        }
        if(!selections.empty()) fail("uncommitted selected place");
        if(b.end.kind==Terminator::Kind::return_value || b.end.kind==Terminator::Kind::yield)
            use(b.end.value,true);
    }
}

void validate_generator_suspensions(const Function& function) {
    using End=Terminator::Kind;
    for(unsigned slot=0;slot<function.locals.size();++slot) {
        auto type=function.locals[slot];
        if(!type.construction && type.kind!=Type::prepared_receiver)continue;
        std::vector<bool> visited(function.blocks.size());
        std::vector<unsigned> pending;
        for(const auto& block:function.blocks)if(block.end.kind==End::yield)pending.push_back(block.end.yes);
        if(function.cleanup_entry)pending.push_back(*function.cleanup_entry);
        while(!pending.empty()) {
            auto index=pending.back();
            pending.pop_back();
            if(visited[index])continue;
            visited[index]=true;
            const auto& block=function.blocks[index];
            bool initialized=false;
            for(const auto& op:block.instructions) {
                if((op.op==Op::load && op.a==slot) || (op.receiver && op.receiver->slot==slot))
                    throw std::invalid_argument("VOID IR: borrowed generator storage survives suspension");
                if((op.op==Op::store || op.op==Op::stage_begin) && op.a==slot) {
                    initialized=true;
                    break;
                }
            }
            if(initialized)continue;
            const auto& end=block.end;
            if(end.kind==End::jump || end.kind==End::yield || end.kind==End::branch)pending.push_back(end.yes);
            if(end.kind==End::branch)pending.push_back(end.no);
        }
    }
}

bool checked_array_shape(Type type, const CPrimitiveAdmission* native) {
    return contains_numeric_enum(type) || (native && native->inline_array(type));
}
bool checked_array_allocation(const Instruction& i, const CPrimitiveAdmission* native) {
    return i.op == Op::array_allocate && i.type.kind == Type::fixed_array && checked_array_shape(i.type, native);
}

std::set<unsigned> checked_array_homes(const Function& f, const CPrimitiveAdmission* native) {
    std::set<unsigned> homes;
    for(const auto& block:f.blocks)for(unsigned n=0;n<block.instructions.size();++n) {
        const auto& i=block.instructions[n];
        if(i.op==Op::array_capture && i.a<n && i.immediate>=0 &&
           static_cast<std::size_t>(i.immediate)<f.locals.size() && checked_array_allocation(block.instructions[i.a], native))
            homes.insert(static_cast<unsigned>(i.immediate));
    }
    return homes;
}

namespace {
bool checked_storage_slot(Type type, const CPrimitiveAdmission* native) {
    if (contains_numeric_enum(type)) return true;
    if (!native || native->revision() < 2) return false;
    // Check scalar inputs too. A forged aggregate construction must not be
    // able to wrap an uninitialized scalar in otherwise valid record storage.
    return numeric::integer(type) || numeric::floating(type) || type == Type::boolean ||
        type.kind == Type::aggregate || type.kind == Type::fixed_array ||
        type.kind == Type::growable_array || native->resource_type(type);
}
}

bool checked_storage_cleanup_return(const Function& f, unsigned id, const CPrimitiveAdmission* native) {
    if(!f.cleanup_entry || id<*f.cleanup_entry)return false;
    const auto& block=f.blocks[id];
    return block.end.kind==Terminator::Kind::return_value && !block.instructions.empty() &&
           block.end.value==block.instructions.size()-1 && block.instructions.back().op==Op::load &&
           checked_storage_slot(f.locals.at(block.instructions.back().a), native);
}

void validate_numeric_enum_slots(const Function& f, const CPrimitiveAdmission* native) {
    std::set<unsigned> tracked,entry;
    for(unsigned n=0;n<f.locals.size();++n)
        if(checked_storage_slot(f.locals[n], native)) tracked.insert(n);
    if(tracked.empty())return;
    if(f.receiver && tracked.contains(0))entry.insert(0);

    // Intersect definitely initialized slots at explicit joins. A failure can
    // also enter cleanup before any body write. Its terminal return load is
    // emitted only on success and still needs a proof on the normal CFG.
    auto verify=[&](unsigned start,bool unwinding) {
        std::vector<std::optional<std::set<unsigned>>> inputs(f.blocks.size());
        inputs[start]=entry;
        std::vector<unsigned> work{start};
        while(!work.empty()) {
            const auto id=work.back();work.pop_back();
            auto state=*inputs[id];
            const auto& block=f.blocks[id];
            auto require=[&](unsigned slot) {
                if(tracked.contains(slot) && !state.contains(slot))
                    throw std::invalid_argument(std::string("VOID IR: read of uninitialized ") +
                        (contains_numeric_enum(f.locals[slot]) ? "numeric enum" : "C binding carrier") + " storage in " + f.name);
            };
            for(const auto& i:block.instructions) {
                if(i.op==Op::load && !(unwinding && &i==&block.instructions.back() && checked_storage_cleanup_return(f,id,native)))require(i.a);
                if(i.receiver)require(i.receiver->slot);
                if(i.op==Op::store && tracked.contains(i.a))state.insert(i.a);
                if(i.op==Op::array_capture && tracked.contains(i.immediate))state.insert(static_cast<unsigned>(i.immediate));
            }
            auto edge=[&](unsigned target) {
                auto& known=inputs[target];
                if(!known){known=state;work.push_back(target);return;}
                const auto removed=std::erase_if(*known,[&](unsigned slot){return !state.contains(slot);});
                if(removed)work.push_back(target);
            };
            if(block.end.kind==Terminator::Kind::jump || block.end.kind==Terminator::Kind::branch ||
               block.end.kind==Terminator::Kind::yield)edge(block.end.yes);
            if(block.end.kind==Terminator::Kind::branch)edge(block.end.no);
        }
    };
    verify(0,false);
    if(f.cleanup_entry && *f.cleanup_entry)verify(*f.cleanup_entry,true);
}
}
