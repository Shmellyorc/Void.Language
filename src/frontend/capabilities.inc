// Effect/capability analysis over the already typed AST. No nominal typing or
// code generation is duplicated here. Joins are conservative and monotone.
struct ReferenceView {
    bool readonly = false;
    std::set<unsigned> origins;
    bool join(const ReferenceView& other) {
        bool changed=other.readonly && !readonly; readonly|=other.readonly;
        for (auto origin:other.origins) changed|=origins.insert(origin).second;
        return changed;
    }
};
void analyze_capabilities(std::map<std::string,std::unique_ptr<Unit>>& units, Diagnostics& diagnostics) {
    std::map<std::string,Function*> functions;
    for (auto& [id,u]:units) for (auto& f:u->functions) functions.emplace(f.symbol,&f);
    auto run=[&](Unit& unit,Function& f,bool report) {
        bool changed=false, local_changed=false;
        std::vector<ReferenceView> locals(f.locals.size());
        unsigned index=0;
        if (f.receiver) { locals[f.receiver->slot].origins.insert(index++); }
        for (const auto& p:f.parameters) {
            if (p.type.kind==Type::managed_ref) { locals[p.slot].origins.insert(index); locals[p.slot].readonly=p.type.readonly; }
            ++index;
        }
        auto issue=[&](const Token& token,const std::string& message) {
            if (!report) return;
            try { error(unit.source,token,Code::readonly,message,"Use an independently held mutable reference or construct a separate object; copying a readonly reference does not restore mutation."); }
            catch (const CompilationError& failure) { diagnostics.add(failure); }
        };
        auto effect=[&](std::set<unsigned>& into,const ReferenceView& view) {
            for (auto origin:view.origins) changed|=into.insert(origin).second;
        };
        std::function<ReferenceView(Expr&)> expression;
        auto call=[&](Function& target,const std::vector<ReferenceView>& arguments,const std::vector<Type>& types,const Token& token) {
            for (unsigned n=0;n<arguments.size();++n) {
                const auto first=target.receiver ? 1u : 0u;
                bool mutable_parameter=n>=first && n-first<target.parameters.size() && arguments[n].readonly && types[n].kind==Type::managed_ref && !target.parameters[n-first].type.readonly;
                if (mutable_parameter)
                    issue(token,"A mutable reference parameter cannot receive a readonly view; use a const parameter for readonly access.");
                if (target.mutations.contains(n)) { effect(f.mutations,arguments[n]); if (arguments[n].readonly && !mutable_parameter) issue(token,"A call may mutate a readonly reference or receiver."); }
                if (target.retained.contains(n)) { effect(f.retained,arguments[n]); if (arguments[n].readonly && types[n].kind==Type::managed_ref && !mutable_parameter && !target.mutations.contains(n)) issue(token,"A call may retain a readonly reference in mutable object storage."); }
            }
            ReferenceView result; result.readonly=target.returns_readonly;
            for (auto origin:target.returned_origins) if (origin<arguments.size()) result.join(arguments[origin]);
            return result;
        };
        expression=[&](Expr& e)->ReferenceView {
            ReferenceView view;
            using K=Expr::Kind;
            if (e.kind==K::name) view=locals.at(e.slot);
            else if (e.kind==K::field || e.kind==K::conversion) view=expression(*e.parts[0]);
            else if (e.kind==K::call || e.kind==K::method_call) {
                std::vector<ReferenceView> arguments; std::vector<Type> types;
                for (auto& part:e.parts) { arguments.push_back(expression(*part)); types.push_back(part->type); }
                if (auto target=functions.find(e.target);target!=functions.end()) view=call(*target->second,arguments,types,e.token);
                if (e.type.kind!=Type::managed_ref) view={}; // Ordinary value results copy.
            } else if (e.kind==K::construction) {
                std::vector<ReferenceView> arguments; std::vector<Type> types;
                if (!e.initializer.empty()) { arguments.resize(e.parts.size()+1); types.resize(e.parts.size()+1); types[0]=e.type; }
                for (unsigned n=0;n<e.parts.size();++n) {
                    auto child=expression(*e.parts[n]);
                    if (!e.initializer.empty()) { arguments[e.field_indexes[n]+1]=child; types[e.field_indexes[n]+1]=e.parts[n]->type; }
                    else if (e.type.kind==Type::managed_ref && e.parts[n]->type.kind==Type::managed_ref) {
                        effect(f.retained,child);
                        if (child.readonly) issue(e.parts[n]->token,"Cannot store a readonly reference in a mutable managed field.");
                    }
                }
                for (auto& child:e.omitted) expression(*child);
                if (!e.initializer.empty()) call(*functions.at(e.initializer),arguments,types,e.token);
            } else for (auto& child:e.parts) expression(*child);
            if (e.type.kind==Type::managed_ref) e.type.readonly=view.readonly;
            return view;
        };
        std::function<void(std::vector<Statement>&)> sequence;
        sequence=[&](auto& body) {
            for (auto& s:body) {
                using K=Statement::Kind;
                ReferenceView rhs;
                if (s.expression) rhs=expression(*s.expression);
                if (s.kind==K::variable || (s.kind==K::assignment && s.fields.empty())) {
                    auto t=f.locals.at(s.slot);
                    if (t.kind!=Type::managed_ref) rhs={}; // Independent reference-free value copy.
                    if (s.kind==K::variable && s.readonly) rhs.readonly=true;
                    if (s.kind==K::assignment && t.kind==Type::managed_ref) {
                        if (rhs.readonly && !locals[s.slot].readonly) issue(s.token,"Cannot assign a readonly view to an existing mutable-reference binding.");
                        // A binding's inferred capability does not change on
                        // reassignment or retroactively qualify earlier aliases.
                        rhs.readonly=locals[s.slot].readonly;
                    }
                    local_changed|=locals[s.slot].join(rhs);
                } else if (s.kind==K::assignment) {
                    auto base=locals.at(s.slot); auto t=f.locals.at(s.slot);
                    if (t.kind==Type::managed_ref || (f.receiver && s.slot==f.receiver->slot)) {
                        effect(f.mutations,base);
                        if (base.readonly) issue(s.token,"Cannot write through a readonly object view.");
                    }
                    for (auto field:s.field_indexes) t=t.definition->fields.at(field).type;
                    if (t.kind==Type::managed_ref) {
                        effect(f.retained,rhs);
                        if (rhs.readonly) issue(s.expression->token,"Cannot store a readonly reference in a mutable managed field.");
                    }
                } else if (s.kind==K::returned && f.result.kind==Type::managed_ref) {
                    effect(f.returned_origins,rhs);
                    if (rhs.readonly && !f.returns_readonly) { f.returns_readonly=true; changed=true; }
                }
                sequence(s.body); sequence(s.otherwise);
            }
        };
        do { local_changed=false; sequence(f.body); } while (local_changed);
        if (f.receiver && (f.mutations.contains(0) || f.retained.contains(0)) && !f.receiver_mutates) { f.receiver_mutates=true; changed=true; }
        if (report) for (unsigned n=0;n<locals.size();++n) if (f.locals[n].kind==Type::managed_ref) f.locals[n].readonly=locals[n].readonly;
        return changed;
    };
    bool changed;
    do {
        changed=false;
        for (auto& [id,u]:units) for (auto& f:u->functions) changed|=run(*u,f,false);
    } while (changed);
    for (auto& [id,u]:units) for (auto& f:u->functions) {
        run(*u,f,true);
        auto record=[&](const auto& values) { std::string out; for(auto n:values) out+=std::to_string(n)+","; return out; };
        f.capability_contract=record(f.mutations)+";"+record(f.retained)+";"+record(f.returned_origins)+";"+(f.returns_readonly ? "readonly" : "mutable");
    }
}
