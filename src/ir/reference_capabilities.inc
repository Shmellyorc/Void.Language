// Independent capability proof over nominally validated IR. Source summaries
// are deliberately not inputs: malformed hand-built IR receives the same proof.
std::set<unsigned> validate_reference_initialization(const Function& f) {
    std::set<unsigned> required,entry,all;
    for (unsigned slot=0;slot<f.locals.size();++slot) {
        all.insert(slot);
        for (const auto& path:reference_paths(f.locals[slot])) {
            auto leaf=f.locals[slot];for(auto field:path) leaf=field_type(leaf,field);
            if (!leaf.nullable) required.insert(slot);
        }
    }
    if (f.receiver) entry.insert(0);
    std::vector<std::vector<unsigned>> predecessors(f.blocks.size());
    std::set<unsigned> reachable;
    std::vector<unsigned> pending{0};
    while(!pending.empty()) {
        auto block=pending.back();pending.pop_back();if(!reachable.insert(block).second) continue;
        const auto& end=f.blocks[block].end;
        auto edge=[&](unsigned target){predecessors[target].push_back(block);pending.push_back(target);};
        if(end.kind==Terminator::Kind::branch){edge(end.yes);edge(end.no);}
        else if(end.kind==Terminator::Kind::jump) edge(end.yes);
    }
    std::vector<std::set<unsigned>> incoming(f.blocks.size(),all),outgoing(f.blocks.size(),all);
    bool changed;
    do {
        changed=false;
        for(auto block:reachable) {
            auto state=block==0 ? entry : all;
            for(auto predecessor:predecessors[block])
                std::erase_if(state,[&](auto slot){return !outgoing[predecessor].contains(slot);});
            if(incoming[block]!=state){incoming[block]=state;changed=true;}
            for(const auto& i:f.blocks[block].instructions) if(i.op==Op::store) state.insert(i.a);
            if(outgoing[block]!=state){outgoing[block]=state;changed=true;}
        }
    }while(changed);
    std::set<unsigned> observed_defaults;
    for(auto block:reachable) {
        auto state=incoming[block];
        auto require=[&](unsigned slot) {
            if(!state.contains(slot)) {
                if(required.contains(slot)) throw std::invalid_argument("VOID IR: non-nullable reference storage read before initialization in "+f.name);
                observed_defaults.insert(slot);
            }
        };
        for(const auto& i:f.blocks[block].instructions) {
            if(i.op==Op::load) require(i.a);
            if(i.receiver) require(i.receiver->slot);
            if(i.op==Op::store) state.insert(i.a);
        }
    }
    return observed_defaults;
}
void validate_reference_capabilities(const Module& module,InstructionTypes& types) {
    std::vector<std::set<unsigned>> observed_defaults;
    for(const auto& f:module.functions) observed_defaults.push_back(validate_reference_initialization(f));
    std::map<std::string,unsigned> indexes;
    for (unsigned n=0;n<module.functions.size();++n) indexes.emplace(module.functions[n].name,n);
    std::vector<ReferenceEffects> summaries(module.functions.size());
    auto run=[&](unsigned function,bool report,bool requirements) {
        const auto& f=module.functions[function]; auto& summary=summaries[function];
        bool changed=false,local_changed=false;
        std::vector<ReferenceFacts> locals(f.locals.size());
        std::vector<ReferenceFacts> capacities(f.locals.size());
        std::vector<std::optional<std::pair<unsigned,unsigned>>> initializations(f.locals.size());
        if (f.receiver) locals[0]=formal_facts(f.parameters[0],0);
        if (f.receiver) {capacities[0]=locals[0];initializations[0]=std::pair{~0u,~0u};}
        for(auto slot:observed_defaults[function]) {capacities[slot]=reference_facts(f.locals[slot]);initializations[slot]=std::pair{~0u,~0u};}
        auto fail=[&](const std::string& message) {
            if (report) throw std::invalid_argument("VOID IR: "+message+" in "+f.name);
        };
        auto check=[&](const ReferenceFacts& actual,Type expected,bool require_mutable=false,const ReferenceFacts* capacity=nullptr) {
            if (!capability_assignable(fact_type(expected,actual),expected)) fail("readonly reference capability would be discarded");
            if(requirements && require_mutable) for(const auto& origin:mutable_requirements(actual,expected,capacity))
                changed|=summary.required_mutable.insert(origin).second;
        };
        auto effect=[&](std::set<ReferenceOrigin>& into,const ReferenceFact& fact) {
            for (const auto& origin:fact.origins) changed|=into.insert(origin).second;
        };
        auto retain=[&](const ReferenceFacts& facts) {
            for (const auto& [path,fact]:facts) {
                effect(summary.retained,fact);
                if (fact.readonly) fail("readonly reference stored in mutable managed field");
            }
        };
        do {
            local_changed=false;
            for (unsigned block=0;block<f.blocks.size();++block) {
                const auto& b=f.blocks[block]; std::vector<ReferenceFacts> values;
                for (unsigned n=0;n<b.instructions.size();++n) {
                    const auto& i=b.instructions[n]; auto type=types[function][block][n];
                    auto value=reference_facts(type);
                    switch (i.op) {
                    case Op::argument: value=formal_facts(f.parameters[i.a],i.a); break;
                    case Op::load: value=reference_facts(f.locals[i.a]);join_facts(value,locals[i.a]);break;
                    case Op::store: {
                        if (!capability_assignable(types[function][block][i.b],f.locals[i.a])) fail("store discards reference or construction-place capability");
                        const auto position=std::pair{block,n};
                        if(!initializations[i.a]) initializations[i.a]=position;
                        if(*initializations[i.a]==position) join_facts(capacities[i.a],values[i.b]);
                        check(values[i.b],f.locals[i.a],*initializations[i.a]!=position,&capacities[i.a]);
                        auto stored=values[i.b]; auto capacity=readonly_paths(f.locals[i.a]);
                        for (auto& [path,fact]:stored) fact.readonly=capacity.contains(path);
                        local_changed|=join_facts(locals[i.a],stored); value={}; break;
                    }
                    case Op::extract: case Op::gc_read:
                        value=field_facts(types[function][block][i.a],values[i.a],static_cast<unsigned>(i.immediate)); break;
                    case Op::insert:
                        value=replaced_facts(values[i.a],static_cast<unsigned>(i.immediate),values[i.b]); break;
                    case Op::aggregate:
                        for(unsigned field=0;field<i.arguments.size();++field)
                            value=replaced_facts(std::move(value),field,values[i.arguments[field]]);
                        join_facts(value,reference_facts(i.type));
                        break;
                    case Op::convert: value=values[i.a];join_facts(value,reference_facts(i.type));break;
                    case Op::gc_construct:
                        for(auto argument:i.arguments) retain(values[argument]);
                        break;
                    case Op::gc_write:
                        if (auto found=values[i.a].find({});found!=values[i.a].end()) {
                            effect(summary.mutations,found->second);
                            if (found->second.readonly) fail("write through readonly reference");
                        }
                        retain(values[i.b]); value={}; break;
                    case Op::call: {
                        auto found=indexes.find(i.callee);
                        if (found==indexes.end()) break; // Existing trusted scalar ABI.
                        const auto& target=module.functions[found->second]; const auto& effects=summaries[found->second];
                        std::vector<ReferenceFacts> arguments;
                        Type receiver_type; bool inline_place=false;
                        std::optional<ReferenceFact> heap_owner;
                        bool readonly_owner=false;
                        if (i.receiver) {
                            receiver_type=f.locals[i.receiver->slot]; auto facts=locals[i.receiver->slot];
                            inline_place=receiver_type.kind==Type::aggregate;
                            for(auto field:i.receiver->fields) {
                                inline_place&=receiver_type.kind==Type::aggregate;
                                if(receiver_type.kind==Type::managed_ref) {
                                    readonly_owner=receiver_type.readonly;
                                    if(auto leaf=facts.find({});leaf!=facts.end()) heap_owner=leaf->second;
                                }
                                facts=field_facts(receiver_type,facts,field); receiver_type=field_type(receiver_type,field);
                            }
                            arguments.push_back(std::move(facts));
                            if(target.receiver_mutates && readonly_owner) fail("mutating inline receiver belongs to readonly storage");
                            if(target.receiver_mutates && heap_owner) {
                                effect(summary.mutations,*heap_owner);
                                if(heap_owner->readonly) fail("mutating inline receiver belongs to readonly heap storage");
                            }
                            if (i.initialization) check(arguments[0],target.parameters[0]);
                            if (inline_place && f.receiver && !f.receiver_mutates && i.receiver->slot==0 && target.receiver_mutates)
                                fail("non-mutating method calls mutating inline receiver");
                        }
                        auto operands=i.arguments;
                        if (!target.receiver && operands.empty() && target.parameters.size()==1) operands.push_back(i.a);
                        for(auto argument:operands) {
                            if (!capability_assignable(types[function][block][argument],target.parameters[arguments.size()])) fail("call discards reference or construction-place capability");
                            check(values[argument],target.parameters[arguments.size()],true); arguments.push_back(values[argument]);
                        }
                        for(auto [set,kind]:{std::pair{&effects.mutations,'M'},std::pair{&effects.retained,'H'},std::pair{&effects.required_mutable,'Q'}}) {
                            if(kind=='Q' && !requirements) continue;
                            for(const auto& origin:*set) if(origin.parameter<arguments.size()) {
                                if(auto leaf=arguments[origin.parameter].find(origin.path);leaf!=arguments[origin.parameter].end()) {
                                    effect(kind=='H' ? summary.retained : kind=='Q' ? summary.required_mutable : summary.mutations,leaf->second);
                                    if(leaf->second.readonly) fail(kind=='H' ? "call retains readonly reference" : kind=='Q' ? "call requires mutable reference authority" : "call mutates readonly reference");
                                }
                            }
                        }
                        if(i.receiver && inline_place) {
                            auto after=instantiate_result(effects.receiver_after,arguments); ReferenceFacts prefixed;
                            for(const auto& [leaf,fact]:after) {
                                auto path=i.receiver->fields; path.insert(path.end(),leaf.begin(),leaf.end()); prefixed.emplace(std::move(path),fact);
                            }
                            check(prefixed,f.locals[i.receiver->slot]);
                            local_changed|=join_facts(locals[i.receiver->slot],prefixed);
                        }
                        value=instantiate_result(effects.returned,arguments); break;
                    }
                    default: break;
                    }
                    for(const auto& path:reference_paths(type)) value.try_emplace(path);
                    if(report) types[function][block][n]=fact_type(type,value);
                    values.push_back(std::move(value));
                }
                if(b.end.kind==Terminator::Kind::return_value) {
                    check(values[b.end.value],f.result);
                    auto returned=values[b.end.value];join_facts(returned,reference_facts(f.result));
                    changed|=join_facts(summary.returned,returned);
                }
            }
        } while(local_changed);
        if(f.receiver && f.parameters[0].kind==Type::aggregate)
            changed|=join_facts(summary.receiver_after,locals[0]);
        if(f.receiver && f.parameters[0].kind==Type::managed_ref && !f.receiver_mutates)
            for(const auto& origin:summary.mutations) if(origin.parameter==0) fail("non-mutating receiver has a referent mutation effect");
        return changed;
    };
    bool changed;
    for(bool requirements:{false,true}) do {
        changed=false;
        for(unsigned n=0;n<module.functions.size();++n) changed|=run(n,false,requirements);
    } while(changed);
    for(unsigned n=0;n<module.functions.size();++n) run(n,true,true);
}
