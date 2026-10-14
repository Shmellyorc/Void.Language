// Declaration-only hierarchy checks precede body/effect analysis. The existing
// owner maps remain authoritative; inherited bodies are never duplicated.
void validate_inheritance(std::map<std::string,std::unique_ptr<Unit>>& units,Unit& root,Diagnostics& diagnostics) {
    std::map<std::string,std::pair<Unit*,Structure*>> structures;
    std::map<std::string,std::pair<Unit*,Function*>> methods;
    for (auto& [id,u]:units) {
        for (auto& s:u->structures) structures.emplace(s.definition->identity,std::pair{u.get(),&s});
        for (auto& f:u->functions) if (f.receiver) methods.emplace(f.symbol,std::pair{u.get(),&f});
    }
    auto inherited=[&](const Structure& s,const std::string& name)->std::pair<Unit*,Function*> {
        for (auto base=s.definition->base.lock();base;base=base->base.lock()) {
            auto found=methods.find(base->identity+"::method::"+name);
            if (found!=methods.end()) return found->second;
        }
        return {};
    };
    // Base declarations first regardless of source/import order.
    std::vector<Structure*> ordered;
    for (auto& [identity,entry]:structures) if (entry.second->layout_state==2) ordered.push_back(entry.second);
    auto depth=[](const Structure* s) { unsigned n=0; for(auto base=s->definition->base.lock();base;base=base->base.lock()) ++n; return n; };
    std::stable_sort(ordered.begin(),ordered.end(),[&](auto a,auto b){return depth(a)<depth(b);});
    for (auto* s:ordered) {
        auto& owner=*structures.at(s->definition->identity).first;
        try {
            for (const auto& field:s->fields) {
                if (auto previous=inherited(*s,field.name);previous.second)
                    error(owner.source,field.token,Code::duplicate,"A stored field cannot hide inherited method '"+field.name+"'.",{},{{span(previous.first->source,previous.second->token),"Inherited method declared here."}});
            }
            for (auto& f:owner.functions) if (f.receiver && f.receiver->type.definition==s->definition && !f.parse_failed) {
                auto previous=inherited(*s,f.token.text);
                if (f.initializer) {
                    if (f.virtual_method || f.overriding) error(owner.source,f.token,Code::initializer,"An initializer cannot be virtual or override.");
                    if (s->base_type) {
                        if (f.body.empty() || f.body.front().kind!=Statement::Kind::expression || !f.body.front().expression)
                            error(owner.source,f.token,Code::initializer,"A derived initializer must begin with super.init(...).");
                        auto& e=*f.body.front().expression;
                        if (e.kind!=Expr::Kind::method_call || e.token.text!="init" || e.parts.empty() || e.parts[0]->kind!=Expr::Kind::name || e.parts[0]->token.text!="super")
                            error(owner.source,f.body.front().token,Code::initializer,"A derived initializer must begin with super.init(...).");
                        e.parts.erase(e.parts.begin());
                        f.base_prelude=std::move(f.body.front().expression); f.body.erase(f.body.begin());
                    }
                    continue; // Initializers are not inherited members.
                }
                if (previous.second) {
                    if (previous.second->virtual_slot.empty() || !f.overriding || f.virtual_method)
                        error(owner.source,f.token,Code::receiver,"Inherited methods cannot be hidden; a virtual method requires override func.",{},{{span(previous.first->source,previous.second->token),"Inherited method declared here."}});
                    auto& p=*previous.second;
                    bool accessible=(previous.first==&owner || !p.file_private) &&
                        (previous.first==&owner || previous.first==&root || std::any_of(owner.imports.begin(),owner.imports.end(),[&](const auto& i){return i.target==previous.first->identity;}));
                    if (!accessible) error(owner.source,f.token,Code::visibility,"Override requires access to its original method owner.");
                    bool exact=f.result==p.result && f.visibility==p.visibility && f.parameters.size()==p.parameters.size();
                    if (exact) for (unsigned n=0;n<f.parameters.size();++n)
                        exact&=f.parameters[n].type==p.parameters[n].type && f.parameters[n].type.readonly==p.parameters[n].type.readonly;
                    if (!exact) error(owner.source,f.token,Code::receiver,"Override must match the inherited virtual signature and visibility exactly.",{},{{span(previous.first->source,p.token),"Virtual contract declared here."}});
                    f.virtual_slot=p.virtual_slot;
                } else {
                    if (f.overriding) error(owner.source,f.token,Code::receiver,"override requires an inherited virtual method; using components do not create inherited slots.");
                    if (f.virtual_method) f.virtual_slot=f.symbol;
                }
                if (!f.virtual_slot.empty() && (f.receiver->type.kind!=Type::managed_ref || f.file_private))
                    error(owner.source,f.token,Code::receiver,"Virtual methods require a public or internal managed receiver.");
            }
            if (!s->initializer) for (auto base=s->definition->base.lock();base;base=base->base.lock()) {
                const auto [base_owner,parent]=structures.at(base->identity);
                if (parent->initializer) error(owner.source,s->token,Code::initializer,"A type with an init-bearing ancestor must declare its own initializer beginning with super.init(...).",{},{{span(base_owner->source,parent->initializer->token),"Ancestor initializer declared here."}});
            }
        } catch (const CompilationError& failure) { diagnostics.add(failure); }
    }
    for (auto& [id,u]:units) for (auto& f:u->functions) if (!f.receiver && (f.virtual_method || f.overriding)) {
        try { error(u->source,f.token,Code::receiver,"virtual and override require a managed receiver method."); }
        catch (const CompilationError& failure) { diagnostics.add(failure); }
    }
}
