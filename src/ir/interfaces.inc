// Logical selection is shared with source diagnostics. Neither layout nor code
// generation decides which accessible member a structural requirement means.
bool method_accessible(const Module& m,const Function& f,std::string_view scope) {
    if(f.module_identity.empty()) return false;
    if(f.visibility=="private" && f.module_identity!=scope) return false;
    if(f.visibility=="internal" && f.module_identity.starts_with("/@stdlib/")!=scope.starts_with("/@stdlib/"))return false;
    if(f.module_identity==scope || (f.module_identity==m.root_identity && scope.starts_with("/@stdlib/")==m.root_identity.starts_with("/@stdlib/"))) return true;
    auto found=m.source_imports.find(std::string(scope));
    return found!=m.source_imports.end() && found->second.contains(f.module_identity);
}
bool field_accessible(const Module& m,Type container,unsigned index,std::string_view scope,std::string_view lexical_owner) {
    const auto& field=container.definition->fields.at(index);
    if(field.visibility=="private")return !field.owner.empty() && field.owner==lexical_owner;
    if(field.visibility=="internal") {
        const ValueType* owner=container.definition.get();
        for(const auto& t:m.nominal_types)if(t.definition && t.definition->identity==field.owner){owner=t.definition.get();break;}
        return owner->module_identity.starts_with("/@stdlib/")==scope.starts_with("/@stdlib/");
    }
    return field.visibility=="public";
}
MemberSelection select_member(const Module& m,Type t,std::string_view name,std::string_view scope,std::size_t* work,std::string_view lexical_owner) {
    if(!t.definition || t.nullable)return {};
    if(t.kind==Type::interface_ref) {
        for(const auto& f:m.interface_requirements)if(f.method_name==name && !f.parameters.empty() && f.parameters[0].definition==t.definition)
            return {method_accessible(m,f,scope)?MemberSelection::found:MemberSelection::inaccessible,{},f.name,{}};
        return {};
    }
    if(t.kind!=Type::aggregate && t.kind!=Type::managed_ref)return {};
    auto charge=[&] {if(work) {if(!*work) throw std::length_error("Interface member-selection work budget of 8388608 visits exceeded");--*work;}};
    auto method=[&](const std::shared_ptr<const ValueType>& d)->const Function* {
        for(const auto& f:m.functions) {charge();if(f.receiver && f.method_name==name && !f.initializer &&
            !f.parameters.empty() && !f.parameters[0].construction && f.parameters[0].definition && f.parameters[0].definition->identity==d->identity) return &f;}
        return nullptr;
    };
    for(unsigned n=0;n<t.definition->fields.size();++n) {charge();if(t.definition->fields[n].name==name)
        return {field_accessible(m,t,n,scope,lexical_owner)?MemberSelection::found:MemberSelection::inaccessible,{n},{},{}};}
    for(auto d=t.definition;d;d=d->base.lock()) if(auto f=method(d))
        return {method_accessible(m,*f,scope) ? MemberSelection::found : MemberSelection::inaccessible,{},f->name,{}};
    MemberSelection result; bool hidden=false;
    for(unsigned n=0;n<t.definition->fields.size();++n) {charge();if(!t.definition->fields[n].using_field) continue;
        Type child=t.definition->fields[n].type;
        if(!child.definition) continue;
        auto base=child.definition->base.lock();
        bool field=false;
        for(unsigned k=base ? static_cast<unsigned>(base->fields.size()) : 0;k<child.definition->fields.size();++k)
            {charge();if(child.definition->fields[k].name==name) {result.candidates.push_back({{n,k},{}});field=true;break;}}
        if(!field) if(auto f=method(child.definition)) {
            if(!method_accessible(m,*f,scope)) hidden=true;
            else result.candidates.push_back({{n},f->name});
        }
    }
    if(result.candidates.size()>1) result.status=MemberSelection::ambiguous;
    else if(result.candidates.size()==1) {
        result.status=MemberSelection::found;result.path=result.candidates[0].first;result.method=result.candidates[0].second;
        auto owner=t;
        for(auto field:result.path){if(!field_accessible(m,owner,field,scope,lexical_owner))result.status=MemberSelection::inaccessible;owner=field_type(owner,field);}
    }
    else if(hidden) result.status=MemberSelection::inaccessible;
    return result;
}
std::string binding_key(const Module::Binding& binding) {
    auto encode=[](std::string_view text){return std::to_string(text.size())+":"+std::string(text);};
    std::string result="interface:"+encode(Type::key(binding.interface))+encode(Type::key(binding.view));
    for(const auto& [name,target]:binding.methods) {
        result+=encode(name)+std::to_string(target.first.size())+":";
        for(auto field:target.first) result+=std::to_string(field)+",";
        result+=encode(target.second);
    }
    return result;
}
bool binding_requires_readonly(const Module::Binding& binding) {
    for(const auto& [name,route]:binding.methods) {
        auto owner=binding.view;
        for(auto index:route.first) {
            if(owner.definition->fields.at(index).readonly)return true;
            owner=field_type(owner,index);
            if(owner.readonly || !readonly_paths(owner).empty())return true;
        }
    }
    return false;
}
static Module::Binding structural_conformance(const Module& m,Type view,Type interface,std::string_view scope,std::size_t* work) {
    std::size_t local_work=8388608;if(!work) work=&local_work;
    auto fail=[](const std::string& text){throw std::invalid_argument("VOID interface: "+text);};
    if((view.kind!=Type::managed_ref && view.kind!=Type::aggregate && view.kind!=Type::interface_ref) || view.construction || !view.definition || view.nullable)
        fail("A constraint requires a complete non-nullable struct or interface reference");
    if(interface.kind!=Type::interface_ref || !interface.definition || !interface.definition->interface_type) fail("Invalid interface type");
    auto visible=[&](const ValueType& d) {
        if(d.visibility=="private" && d.module_identity!=scope) return false;
        if(d.visibility=="internal" && d.module_identity.starts_with("/@stdlib/")!=scope.starts_with("/@stdlib/"))return false;
        if(d.module_identity==scope || (d.module_identity==m.root_identity && scope.starts_with("/@stdlib/")==m.root_identity.starts_with("/@stdlib/"))) return true;
        auto imports=m.source_imports.find(std::string(scope));
        return imports!=m.source_imports.end() && imports->second.contains(d.module_identity);
    };
    if(!m.source_imports.contains(std::string(scope)) || !visible(*interface.definition)) fail("Interface is not accessible in the binding scope");
    Module::Binding binding;binding.view=view;binding.view.readonly=false;binding.interface=interface;
    binding.interface.nullable=false;binding.interface.readonly=false;
    for(const auto& requirement:m.interface_requirements) {
        if(requirement.parameters.empty() || requirement.parameters[0].definition!=interface.definition) continue;
        auto selected=select_member(m,view,requirement.method_name,scope,work);
        if(selected.status==MemberSelection::ambiguous) fail("Ambiguous promoted requirement '"+requirement.method_name+"'; qualify the component; signatures never disambiguate");
        if(selected.status==MemberSelection::inaccessible) fail("Requirement '"+requirement.method_name+"' is inaccessible; import its original owner");
        if(selected.status!=MemberSelection::found) fail("Missing requirement '"+requirement.method_name+"'");
        if(selected.method.empty()) fail("A field cannot satisfy callable requirement '"+requirement.method_name+"'");
        const Function* target=nullptr;
        for(const auto* family:{&m.functions,&m.interface_requirements})for(const auto& f:*family)if(f.name==selected.method)target=&f;
        if(!target) fail("Missing binding method header");
        if(target->asynchronous)fail("An async method cannot satisfy a synchronous interface requirement");
        if(target->accessor)fail("A property cannot satisfy a method requirement");
        bool exact=target->parameters.size()==requirement.parameters.size() && target->result==requirement.result;
        for(unsigned n=1;exact && n<requirement.parameters.size();++n)
            exact=target->parameters[n]==requirement.parameters[n] && target->parameters[n].readonly==requirement.parameters[n].readonly && readonly_paths(target->parameters[n])==readonly_paths(requirement.parameters[n]);
        if(!exact) fail("Exact signature mismatch for requirement '"+requirement.method_name+"'");
        binding.methods.emplace(requirement.name,std::pair{selected.path,selected.method});
    }
    if(binding.methods.empty()) fail("An interface requires at least one method");
    binding.key=binding_key(binding);return binding;
}

