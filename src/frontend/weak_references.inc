// Weak cells use nominal reference storage, but have no source constructor or fields.
static constexpr auto weak_creation_doc =
    "Creates a non-owning reference to a managed object. The reference does not keep the target alive.";
static constexpr auto weak_promotion_doc =
    "Returns a strong reference to the target if it is still live, or null after collection or release.";

VOID_SEMANTIC_HANDLER Type weak_creation(Expr& e) {
    if(e.parts.size()!=1 || !e.type_arguments.empty() ||
       (!e.names.empty() && !e.names[0].text.empty() && e.names[0].text!="target"))
        error(source,e.token,Code::argument_count,"weak(target) requires one managed object and infers its type.");
    auto target_type=expression(*e.parts[0]);
    if(target_type==error_type)throw Poison{};
    if(!ir::weak_target_type(target_type))
        error(source,e.parts[0]->token,Code::type_mismatch,"A weak reference requires a non-null concrete managed object.");
    auto result=unit.make_weak(target_type);
    Function target;
    target.token=e.token;
    target.symbol="$weak.create::"+Type::key(result)+ir::capability_key(result);
    target.parameters.push_back({Token{TokenKind::name,"target",e.token.offset},target_type});
    target.result=result;
    builtin_record(target,weak_creation_doc);
    e.source_target=target.symbol;
    e.target="$weak.create";
    return e.type=result;
}

Function weak_promotion_signature(Type receiver,const Token& token) {
    Function method;
    method.token=token;
    method.symbol="$weak.lock::"+Type::key(receiver)+ir::capability_key(receiver);
    method.result=*receiver.definition->weak_target;
    method.result.nullable=true;
    if(receiver.readonly)method.result=ir::readonly_view(method.result);
    builtin_record(method,weak_promotion_doc);
    return method;
}

VOID_SEMANTIC_HANDLER Type weak_promotion(Expr& e,Type receiver) {
    if(receiver.nullable)
        error(source,e.token,Code::type_mismatch,"lock() requires a non-null weak reference.");
    if(e.parts.size()!=1 || !e.type_arguments.empty())
        error(source,e.token,Code::argument_count,"Weak reference lock() takes no arguments.");
    auto method=weak_promotion_signature(receiver,e.token);
    e.source_target=method.symbol;
    e.target="$weak.lock";
    return e.type=method.result;
}

VOID_SEMANTIC_HANDLER void weak_member_candidates(Expr& e,Type receiver) {
    if(!std::string_view("lock").starts_with(diagnostics.editor->prefix))throw EditorProbeFound{};
    auto site=e.token;
    site.text="lock";
    auto method=weak_promotion_signature(receiver,site);
    diagnostics.editor->result.candidates.push_back({"lock",editor_signature(method),2,method.symbol,weak_promotion_doc});
    throw EditorProbeFound{};
}
