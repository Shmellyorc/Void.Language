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
    auto exit=entry;for(const auto& i:f.blocks[b].instructions)if(i.op==Op::store || i.op==Op::stage_begin)exit.insert(i.a);else if(i.op==Op::array_capture)exit.insert(static_cast<unsigned>(i.immediate));
    if(incoming[b]!=entry || outgoing[b]!=exit){incoming[b]=std::move(entry);outgoing[b]=std::move(exit);changed=true;}
}}while(changed);
for(unsigned b=0;b<f.blocks.size();++b)if(reachable[b]){auto initialized=incoming[b];for(const auto& i:f.blocks[b].instructions){
    if(i.op==Op::load && required(f.locals[i.a]) && !initialized.contains(i.a))fail("async frame reads an uninitialized required local "+std::to_string(i.a)+" in "+f.name);
    if(i.receiver && required(f.locals[i.receiver->slot]) && !initialized.contains(i.receiver->slot))fail("async frame uses an uninitialized receiver");
    if(i.op==Op::store || i.op==Op::stage_begin)initialized.insert(i.a);else if(i.op==Op::array_capture)initialized.insert(static_cast<unsigned>(i.immediate));
}}
// A saved stack-only construction/prepared place must not be read after await.
for(unsigned block=0;block<f.blocks.size();++block)for(unsigned position=0;position<f.blocks[block].instructions.size();++position)if(f.blocks[block].instructions[position].op==Op::await || f.blocks[block].instructions[position].op==Op::async_group_join || f.blocks[block].instructions[position].op==Op::async_drain){
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
        switch(i.op){case Op::async_cancel_children:case Op::async_group_enter:case Op::async_drain:case Op::constant:case Op::argument:case Op::string_literal:case Op::load:case Op::stage_begin:break;case Op::store:check(i.b);break;case Op::native_call:case Op::call:case Op::async_call:case Op::virtual_call:case Op::interface_call:case Op::base_init:for(auto id:i.arguments)check(id);break;default:check(i.a);check(i.b);for(auto id:i.arguments)check(id);break;}
    }
}

// Lifecycle validation is independent of source syntax. Opaque group tokens may
// only be saved in a dedicated local and joined in strict lexical LIFO order.
if(!f.cleanup_entry || f.blocks[*f.cleanup_entry].instructions.empty() || f.blocks[*f.cleanup_entry].instructions[0].op!=Op::async_drain)fail("async activation lacks structured drain");
using GroupToken=std::pair<unsigned,unsigned>;
std::map<unsigned,GroupToken> group_homes;
for(unsigned b=0;b<f.blocks.size();++b)for(unsigned n=0;n<f.blocks[b].instructions.size();++n){const auto& i=f.blocks[b].instructions[n];if(i.op==Op::store && f.blocks[b].instructions[i.b].op==Op::async_group_enter){if(!group_homes.emplace(i.a,GroupToken{b,i.b}).second)fail("duplicate group home");}}
for(unsigned b=0;b<f.blocks.size();++b)for(unsigned n=0;n<f.blocks[b].instructions.size();++n){const auto& i=f.blocks[b].instructions[n];if(i.op==Op::store && group_homes.contains(i.a) && group_homes.at(i.a)!=GroupToken{b,i.b})fail("forged async group home");}
struct Lifecycle {std::vector<GroupToken> groups;bool drained=false;bool operator==(const Lifecycle&)const=default;};
std::map<unsigned,Lifecycle> states;std::vector<std::pair<unsigned,Lifecycle>> work{{0,{}}};
while(!work.empty()){
    auto [b,state]=std::move(work.back());work.pop_back();auto [found,added]=states.emplace(b,state);if(!added){if(found->second!=state)fail("inconsistent async group ownership across CFG");continue;}
    const auto& block=f.blocks[b];
    for(unsigned n=0;n<block.instructions.size();++n){const auto& i=block.instructions[n];
        if(i.op==Op::async_drain){state.groups.clear();state.drained=true;}
        if(state.drained && (i.op==Op::filesystem_async || i.op==Op::async_call || i.op==Op::async_cancel_children || i.op==Op::async_cancel || i.op==Op::async_group_enter || i.op==Op::async_group_join || i.op==Op::await))fail("async operation after structured drain");
        if(i.op==Op::async_group_enter)state.groups.push_back({b,n});
        if(i.op==Op::async_group_join){const auto& source=block.instructions[i.a];GroupToken token{b,i.a};if(source.op==Op::load){if(!group_homes.contains(source.a))fail("forged async group token");token=group_homes.at(source.a);}else if(source.op!=Op::async_group_enter)fail("forged async group token");
            if(state.groups.empty() || state.groups.back()!=token)fail("invalid async group join order");state.groups.pop_back();
        }
    }
    auto add=[&](unsigned to){auto next=state;if(to==*f.cleanup_entry){if(!next.groups.empty())fail("async exit bypasses group join");if(next.drained)fail("async drain executed twice");}work.push_back({to,std::move(next)});};
    const auto& end=block.end;if(end.kind==Terminator::Kind::jump)add(end.yes);else if(end.kind==Terminator::Kind::branch){add(end.yes);add(end.no);}else if((end.kind==Terminator::Kind::return_value || end.kind==Terminator::Kind::return_void) && !state.drained)fail("async return bypasses structured drain");
}