Module::Binding structural_binding(const Module& m,Type view,Type interface,std::string_view scope,std::size_t* work) {
    if(view.kind!=Type::managed_ref || view.construction || !view.definition || !view.definition->managed || view.nullable)
        throw std::invalid_argument("VOID interface: Only complete non-nullable managed views support interface erasure; standalone value erasure is deferred");
    return structural_conformance(m,view,interface,scope,work);
}
void satisfy_constraint(const Module& m,Type view,Type interface,std::string_view scope) {
    (void)structural_conformance(m,view,interface,scope,nullptr);
}

std::vector<Module::Binding> interface_universe(const Module& m) {
    std::map<std::string,Module::Binding> bindings;std::size_t attempts=0,work=8388608;
    for(auto view:m.nominal_types) if(view.kind==Type::managed_ref)
        for(auto interface:m.nominal_types) if(interface.kind==Type::interface_ref)
            for(const auto& [scope,imports]:m.source_imports) {
                if(++attempts>65536) throw std::length_error("Interface binding analysis budget of 65536 scope/view/interface combinations exceeded");
                try {auto binding=structural_binding(m,view,interface,scope,&work);bindings.emplace(binding.key,std::move(binding));}
                catch(const std::invalid_argument&) {} // Non-providers are valid declarations.
            }
    std::vector<Module::Binding> result;for(auto& [key,binding]:bindings) result.push_back(std::move(binding));return result;
}

