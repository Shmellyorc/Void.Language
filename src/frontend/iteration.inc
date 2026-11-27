// Custom iteration is expanded into ordinary checked calls, an option pattern
// loop and (for resource-owning managed state) the existing with scope. All
// subsequent effect, construction, capability, IR and backend passes see the
// same operations as authored source; there is no second iteration ABI.
VOID_SEMANTIC_HANDLER Type iteration_expression(Expr& e) {
    const auto operation=e.token.text;
    const bool async_next=e.op=="$async_iteration_next";
    auto type=method_expression(e);
    if(type==error_type)throw Poison{};
    if(e.target=="$generator.next")return type;
    const Function* selected=nullptr;
    for(const auto& [id,owner]:units)for(const auto& f:owner->functions)
        if(f.symbol==e.target)selected=&f;
    if(!selected || selected->token.text!=operation || selected->accessor ||
       !selected->parameters.empty() || selected->asynchronous!=async_next)
        error(source,e.token,Code::unsupported_iteration,
            "Custom iteration requires an ordinary "+std::string(async_next?"async":"synchronous")+" '"+operation+"()' method with no parameters.");
    if(operation=="iterator" || operation=="reverse_iterator" || operation=="async_iterator" || operation=="reverse_async_iterator") {
        if(type.nullable || !type.definition ||
           (type.kind!=Type::managed_ref && type.kind!=Type::aggregate) ||
           type.definition->callable || !type.definition->cases.empty() ||
           type.definition->nullable_value)
            error(source,e.token,Code::unsupported_iteration,
                "Custom '"+operation+"()' must return a non-nullable concrete iterator struct.");
    } else if(operation=="next") {
        auto yielded=async_next && type.definition && type.definition->task_result ? Type(*type.definition->task_result) : type;
        if(!yielded.definition || !yielded.definition->option_type)
            error(source,e.token,Code::unsupported_iteration,
                "Custom iterator next() must return option<T>; Some(value) yields a value and None completes the loop.");
    } else if(type==Type::none) {
        error(source,e.token,Code::unsupported_iteration,
            "Custom iterator index() must return the index or key of the value just yielded by next().");
    }
    return type;
}

VOID_SEMANTIC_HANDLER void custom_iteration(Statement& s) {
    auto call=[&](const std::string& name,std::unique_ptr<Expr> receiver) {
        Token site{TokenKind::name,name,s.token.offset};site.source_width=s.token.text.size();
        auto e=std::make_unique<Expr>(Expr::Kind::method_call,site);
        e->op="$iteration";e->implicit=true;e->parts.push_back(std::move(receiver));return e;
    };
    auto factory=ir::generator_type(s.expression->type)?std::move(s.expression):call(s.asynchronous_iteration?(s.reverse?"reverse_async_iterator":"async_iterator"):
        (s.reverse?"reverse_iterator":"iterator"),std::move(s.expression));
    const auto state=expression(*factory);
    if(state==error_type)throw Poison{};
    const bool generator=ir::generator_type(state);
    const bool async_generator=ir::async_generator_type(state);
    if(generator && (s.asynchronous_iteration!=async_generator || s.reverse || !s.index_binder.text.empty()))
        error(source,s.token,Code::unsupported_iteration,
              async_generator?"An asynchronous generator requires forward for await with one item binder.":
                              "A synchronous generator requires forward for with one item binder.");
    Token hidden{TokenKind::name,"$iterator$"+std::to_string(s.token.offset),s.token.offset};
    hidden.source_width=0;
    auto state_read=[&] {
        auto e=std::make_unique<Expr>(Expr::Kind::name,hidden);e->implicit=true;return e;
    };
    Statement loop(Statement::Kind::loop,s.token);
    loop.expression=std::make_unique<Expr>(Expr::Kind::test,s.token);
    loop.expression->implicit=true;
    auto next=call("next",state_read());
    if(async_generator)next->op="$generator.async.next";
    else if(s.asynchronous_iteration) {
        next->op="$async_iteration_next";
        Token token{TokenKind::name,"await",s.token.offset};token.source_width=s.token.text.size();
        auto awaited=std::make_unique<Expr>(Expr::Kind::awaited,token);awaited->implicit=true;
        awaited->parts.push_back(std::move(next));next=std::move(awaited);
    }
    loop.expression->parts.push_back(std::move(next));
    Pattern some;some.kind=Pattern::Kind::constructor;some.spelling="Some";some.token=s.token;
    Pattern item;item.kind=Pattern::Kind::capture;item.token=s.binder;
    some.children.push_back(std::move(item));some.fields.push_back({});
    loop.expression->patterns.push_back(std::move(some));
    if(!s.index_binder.text.empty()) {
        Statement index(Statement::Kind::variable,s.index_binder);index.name=s.index_binder.text;
        index.expression=call("index",state_read());loop.body.push_back(std::move(index));
    }
    for(auto& child:s.body)loop.body.push_back(std::move(child));

    if(async_generator) {
        Statement scope(Statement::Kind::scoped,s.token);
        scope.generator_iteration=true;
        scope.binder={TokenKind::name,"$generator_group$"+std::to_string(s.token.offset),s.token.offset};
        scope.source_end=s.source_end;
        Statement binding(Statement::Kind::variable,hidden);
        binding.name=hidden.text;
        binding.expression=std::move(factory);
        scope.body.push_back(std::move(binding));
        Statement bind(Statement::Kind::expression,s.token);
        bind.expression=call("next",state_read());
        bind.expression->op="$generator.async.bind";
        scope.body.push_back(std::move(bind));
        scope.body.push_back(std::move(loop));
        s=std::move(scope);
        return;
    }

    // A declared destructor is a capability, including inaccessible/ambiguous
    // declarations: ordinary with checking must diagnose them rather than omit
    // cleanup. Value structs have no deinit under the existing lifetime model.
    bool resource=generator;
    if(!generator && state.kind==Type::managed_ref) {
        const auto selected=ir::select_member(*root.member_catalog,state,"deinit",unit.identity,nullptr,lexical_owner());
        resource=selected.status!=ir::MemberSelection::missing;
    }
    Statement scope(resource?Statement::Kind::scoped:Statement::Kind::conditional,s.token);
    scope.source_end=s.source_end;
    if(resource) {
        scope.binder=hidden;scope.expression=std::move(factory);
    } else {
        scope.expression=std::make_unique<Expr>(Expr::Kind::literal,s.token);
        scope.expression->type=Type::boolean;scope.expression->value=1;scope.expression->implicit=true;
        Statement binding(Statement::Kind::variable,hidden);binding.name=hidden.text;binding.expression=std::move(factory);
        scope.body.push_back(std::move(binding));
    }
    scope.body.push_back(std::move(loop));s=std::move(scope);
}
