// Construction capability/readiness analysis on the already nominally typed AST.
// Contexts are finite parameter masks, never dynamic object identities.
void analyze_construction(std::map<std::string,std::unique_ptr<Unit>>& units, Diagnostics& diagnostics) {
    using Mask=std::set<unsigned>;
    using Ready=std::set<std::pair<unsigned,unsigned>>;
    struct Request { Unit* owner; Function* function; Mask mask; std::optional<std::pair<Unit*,Token>> caller; };
    std::map<std::string,std::pair<Unit*,Function*>> functions;
    std::vector<Request> pending;
    auto request=[&](Unit& owner,Function& f,const Mask& mask,std::optional<std::pair<Unit*,Token>> caller) {
        if (f.construction_contexts.contains(mask)) return;
        if (pending.size()>=1024) error(owner.source,f.token,Code::nesting,"Construction helper context limit exceeded.","Reduce distinct construction helper parameter combinations.");
        f.construction_contexts.emplace(mask,ConstructionContext{mask,{},{}});
        pending.push_back({&owner,&f,mask,std::move(caller)});
    };
    for (auto& [id,u]:units) for (auto& f:u->functions) {
        functions.emplace(f.symbol,std::pair{u.get(),&f});
        if (f.initializer && f.receiver->type.kind==Type::managed_ref) request(*u,f,{0},{});
    }
    for (std::size_t n=0;n<pending.size();++n) {
        auto work=pending[n]; auto& unit=*work.owner; auto& f=*work.function;
        auto& context=f.construction_contexts.at(work.mask);
        std::vector<Type> parameters;
        if (f.receiver) parameters.push_back(f.receiver->type);
        for (const auto& p:f.parameters) parameters.push_back(p.type);
        for (auto index:work.mask) context.aliases[index]=index;
        auto issue=[&](const Token& token,const std::string& message) -> void {
            if (work.caller) error(work.caller->first->source,work.caller->second,Code::initializer,"Unsafe construction helper: "+message,
                "Use field operations on the construction view, or call this helper after construction.",{{span(unit.source,token),"Prohibited use in this helper context."}});
            error(unit.source,token,Code::initializer,message,"Construction views cannot become ordinary references before successful initialization and commit.");
        };
        Ready initial;
        for (auto origin:work.mask) {
            const auto& fields=parameters.at(origin).definition->fields;
            for (unsigned field=0;field<fields.size();++field) {
                Type t=fields[field].type;
                if (!f.initializer || !ir::reference_value(t) || t.nullable) initial.emplace(origin,field);
            }
        }
        auto require_field=[&](unsigned origin,unsigned field,const Ready& ready,const Token& token) {
            if (!ready.contains({origin,field})) issue(token,"Field '"+parameters.at(origin).definition->fields.at(field).name+"' must be initialized before it is read.");
        };
        auto require_all=[&](unsigned origin,const Ready& ready,const Token& token) {
            for (unsigned field=0;field<parameters.at(origin).definition->fields.size();++field)
                if (!ready.contains({origin,field})) issue(token,"Construction requires initialized field '"+parameters.at(origin).definition->fields[field].name+"' on every successful exit and before whole-view helper calls.");
        };
        std::function<std::optional<unsigned>(const Expr&,const Ready&)> expression;
        expression=[&](const Expr& e,const Ready& ready)->std::optional<unsigned> {
            using K=Expr::Kind;
            if (e.kind==K::name) {
                if (auto i=context.aliases.find(e.slot);i!=context.aliases.end()) return i->second;
                return {};
            }
            if (e.kind==K::field) {
                if (auto origin=expression(*e.parts[0],ready)) require_field(*origin,e.slot,ready,e.token);
                return {}; // A field is a complete child reference or independent value.
            }
            if(e.kind==K::conversion && e.op=="upcast" && e.implicit) return expression(*e.parts[0],ready);
            if (e.kind==K::call || e.kind==K::method_call) {
                Mask mask;
                for (unsigned i=0;i<e.parts.size();++i) if(auto origin=expression(*e.parts[i],ready)) {
                    if(e.interface_call) issue(e.token,"Interface dispatch is prohibited on unfinished self or its aliases.");
                    if (!e.virtual_slot.empty()) issue(e.token,"Virtual-family calls are prohibited on unfinished self or its aliases, including direct super calls.");
                    for(unsigned field=0;field<e.parts[i]->type.definition->fields.size();++field) require_field(*origin,field,ready,e.token);
                    mask.insert(i);
                }
                if (!mask.empty()) {
                    auto target=functions.find(e.target);
                    if (target==functions.end()) issue(e.token,"A construction view cannot enter an opaque call.");
                    auto& callee=*target->second.second;
                    for (auto index:mask) {
                        auto first=callee.receiver ? 1u : 0u;
                        auto t=index<first ? callee.receiver->type : callee.parameters.at(index-first).type;
                        if (t.nullable) issue(e.parts[index]->token,"A construction view cannot be injected into a nullable reference.");
                    }
                    context.calls.emplace(&e,mask);
                    request(*target->second.first,callee,mask,std::pair{&unit,e.token});
                }
                return {};
            }
            for (const auto& part:e.parts) if (expression(*part,ready))
                issue(part->token,e.kind==K::construction ? "A construction view cannot be a constructor argument." : "Construction views cannot be cast, compared for identity or used as ordinary values.");
            for (const auto& child:e.omitted) expression(*child,ready);
            return {};
        };
        if (f.base_prelude) {
            for(const auto& argument:f.base_prelude->parts) if(expression(*argument,initial))
                issue(argument->token,"A super.init prelude argument cannot access unfinished self or its aliases.");
            // Field access through self is also forbidden here, even for safe
            // defaults: arguments precede all base/default preparation.
            std::vector<const Expr*> scan; for(const auto& e:f.base_prelude->parts) scan.push_back(e.get());
            while(!scan.empty()) {
                auto e=scan.back();scan.pop_back();
                if(e->kind==Expr::Kind::name && context.aliases.contains(e->slot)) issue(e->token,"super.init arguments cannot access self or its fields.");
                for(const auto& child:e->parts) scan.push_back(child.get());
            }
            for(unsigned field=0;field<f.base_prelude->type.definition->fields.size();++field) initial.emplace(0,field);
        }
        struct Flow { std::optional<Ready> next; std::vector<Ready> breaks,continues; };
        auto merge=[](std::optional<Ready> a,const std::optional<Ready>& b) {
            if (!a) return b;
            if (!b) return a;
            Ready into; std::set_intersection(a->begin(),a->end(),b->begin(),b->end(),std::inserter(into,into.end())); return std::optional{into};
        };
        std::function<Flow(const std::vector<Statement>&,Ready)> sequence;
        sequence=[&](const auto& body,Ready ready) {
            Flow result{ready,{},{}};
            for (const auto& s:body) {
                if (!result.next) break;
                auto current=*result.next;
                using K=Statement::Kind;
                if (s.kind==K::assignment) {
                    auto alias=context.aliases.find(s.slot);
                    if (alias!=context.aliases.end()) {
                        if (s.fields.empty()) issue(s.token,"A construction receiver or alias cannot be rebound.");
                        if (s.index || s.field_indexes.size()>1 || s.op!="=") require_field(alias->second,s.field_indexes.front(),current,s.token);
                    }
                    if (s.index && expression(*s.index,current)) issue(s.index->token,"A construction view cannot be an index.");
                    if (s.expression && expression(*s.expression,current)) issue(s.expression->token,"A construction view cannot be stored or published through assignment.");
                    if (alias!=context.aliases.end()) current.emplace(alias->second,s.field_indexes.front());
                } else if (s.kind==K::variable) {
                    if (s.expression) if (auto origin=expression(*s.expression,current)) {
                        if (f.locals.at(s.slot).nullable) issue(s.token,"A construction view cannot be injected into a nullable reference.");
                        context.aliases[s.slot]=*origin;
                    }
                } else {
                    if (s.expression && expression(*s.expression,current)) issue(s.expression->token,"A construction view cannot be returned or used as an ordinary value.");
                }
                if (s.kind==K::returned) {
                    if (f.initializer) require_all(0,current,s.token);
                    result.next.reset(); continue;
                }
                if (s.kind==K::broken || s.kind==K::continued) {
                    (s.kind==K::broken ? result.breaks : result.continues).push_back(current); result.next.reset(); continue;
                }
                if (s.kind==K::conditional) {
                    auto yes=sequence(s.body,current), no=sequence(s.otherwise,current);
                    result.next=merge(yes.next,no.next);
                    result.breaks.insert(result.breaks.end(),yes.breaks.begin(),yes.breaks.end()); result.breaks.insert(result.breaks.end(),no.breaks.begin(),no.breaks.end());
                    result.continues.insert(result.continues.end(),yes.continues.begin(),yes.continues.end()); result.continues.insert(result.continues.end(),no.continues.begin(),no.continues.end());
                } else if (s.kind==K::loop) {
                    auto inner=sequence(s.body,current);
                    result.next=always(s.expression.get()) ? std::optional<Ready>{} : std::optional{current};
                    for (const auto& path:inner.breaks) result.next=merge(result.next,path);
                } else result.next=current;
            }
            return result;
        };
        try {
            auto flow=sequence(f.body,initial);
            if (f.initializer && flow.next) require_all(0,*flow.next,f.token);
        } catch (const CompilationError& failure) { diagnostics.add(failure); }
    }
    for (auto& [id,u]:units) for (auto& f:u->functions) {
        for (const auto& [mask,context]:f.construction_contexts) {
            f.construction_contract+='[';
            for (auto p:mask) f.construction_contract+=std::to_string(p)+",";
            f.construction_contract+=']';
        }
        f.capability_contract+=';'+f.construction_contract;
    }
}