// Operators share ordinary nominal declarations and access scopes. Selection
// examines only the static operand owners, with no ancestry/promotion or ranking.
std::string generic_identity(std::string_view declaration,const std::vector<Type>& arguments) {
    std::string result=std::string(declaration)+"<";
    for(auto type:arguments) {
        auto key=std::to_string(static_cast<unsigned>(type.kind))+":"+Type::key(type)+(type.nullable?"?":"");
        result+=std::to_string(key.size())+":"+key;
    }
    return result+">";
}
std::string generic_instance_identity(const Module& m,const Module::GenericInstance& proof,const Function* function) {
    auto declaration=m.generic_declarations.find(proof.declaration);
    if(declaration==m.generic_declarations.end() || proof.arguments.size()!=declaration->second.arity)
        throw std::invalid_argument("Invalid generic declaration/argument provenance.");
    auto base=proof.declaration;
    if(function && function->receiver) {
        const auto separator=base.rfind("::method::");
        if(separator==std::string::npos || function->parameters.empty())
            throw std::invalid_argument("Generic method has no declaring receiver.");
        const auto owner=base.substr(0,separator), receiver=Type::key(function->parameters[0]);
        if(owner!=receiver) {
            auto bound=m.generic_instances.find(receiver);
            auto nominal=m.generic_declarations.find(owner);
            if(bound==m.generic_instances.end() || nominal==m.generic_declarations.end() ||
               !nominal->second.nominal || bound->second.declaration!=owner ||
               bound->second.arguments.size()!=nominal->second.arity ||
               generic_identity(owner,bound->second.arguments)!=receiver ||
               nominal->second.module_identity!=declaration->second.module_identity)
                throw std::invalid_argument("Generic method has no matching nominal specialization.");
        }
        base=receiver+base.substr(separator);
    }
    auto identity=generic_identity(base,proof.arguments);
    if(function && !function->initializer) {
        std::vector<unsigned> context;
        for(unsigned n=0;n<function->parameters.size();++n)if(function->parameters[n].construction)context.push_back(n);
        identity=construction_name(identity,context);
    }
    return identity;
}
std::vector<Type> generic_operator_arguments(const Module& m,const Function& function) {
    std::vector<Type> result;
    auto instance=[&](const std::string& identity) {
        auto found=m.generic_instances.find(identity);
        if(found==m.generic_instances.end())return;
        const auto& proof=found->second;
        auto concrete=std::find_if(m.functions.begin(),m.functions.end(),[&](const auto& f){return f.name==identity;});
        if(generic_instance_identity(m,proof,concrete==m.functions.end()?nullptr:&*concrete)!=identity)
            throw std::invalid_argument("Invalid generic operator specialization provenance.");
        result.insert(result.end(),proof.arguments.begin(),proof.arguments.end());
    };
    const auto* current=&function;std::set<std::string> seen;
    for(;;) {
        if(!seen.insert(current->name).second)throw std::invalid_argument("Cyclic generic operator lexical provenance.");
        instance(current->name);
        if(current->receiver && !current->parameters.empty())instance(Type::key(current->parameters[0]));
        if(current->lexical_parent.empty())break;
        auto parent=std::find_if(m.functions.begin(),m.functions.end(),[&](const auto& f){return f.name==current->lexical_parent;});
        if(!current->closure || parent==m.functions.end() || current->module_identity!=parent->module_identity ||
           !current->name.starts_with(parent->name+"::$lambda$"))
            throw std::invalid_argument("Invalid generic operator lexical provenance.");
        bool created=false;
        for(const auto& block:parent->blocks)for(const auto& instruction:block.instructions)
            created|=instruction.op==Op::function_reference && instruction.callee==current->name;
        if(!created)throw std::invalid_argument("Generic operator closure has no lexical creation.");
        current=&*parent;
    }
    return result;
}
MemberSelection select_operator(const Module& m,std::string_view glyph,const std::vector<Type>& operands,std::string_view scope,std::string_view lexical_owner,const std::vector<Type>& generic_arguments) {
    MemberSelection result;std::set<std::string> seen;std::vector<std::pair<std::vector<unsigned>,std::string>> hidden;
    if(std::none_of(operands.begin(),operands.end(),[](auto t){return t.definition && (t.kind==Type::aggregate || t.kind==Type::managed_ref);}))return result;
    for(const auto& f:m.functions) {
        if(f.operator_glyph!=glyph || f.parameters.size()!=operands.size() || !f.operator_owner.definition)continue;
        bool direct=false,exact=true;
        for(unsigned n=0;n<operands.size();++n){const auto t=operands[n];direct |= (t.kind==Type::aggregate || t.kind==Type::managed_ref) && Type::key(t)==Type::key(f.operator_owner);exact &= t==f.parameters[n];}
        if(!direct || !exact || !seen.insert(f.name).second)continue;
        const auto& owner=*f.operator_owner.definition;
        const bool same_domain=scope.starts_with("/@stdlib/")==owner.module_identity.starts_with("/@stdlib/");
        bool accessible=owner.visibility=="public" || (owner.visibility=="internal" && same_domain) || (owner.visibility=="private" && scope==owner.module_identity);
        Function visibility;visibility.module_identity=f.module_identity;visibility.visibility="public"; // Private operators use lexical owner, not file-method policy.
        accessible &= method_accessible(m,visibility,scope) && (f.visibility=="public" || (f.visibility=="internal" && same_domain) || (f.visibility=="private" && lexical_owner==Type::key(f.operator_owner)));
        if(!accessible && owner.visibility=="public" && f.visibility=="public")
            accessible=std::any_of(generic_arguments.begin(),generic_arguments.end(),[&](auto type) {
                return type.definition && (type.kind==Type::aggregate || type.kind==Type::managed_ref) &&
                       Type::key(type)==Type::key(f.operator_owner);
            });
        if(!accessible){hidden.push_back({{},f.name});continue;}
        for(unsigned n=0;n<operands.size();++n)if(!capability_assignable(operands[n],f.parameters[n]))throw std::invalid_argument("Operator operands cannot discard readonly capabilities.");
        result.candidates.push_back({{},f.name});
    }
    if(result.candidates.size()>1)result.status=MemberSelection::ambiguous;
    else if(result.candidates.size()==1){result.status=MemberSelection::found;result.method=result.candidates[0].second;}
    else if(!hidden.empty()){result.status=MemberSelection::inaccessible;result.candidates=std::move(hidden);}
    return result;
}
