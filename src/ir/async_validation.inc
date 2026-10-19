// Independently establish initialized frame reads over the typed CFG. Numeric,
// nullable and empty-string homes have valid zero defaults; required references,
// function/sum values and collection handles must be assigned before use.
auto required=[](Type t){
    std::vector<Type> pending{t};std::set<const ValueType*> seen;
    while(!pending.empty()){auto value=pending.back();pending.pop_back();if(requires_initialization(value) || collection_type(value))return true;if(value.kind==Type::aggregate && seen.insert(value.definition.get()).second)for(const auto& field:value.definition->fields)pending.push_back(field.type);}
    return false;
};
std::set<unsigned> universe;for(unsigned n=0;n<f.locals.size();++n)universe.insert(n);
std::vector<std::vector<unsigned>> predecessors(f.blocks.size());std::vector<bool> reachable(f.blocks.size());std::vector<unsigned> todo{0};
while(!todo.empty()){auto b=todo.back();todo.pop_back();if(reachable[b])continue;reachable[b]=true;auto add=[&](unsigned target){predecessors[target].push_back(b);todo.push_back(target);};const auto& end=f.blocks[b].end;if(end.kind==Terminator::Kind::jump)add(end.yes);else if(end.kind==Terminator::Kind::branch){add(end.yes);add(end.no);}}
std::vector<std::set<unsigned>> incoming(f.blocks.size(),universe),outgoing=incoming;
incoming[0].clear();if(f.receiver)incoming[0].insert(0);
bool changed;
do{changed=false;for(unsigned b=0;b<f.blocks.size();++b)if(reachable[b]){
    auto entry=incoming[b];if(b){entry=universe;for(auto p:predecessors[b]){std::set<unsigned> intersection;std::set_intersection(entry.begin(),entry.end(),outgoing[p].begin(),outgoing[p].end(),std::inserter(intersection,intersection.end()));entry=std::move(intersection);}}
    auto exit=entry;for(const auto& i:f.blocks[b].instructions)if(i.op==Op::store || i.op==Op::stage_begin)exit.insert(i.a);
    if(incoming[b]!=entry || outgoing[b]!=exit){incoming[b]=std::move(entry);outgoing[b]=std::move(exit);changed=true;}
}}while(changed);
for(unsigned b=0;b<f.blocks.size();++b)if(reachable[b]){auto initialized=incoming[b];for(const auto& i:f.blocks[b].instructions){
    if(i.op==Op::load && required(f.locals[i.a]) && !initialized.contains(i.a))fail("async frame reads an uninitialized required local");
    if(i.receiver && required(f.locals[i.receiver->slot]) && !initialized.contains(i.receiver->slot))fail("async frame uses an uninitialized receiver");
    if(i.op==Op::store || i.op==Op::stage_begin)initialized.insert(i.a);
}}
// A saved stack-only construction/prepared place must not be read after await.
for(unsigned block=0;block<f.blocks.size();++block)for(unsigned position=0;position<f.blocks[block].instructions.size();++position)if(f.blocks[block].instructions[position].op==Op::await){
    std::set<unsigned> borrowed;for(unsigned slot=0;slot<f.locals.size();++slot)if(f.locals[slot].construction || f.locals[slot].kind==Type::prepared_receiver)borrowed.insert(slot);
    struct Pending{unsigned block,position;std::set<unsigned> borrowed;};std::vector<Pending> pending{{block,position+1,borrowed}};
    std::map<std::pair<unsigned,unsigned>,std::set<unsigned>> visited;
    while(!pending.empty()){
        auto item=std::move(pending.back());pending.pop_back();auto key=std::pair{item.block,item.position};auto [found,added]=visited.emplace(key,item.borrowed);if(!added){auto size=found->second.size();found->second.insert(item.borrowed.begin(),item.borrowed.end());if(found->second.size()==size)continue;item.borrowed=found->second;}
        const auto& current=f.blocks[item.block];for(unsigned n=item.position;n<current.instructions.size();++n){const auto& i=current.instructions[n];
            if(i.op==Op::load && item.borrowed.contains(i.a))fail("borrowed construction/prepared receiver survives suspension");
            if(i.receiver && item.borrowed.contains(i.receiver->slot))fail("borrowed receiver survives suspension");
            if(i.op==Op::store || i.op==Op::stage_begin)item.borrowed.erase(i.a);
        }
        const auto& end=current.end;if(end.kind==Terminator::Kind::branch){pending.push_back({end.yes,0,item.borrowed});pending.push_back({end.no,0,item.borrowed});}else if(end.kind==Terminator::Kind::jump)pending.push_back({end.yes,0,item.borrowed});
    }
    for(unsigned n=position+1;n<f.blocks[block].instructions.size();++n){const auto& i=f.blocks[block].instructions[n];auto check=[&](unsigned id){if(id<position && (body_types[block][id].construction || body_types[block][id].kind==Type::prepared_receiver))fail("transient instruction value survives suspension");};
        switch(i.op){case Op::constant:case Op::argument:case Op::string_literal:case Op::load:case Op::stage_begin:break;case Op::store:check(i.b);break;case Op::call:case Op::async_call:case Op::virtual_call:case Op::interface_call:case Op::base_init:for(auto id:i.arguments)check(id);break;default:check(i.a);check(i.b);for(auto id:i.arguments)check(id);break;}
    }
}
