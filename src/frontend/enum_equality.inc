// Derived enum equality is an ordinary, private function in the requesting Unit.
// Its case tests and payload == expressions use the existing semantic checker,
// including custom operators, nominal identity, readonly views and diagnostics.
Type enum_equality(Expr& expression,Type enumeration) {
    const auto parent=current?current->symbol:std::string{};
    const auto key=std::to_string(parent.size())+":"+parent+Type::key(enumeration)+"|"+ir::capability_key(enumeration);
    Function* helper=nullptr;
    if(auto found=unit.enum_equalities.find(key);found!=unit.enum_equalities.end())helper=found->second;
    else {
        Function function;function.token=expression.token;
        function.token.text="$equality$enum$"+std::to_string(unit.enum_equalities.size());
        function.symbol=(parent.empty()?unit.identity:parent)+"::"+function.token.text;function.visibility="private";
        function.concrete_name=function.symbol.substr(unit.identity.size()+2);function.display_name="<enum equality>";
        if(current){function.lexical_owner=lexical_owner();function.lexical_parent=parent;function.substitutions=current->substitutions;function.substitution_sources=current->substitution_sources;function.specialization_depth=current->specialization_depth;function.instantiation_context=current->instantiation_context;}
        function.prepared=true;function.enum_adapter=true;function.result=Type::boolean;
        function.body_syntax="enum-equality-v1:"+key;
        const auto operand=ir::readonly_view(enumeration);
        auto token=[&](std::string text){auto site=expression.token;site.kind=TokenKind::name;site.text=std::move(text);return site;};
        function.parameters.push_back({token("$enum_left"),operand});
        function.parameters.push_back({token("$enum_right"),operand});
        auto name=[&](const std::string& value){return std::make_unique<Expr>(Expr::Kind::name,token(value));};
        auto boolean=[&](bool value){auto e=std::make_unique<Expr>(Expr::Kind::literal,token(value?"true":"false"));e->type=Type::boolean;e->value=value;return e;};
        auto returned=[&](std::unique_ptr<Expr> value){Statement s(Statement::Kind::returned,expression.token);s.expression=std::move(value);return s;};
        for(const auto& variant:enumeration.definition->cases) {
            function.body_syntax+="|"+variant.name+":"+std::to_string(variant.count);
            auto condition=[&](bool right){
                auto test=std::make_unique<Expr>(Expr::Kind::test,expression.token);
                test->parts.push_back(name(right?"$enum_right":"$enum_left"));
                Pattern pattern;pattern.kind=Pattern::Kind::constructor;pattern.token=token(variant.name);
                pattern.spelling=variant.name;pattern.derived_case=true;
                for(unsigned n=0;n<variant.count;++n) {
                    Pattern capture;capture.kind=Pattern::Kind::capture;
                    capture.token=token(std::string(right?"$right":"$left")+std::to_string(n));
                    pattern.children.push_back(std::move(capture));pattern.fields.push_back(token(""));
                }
                test->patterns.push_back(std::move(pattern));return test;
            };
            Statement left(Statement::Kind::conditional,expression.token);left.expression=condition(false);
            Statement right(Statement::Kind::conditional,expression.token);right.expression=condition(true);
            std::unique_ptr<Expr> value=boolean(true);
            for(unsigned n=0;n<variant.count;++n) {
                auto comparison=std::make_unique<Expr>(Expr::Kind::binary,expression.token);comparison->op="==";
                comparison->parts.push_back(name("$left"+std::to_string(n)));comparison->parts.push_back(name("$right"+std::to_string(n)));
                if(n==0)value=std::move(comparison);
                else {auto conjunction=std::make_unique<Expr>(Expr::Kind::binary,expression.token);conjunction->op="and";conjunction->parts.push_back(std::move(value));conjunction->parts.push_back(std::move(comparison));value=std::move(conjunction);}
            }
            right.body.push_back(returned(std::move(value)));left.body.push_back(std::move(right));
            left.body.push_back(returned(boolean(false)));function.body.push_back(std::move(left));
        }
        function.body.push_back(returned(boolean(false)));
        unit.functions.push_back(std::move(function));helper=&unit.functions.back();
        unit.enum_equalities.emplace(key,helper);
        // Use the pending-function pass, as collection equality does. Payloads
        // can alternate enums and collections; neither edge may analyze a child
        // helper while the parent's semantic frames are still live.
    }
    const bool negate=expression.op=="!=";
    expression.kind=Expr::Kind::call;expression.op="$enum_equal";expression.target=helper->symbol;
    expression.argument_indexes={0,1};expression.type=Type::boolean;
    if(negate){auto call=std::make_unique<Expr>(std::move(expression));expression=Expr(Expr::Kind::unary,call->token);expression.op="not";expression.type=Type::boolean;expression.parts.push_back(std::move(call));}
    return Type::boolean;
}
