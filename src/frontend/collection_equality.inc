// Content equality is derived as an ordinary private function, like enum
// equality. Element comparisons retain the normal operator/access/effect rules;
// dictionary keys retain the existing data-only hash/equality contract.
Type collection_equality(Expr& expression,Type collection) {
    // Structural type cycles can produce cyclic mutable collection graphs.
    // Managed/interface/callable leaves stop at their existing identity/custom
    // equality; recursive structural shapes diagnose instead of recursing forever.
    std::set<const ir::ValueType*> active,done;
    std::vector<std::pair<Type,bool>> work{{collection,false}};
    while(!work.empty()) {
        auto [type,exit]=work.back();work.pop_back();
        if(!type.definition || type.kind==Type::managed_ref || type.kind==Type::interface_ref || type.definition->callable || type.definition->custom_equality)continue;
        const bool structural=ir::collection_type(type) || (type.kind==Type::aggregate && !type.definition->cases.empty());
        if(!structural)continue;
        const auto* identity=type.definition.get();
        if(exit){active.erase(identity);done.insert(identity);continue;}
        if(done.contains(identity))continue;
        if(!active.insert(identity).second)error(source,expression.token,Code::numeric_operator,"Collection equality requires a non-recursive structural element shape; managed references retain identity equality.");
        if(active.size()>128)error(source,expression.token,Code::nesting,"Collection equality type nesting limit exceeded.");
        work.emplace_back(type,true);
        if(ir::collection_type(type))work.emplace_back(type.kind==Type::dictionary?ir::dictionary_value(type):ir::array_element(type),false);
        else for(const auto& field:type.definition->fields)work.emplace_back(field.type,false);
    }
    const auto parent=current?current->symbol:std::string{};
    const auto key=std::to_string(parent.size())+":"+parent+"collection:"+Type::key(collection)+"|"+ir::capability_key(collection);
    Function* helper=nullptr;
    if(auto found=unit.enum_equalities.find(key);found!=unit.enum_equalities.end())helper=found->second;
    else {
        Function function;function.token=expression.token;function.token.text="$equality$collection$"+std::to_string(unit.enum_equalities.size());
        function.symbol=(parent.empty()?unit.identity:parent)+"::"+function.token.text;function.visibility="private";
        function.concrete_name=function.symbol.substr(unit.identity.size()+2);function.display_name="<collection equality>";
        if(current){function.lexical_owner=lexical_owner();function.lexical_parent=parent;function.substitutions=current->substitutions;function.substitution_sources=current->substitution_sources;function.specialization_depth=current->specialization_depth;function.instantiation_context=current->instantiation_context;}
        function.prepared=true;function.enum_adapter=true;function.collection_adapter=true;function.result=Type::boolean;
        function.body_syntax="collection-equality-v1:"+key;
        auto token=[&](std::string text){auto site=expression.token;site.kind=TokenKind::name;site.text=std::move(text);return site;};
        auto name=[&](const std::string& value){return std::make_unique<Expr>(Expr::Kind::name,token(value));};
        auto boolean=[&](bool value){auto e=std::make_unique<Expr>(Expr::Kind::literal,token(value?"true":"false"));e->type=Type::boolean;e->value=value;return e;};
        auto binary=[&](std::string op,std::unique_ptr<Expr> a,std::unique_ptr<Expr> b){auto e=std::make_unique<Expr>(Expr::Kind::binary,expression.token);e->op=std::move(op);e->parts.push_back(std::move(a));e->parts.push_back(std::move(b));return e;};
        auto negate=[&](std::unique_ptr<Expr> value){auto e=std::make_unique<Expr>(Expr::Kind::unary,expression.token);e->op="not";e->parts.push_back(std::move(value));return e;};
        auto returned=[&](std::unique_ptr<Expr> value){Statement s(Statement::Kind::returned,expression.token);s.expression=std::move(value);return s;};
        auto conditional=[&](std::unique_ptr<Expr> condition){Statement s(Statement::Kind::conditional,expression.token);s.expression=std::move(condition);return s;};
        auto variable=[&](std::string label,std::unique_ptr<Expr> value){Statement s(Statement::Kind::variable,token(label));s.name=std::move(label);s.readonly=true;s.expression=std::move(value);return s;};
        auto call=[&](std::string label,std::vector<std::unique_ptr<Expr>> args){auto e=std::make_unique<Expr>(Expr::Kind::call,token(label));for(auto& a:args){e->parts.push_back(std::move(a));e->names.push_back(token(""));}return e;};
        auto guard=[&](std::string label,bool check){std::vector<std::unique_ptr<Expr>> args;args.push_back(name(label));if(check)args.push_back(name(label+"_version"));return call(check?"$collection_check":"$collection_version",std::move(args));};
        auto count=[&](std::string label){auto e=std::make_unique<Expr>(Expr::Kind::field,token("count"));e->parts.push_back(name(label));return e;};
        const auto operand=ir::readonly_view(collection);
        const std::string left="$collection_left",right="$collection_right";
        function.parameters.push_back({token(left),operand});function.parameters.push_back({token(right),operand});
        std::string a=left,b=right;
        auto length=conditional(binary("!=",count(a),count(b)));length.body.push_back(returned(boolean(false)));function.body.push_back(std::move(length));
        const bool dynamic=collection.kind!=Type::fixed_array;
        if(dynamic)for(const auto& label:{a,b})function.body.push_back(variable(label+"_version",guard(label,false)));
        auto checks=[&](std::vector<Statement>& body){if(dynamic)for(const auto& label:{a,b}){Statement check(Statement::Kind::expression,expression.token);check.expression=guard(label,true);body.push_back(std::move(check));}};
        Statement loop(Statement::Kind::loop,expression.token);loop.binder=token("$collection_value");loop.index_binder=token("$collection_index");loop.expression=name(a);
        checks(loop.body);
        if(collection.kind==Type::dictionary) {
            auto contains=std::make_unique<Expr>(Expr::Kind::method_call,token("contains"));contains->parts.push_back(name(b));contains->parts.push_back(name("$collection_index"));contains->names.push_back(token(""));
            auto missing=conditional(negate(std::move(contains)));missing.body.push_back(returned(boolean(false)));loop.body.push_back(std::move(missing));
        }
        auto indexed=std::make_unique<Expr>(Expr::Kind::array_index,expression.token);indexed->parts.push_back(name(b));indexed->parts.push_back(name("$collection_index"));
        loop.body.push_back(variable("$collection_matches",binary("==",name("$collection_value"),std::move(indexed))));
        checks(loop.body);
        auto different=conditional(negate(name("$collection_matches")));different.body.push_back(returned(boolean(false)));loop.body.push_back(std::move(different));
        function.body.push_back(std::move(loop));checks(function.body);function.body.push_back(returned(boolean(true)));
        unit.functions.push_back(std::move(function));helper=&unit.functions.back();unit.enum_equalities.emplace(key,helper);
        // The prepared body is picked up by the ordinary pending-function pass.
        // Analyzing it here would retain every parent's semantic frames while
        // nested element comparisons request their own helpers.
    }
    const bool negate=expression.op=="!=";
    expression.kind=Expr::Kind::call;expression.op="$collection_equal";expression.target=helper->symbol;
    expression.argument_indexes={0,1};expression.type=Type::boolean;
    if(negate){auto call=std::make_unique<Expr>(std::move(expression));expression=Expr(Expr::Kind::unary,call->token);expression.op="not";expression.type=Type::boolean;expression.parts.push_back(std::move(call));}
    return Type::boolean;
}
