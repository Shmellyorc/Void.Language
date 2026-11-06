// Private rooted-cleanup slots have dedicated activation storage. Check this
// independently of source selection: no activation without a completed binding,
// no reassignments, and one reverse-ordered unwind visit per registered pair.
{
    struct Cleanup {unsigned binding;std::string callee;};
    std::map<unsigned,Cleanup> cleanups;
    for(const auto& block:f.blocks)for(const auto& i:block.instructions)if(i.op==Op::local_cleanup){
        auto [entry,added]=cleanups.emplace(i.b,Cleanup{i.a,i.callee});
        if(!added && (entry->second.binding!=i.a || entry->second.callee!=i.callee))fail("inconsistent rooted cleanup slot authority");
    }
    if(!cleanups.empty()) {
        std::set<unsigned> bindings,activated,unwound;unsigned previous=~0u;
        for(const auto& [flag,cleanup]:cleanups)if(!bindings.insert(cleanup.binding).second)fail("aliased rooted cleanup binding");
        for(unsigned b=0;b<f.blocks.size();++b)for(unsigned n=0;n<f.blocks[b].instructions.size();++n){
            const auto& block=f.blocks[b];const auto& i=block.instructions[n];
            if(i.op==Op::cleanup_call)fail("dynamic cleanup mixed with local cleanup");
            if(i.op==Op::store && cleanups.contains(i.a)){
                if(i.b>=n)fail("invalid rooted cleanup activation source");const auto& value=block.instructions[i.b];
                if(value.op!=Op::constant || value.type!=Type::boolean || (value.immediate!=0 && value.immediate!=1))fail("forged rooted cleanup activation");
                if(value.immediate==1){if(n<2 || i.b!=n-1 || block.instructions[n-2].op!=Op::store || block.instructions[n-2].a!=cleanups.at(i.a).binding)fail("cleanup activated before completed binding");activated.insert(i.a);}
            }
            if(i.op==Op::store && bindings.contains(i.a)){
                if(n+2>=block.instructions.size() || block.instructions[n+1].op!=Op::constant || block.instructions[n+2].op!=Op::store || !cleanups.contains(block.instructions[n+2].a) || cleanups.at(block.instructions[n+2].a).binding!=i.a || block.instructions[n+2].b!=n+1 || block.instructions[n+1].immediate!=1)fail("reassigned rooted cleanup binding");
            }
            if(i.op==Op::local_cleanup && b>=*f.cleanup_entry){
                if(i.b>=previous || !unwound.insert(i.b).second)fail("invalid local cleanup unwind order");previous=i.b;
            }
        }
        if(activated.size()!=cleanups.size() || unwound.size()!=cleanups.size())fail("incomplete rooted cleanup lifecycle");
    }
}
