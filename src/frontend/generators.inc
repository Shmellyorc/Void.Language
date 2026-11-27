Function generator_signature(Type receiver,const Token& token) {
    Function method;
    method.token=token;
    method.symbol="$generator."+token.text+"::"+Type::key(receiver);
    if(token.text=="next")method.result=*receiver.definition->generator_result;
    builtin_record(method,token.text=="next"
        ? "Runs the generator until the next yield or completion. Completed generators return None."
        : "Closes the generator and runs reached cleanup. Closing a completed generator is harmless.");
    return method;
}

VOID_SEMANTIC_HANDLER Type generator_method(Expr& e,Type receiver) {
    if(ir::async_generator_type(receiver))
        error(source,e.token,Code::unsupported_type,"An asynchronous generator is consumed in for await.");
    if(receiver.nullable || receiver.readonly)
        error(source,e.token,Code::type_mismatch,"Generator advancement and closure require a mutable, non-null generator.");
    if(e.parts.size()!=1 || !e.type_arguments.empty())
        error(source,e.token,Code::argument_count,"Generator next() and close() take no arguments.");
    auto method=generator_signature(receiver,e.token);
    e.source_target=method.symbol;
    e.target="$generator."+e.token.text;
    return e.type=method.result;
}

VOID_SEMANTIC_HANDLER void generator_member_candidates(Expr& e,Type receiver) {
    if(!ir::async_generator_type(receiver) && !receiver.readonly)for(auto name:{"next","close"}) {
        if(!std::string_view(name).starts_with(diagnostics.editor->prefix))continue;
        auto token=e.token;
        token.text=name;
        auto method=generator_signature(receiver,token);
        auto doc=unit.builtins.at(method.symbol).documentation.text(method.result!=Type::none);
        diagnostics.editor->result.candidates.push_back({name,editor_signature(method),2,method.symbol,doc});
    }
    throw EditorProbeFound{};
}
