// Capability/effect analysis consumes the nominally checked AST. Logical leaf
// operations are shared with the independent IR verifier; no runtime permission tags exist.
void analyze_capabilities(std::map<std::string,std::unique_ptr<Unit>>& units, Diagnostics& diagnostics) {
    using ir::ReferenceFacts;
    using ir::ReferenceFact;
    using ir::ReferenceOrigin;
    std::map<std::string,Function*> functions;
    std::map<std::string,Unit*> function_units;
    std::map<std::string,std::pair<Unit*,Structure*>> structures;
    for (auto& [id,u]:units) {
        for (auto& f:u->functions) { functions.emplace(f.symbol,&f); function_units.emplace(f.symbol,u.get()); }
        for (auto& s:u->structures) structures.emplace(s.definition->identity,std::pair{u.get(),&s});
    }
    std::map<std::string, Global*> globals;
    for (auto& [id, owner] : units) for (auto& value : owner->globals) globals.emplace(value.identity, &value);
    struct Edges {std::set<std::string> calls;std::set<Expr*> spawns;bool unsafe=false;};
    std::map<std::string,Edges> graph;for(const auto& [name,f]:functions)graph.try_emplace(name);
    auto leaf_related=[&](Type type,const ir::ReferencePath& path) {
        std::vector<RelatedLocation> result;
        if (type.definition) if (auto found=structures.find(type.definition->identity);found!=structures.end())
            result.push_back({span(found->second.first->source,found->second.second->token),"Reference type declared here."});
        const auto name=ir::reference_path_name(type,path);
        for (auto field:path) {
            if (auto found=structures.find(type.definition->identity);found!=structures.end()) {
                const auto [owner,declaration]=found->second.second->effective_fields.at(field);
                result={{span(owner->source,declaration->token),"Reference leaf '"+name+"' declared here."}};
            }
            type=ir::field_type(type,field);
        }
        return result;
    };
    auto run=[&](Unit& unit,Function& f,bool report,bool requirements) {
        if(f.interface_requirement) return false;
        if(!f.native_symbol.empty()){
            graph[f.symbol].unsafe=true;
            for(unsigned n=0;n<f.parameters.size();++n)for(const auto& [path,fact]:ir::formal_facts(f.parameters[n].type,n))for(const auto& origin:fact.origins){f.reference_effects.mutations.insert(origin);f.reference_effects.retained.insert(origin);}
            f.capability_contract="native:"+f.native_symbol;return false;
        }
        bool changed=false,local_changed=false;
        if(f.receiver && f.token.text=="deinit"){changed|=f.reference_effects.mutations.insert(ir::ReferenceOrigin{0,{}}).second;if(!f.receiver_mutates){f.receiver_mutates=true;changed=true;}}
        std::vector<ReferenceFacts> locals(f.locals.size());
        std::map<const Expr*,ReferenceFacts> evaluated;
        unsigned index=0;
        if (f.receiver) locals[f.receiver->slot]=ir::formal_facts(f.receiver->type,index++);
        for (const auto& p:f.parameters) locals[p.slot]=ir::formal_facts(p.type,index++);
        for(const auto& [slot,field]:f.capture_inputs){auto env=f.parameters.at(0).type;auto facts=ir::field_facts(env,ir::formal_facts(env,0),field);auto cell=ir::field_type(env,field);locals[slot]=ir::field_facts(cell,facts,0);}
        auto issue=[&](const Token& token,const std::string& message,std::vector<RelatedLocation> related = {}) {
            if (!report) return;
            try { error(unit.source,token,Code::readonly,message,"Use a const parameter for readonly access or independently held mutable references. Copying value storage does not restore reference authority.",std::move(related)); }
            catch (const CompilationError& failure) { diagnostics.add(failure); }
        };
        auto effect=[&](std::set<ReferenceOrigin>& into,const ReferenceFact& fact) {
            for (const auto& origin:fact.origins) changed|=into.insert(origin).second;
        };
        auto retain=[&](const ReferenceFacts& facts,Type type,const Token& token) {
            for (const auto& [path,fact]:facts) {
                if(ir::readonly_paths(type).contains(path))continue;
                effect(f.reference_effects.retained,fact);
                if (report && fact.readonly) issue(token,"Cannot store readonly reference leaf '"+ir::reference_path_name(type,path)+"' in mutable managed field storage.",leaf_related(type,path));
            }
        };
        auto compatible=[&](const ReferenceFacts& actual,Type expected,const Token& token,const ReferenceFacts* capacity=nullptr,std::vector<RelatedLocation> related = {}) {
            if (requirements) for (const auto& origin:ir::mutable_requirements(actual,expected,capacity))
                changed|=f.reference_effects.required_mutable.insert(origin).second;
            if (!report) return;
            // Recursive collection paths can have equivalent unfolded spellings.
            // The shared authority predicate normalizes them conservatively; a
            // second raw-path subset check must not reject its accepted view.
            if(ir::capability_assignable(ir::fact_type(expected,actual),expected))return;
            const auto allowed=ir::readonly_paths(expected);
            bool readonly_failure=false;for(const auto& [path,fact]:actual)readonly_failure|=fact.readonly && !allowed.contains(path);
            if(!readonly_failure)issue(token,"Mutable list storage requires an invariant element authority profile.",related);
            for (const auto& [path,fact]:actual) if(fact.readonly && !allowed.contains(path)) {
                auto fields=leaf_related(expected,path);fields.insert(fields.end(),related.begin(),related.end());
                issue(token,"Cannot assign readonly reference leaf '"+ir::reference_path_name(expected,path)+"' to storage or a parameter promising mutable access.",std::move(fields));
                break;
            }
        };
        auto call=[&](Function& target,const std::vector<ReferenceFacts>& arguments,const Token& token,bool dynamic=false) {
            graph[f.symbol].calls.insert(target.symbol);if(dynamic || target.asynchronous)graph[f.symbol].unsafe=true;
            const auto& target_effects=dynamic ? target.virtual_effects : target.reference_effects;
            unsigned first=target.receiver ? 1u : 0u;
            auto formal=[&](unsigned n) { return n<first ? target.receiver->type : target.parameters.at(n-first).type; };
            auto declaration=[&](unsigned n) { return span(function_units.at(target.symbol)->source,n<first ? target.receiver->token : target.parameters.at(n-first).token); };
            for (unsigned n=first;n<arguments.size();++n) {
                std::vector<RelatedLocation> related;
                if(report && !ir::capability_assignable(ir::fact_type(formal(n),arguments[n]),formal(n)))
                    related.push_back({declaration(n),"Parameter declared here; const accepts readonly reference leaves."});
                compatible(arguments[n],formal(n),token,nullptr,std::move(related));
            }
            for (auto [effects,kind]:{std::pair{&target_effects.mutations,'M'},std::pair{&target_effects.retained,'H'},std::pair{&target_effects.required_mutable,'Q'}}) {
                if(kind=='Q' && !requirements) continue;
                for (const auto& origin:*effects) if (origin.parameter<arguments.size()) {
                    if (auto found=arguments[origin.parameter].find(origin.path);found!=arguments[origin.parameter].end()) {
                        effect(kind=='H' ? f.reference_effects.retained : kind=='Q' ? f.reference_effects.required_mutable : f.reference_effects.mutations,found->second);
                        bool boundary=origin.parameter>=first && !ir::capability_assignable(ir::fact_type(formal(origin.parameter),arguments[origin.parameter]),formal(origin.parameter));
                        bool stronger=kind=='Q' && (target_effects.mutations.contains(origin) || target_effects.retained.contains(origin));
                        if (report && found->second.readonly && !boundary && !stronger) {
                            auto type=formal(origin.parameter);auto related=leaf_related(type,origin.path);related.push_back({declaration(origin.parameter),"Reference authority enters this declaration here."});
                            const auto path=ir::reference_path_name(type,origin.path);
                            issue(token,kind=='H' ? "A call may retain readonly reference leaf '"+path+"' in mutable object storage." : kind=='Q' ? "A call requires mutable authority for reference leaf '"+path+"'." : "A call may mutate readonly reference leaf '"+path+"'.",std::move(related));
                        }
                    }
                }
            }
            return ir::instantiate_result(target_effects.returned,arguments);
        };
        std::function<void(Expr&,const ReferenceFacts&)> write_selection;
        auto invoke=[&](Expr& e,const std::string& symbol,const std::vector<ReferenceFacts>& arguments)->ReferenceFacts {
                if (auto found=functions.find(symbol);found!=functions.end()) {
                    auto& target=*found->second;
                    auto result=call(target,arguments,e.token,!e.virtual_slot.empty() && !e.super_call);
                    if (e.kind==Expr::Kind::method_call && target.receiver->type.kind==Type::aggregate && e.receiver_addressable && !e.selected_receiver && !e.receiver_global) {
                        auto after=ir::instantiate_result(target.reference_effects.receiver_after,arguments);
                        ReferenceFacts prefixed;
                        for (const auto& [leaf,fact]:after) {
                            auto path=e.field_indexes; path.insert(path.end(),leaf.begin(),leaf.end()); prefixed.emplace(std::move(path),fact);
                        }
                        // Only inline receiver Places share local storage.
                        auto root=f.locals.at(e.slot); bool inline_place=root.kind==Type::aggregate;
                        for (auto field:e.field_indexes) { inline_place&=root.kind==Type::aggregate; root=ir::field_type(root,field); }
                        if (inline_place) local_changed|=ir::join_facts(locals[e.slot],prefixed);
                    }
                    if (e.kind == Expr::Kind::method_call && e.receiver_global && target.receiver->type.kind == Type::aggregate && target.receiver_mutates) {
                        auto after = arguments[0]; ir::join_facts(after, ir::instantiate_result(target.reference_effects.receiver_after, arguments));
                        retain(after, e.parts[0]->type, e.token);
                    }
                    if(e.kind==Expr::Kind::method_call && target.receiver_mutates && target.receiver->type.kind==Type::aggregate && e.selected_receiver) {
                        auto after=arguments[0];ir::join_facts(after,ir::instantiate_result(target.reference_effects.receiver_after,arguments));
                        write_selection(*e.parts[0],after);
                    }
                    if(report && e.kind==Expr::Kind::method_call)e.receiver_writes=target.receiver_mutates;
                    if(report && e.property && symbol==e.setter)e.setter_mutates=target.receiver_mutates;
                    if (e.kind==Expr::Kind::method_call && target.receiver_mutates) {
                        // A by-place inline write can be a heap mutation even
                        // when the selected value contains no reference leaves.
                        // Keep the selected owner's authority distinct from the
                        // copied value's embedded-reference capabilities.
                        const Expr* selected=e.parts[0].get();
                        while (selected->kind==Expr::Kind::field || selected->kind==Expr::Kind::array_index || selected->property) {
                            const auto* owner=selected->parts[0].get();
                            if (owner->type.kind==Type::managed_ref || owner->type.kind==Type::growable_array || owner->type.kind==Type::dictionary) {
                                if (auto leaf=evaluated.at(owner).find({});leaf!=evaluated.at(owner).end()) {
                                    effect(f.reference_effects.mutations,leaf->second);
                                    if (leaf->second.readonly && !e.receiver_readonly) issue(e.token,"Cannot call a mutating method on storage owned by a readonly reference.");
                                }
                                break;
                            }
                            selected=owner;
                        }
                        if (e.receiver_readonly) issue(e.token,"Cannot call a mutating method through readonly receiver storage.");
                        if (!e.receiver_addressable && report) {
                            try { error(unit.source,e.token,Code::receiver,"A mutating method requires mutable receiver storage.","Bind the value to var before calling this method."); }
                            catch (const CompilationError& failure) { diagnostics.add(failure); }
                        }
                        // Only inline aliases denote the receiver's own value
                        // storage. Crossing a reference is a referent effect.
                        auto type=f.receiver ? f.receiver->type : Type::none;
                        bool inline_place=f.receiver && e.receiver_addressable && !e.receiver_global && e.slot==f.receiver->slot;
                        for (auto field:e.field_indexes) {
                            inline_place&=type.kind==Type::aggregate;
                            if (type.kind==Type::aggregate || type.kind==Type::managed_ref) type=ir::field_type(type,field);
                        }
                        if (inline_place && !f.receiver_mutates) { f.receiver_mutates=true; changed=true; }
                    }
                    if(target.asynchronous) {
                        for(unsigned n=0;n<arguments.size();++n)retain(arguments[n],n==0 && target.receiver?target.receiver->type:target.parameters[n-(target.receiver?1:0)].type,e.token);
                        result=ir::replaced_facts(ir::reference_facts(e.type),1,result);
                    }
                    return result;
                }
            return {};
        };
        std::function<ReferenceFacts(Expr&)> expression;
        std::function<void(Pattern&,const ReferenceFacts&)> pattern;
        pattern=[&](Pattern& p,const ReferenceFacts& facts) {
            using K=Pattern::Kind;
            for(auto& value:p.values)expression(*value);
            if(p.kind==K::capture){local_changed|=ir::join_facts(locals[p.slot],facts);return;}
            for(unsigned n=0;n<p.children.size();++n) {
                ReferenceFacts child;
                if(p.kind==K::list){child=ir::field_facts(p.type,facts,ir::array_element_path);if(p.rest && n+1==p.children.size()){auto leaves=child;child=ir::reference_facts(p.children[n].type);for(const auto& [path,fact]:leaves){auto nested=path;nested.insert(nested.begin(),ir::array_element_path);child[nested].join(fact);}}}
                else if(p.kind==K::dictionary)child=ir::field_facts(ir::array_element(p.type),ir::field_facts(p.type,facts,ir::array_element_path),1);
                else if(p.nullable || p.rest)child=facts;
                else child=ir::field_facts(p.type,facts,p.indexes[n]);
                pattern(p.children[n],child);
            }
        };
        expression=[&](Expr& e)->ReferenceFacts {
            using K=Expr::Kind;
            auto& edges=graph[f.symbol];
            if (e.kind == K::global) {
                edges.unsafe = true;
                auto type = globals.at(e.target)->type;
                if (ir::capability_key(e.type) != ir::capability_key(type)) { e.type = type; changed = true; }
            }
            if(e.kind==K::spawned){edges.spawns.insert(&e);edges.unsafe=true;}
            if(e.kind==K::awaited || e.kind==K::task_cancel || e.kind==K::function_call || e.asynchronous_call || e.interface_call || !e.virtual_slot.empty())edges.unsafe=true;
            if(e.kind==K::function_reference)edges.calls.insert(e.target);
            if(e.target.starts_with("$time.") && temporal::operations.at(std::stoul(e.target.substr(6))).clock)edges.unsafe=true;
            if((e.kind==K::call || e.kind==K::method_call) && !functions.contains(e.target) && !e.target.starts_with("$collection.") && !e.target.starts_with("$array.") && !e.target.starts_with("$dict.") && !e.target.starts_with("$time.") && !e.target.starts_with("$layout.") && !e.target.starts_with("$display.") && e.target!="$interpolation" && e.target!="$hole")edges.unsafe=true;
            auto view=ir::reference_facts(e.type);
            if(e.kind==K::spawned)frontend_case([&] {auto result=expression(*e.parts[0]);view=ir::replaced_facts(view,1,result);
            }); else if(e.kind==K::awaited)frontend_case([&] {
                auto task=expression(*e.parts[0]);view=ir::field_facts(e.parts[0]->type,task,1);
            }); else if(e.kind==K::capture_reference)frontend_case([&] {
                auto facts=e.parts.empty()?locals.at(e.slot):expression(*e.parts[0]);auto stored=ir::field_type(e.type,0);auto combined=ir::reference_facts(stored);ir::join_facts(combined,facts);auto profile=ir::fact_type(stored,combined);
                if(ir::capability_key(stored)!=ir::capability_key(profile)){std::const_pointer_cast<ir::ValueType>(e.type.definition)->fields[0].type=profile;changed=true;}
                retain(facts,profile,e.token);
            }); else if(e.kind==K::function_reference) frontend_case([&] {
                std::vector<ReferenceFacts> arguments;for(auto& part:e.parts)arguments.push_back(expression(*part));
                auto& target=*functions.at(e.target);if(!target.native_symbol.empty())issue(e.token,"Native bindings cannot escape as function values.");
                if(e.op=="value_bound"){
                    auto field=ir::field_type(e.environment,0);auto combined=ir::reference_facts(field);ir::join_facts(combined,arguments[0]);auto profile=ir::fact_type(field,combined);if(ir::capability_key(field)!=ir::capability_key(profile)){std::const_pointer_cast<ir::ValueType>(e.environment.definition)->fields[0].type=profile;changed=true;}retain(arguments[0],profile,e.token);arguments={ir::reference_facts(e.environment)};
                }
                if(e.op=="bound"){
                    const auto& facts=arguments[0];for(const auto* effects:{&target.reference_effects.mutations,&target.reference_effects.retained,&target.reference_effects.required_mutable})for(const auto& origin:*effects)if(origin.parameter==0)if(auto found=facts.find(origin.path);found!=facts.end()){effect(f.reference_effects.retained,found->second);if(found->second.readonly)issue(e.token,"Bound callback requires mutable receiver authority.");}
                    if(report && !ir::capability_assignable(ir::fact_type(target.result,ir::instantiate_result(target.reference_effects.returned,arguments)),*e.type.definition->call_result))issue(e.token,"Bound callback would discard returned readonly authority.");
                }
                if(report){auto& target=*functions.at(e.target);if(!ir::capability_assignable(ir::fact_type(target.result,target.reference_effects.returned),*e.type.definition->call_result))issue(e.token,"Function value would discard returned readonly authority.");}
            }); else if(e.kind==K::function_call) frontend_case([&] {
                expression(*e.parts[0]);std::vector<ReferenceFacts> arguments;
                for(unsigned n=1;n<e.parts.size();++n){auto facts=expression(*e.parts[n]);auto formal=Type(e.parts[0]->type.definition->call_parameters[n-1]);compatible(facts,formal,e.parts[n]->token);for(const auto& [path,fact]:facts)if(!ir::readonly_paths(formal).contains(path)){effect(f.reference_effects.mutations,fact);effect(f.reference_effects.retained,fact);}arguments.push_back(std::move(facts));}
                view=ir::callable_result(e.parts[0]->type,arguments);
            }); else if(e.kind==K::test) frontend_case([&] {
                auto subject=expression(*e.parts[0]);pattern(e.patterns[0],subject);
            }); else if(e.kind==K::match) frontend_case([&] {
                auto subject=expression(*e.parts[0]);
                for(unsigned n=0;n<e.patterns.size();++n){pattern(e.patterns[n],subject);ir::join_facts(view,expression(*e.parts[n+1]));}
            }); else if(e.kind==K::choice) frontend_case([&] {
                expression(*e.parts[0]);ir::join_facts(view,expression(*e.parts[1]));ir::join_facts(view,expression(*e.parts[2]));
            }); else if(e.kind==K::sum) frontend_case([&] {
                const auto& variant=e.type.definition->cases.at(e.slot);
                for(unsigned n=0;n<e.parts.size();++n)view=ir::replaced_facts(std::move(view),variant.begin+n,expression(*e.parts[n]));
            }); else if(e.kind==K::propagate) frontend_case([&] {
                auto child=expression(*e.parts[0]);view=ir::field_facts(e.parts[0]->type,child,1);
                // Error propagation is a real return edge, with the same authority checks.
                auto failure=ir::replaced_facts(ir::reference_facts(f.result),2,ir::field_facts(e.parts[0]->type,child,2));
                compatible(failure,f.result,e.token);
                for(const auto& [path,fact]:failure)changed|=f.reference_effects.returned[path].join(fact);
            }); else if(e.kind==K::array_literal || e.kind==K::array_default) frontend_case([&] {
                for(auto& part:e.parts) {
                    auto child=expression(*part);
                    for(const auto& [leaf,fact]:child) {auto path=leaf;path.insert(path.begin(),ir::array_element_path);view[path].join(fact);}
                }
            }); else if(e.kind==K::dictionary_literal) frontend_case([&] {
                for(unsigned n=0;n<e.parts.size();n+=2)for(unsigned field=0;field<2;++field){auto child=expression(*e.parts[n+field]);for(const auto& [leaf,fact]:child){auto path=leaf;path.insert(path.begin(),field);path.insert(path.begin(),ir::array_element_path);view[path].join(fact);}}
            }); else if(e.kind==K::array_slice) frontend_case([&] {
                auto owner=expression(*e.parts[0]);expression(*e.parts[1]);expression(*e.parts[2]);
                view=ir::replaced_facts(std::move(view),ir::array_element_path,ir::field_facts(e.parts[0]->type,owner,ir::array_element_path));
            }); else if(e.kind==K::array_index) frontend_case([&] {
                auto owner=expression(*e.parts[0]);expression(*e.parts[1]);if(e.parts[0]->type!=Type::string){view=ir::field_facts(e.parts[0]->type,owner,ir::array_element_path);if(e.parts[0]->type.kind==Type::dictionary)view=ir::field_facts(ir::array_element(e.parts[0]->type),view,1);}
            }); else if (e.kind==K::name) view=locals.at(e.slot);
            else if (e.kind==K::field) frontend_case([&] {
                auto base=expression(*e.parts[0]);
                if(e.target!="$array.count" && e.target!="$dict.count" && !e.target.starts_with("$text."))view=ir::field_facts(e.parts[0]->type,base,e.slot);
            }); else if (e.kind==K::conversion) {
                view=expression(*e.parts[0]);
                if(e.op=="interface")for(const auto& [id,root]:units)if(root->member_catalog) {
                    for(const auto& binding:root->member_catalog->interface_bindings)if(binding.key==e.target && ir::binding_requires_readonly(binding))view[{}].readonly=true;
                    break;
                }
            }
            else if (e.kind==K::call || e.kind==K::method_call) frontend_case([&] {
                std::vector<ReferenceFacts> arguments;
                for (auto& part:e.parts) arguments.push_back(expression(*part));
                if(e.target.starts_with("$safe.")){
                    auto child=ir::field_facts(e.parts[0]->type,arguments[0],ir::array_element_path);
                    if(e.parts[0]->type.kind==Type::dictionary)child=ir::field_facts(ir::array_element(e.parts[0]->type),child,1);
                    view=ir::replaced_facts(std::move(view),1,child);
                    if(e.target=="$safe.array.pop")if(auto leaf=arguments[0].find({});leaf!=arguments[0].end()){effect(f.reference_effects.mutations,leaf->second);if(report && leaf->second.readonly)issue(e.token,"Cannot pop a readonly list.");}
                }
                if(e.target=="$event.emit"){
                    auto signature=ir::array_element(e.parts[0]->type);for(unsigned n=1;n<arguments.size();++n){auto formal=Type(signature.definition->call_parameters[n-1]);compatible(arguments[n],formal,e.token);for(const auto& [path,fact]:arguments[n])if(!ir::readonly_paths(formal).contains(path)){effect(f.reference_effects.mutations,fact);effect(f.reference_effects.retained,fact);}}
                }
                if(e.target.starts_with("$dict.") && e.token.text!="contains")if(auto leaf=arguments[0].find({});leaf!=arguments[0].end()){effect(f.reference_effects.mutations,leaf->second);if(report && leaf->second.readonly)issue(e.token,"Cannot mutate a readonly dictionary.");}
                if(e.target.starts_with("$array.")) {
                    if(e.token.text=="push")compatible(arguments[1],ir::array_element(e.parts[0]->type),e.token);
                    else if(e.token.text=="insert")compatible(arguments[2],ir::array_element(e.parts[0]->type),e.token);
                    else if(e.token.text=="remove_at")view=ir::field_facts(e.parts[0]->type,arguments[0],ir::array_element_path);
                    if(auto leaf=arguments[0].find({});leaf!=arguments[0].end()) {
                        effect(f.reference_effects.mutations,leaf->second);
                        if(report && leaf->second.readonly)issue(e.token,"Cannot mutate a readonly list.");
                    }
                }
                if(!e.display_adapter.empty())invoke(e,e.display_adapter,arguments);
                if(functions.contains(e.target)) {
                    if(!e.argument_indexes.empty()) {auto ordered=arguments;const unsigned first=e.kind==K::method_call?1:0;for(unsigned n=first;n<arguments.size();++n)ordered[first+e.argument_indexes[n-first]]=arguments[n];arguments=std::move(ordered);}
                    view=invoke(e,e.target,arguments);
                }
            }); else if (e.kind==K::construction || e.kind==K::base_initialization) frontend_case([&] {
                view=ir::reference_facts(e.type);
                if (e.initializer.empty()) {
                    for (unsigned n=0;n<e.parts.size();++n) {
                        auto child=expression(*e.parts[n]);
                        if (e.type.kind==Type::managed_ref) retain(child,ir::field_type(e.type,e.field_indexes[n]),e.parts[n]->token);
                        else view=ir::replaced_facts(std::move(view),e.field_indexes[n],child);
                    }
                    for (auto& child:e.omitted) {
                        auto facts=expression(*child);
                        if (e.type.kind==Type::managed_ref) retain(facts,ir::field_type(e.type,child->slot),child->token);
                        else view=ir::replaced_facts(std::move(view),child->slot,facts);
                    }
                } else {
                    auto& target=*functions.at(e.initializer);
                    std::vector<ReferenceFacts> arguments(e.parts.size()+1);
                    arguments[0]=ir::reference_facts(target.receiver->type);
                    for (unsigned n=0;n<e.parts.size();++n) arguments[e.field_indexes[n]+1]=expression(*e.parts[n]);
                    for (auto& child:e.omitted) {
                        auto facts=expression(*child);
                        if (e.type.kind==Type::managed_ref) retain(facts,ir::field_type(e.type,child->slot),child->token);
                        else compatible(facts,ir::field_type(target.receiver->type,child->slot),child->token);
                    }
                    call(target,arguments,e.token);
                    if (e.type.kind==Type::aggregate) {
                        view=ir::instantiate_result(target.reference_effects.receiver_after,arguments);
                        ir::join_facts(view,ir::reference_facts(target.receiver->type));
                    }
                }
            }); else for (auto& child:e.parts) expression(*child);
            // Null/default values still have reference authority. An absent
            // provenance origin must never erase their logical reference leaf.
            for (const auto& path:ir::reference_paths(e.type)) view.try_emplace(path);
            e.type=ir::fact_type(e.type,view);
            evaluated[&e]=view;
            return view;
        };
        write_selection=[&](Expr& e,const ReferenceFacts& value) {
            compatible(value,e.type,e.token);
            if(e.property) {
                if(e.setter.empty()) {issue(e.token,"Property has no setter.");return;}
                if(e.parts[0]->type.kind==Type::aggregate && !e.receiver_addressable) {issue(e.token,"A value property setter requires mutable receiver storage; indexed inline receivers are not supported.");return;}
                if(e.receiver_readonly)issue(e.token,"Cannot set a property through readonly receiver storage.");
                for(auto* selected=&e;selected->kind==Expr::Kind::field || selected->kind==Expr::Kind::array_index || selected->property;selected=selected->parts[0].get()) {
                    auto& owner=*selected->parts[0];
                    if(owner.type.kind==Type::managed_ref || owner.type.kind==Type::growable_array || owner.type.kind==Type::dictionary)
                        if(auto found=evaluated.at(&owner).find({});found!=evaluated.at(&owner).end() && found->second.readonly)issue(e.token,"Cannot set a property through readonly receiver authority.");
                }
                invoke(e,e.setter,{evaluated.at(e.parts[0].get()),value});return;
            }
            if(e.kind==Expr::Kind::global) { retain(value, e.type, e.token); return; }
            if(e.kind==Expr::Kind::name) {
                local_changed|=ir::join_facts(locals[e.slot],value);
                if(f.receiver && e.slot==f.receiver->slot && !f.receiver_mutates){f.receiver_mutates=true;changed=true;}
                return;
            }
            auto& owner=*e.parts[0];
            const bool indexed=e.kind==Expr::Kind::array_index;
            const bool heap=owner.type.kind==Type::managed_ref || (indexed && (owner.type.kind==Type::growable_array || owner.type.kind==Type::dictionary));
            if(heap) {
                if(auto leaf=evaluated.at(&owner).find({});leaf!=evaluated.at(&owner).end()) {
                    effect(f.reference_effects.mutations,leaf->second);
                    if(report && leaf->second.readonly)issue(e.token,"Cannot mutate storage through readonly array/reference authority.");
                }
                if(owner.type.kind==Type::managed_ref)retain(value,e.type,e.token);
                return;
            }
            auto after=evaluated.at(&owner);
            after=ir::replaced_facts(std::move(after),indexed?ir::array_element_path:e.slot,value);
            write_selection(owner,after);
        };
        std::function<void(std::vector<Statement>&)> sequence;
        sequence=[&](auto& body) {
            for (auto& s:body) {
                using K=Statement::Kind;
                if(s.task_group)graph[f.symbol].unsafe=true;
                ReferenceFacts rhs;
                if (s.selection)expression(*s.selection);
                if (s.range_end) expression(*s.range_end);
                if (s.index) expression(*s.index);
                ReferenceFacts old;
                if(!s.compound_target.empty()) {
                    if(s.selection)old=evaluated.at(s.selection.get());
                    else {auto type=ir::fact_type(f.locals.at(s.slot),locals.at(s.slot));old=locals.at(s.slot);for(auto field:s.field_indexes){old=ir::field_facts(type,old,field);type=ir::field_type(type,field);}if(s.index)old=ir::field_facts(type,old,ir::array_element_path);}
                }
                if (s.expression && !s.task_group) rhs=expression(*s.expression);
                else if (s.kind==K::variable) rhs=ir::reference_facts(f.locals.at(s.slot));
                if(!s.compound_target.empty()){
                    rhs=call(*functions.at(s.compound_target),{old,rhs},s.compound_token);
                    if(s.compound_conversion){local_changed|=ir::join_facts(locals[s.compound_slot],rhs);rhs=expression(*s.compound_conversion);}
                }
                if (s.kind==K::variable || (s.kind==K::scoped && !s.task_group)) {
                    if (s.readonly) for (auto& [path,fact]:rhs) fact.readonly=true;
                    local_changed|=ir::join_facts(locals[s.slot],rhs);
                } else if(s.kind==K::assignment && s.event){
                    auto facts=s.selection?evaluated.at(s.selection.get()):locals.at(s.slot);auto type=s.selection?s.selection->type:f.locals.at(s.slot);if(!s.selection)for(auto index:s.field_indexes){facts=ir::field_facts(type,facts,index);type=ir::field_type(type,index);}if(auto found=facts.find({});found!=facts.end()){effect(f.reference_effects.mutations,found->second);if(found->second.readonly)issue(s.token,"Cannot change subscriptions through a readonly event.");}
                } else if(s.kind==K::assignment && s.selection) {
                    write_selection(*s.selection,rhs);
                } else if (s.kind==K::assignment) {
                    auto type=ir::fact_type(f.locals.at(s.slot),locals.at(s.slot));
                    auto selected=locals.at(s.slot);
                    ir::ReferencePath prefix;
                    bool heap=false;
                    for (auto field:s.field_indexes) {
                        if (type.kind==Type::managed_ref) {
                            heap=true;
                            if (auto leaf=selected.find({});leaf!=selected.end()) {
                                effect(f.reference_effects.mutations,leaf->second);
                                if (report && leaf->second.readonly) issue(s.token,"Cannot write through readonly reference leaf '"+ir::reference_path_name(f.locals[s.slot],prefix)+"'.",leaf_related(f.locals[s.slot],prefix));
                            }
                        }
                        selected=ir::field_facts(type,selected,field); type=ir::field_type(type,field);
                        prefix.push_back(field);
                    }
                    if(s.index && ir::collection_type(type)) {
                        if(auto leaf=selected.find({});leaf!=selected.end()) {effect(f.reference_effects.mutations,leaf->second);if(report && leaf->second.readonly)issue(s.token,"Cannot mutate a readonly list.");}
                        compatible(rhs,type.kind==Type::dictionary?ir::dictionary_value(type):ir::array_element(type),s.expression->token);
                        if(type.kind==Type::fixed_array && !heap) {
                            ir::ReferenceFacts prefixed;
                            for(const auto& [leaf,fact]:rhs) {auto path=s.field_indexes;path.push_back(ir::array_element_path);path.insert(path.end(),leaf.begin(),leaf.end());prefixed[path]=fact;}
                            local_changed|=ir::join_facts(locals[s.slot],prefixed);
                            if(f.receiver && s.slot==f.receiver->slot && !f.receiver_mutates){f.receiver_mutates=true;changed=true;}
                        }
                    }
                    else if (heap) retain(rhs,type,s.expression->token);
                    else {
                        const bool receiver=f.receiver && s.slot==f.receiver->slot;
                        if (receiver && !f.receiver_mutates) { f.receiver_mutates=true; changed=true; }
                        if (receiver && f.initializer && f.receiver->type.kind==Type::aggregate) {
                            // C1 starts complete. The fresh receiver has one
                            // fixed conservative profile for its entire body.
                            ir::ReferencePaths profile=ir::readonly_paths(f.receiver->type);
                            for (const auto& [leaf,fact]:rhs) if (fact.readonly) {
                                auto path=s.field_indexes; path.insert(path.end(),leaf.begin(),leaf.end()); profile.insert(std::move(path));
                            }
                            if (profile!=ir::readonly_paths(f.receiver->type)) {
                                f.receiver->type=ir::with_readonly_paths(f.receiver->type,profile);
                                f.locals[s.slot]=f.receiver->type; changed=true;
                            }
                        } else compatible(rhs,type,s.expression->token,&selected);
                        // Preserve the binding's capacity, joining provenance
                        // only. Whole-value assignment must not thaw or expand it.
                        auto updated=rhs;
                        auto capacity=ir::readonly_paths(type);
                        for (auto& [path,fact]:updated) fact.readonly=capacity.contains(path);
                        // Splice the selected inline prefix without traversing
                        // heap edges (heap storage was handled above).
                        ReferenceFacts prefixed;
                        for (const auto& [leaf,fact]:updated) { auto path=s.field_indexes; path.insert(path.end(),leaf.begin(),leaf.end()); prefixed.emplace(std::move(path),fact); }
                        local_changed|=ir::join_facts(locals[s.slot],prefixed);
                    }
                } else if (s.kind==K::returned) {
                    changed|=ir::join_facts(f.reference_effects.returned,rhs);
                }
                if(s.kind==K::loop && !s.binder.text.empty()) {
                    auto item=s.range_end ? ir::ReferenceFacts{} : ir::field_facts(s.expression->type,rhs,ir::array_element_path);
                    if(s.expression->type.kind==Type::dictionary){auto pair=item;item=ir::field_facts(ir::array_element(s.expression->type),pair,1);if(!s.index_binder.text.empty()){auto key=ir::field_facts(ir::array_element(s.expression->type),pair,0);f.locals[s.index_slot]=ir::fact_type(f.locals[s.index_slot],key);local_changed|=ir::join_facts(locals[s.index_slot],key);}}
                    f.locals[s.slot]=ir::fact_type(f.locals[s.slot],item);local_changed|=ir::join_facts(locals[s.slot],item);
                }
                if(s.kind==K::match){for(auto& arm:s.body){pattern(*arm.pattern,rhs);sequence(arm.body);}}else sequence(s.body);
                sequence(s.otherwise);
            }
        };
        if(f.base_prelude) expression(*f.base_prelude);
        for (auto& child:f.initialization_defaults) {
            auto facts=expression(*child);
            if (f.receiver->type.kind==Type::managed_ref) retain(facts,ir::field_type(f.receiver->type,child->slot),child->token);
            else {
                auto profile=ir::readonly_paths(f.receiver->type);
                for (const auto& [leaf,fact]:facts) if (fact.readonly) {
                    auto path=leaf; path.insert(path.begin(),child->slot); profile.insert(std::move(path));
                }
                if (profile!=ir::readonly_paths(f.receiver->type)) {
                    f.receiver->type=ir::with_readonly_paths(f.receiver->type,profile);
                    f.locals[f.receiver->slot]=f.receiver->type; changed=true;
                }
            }
        }
        do { local_changed=false; sequence(f.body); } while (local_changed);
        if (f.receiver && f.receiver->type.kind==Type::aggregate)
            changed|=ir::join_facts(f.reference_effects.receiver_after,locals[f.receiver->slot]);
        if (f.receiver && ir::reference_value(f.receiver->type) && !f.receiver_mutates) {
            for (const auto& origin:f.reference_effects.mutations) if (origin.parameter==0) { f.receiver_mutates=true; changed=true; }
        }
        for(const auto& [slot,cell]:f.captured_locals){
            auto stored=ir::field_type(cell,0);auto combined=ir::reference_facts(stored);ir::join_facts(combined,locals[slot]);auto profile=ir::fact_type(stored,combined);
            if(ir::capability_key(stored)!=ir::capability_key(profile)){std::const_pointer_cast<ir::ValueType>(cell.definition)->fields[0].type=profile;changed=true;}
        }
        if (f.global_initializer) {
            auto value = std::find_if(unit.globals.begin(), unit.globals.end(), [&](const auto& g) { return g.initializer == &f; });
            auto type = ir::fact_type(value->type, locals.at(f.body.front().slot));
            if (ir::capability_key(type) != ir::capability_key(value->type)) {
                value->type = type; std::const_pointer_cast<ir::ValueType>(value->cell.definition)->fields[0].type = type; changed = true;
            }
            f.result = type;
        }
        if (report) {
            for (unsigned n=0;n<locals.size();++n) f.locals[n]=ir::fact_type(f.locals[n],locals[n]);
            f.result=ir::fact_type(f.result,f.reference_effects.returned);
        }
        return changed;
    };
    bool changed;
    // First stabilize provenance/profiles, then impose mutable-boundary
    // requirements. This avoids freezing an empty, not-yet-known helper origin
    // as an independent mutable capacity in a recursive call graph.
    for(bool requirements:{false,true}) do {
        changed=false;
        for (auto& [id,u]:units) for (auto& f:u->functions) changed|=run(*u,f,false,requirements);
        for(auto& [id,u]:units) for(auto& requirement:u->functions) if(requirement.interface_requirement) {
            bool provider=false;
            for(const auto& [root_id,root]:units) if(root->member_catalog) {
                for(const auto& binding:root->member_catalog->interface_bindings) if(auto selected=binding.methods.find(requirement.symbol);selected!=binding.methods.end()) {
                    auto& method=*functions.at(selected->second.second);provider=true;
                    auto effects=!method.virtual_slot.empty() ? method.virtual_effects : method.reference_effects;
                    changed|=ir::join_effects(requirement.reference_effects,ir::abstract_interface_effects(effects,method.receiver_mutates));
                }
                break;
            }
            if(!provider) {
                ir::Function header;header.parameters={requirement.receiver->type};for(const auto& p:requirement.parameters) header.parameters.push_back(p.type);header.result=requirement.result;
                changed|=ir::join_effects(requirement.reference_effects,ir::unknown_interface_effects(header));
            }
        }
        std::map<std::string,ir::ReferenceEffects> families;
        for(auto& [id,u]:units) for(auto& f:u->functions) if(!f.virtual_slot.empty()) ir::join_effects(families[f.virtual_slot],f.reference_effects);
        for(auto& [id,u]:units) for(auto& f:u->functions) if(!f.virtual_slot.empty()) {
            changed|=ir::join_effects(f.virtual_effects,families.at(f.virtual_slot));
        }
    } while (changed);
    for (auto& [id,u]:units) for (auto& f:u->functions) {
        run(*u,f,true,true);
        if(f.accessor==1 && (f.receiver_mutates || std::any_of(f.reference_effects.mutations.begin(),f.reference_effects.mutations.end(),[](const auto& o){return o.parameter==0;}))) {
            try {error(u->source,f.token,Code::readonly,"A property getter cannot mutate its receiver.");}
            catch(const CompilationError& failure){diagnostics.add(failure);}
        }
        if(f.receiver && !f.interface_requirement && (f.token.text=="string" || f.token.text=="format")) {
            bool signature=f.token.text=="string" ? f.parameters.empty() && f.result==Type::string : f.parameters.size()==1 && f.parameters[0].type==Type::string && f.result.definition && f.result.definition->result_type && ir::field_type(f.result,1)==Type::string && ir::field_type(f.result,2)==Type::string;
            bool mutating=f.receiver_mutates;
            for(const auto* effects:{&f.reference_effects.mutations,&f.reference_effects.required_mutable})for(const auto& origin:*effects)mutating|=origin.parameter==0;
            if(!signature || f.asynchronous || mutating)try{error(u->source,f.token,Code::readonly,"A display hook must have an exact synchronous string/format signature and a nonmutating readonly receiver.");}catch(const CompilationError& failure){diagnostics.add(failure);}
        }
        if(f.interface_requirement) {f.result=ir::fact_type(f.result,f.reference_effects.returned);f.receiver_mutates=std::any_of(f.reference_effects.mutations.begin(),f.reference_effects.mutations.end(),[](const auto& origin){return origin.parameter==0;});}
        auto origin=[](const ReferenceOrigin& value) {
            std::string out=std::to_string(value.parameter)+":";
            for(auto field:value.path) out+=std::to_string(field)+",";
            return out+";";
        };
        std::string contract="accessor:"+std::to_string(f.accessor)+"|"+ir::capability_key(f.receiver ? f.receiver->type : Type::none)+"|";
        for(const auto& value:f.reference_effects.mutations) contract+="M"+origin(value);
        for(const auto& value:f.reference_effects.retained) contract+="H"+origin(value);
        for(const auto& value:f.reference_effects.required_mutable) contract+="Q"+origin(value);
        for(const auto& [path,fact]:f.reference_effects.returned) {
            contract+="R["; for(auto field:path) contract+=std::to_string(field)+","; contract+=fact.readonly ? "]C" : "]V";
            for(const auto& value:fact.origins) contract+=origin(value);
        }
        for(const auto& [path,fact]:f.reference_effects.receiver_after) {
            contract+="S["; for(auto field:path) contract+=std::to_string(field)+","; contract+=fact.readonly ? "]C" : "]V";
            for(const auto& value:fact.origins) contract+=origin(value);
        }
        f.capability_contract=std::move(contract)+(f.native_symbol.empty()?"":"|native:"+f.native_symbol);
    }
    for(const auto& [name,edges]:graph)for(auto* spawn:edges.spawns){
        std::set<std::string> seen;std::vector<std::string> pending{spawn->parts[0]->target};
        const auto& effects=functions.at(spawn->parts[0]->target)->reference_effects;
        bool unsafe=!effects.mutations.empty() || !effects.retained.empty() || !effects.required_mutable.empty();
        while(!pending.empty()){auto target=std::move(pending.back());pending.pop_back();if(!seen.insert(target).second)continue;
            auto found=graph.find(target);if(found==graph.end() || functions.at(target)->asynchronous){unsafe=true;break;}
            if(found->second.unsafe){unsafe=true;break;}pending.insert(pending.end(),found->second.calls.begin(),found->second.calls.end());
        }
        if(unsafe)try{error(function_units.at(name)->source,spawn->token,Code::unsupported_type,"Worker reachable calls must be synchronous and have known private effects; task operations and indirect/native calls are unsupported.");}catch(const CompilationError& error){diagnostics.add(error);}
    }

}
