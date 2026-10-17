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
        if(end.kind==Terminator::Kind::jump) edge(end.yes);
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
        if(b.end.kind==Terminator::Kind::return_value) use(b.end.value,true);
    }
}
