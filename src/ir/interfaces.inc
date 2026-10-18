// Logical selection is shared with source diagnostics. Neither layout nor code
// generation decides which accessible member a structural requirement means.
bool method_accessible(const Module& m,const Function& f,std::string_view scope) {
    if(f.module_identity.empty()) return false;
    if(f.visibility=="private" && f.module_identity!=scope) return false;
    if(f.module_identity==scope || f.module_identity==m.root_identity) return true;
    auto found=m.source_imports.find(std::string(scope));
    return found!=m.source_imports.end() && found->second.contains(f.module_identity);
}
MemberSelection select_member(const Module& m,Type t,std::string_view name,std::string_view scope,std::size_t* work) {
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
        return {MemberSelection::found,{n},{},{}};}
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
    else if(result.candidates.size()==1) {result.status=MemberSelection::found;result.path=result.candidates[0].first;result.method=result.candidates[0].second;}
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
static Module::Binding structural_conformance(const Module& m,Type view,Type interface,std::string_view scope,std::size_t* work) {
    std::size_t local_work=8388608;if(!work) work=&local_work;
    auto fail=[](const std::string& text){throw std::invalid_argument("VOID interface: "+text);};
    if((view.kind!=Type::managed_ref && view.kind!=Type::aggregate && view.kind!=Type::interface_ref) || view.construction || !view.definition || view.nullable)
        fail("A constraint requires a complete non-nullable struct or interface reference");
    if(interface.kind!=Type::interface_ref || !interface.definition || !interface.definition->interface_type) fail("Invalid interface type");
    auto visible=[&](const ValueType& d) {
        if(d.visibility=="private" && d.module_identity!=scope) return false;
        if(d.module_identity==scope || d.module_identity==m.root_identity) return true;
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
