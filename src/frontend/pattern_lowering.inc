    void lower_condition(const Expr& e,unsigned yes,unsigned no) {
        if(e.kind==Expr::Kind::test){auto subject=capture(expression(*e.parts[0]),e.parts[0]->type,e.token);lower_pattern(e.patterns[0],subject,yes,no);return;}
        if(e.kind==Expr::Kind::binary && (e.op=="and" || e.op=="or")){auto next=block();lower_condition(*e.parts[0],e.op=="and"?next:yes,e.op=="and"?no:next);current=next;lower_condition(*e.parts[1],yes,no);return;}
        if(e.kind==Expr::Kind::unary && e.op=="not"){lower_condition(*e.parts[0],no,yes);return;}
        pattern_branch(expression(e),yes,no);
    }
    void pattern_branch(unsigned condition,unsigned yes,unsigned no) {terminate({ir::Terminator::Kind::branch,condition,yes,no});}
    void lower_pattern(const Pattern& p,unsigned source,unsigned yes,unsigned no) {
        using K=Pattern::Kind;
        const auto& token=p.token;
        auto load=[&](unsigned slot){return emit({ir::Op::load,slot},token);};
        auto test=[&](unsigned condition){auto next=block();pattern_branch(condition,next,no);current=next;};
        auto child=[&](const Pattern& pattern,unsigned value){auto slot=capture(value,pattern.type,token);auto next=block();lower_pattern(pattern,slot,next,no);current=next;};
        switch(p.kind) {
        case K::wildcard:jump(yes);return;
        case K::capture:{ir::Instruction store{ir::Op::store,p.slot,load(source)};store.initialization=true;emit(store,token);}jump(yes);return;
        case K::value: {
            if(p.values[0]->kind==Expr::Kind::sum && p.values[0]->parts.empty()){
                pattern_branch(emit({ir::Op::equal,extract(load(source),0,token),constant(Type::i32,{p.values[0]->slot,{}},token)},token),yes,no);return;
            }
            auto rhs=snapshot(expression(*p.values[0]),p.type,token);ir::Instruction compare{ir::Op::pattern_equal,load(source),load(rhs)};
            pattern_branch(emit(compare,token),yes,no);return;
        }
        case K::range: {
            auto first=snapshot(expression(*p.values[0]),p.type,token),last=snapshot(expression(*p.values[1]),p.type,token);
            auto forward=block(),reverse=block();pattern_branch(emit({ir::Op::less_equal,load(first),load(last)},token),forward,reverse);
            current=forward;test(emit({ir::Op::greater_equal,load(source),load(first)},token));pattern_branch(emit({p.exclusive?ir::Op::less:ir::Op::less_equal,load(source),load(last)},token),yes,no);
            current=reverse;test(emit({ir::Op::less_equal,load(source),load(first)},token));pattern_branch(emit({p.exclusive?ir::Op::greater:ir::Op::greater_equal,load(source),load(last)},token),yes,no);return;
        }
        case K::list: {
            const auto fixed=p.children.size()-(p.rest?1:0);
            auto count=snapshot(emit({ir::Op::array_count,load(source)},token),Type::i64,token);
            test(emit({p.rest?ir::Op::greater_equal:ir::Op::equal,load(count),constant(Type::i64,{fixed,{}},token)},token));
            for(unsigned n=0;n<fixed;++n)child(p.children[n],emit({ir::Op::array_get,load(source),constant(Type::i64,{n,{}},token)},token));
            if(p.rest && p.children.back().kind!=K::wildcard) {
                ir::Instruction slice{ir::Op::array_slice,load(source),constant(Type::i64,{fixed,{}},token)};slice.type=p.children.back().type;slice.immediate=1;slice.arguments={load(count)};
                child(p.children.back(),emit(slice,token));
            }
            jump(yes);return;
        }
        case K::dictionary:
            for(unsigned n=0;n<p.children.size();++n) {
                auto key=snapshot(expression(*p.values[n]),p.values[n]->type,token);
                test(emit({ir::Op::dict_contains,load(source),load(key)},token));child(p.children[n],emit({ir::Op::dict_get,load(source),load(key)},token));
            }
            jump(yes);return;
        case K::constructor: {
            if(p.nullable) {
                auto nulltype=p.type;nulltype.nullable=true;test(emit({ir::Op::not_equal,load(source),constant(nulltype,{0,{}},token)},token));
                ir::Instruction convert{ir::Op::convert,load(source)};convert.type=p.children[0].type;child(p.children[0],emit(convert,token));jump(yes);return;
            }
            if(p.type.definition && !p.type.definition->cases.empty()) {
                test(emit({ir::Op::equal,extract(load(source),0,token),constant(Type::i32,{p.tag,{}},token)},token));
                for(unsigned n=0;n<p.children.size();++n){ir::Instruction get{ir::Op::sum_get,load(source)};get.immediate=p.indexes[n];child(p.children[n],emit(get,token));}
                jump(yes);return;
            }
            unsigned object=source;
            if(ir::reference_value(p.narrowed)) {
                ir::Instruction type_test{ir::Op::type_test,load(source)};type_test.type=p.narrowed;test(emit(type_test,token));
                ir::Instruction cast{ir::Op::type_cast,load(source)};cast.type=p.narrowed;object=capture(emit(cast,token),p.narrowed,token);
            }
            if(p.rest)child(p.children[0],load(object));
            else for(unsigned n=0;n<p.children.size();++n){ir::Instruction field{p.narrowed.kind==Type::managed_ref?ir::Op::gc_read:ir::Op::extract,load(object)};field.immediate=p.indexes[n];child(p.children[n],emit(field,token));}
            jump(yes);return;
        }
        }
    }
    unsigned lower_test(const Expr& e) {
        auto source=capture(expression(*e.parts[0]),e.parts[0]->type,e.token);auto result=static_cast<unsigned>(output.locals.size());output.locals.push_back(Type::boolean);
        auto yes=block(),no=block(),done=block();lower_pattern(e.patterns[0],source,yes,no);
        current=yes;emit({ir::Op::store,result,constant(Type::boolean,{1,{}},e.token)},e.token);jump(done);
        current=no;emit({ir::Op::store,result,constant(Type::boolean,{0,{}},e.token)},e.token);jump(done);
        current=done;return emit({ir::Op::load,result},e.token);
    }
    unsigned lower_match(const Expr& e) {
        auto source=capture(expression(*e.parts[0]),e.parts[0]->type,e.token);auto result=static_cast<unsigned>(output.locals.size());output.locals.push_back(e.type);auto done=block();
        for(unsigned n=0;n<e.patterns.size();++n){auto yes=block(),no=block();lower_pattern(e.patterns[n],source,yes,no);current=yes;auto value=expression(*e.parts[n+1]);emit({ir::Op::store,result,value},e.token);jump(done);current=no;}
        ir::Instruction failure{ir::Op::panic};failure.callee="No match expression arm selected";emit(failure,e.token);terminate({ir::Terminator::Kind::unreachable});current=done;return emit({ir::Op::load,result},e.token);
    }
