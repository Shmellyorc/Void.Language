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
        bool changed=false,local_changed=false;
        std::vector<ReferenceFacts> locals(f.locals.size());
        std::map<const Expr*,ReferenceFacts> evaluated;
        unsigned index=0;
        if (f.receiver) locals[f.receiver->slot]=ir::formal_facts(f.receiver->type,index++);
        for (const auto& p:f.parameters) locals[p.slot]=ir::formal_facts(p.type,index++);
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
                effect(f.reference_effects.retained,fact);
                if (report && fact.readonly) issue(token,"Cannot store readonly reference leaf '"+ir::reference_path_name(type,path)+"' in mutable managed field storage.",leaf_related(type,path));
            }
        };
        auto compatible=[&](const ReferenceFacts& actual,Type expected,const Token& token,const ReferenceFacts* capacity=nullptr,std::vector<RelatedLocation> related = {}) {
            if (requirements) for (const auto& origin:ir::mutable_requirements(actual,expected,capacity))
                changed|=f.reference_effects.required_mutable.insert(origin).second;
            if (!report) return;
            const auto allowed=ir::readonly_paths(expected);
            if(!ir::capability_assignable(ir::fact_type(expected,actual),expected)) {
                bool readonly_failure=false;for(const auto& [path,fact]:actual)readonly_failure|=fact.readonly && !allowed.contains(path);
                if(!readonly_failure)issue(token,"Mutable list storage requires an invariant element authority profile.",related);
            }
            for (const auto& [path,fact]:actual) if(fact.readonly && !allowed.contains(path)) {
                auto fields=leaf_related(expected,path);fields.insert(fields.end(),related.begin(),related.end());
                issue(token,"Cannot assign readonly reference leaf '"+ir::reference_path_name(expected,path)+"' to storage or a parameter promising mutable access.",std::move(fields));
                break;
            }
        };
        auto call=[&](Function& target,const std::vector<ReferenceFacts>& arguments,const Token& token,bool dynamic=false) {
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
        std::function<ReferenceFacts(Expr&)> expression;
        expression=[&](Expr& e)->ReferenceFacts {
            using K=Expr::Kind;
            auto view=ir::reference_facts(e.type);
            if(e.kind==K::array_literal || e.kind==K::array_default) {
                for(auto& part:e.parts) {
                    auto child=expression(*part);
                    for(const auto& [leaf,fact]:child) {auto path=leaf;path.insert(path.begin(),ir::array_element_path);view[path].join(fact);}
                }
            } else if(e.kind==K::array_index) {
                auto owner=expression(*e.parts[0]);expression(*e.parts[1]);view=ir::field_facts(e.parts[0]->type,owner,ir::array_element_path);
            } else if (e.kind==K::name) view=locals.at(e.slot);
            else if (e.kind==K::field) {
                auto base=expression(*e.parts[0]);
                if(e.target!="$array.count")view=ir::field_facts(e.parts[0]->type,base,e.slot);
            } else if (e.kind==K::conversion) view=expression(*e.parts[0]);
            else if (e.kind==K::call || e.kind==K::method_call) {
                std::vector<ReferenceFacts> arguments;
                for (auto& part:e.parts) arguments.push_back(expression(*part));
                if(e.target.starts_with("$array.")) {
                    if(e.token.text=="push")compatible(arguments[1],ir::array_element(e.parts[0]->type),e.token);
                    else view=ir::field_facts(e.parts[0]->type,arguments[0],ir::array_element_path);
                    if(auto leaf=arguments[0].find({});leaf!=arguments[0].end()) {
                        effect(f.reference_effects.mutations,leaf->second);
                        if(report && leaf->second.readonly)issue(e.token,"Cannot mutate a readonly list.");
                    }
                }
                if (auto found=functions.find(e.target);found!=functions.end()) {
                    auto& target=*found->second;
                    view=call(target,arguments,e.token,!e.virtual_slot.empty() && !e.super_call);
                    if (e.kind==K::method_call && target.receiver->type.kind==Type::aggregate && e.receiver_addressable) {
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
                    if (e.kind==K::method_call && target.receiver_mutates) {
                        // A by-place inline write can be a heap mutation even
                        // when the selected value contains no reference leaves.
                        // Keep the selected owner's authority distinct from the
                        // copied value's embedded-reference capabilities.
                        const Expr* selected=e.parts[0].get();
                        while (selected->kind==K::field) {
                            const auto* owner=selected->parts[0].get();
                            if (owner->type.kind==Type::managed_ref) {
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
                        bool inline_place=f.receiver && e.receiver_addressable && e.slot==f.receiver->slot;
                        for (auto field:e.field_indexes) {
                            inline_place&=type.kind==Type::aggregate;
                            if (type.kind==Type::aggregate || type.kind==Type::managed_ref) type=ir::field_type(type,field);
                        }
                        if (inline_place && !f.receiver_mutates) { f.receiver_mutates=true; changed=true; }
                    }
                }
            } else if (e.kind==K::construction || e.kind==K::base_initialization) {
                view=ir::reference_facts(e.type);
                if (e.initializer.empty()) {
                    for (unsigned n=0;n<e.parts.size();++n) {
                        auto child=expression(*e.parts[n]);
                        if (e.type.kind==Type::managed_ref) retain(child,e.parts[n]->type,e.parts[n]->token);
                        else view=ir::replaced_facts(std::move(view),e.field_indexes[n],child);
                    }
                    for (auto& child:e.omitted) {
                        auto facts=expression(*child);
                        if (e.type.kind==Type::managed_ref) retain(facts,child->type,child->token);
                        else view=ir::replaced_facts(std::move(view),child->slot,facts);
                    }
                } else {
                    auto& target=*functions.at(e.initializer);
                    std::vector<ReferenceFacts> arguments(e.parts.size()+1);
                    arguments[0]=ir::reference_facts(target.receiver->type);
                    for (unsigned n=0;n<e.parts.size();++n) arguments[e.field_indexes[n]+1]=expression(*e.parts[n]);
                    for (auto& child:e.omitted) {
                        auto facts=expression(*child);
                        if (e.type.kind==Type::managed_ref) retain(facts,child->type,child->token);
                        else compatible(facts,ir::field_type(target.receiver->type,child->slot),child->token);
                    }
                    call(target,arguments,e.token);
                    if (e.type.kind==Type::aggregate) {
                        view=ir::instantiate_result(target.reference_effects.receiver_after,arguments);
                        ir::join_facts(view,ir::reference_facts(target.receiver->type));
                    }
                }
            } else for (auto& child:e.parts) expression(*child);
            // Null/default values still have reference authority. An absent
            // provenance origin must never erase their logical reference leaf.
            for (const auto& path:ir::reference_paths(e.type)) view.try_emplace(path);
            e.type=ir::fact_type(e.type,view);
            evaluated[&e]=view;
            return view;
        };
        std::function<void(std::vector<Statement>&)> sequence;
        sequence=[&](auto& body) {
            for (auto& s:body) {
                using K=Statement::Kind;
                ReferenceFacts rhs;
                if (s.selection)expression(*s.selection);
                if (s.range_end) expression(*s.range_end);
                if (s.index) expression(*s.index);
                if (s.expression) rhs=expression(*s.expression);
                else if (s.kind==K::variable) rhs=ir::reference_facts(f.locals.at(s.slot));
                if (s.kind==K::variable) {
                    if (s.readonly) for (auto& [path,fact]:rhs) fact.readonly=true;
                    local_changed|=ir::join_facts(locals[s.slot],rhs);
                } else if(s.kind==K::assignment && s.selection) {
                    compatible(rhs,s.selection->type,s.expression->token);
                    bool heap=false;ir::ReferencePath path;
                    std::function<void(const Expr&)> inspect=[&](const Expr& e) {
                        if(e.kind==Expr::Kind::name)return;
                        const auto& owner=*e.parts[0];inspect(owner);
                        if(owner.type.kind==Type::managed_ref || (e.kind==Expr::Kind::array_index && owner.type.kind==Type::growable_array)) {
                            heap=true;if(auto leaf=evaluated.at(&owner).find({});leaf!=evaluated.at(&owner).end()) {effect(f.reference_effects.mutations,leaf->second);if(report && leaf->second.readonly)issue(e.token,"Cannot mutate storage through readonly array/reference authority.");}
                        }
                        path.push_back(e.kind==Expr::Kind::array_index ? ir::array_element_path : e.slot);
                    };inspect(*s.selection);
                    if(!heap) {ir::ReferenceFacts inserted;for(const auto& [leaf,fact]:rhs) {auto target=path;target.insert(target.end(),leaf.begin(),leaf.end());inserted[target]=fact;}local_changed|=ir::join_facts(locals[s.slot],inserted);if(f.receiver && s.slot==f.receiver->slot && !f.receiver_mutates){f.receiver_mutates=true;changed=true;}}
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
                    if(s.index && ir::array_type(type)) {
                        if(auto leaf=selected.find({});leaf!=selected.end()) {effect(f.reference_effects.mutations,leaf->second);if(report && leaf->second.readonly)issue(s.token,"Cannot mutate a readonly list.");}
                        compatible(rhs,ir::array_element(type),s.expression->token);
                        if(type.kind==Type::fixed_array && !heap) {
                            ir::ReferenceFacts prefixed;
                            for(const auto& [leaf,fact]:rhs) {auto path=s.field_indexes;path.push_back(ir::array_element_path);path.insert(path.end(),leaf.begin(),leaf.end());prefixed[path]=fact;}
                            local_changed|=ir::join_facts(locals[s.slot],prefixed);
                            if(f.receiver && s.slot==f.receiver->slot && !f.receiver_mutates){f.receiver_mutates=true;changed=true;}
                        }
                    }
                    else if (heap) retain(rhs,s.expression->type,s.expression->token);
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
                    f.locals[s.slot]=ir::fact_type(f.locals[s.slot],item);local_changed|=ir::join_facts(locals[s.slot],item);
                }
                sequence(s.body); sequence(s.otherwise);
            }
        };
        if(f.base_prelude) expression(*f.base_prelude);
        for (auto& child:f.initialization_defaults) {
            auto facts=expression(*child);
            if (f.receiver->type.kind==Type::managed_ref) retain(facts,child->type,child->token);
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
        if(f.interface_requirement) {f.result=ir::fact_type(f.result,f.reference_effects.returned);f.receiver_mutates=std::any_of(f.reference_effects.mutations.begin(),f.reference_effects.mutations.end(),[](const auto& origin){return origin.parameter==0;});}
        auto origin=[](const ReferenceOrigin& value) {
            std::string out=std::to_string(value.parameter)+":";
            for(auto field:value.path) out+=std::to_string(field)+",";
            return out+";";
        };
        std::string contract=ir::capability_key(f.receiver ? f.receiver->type : Type::none)+"|";
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
        f.capability_contract=std::move(contract);
    }
}
