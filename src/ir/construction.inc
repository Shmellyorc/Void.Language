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
    if (f.construction_origins.empty() && owned.empty() && std::none_of(f.parameters.begin(),f.parameters.end(),[](auto t){return t.construction;}) && std::none_of(f.locals.begin(),f.locals.end(),[](auto t){return t.construction;})) return;
    for (unsigned parameter=0;parameter<f.parameters.size();++parameter) if (f.parameters[parameter].construction) {
        if (parameter>=f.locals.size() || f.locals[parameter]!=f.parameters[parameter] || !f.construction_origins.contains(parameter) || f.construction_origins.at(parameter)!=parameter || owned.contains(parameter)) fail("invalid construction formal storage");
    }
    for (auto owner:owned) if (!f.construction_origins.contains(owner) || f.construction_origins.at(owner)!=owner) fail("staging owner has a different origin");
    for (unsigned slot=0;slot<f.locals.size();++slot)
        if (f.locals[slot].construction!=f.construction_origins.contains(slot)) fail("missing construction origin metadata");
    for (auto [slot,origin]:f.construction_origins) {
        if (slot>=f.locals.size() || origin>=f.locals.size() || !f.locals[origin].construction || !f.construction_origins.contains(origin) || f.construction_origins.at(origin)!=origin || Type::key(f.locals[slot])!=Type::key(f.locals[origin])) fail("invalid construction alias origin");
        if (!owned.contains(origin) && (origin>=f.parameters.size() || !f.parameters[origin].construction)) fail("unowned construction storage");
    }
    enum Phase { inactive=1, preparing=2, initialized=4, committed=8 };
    struct Region { unsigned phase=inactive; std::set<unsigned> ready; bool operator==(const Region&) const=default; };
    struct State { std::map<unsigned,Region> regions; std::set<unsigned> bound; bool operator==(const State&) const=default; };
    State entry;
    auto fields=[&](unsigned root)->const auto& { return f.locals.at(root).definition->fields; };
    for (auto [slot,origin]:f.construction_origins) {
        auto& region=entry.regions[origin];
        if (owned.contains(origin)) continue;
        region.phase=f.initializer ? preparing : initialized;
        for (unsigned n=0;n<fields(origin).size();++n) {
            Type t=fields(origin)[n].type;
            if (!f.initializer || t.kind!=Type::managed_ref || t.nullable) region.ready.insert(n);
        }
    }
    if (f.receiver && f.parameters[0].construction) entry.bound.insert(0);
    auto intersection=[](auto a,const auto& b) { std::erase_if(a,[&](const auto& value){return !b.contains(value);}); return a; };
    auto join=[&](State a,const State& b) {
        a.bound=intersection(a.bound,b.bound);
        for (auto& [root,region]:a.regions) { region.phase|=b.regions.at(root).phase; region.ready=intersection(region.ready,b.regions.at(root).ready); }
        return a;
    };
    std::vector<std::optional<State>> inputs(f.blocks.size()); inputs[0]=entry;
    std::vector<unsigned> pending{0};
    for (std::size_t next=0;next<pending.size();++next) {
        auto block=pending[next]; auto state=*inputs[block];
        std::vector<std::optional<unsigned>> origins;
        auto active=[&](unsigned root) {
            if (state.regions.at(root).phase & (inactive|committed)) fail("construction view used outside its active scope");
        };
        auto require_all=[&](unsigned root) {
            active(root);
            if (state.regions.at(root).ready.size()!=fields(root).size()) fail("uninitialized construction field at call/exit/commit");
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
            if (i.op==Op::stage_begin) {
                if (f.construction_origins.at(i.a)!=i.a || state.regions.at(i.a).phase & (preparing|initialized)) fail("staging restarted before commit");
                state.regions.at(i.a)={preparing,{}};
                std::erase_if(state.bound,[&](auto slot){return f.construction_origins.at(slot)==i.a;}); origin=i.a;
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
                if (i.op==Op::gc_read && !state.regions.at(root).ready.contains(field)) fail("uninitialized construction field read");
                if (i.op==Op::gc_write) {
                    if (owned.contains(root) && state.regions.at(root).phase!=preparing) fail("construction fields changed after initializer completion");
                    state.regions.at(root).ready.insert(field);
                }
            } else if (i.op==Op::call) {
                auto found=functions.find(i.callee);
                if (found!=functions.end()) {
                    const auto& target=*found->second;
                    std::vector<unsigned> roots;
                    if (i.receiver && f.locals.at(i.receiver->slot).construction) {
                        auto root=place_origin(*i.receiver);
                        if (i.receiver->fields.empty()) roots.push_back(root);
                        else {
                            if (!state.regions.at(root).ready.contains(i.receiver->fields.front())) fail("uninitialized projected receiver");
                        }
                    }
                    auto arguments=i.arguments;
                    if (!target.receiver && arguments.empty() && target.parameters.size()==1) arguments.push_back(i.a);
                    for (auto arg:arguments) if (types[block][arg].construction) roots.push_back(value_origin(arg));
                    if (i.initialization && target.parameters[0].construction) {
                        if (roots.size()!=1 || !i.receiver || !i.receiver->fields.empty()) fail("invalid initializer construction operands");
                        auto root=roots[0];
                        if (!owned.contains(root) || state.regions.at(root).phase!=preparing || Type::key(f.locals[root])!=Type::key(target.parameters[0])) fail("initializer called twice or on borrowed storage");
                        for (unsigned field=0;field<fields(root).size();++field) {
                            Type t=fields(root)[field].type;
                            if ((t.kind!=Type::managed_ref || t.nullable) && !state.regions.at(root).ready.contains(field)) fail("initializer missing safe field defaults");
                            state.regions.at(root).ready.insert(field);
                        }
                        state.regions.at(root).phase=initialized;
                    } else for (auto root:roots) require_all(root);
                }
            } else if (i.op==Op::gc_construct && i.receiver) {
                auto root=place_origin(*i.receiver); require_all(root);
                if (!owned.contains(root) || state.regions.at(root).phase!=initialized) fail("construction commit before initializer completion");
                for (unsigned field=0;field<i.arguments.size();++field) {
                    const auto& read=f.blocks[block].instructions.at(i.arguments[field]);
                    if (read.op!=Op::gc_read || read.immediate!=field || value_origin(read.a)!=root) fail("commit fields do not come from the initialized storage");
                }
                state.regions.at(root).phase=committed;
            }
            origins.push_back(origin);
        }
        const auto& end=f.blocks[block].end;
        if (end.kind==Terminator::Kind::return_value || end.kind==Terminator::Kind::return_void) {
            if (f.initializer && f.parameters[0].construction) require_all(0);
            for (auto root:owned) if (state.regions.at(root).phase & (preparing|initialized)) fail("uncommitted construction on successful exit");
        }
        auto propagate=[&](unsigned target) {
            auto merged=inputs[target] ? join(*inputs[target],state) : state;
            if (!inputs[target] || merged!=*inputs[target]) { inputs[target]=std::move(merged); pending.push_back(target); }
        };
        if (end.kind==Terminator::Kind::jump) propagate(end.yes);
        if (end.kind==Terminator::Kind::branch) { propagate(end.yes); propagate(end.no); }
    }
}
