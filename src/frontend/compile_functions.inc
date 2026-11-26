// G93 extends the existing typed constant-expression kernel. Calls carry the
// declaration selected by ordinary binding; local frames contain owned values,
// never Runtime objects, borrowed places, native addresses or executable code.
bool compile_value_type(Type root) {
    std::vector<Type> pending{root};std::set<const ir::ValueType*> seen;
    while(!pending.empty()) {
        auto t=pending.back();pending.pop_back();
        if(t==Type::none || t==Type::string || t==Type::boolean || numeric::number(t) || t.kind==Type::numeric_enum)continue;
        if(t.kind!=Type::aggregate && !ir::collection_type(t))return false;
        if(!t.definition || t.definition->callable || t.definition->managed || t.definition->native_resource)return false;
        if(!seen.insert(t.definition.get()).second)continue;
        if(ir::collection_type(t)) {
            pending.push_back(t.kind==Type::dictionary?ir::dictionary_value(t):ir::array_element(t));
            if(t.kind==Type::dictionary)pending.push_back(ir::dictionary_key(t));
        } else for(const auto& f:t.definition->fields)pending.push_back(f.type);
    }
    return true;
}
void collection_constant_type(const SourceFile&,const Token&,Type);
bool compile_intrinsic(const Expr& e) {
    if(!e.display_adapter.empty())return false;
    return e.target.starts_with("$numeric::") || e.target=="$interpolation" || e.target=="$hole" ||
        e.target=="$display.string" || e.target=="$display.format" || e.op=="$enum_equal" ||
        e.op.starts_with("$enum.") || e.op=="$collection_equal";
}
void validate_compile_function(const SourceFile& source,Function& f) {
    auto reject=[&](const Token& site,const std::string& reason){error(source,site,Code::constant_call,"compile func: "+reason);};
    if(f.asynchronous || !f.native_symbol.empty() || f.c_primitive || f.initializer || f.accessor ||
       f.virtual_method || f.overriding || f.interface_requirement || f.anonymous || f.token.text=="deinit")
        reject(f.token,"requires an ordinary synchronous source function without lifecycle, virtual or native effects.");
    if(f.receiver && f.receiver->type.kind!=Type::aggregate)reject(f.token,"requires a pure value-struct receiver.");
    if(!compile_value_type(f.result))reject(f.token,"result type requires Runtime identity or unsupported storage.");
    for(const auto& t:f.locals)if(!compile_value_type(t))reject(f.token,"local/parameter type requires Runtime identity or unsupported storage.");
    std::vector<const Statement*> statements;for(const auto& s:f.body)statements.push_back(&s);
    std::vector<const Expr*> expressions;
    while(!statements.empty()) {
        const auto& s=*statements.back();statements.pop_back();
        using K=Statement::Kind;
        if(s.kind==K::scoped || s.kind==K::deferred || s.kind==K::match || s.asynchronous_iteration || s.pattern)
            reject(s.token,"resource scopes, deferred effects, patterns and async iteration are not supported constant operations.");
        if(s.kind==K::assignment) {
            if(s.selection || s.event || !s.compound_target.empty() || s.compound_conversion)
                reject(s.token,"only direct local value assignments are supported; external state and property effects are forbidden.");
            if(f.receiver && s.slot==f.receiver->slot)reject(s.token,"a compile-time receiver must remain readonly.");
            Type selected=f.locals.at(s.slot);
            for(auto index:s.field_indexes) {
                if(selected.kind!=Type::aggregate)reject(s.token,"assignment cannot mutate referenced storage.");
                selected=ir::field_type(selected,index);
            }
            if(s.index && selected.kind!=Type::fixed_array)reject(s.token,"only owned fixed-array elements may be mutated.");
        }
        if(s.kind==K::loop && !s.binder.text.empty() && !s.range_end && s.expression->type.kind!=Type::fixed_array)
            reject(s.token,"constant iteration supports ranges, conditions and owned fixed arrays.");
        for(const auto* e:{s.expression.get(),s.selection.get(),s.index.get(),s.range_end.get()})if(e)expressions.push_back(e);
        for(const auto& child:s.body)statements.push_back(&child);
        for(const auto& child:s.otherwise)statements.push_back(&child);
    }
    while(!expressions.empty()) {
        const auto& e=*expressions.back();expressions.pop_back();using K=Expr::Kind;
        if(!compile_value_type(e.type))reject(e.token,"expression requires Runtime identity or unsupported storage.");
        switch(e.kind) {
        case K::name:case K::literal:case K::unary:case K::binary:case K::conversion:case K::choice:
        case K::sum:case K::array_literal:case K::dictionary_literal:case K::array_default:case K::return_pack:break;
        case K::array_index:
            if(!ir::array_type(e.parts.at(0)->type))reject(e.token,"dictionary lookup requires runtime evaluation.");
            break;
        case K::field:
            if(e.property || (e.target.starts_with("$") && e.target!="$array.count"))reject(e.token,"this property requires runtime evaluation.");
            break;
        case K::construction:
            if(e.type.kind!=Type::aggregate || !e.initializer.empty())reject(e.token,"managed allocation and custom initialization are forbidden.");
            break;
        case K::call:case K::method_call:
            if(e.compile_target) {
                if(!e.compile_target->compile_time || e.asynchronous_call || !e.native_symbol.empty() || !e.virtual_slot.empty() || e.interface_call)
                    reject(e.token,"calls require an explicitly marked, statically selected compile func.");
            } else if(!compile_intrinsic(e))reject(e.token,"this call has runtime-only or unsupported effects.");
            if(e.op=="$enum_equal" || e.op=="$collection_equal")collection_constant_type(source,e.token,e.parts.at(0)->type);
            break;
        default:reject(e.token,"operation has runtime-only or unsupported effects.");
        }
        for(const auto& child:e.parts)expressions.push_back(child.get());
        for(const auto& child:e.omitted)expressions.push_back(child.get());
    }
}
struct CompileEvaluation {
    static constexpr std::uint64_t step_limit=1048576, graph_step_limit=8388608;
    static constexpr std::size_t byte_limit=64*1024*1024, graph_byte_limit=256*1024*1024;
    unsigned depth=0, calls=0, nesting=0;
    // Calls retain their enclosing expression frame. Count statement sequences
    // and loop handlers alongside it, so separate limits cannot multiply native
    // stack use. Keep room to report the limit on the required 1 MiB stack.
    struct Nesting {
        CompileEvaluation& state;
        Nesting(CompileEvaluation& state,const SourceFile& source,const Token& site):state(state) {
            if(state.nesting>=256)error(source,site,Code::limit,"Compile-time combined expression/statement nesting limit of 256 exceeded.");
            ++state.nesting;
        }
        ~Nesting(){--state.nesting;}
    };
    std::uint64_t steps=0, graph_steps=0;
    std::size_t bytes=0, graph_bytes=0;
    std::vector<std::optional<ir::ConstantValue>>* locals=nullptr;
    std::function<void(Function&,const SourceFile&,const Token&,bool)> prepare;
    std::function<void(Type)> numeric_definition;
    void step(const SourceFile& source,const Token& site,std::uint64_t work=1) {
        if(compile_evaluation_stop.stop_requested())error(source,site,Code::limit,"Compile-time evaluation cancelled.");
        if(work>step_limit-steps || work>graph_step_limit-graph_steps)
            error(source,site,Code::limit,"Compile-time evaluation work budget exceeded (1048576 steps per expression; 8388608 per analysis).");
        steps+=work;graph_steps+=work;
    }
    void charge(const SourceFile& source,const Token& site,const ir::ConstantValue& value) {
        std::vector<const ir::ConstantValue*> pending{&value};
        while(!pending.empty()) {
            const auto& v=*pending.back();pending.pop_back();
            const auto amount=sizeof(v)+v.text.size();
            if(amount>byte_limit-bytes || amount>graph_byte_limit-graph_bytes)
                error(source,site,Code::limit,"Compile-time temporary value budget exceeded (64 MiB per expression; 256 MiB per analysis).");
            bytes+=amount;graph_bytes+=amount;
            for(const auto& field:v.fields)pending.push_back(&field);
        }
    }
    ir::ConstantValue local(const SourceFile& source,const Expr& e) {
        if(!locals || e.slot>=locals->size() || !locals->at(e.slot))
            error(source,e.token,Code::constant_call,"Compile-time local value is not initialized.");
        return *locals->at(e.slot);
    }
    struct Outcome {unsigned flow=next_flow;ir::ConstantValue value;};
    Outcome sequence(const SourceFile&,const std::vector<Statement>&);
    Outcome loop(const SourceFile&,const Statement&);
    void assign(const SourceFile&,const Statement&);
    ir::ConstantValue call(const SourceFile&,const Expr&);
    struct Selection {
        std::optional<unsigned> slot;
        ir::ConstantValue captured;
        struct Part {std::size_t index;bool array;};
        std::vector<Part> path;
    };
    const ir::ConstantValue& read(const SourceFile&,const Token&,const Selection&);
    Selection select(const SourceFile&,const Expr&);
};
void prepare_compile_call(CompileEvaluation* evaluation,Function& f,const SourceFile& source,const Token& site,bool body) {
    if(!evaluation || !evaluation->prepare)error(source,site,Code::constant_call,"Compile-time function is unavailable during this declaration dependency.");
    evaluation->prepare(f,source,site,body);
}
ir::ConstantValue constant_expression(const SourceFile&,const Expr&,CompileEvaluation*);
ir::ConstantValue constant_value(const SourceFile& source,const Expr& e,CompileEvaluation* evaluation) {
    const bool compile_call=e.compile_target && (e.kind==Expr::Kind::call || e.kind==Expr::Kind::method_call);
    if(!evaluation) {
        if(compile_call)throw std::logic_error("Missing constant evaluation context");
        return constant_expression(source,e,nullptr);
    }
    CompileEvaluation::Nesting nesting(*evaluation,source,e.token);
    if(!evaluation->depth){evaluation->steps=0;evaluation->bytes=0;}
    if(evaluation->depth>=128)error(source,e.token,Code::limit,"Compile-time evaluation nesting limit of 128 exceeded.");
    ++evaluation->depth;
    struct Leave {CompileEvaluation& state;~Leave(){--state.depth;}} leave{*evaluation};
    evaluation->step(source,e.token);
    // Keep recursive calls out of the general expression dispatcher's frame.
    ir::ConstantValue value;
    if(compile_call)value=evaluation->call(source,e);
    else if(e.kind==Expr::Kind::name)value=evaluation->local(source,e);
    else value=constant_expression(source,e,evaluation);
    evaluation->charge(source,e.token,value);
    return value;
}
VOID_SEMANTIC_HANDLER ir::ConstantValue CompileEvaluation::call(const SourceFile& source,const Expr& e) {
    auto& f=*e.compile_target;
    prepare_compile_call(this,f,source,e.token,true);
    if(calls>=64)error(source,e.token,Code::limit,"Compile-time recursive call depth limit of 64 exceeded.");
    if(f.locals.size()>(byte_limit-bytes)/sizeof(std::optional<ir::ConstantValue>))error(source,e.token,Code::limit,"Compile-time local storage budget exceeded.");
    const auto frame_bytes=f.locals.size()*sizeof(std::optional<ir::ConstantValue>);
    if(frame_bytes>graph_byte_limit-graph_bytes)error(source,e.token,Code::limit,"Compile-time local storage budget exceeded.");
    bytes+=frame_bytes;graph_bytes+=frame_bytes;
    std::vector<std::optional<ir::ConstantValue>> frame(f.locals.size());
    // Snapshot supplied arguments in source order, then map to formal slots.
    const auto first=f.receiver?1u:0u;
    if(first)frame.at(f.receiver->slot)=constant_value(source,*e.parts.at(0),this);
    for(std::size_t n=first;n<e.parts.size();++n)
        frame.at(f.parameters.at(e.argument_indexes.at(n-first)).slot)=constant_value(source,*e.parts[n],this);
    auto* previous=locals;locals=&frame;++calls;
    struct Leave {CompileEvaluation& state;decltype(locals) previous;~Leave(){state.locals=previous;--state.calls;}} leave{*this,previous};
    try {return sequence(f.compile_owner->source,f.body).value;}
    catch(const CompilationError& failure) {
        auto diagnostics=failure.diagnostics();
        for(auto& d:diagnostics)if(d.related.size()<12)d.related.push_back({span(source,e.token),"While evaluating compile func '"+f.token.text+"'."});
        throw CompilationError(std::move(diagnostics));
    }
}
VOID_SEMANTIC_HANDLER void CompileEvaluation::assign(const SourceFile& source,const Statement& s) {
    // As in lowering, evaluate place/index and RHS before writing owned storage.
    auto index=s.index?std::optional(constant_value(source,*s.index,this)):std::nullopt;
    auto right=constant_value(source,*s.expression,this);
    auto& storage=locals->at(s.slot);if(!storage)error(source,s.token,Code::constant_call,"Assignment reads an uninitialized compile-time local.");
    auto* place=&*storage;auto type=s.assigned_type;
    for(auto field:s.field_indexes)place=&place->fields.at(field);
    if(index) {
        std::uint64_t selected;
        try {selected=ir::checked_array_index(index->bits,s.index->type,place->repeat.value_or(place->fields.size()));}
        catch(const std::out_of_range&){error(source,s.index->token,Code::invalid_value,"Constant array index out of bounds.");}
        if(place->repeat) {
            const auto count=*place->repeat;
            std::size_t prototype_bytes=0;std::vector<const ir::ConstantValue*> pending{&place->fields.at(0)};
            while(!pending.empty()) {
                auto* v=pending.back();pending.pop_back();const auto amount=sizeof(*v)+v->text.size();
                if(amount>byte_limit-prototype_bytes)error(source,s.token,Code::limit,"Compile-time fixed-array prototype exceeds temporary budget.");
                prototype_bytes+=amount;for(const auto& field:v->fields)pending.push_back(&field);
            }
            if(count>(byte_limit-bytes)/prototype_bytes)error(source,s.token,Code::limit,"Compile-time fixed-array materialization exceeds temporary budget.");
            auto item=place->fields.at(0);place->fields.assign(count,item);place->repeat.reset();charge(source,s.token,*place);
        }
        place=&place->fields.at(selected);
    }
    try {
        if(s.op=="=")*place=std::move(right);
        else if(type==Type::string) {
            if(right.text.size()>ir::string_constant_limit-place->text.size())error(source,s.token,Code::limit,"Folded string exceeds the 16 MiB constant limit.");
            place->text+=right.text;
        } else {
            auto primitive=type.kind==Type::numeric_enum?Type(*type.definition->enum_backing):type;
            place->bits=numeric::binary(operation(s.op.substr(0,s.op.size()-1)),place->bits,right.bits,primitive);
        }
        charge(source,s.token,*place);
    } catch(const std::invalid_argument& failure){error(source,s.compound_token,Code::zero_divisor,failure.what());}
}
const ir::ConstantValue& CompileEvaluation::read(const SourceFile& source,const Token& site,const Selection& selected) {
    const auto* value=&selected.captured;
    if(selected.slot) {
        if(!locals || !locals->at(*selected.slot))error(source,site,Code::constant_call,"Compile-time local value is not initialized.");
        value=&*locals->at(*selected.slot);
    }
    for(auto part:selected.path)value=&value->fields.at(part.array && value->repeat?0:part.index);
    return *value;
}
VOID_SEMANTIC_HANDLER CompileEvaluation::Selection CompileEvaluation::select(const SourceFile& source,const Expr& e) {
    // Match lowering's selected storage: freeze index expressions, keep local
    // value ancestry live, and capture computed roots / growable handles once.
    std::vector<const Expr*> path;const auto* root=&e;
    while(root->kind==Expr::Kind::field || root->kind==Expr::Kind::array_index) {
        step(source,root->token);path.push_back(root);root=root->parts.at(0).get();
    }
    Selection selected;
    if(root->kind==Expr::Kind::name)selected.slot=root->slot;
    else selected.captured=constant_value(source,*root,this);
    for(auto it=path.rbegin();it!=path.rend();++it) {
        const auto& part=**it;
        if(part.kind==Expr::Kind::field)selected.path.push_back({part.slot,false});
        else {
            if(part.parts[0]->type.kind==Type::growable_array) {
                auto captured=read(source,part.token,selected);charge(source,part.token,captured);
                selected.captured=std::move(captured);selected.slot.reset();selected.path.clear();
            }
            auto index=constant_value(source,*part.parts[1],this);
            const auto& array=read(source,part.token,selected);
            try {selected.path.push_back({static_cast<std::size_t>(ir::checked_array_index(index.bits,part.parts[1]->type,array.repeat.value_or(array.fields.size()))),true});}
            catch(const std::out_of_range&){error(source,part.token,Code::invalid_value,"Constant array index out of bounds.");}
        }
    }
    return selected;
}
VOID_SEMANTIC_HANDLER CompileEvaluation::Outcome CompileEvaluation::loop(const SourceFile& source,const Statement& s) {
    Nesting nesting(*this,source,s.token);
    if(s.binder.text.empty()) {
        while(!s.expression || constant_value(source,*s.expression,this).bits) {
            step(source,s.token);auto result=sequence(source,s.body);
            if(result.flow==return_flow)return result;if(result.flow==break_flow)break;
        }
    } else if(s.range_end) {
        auto first=constant_value(source,*s.expression,this).bits,last=constant_value(source,*s.range_end,this).bits;
        const auto type=s.expression->type;bool up=numeric::binary(ir::Op::less_equal,first,last,type);
        auto cursor=s.reverse?last:first,terminal=s.reverse?first:last;
        if(s.reverse){up=!up;if(s.exclusive){if(first==last)return {};cursor=numeric::binary(up?ir::Op::add:ir::Op::subtract,cursor,1,type);}}
        while(!(s.exclusive && !s.reverse && cursor==terminal)) {
            step(source,s.token);locals->at(s.slot)=ir::ConstantValue{cursor,{}};
            auto result=sequence(source,s.body);if(result.flow==return_flow)return result;if(result.flow==break_flow)break;
            if((!s.exclusive || s.reverse) && cursor==terminal)break;
            cursor=numeric::binary(up?ir::Op::add:ir::Op::subtract,cursor,1,type);
        }
    } else {
        const auto selected=select(source,*s.expression);
        const auto& initial=read(source,s.token,selected);auto count=initial.repeat.value_or(initial.fields.size());
        for(std::uint64_t n=0;n<count;++n) {
            step(source,s.token);const auto index=s.reverse?count-1-n:n;
            const auto& array=read(source,s.token,selected);
            locals->at(s.slot)=array.fields.at(array.repeat?0:index);charge(source,s.token,*locals->at(s.slot));
            if(!s.index_binder.text.empty())locals->at(s.index_slot)=ir::ConstantValue{index,{}};
            auto result=sequence(source,s.body);if(result.flow==return_flow)return result;if(result.flow==break_flow)break;
        }
    }
    return {};
}
VOID_SEMANTIC_HANDLER CompileEvaluation::Outcome CompileEvaluation::sequence(const SourceFile& source,const std::vector<Statement>& body) {
    if(body.empty())return {};
    Nesting nesting(*this,source,body.front().token);
    for(const auto& s:body) {
        step(source,s.token);using K=Statement::Kind;
        switch(s.kind) {
        case K::variable:locals->at(s.slot)=s.expression?constant_value(source,*s.expression,this):ir::ConstantValue{};break;
        case K::assignment:assign(source,s);break;
        case K::expression:(void)constant_value(source,*s.expression,this);break;
        case K::returned:return {return_flow,s.expression?constant_value(source,*s.expression,this):ir::ConstantValue{}};
        case K::conditional:{auto result=sequence(source,constant_value(source,*s.expression,this).bits?s.body:s.otherwise);if(result.flow!=next_flow)return result;break;}
        case K::loop:{auto result=loop(source,s);if(result.flow!=next_flow)return result;break;}
        case K::broken:return {break_flow,{}};case K::continued:return {continue_flow,{}};
        case K::no_op:break;
        case K::destructure:{auto value=constant_value(source,*s.expression,this);for(unsigned n=0;n<s.bindings.size();++n)locals->at(s.bindings[n].slot)=value.fields.at(n);break;}
        default:error(source,s.token,Code::constant_call,"Unsupported compile-time statement.");
        }
    }
    return {};
}
