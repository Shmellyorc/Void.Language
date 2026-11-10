// Aggregate fields and fixed-array elements carry value storage. Managed and
// growable handles have separate initialization/publication contracts.
bool contains_numeric_enum(Type root) {
    if(root.kind!=Type::aggregate && root.kind!=Type::fixed_array)return root.kind==Type::numeric_enum;
    std::vector<Type> pending{root};
    std::set<const ValueType*> seen;
    for(std::size_t n=0;n<pending.size();++n) {
        const auto type=pending[n];
        if(type.kind==Type::numeric_enum)return true;
        if(!type.definition || !seen.insert(type.definition.get()).second)continue;
        if(type.kind==Type::aggregate)
            for(const auto& field:type.definition->fields)pending.emplace_back(field.type);
        else if(type.kind==Type::fixed_array && type.definition->element)
            pending.emplace_back(*type.definition->element);
    }
    return false;
}

bool checked_array_shape(Type type, const CPrimitiveAdmission* native) {
    return contains_numeric_enum(type) || (native && native->inline_array(type));
}
bool checked_array_allocation(const Instruction& i, const CPrimitiveAdmission* native) {
    return i.op == Op::array_allocate && i.type.kind == Type::fixed_array && checked_array_shape(i.type, native);
}

std::set<unsigned> checked_array_homes(const Function& f, const CPrimitiveAdmission* native) {
    std::set<unsigned> homes;
    for(const auto& block:f.blocks)for(unsigned n=0;n<block.instructions.size();++n) {
        const auto& i=block.instructions[n];
        if(i.op==Op::array_capture && i.a<n && i.immediate>=0 &&
           static_cast<std::size_t>(i.immediate)<f.locals.size() && checked_array_allocation(block.instructions[i.a], native))
            homes.insert(static_cast<unsigned>(i.immediate));
    }
    return homes;
}

bool checked_storage_slot(Type type, const CPrimitiveAdmission* native) {
    if (contains_numeric_enum(type)) return true;
    if (!native || native->revision() < 2) return false;
    // Check scalar inputs too. A forged aggregate construction must not be
    // able to wrap an uninitialized scalar in otherwise valid record storage.
    return numeric::integer(type) || numeric::floating(type) || type == Type::boolean ||
        type.kind == Type::aggregate || type.kind == Type::fixed_array ||
        type.kind == Type::growable_array || native->resource_type(type);
}

bool checked_storage_cleanup_return(const Function& f, unsigned id, const CPrimitiveAdmission* native) {
    if(!f.cleanup_entry || id<*f.cleanup_entry)return false;
    const auto& block=f.blocks[id];
    return block.end.kind==Terminator::Kind::return_value && !block.instructions.empty() &&
           block.end.value==block.instructions.size()-1 && block.instructions.back().op==Op::load &&
           checked_storage_slot(f.locals.at(block.instructions.back().a), native);
}

void validate_numeric_enum_slots(const Function& f, const CPrimitiveAdmission* native) {
    std::set<unsigned> tracked,entry;
    for(unsigned n=0;n<f.locals.size();++n)
        if(checked_storage_slot(f.locals[n], native)) tracked.insert(n);
    if(tracked.empty())return;
    if(f.receiver && tracked.contains(0))entry.insert(0);

    // Intersect definitely initialized slots at explicit joins. A failure can
    // also enter cleanup before any body write. Its terminal return load is
    // emitted only on success and still needs a proof on the normal CFG.
    auto verify=[&](unsigned start,bool unwinding) {
        std::vector<std::optional<std::set<unsigned>>> inputs(f.blocks.size());
        inputs[start]=entry;
        std::vector<unsigned> work{start};
        while(!work.empty()) {
            const auto id=work.back();work.pop_back();
            auto state=*inputs[id];
            const auto& block=f.blocks[id];
            auto require=[&](unsigned slot) {
                if(tracked.contains(slot) && !state.contains(slot))
                    throw std::invalid_argument(std::string("VOID IR: read of uninitialized ") +
                        (contains_numeric_enum(f.locals[slot]) ? "numeric enum" : "C binding carrier") + " storage in " + f.name);
            };
            for(const auto& i:block.instructions) {
                if(i.op==Op::load && !(unwinding && &i==&block.instructions.back() && checked_storage_cleanup_return(f,id,native)))require(i.a);
                if(i.receiver)require(i.receiver->slot);
                if(i.op==Op::store && tracked.contains(i.a))state.insert(i.a);
                if(i.op==Op::array_capture && tracked.contains(i.immediate))state.insert(static_cast<unsigned>(i.immediate));
            }
            auto edge=[&](unsigned target) {
                auto& known=inputs[target];
                if(!known){known=state;work.push_back(target);return;}
                const auto removed=std::erase_if(*known,[&](unsigned slot){return !state.contains(slot);});
                if(removed)work.push_back(target);
            };
            if(block.end.kind==Terminator::Kind::jump || block.end.kind==Terminator::Kind::branch)edge(block.end.yes);
            if(block.end.kind==Terminator::Kind::branch)edge(block.end.no);
        }
    };
    verify(0,false);
    if(f.cleanup_entry && *f.cleanup_entry)verify(*f.cleanup_entry,true);
}
