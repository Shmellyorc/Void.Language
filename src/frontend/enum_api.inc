// Enum conveniences are compiler-owned declarations with editor documentation.
// Their implementations are ordinary typed expressions/functions, not reflection
// objects or a second enum representation.
bool enum_value_type(Type t) const {
    return t.kind==Type::numeric_enum || (t.kind==Type::aggregate && t.definition && !t.definition->cases.empty() && !t.definition->result_type && !t.definition->option_type);
}
Function enum_api_signature(Type t,const Token& site,const std::string& operation) {
    Function target;target.token=site;target.symbol="$enum.api::"+Type::key(t)+"::"+operation;
    const bool binary=operation=="has" || operation=="has_any" || operation=="with" || operation=="without" || operation=="toggled";
    target.result=operation=="name"?Type(Type::string):operation=="value"?Type(*t.definition->enum_backing):
        operation=="values"?list_type(t):operation=="is_defined" || operation=="has" || operation=="has_any"?Type(Type::boolean):t;
    if(binary)target.parameters.push_back({Token{TokenKind::name,"mask",site.offset},t});
    const std::string description=operation=="name"?"Declared case/member name; an unnamed flags value uses its decimal backing value.":
        operation=="value"?"Extract the exact underlying integer without changing its width or signedness.":
        operation=="values"?"Declared numeric or payload-free enum values, in declaration order; no unnamed flag combinations are invented.":
        operation=="is_defined"?"True only for an exactly declared enum case/member, not every valid flags combination.":
        operation=="has"?"True when all mask bits are present. The zero mask is always present.":
        operation=="has_any"?"True when any mask bit is present. The zero mask always returns false.":
        operation=="with"?"Return a new flags value with mask bits set; the receiver is unchanged.":
        operation=="without"?"Return a new flags value with mask bits cleared; the receiver is unchanged.":
        "Return a new flags value with mask bits toggled; the receiver is unchanged.";
    builtin_record(target,description);auto& record=unit.builtins.at(target.symbol);
    record.kind=(operation=="name" || operation=="value")?AnalyzedDeclaration::Kind::value_field:AnalyzedDeclaration::Kind::receiver_method;
    record.static_member=operation=="values";record.readonly=true;record.receiver=t;record.owner=Type::key(t);
    record.signature=(operation=="name" || operation=="value")?operation+": "+ir::type_name(target.result):editor_signature(target);
    record.documentation.returns=DocumentationText{description,span(source,site)};
    if(binary)record.documentation.parameters.push_back({"mask","Bits from the same flags enum; no unrelated enum or integer is implicitly converted.",span(source,site)});
    return target;
}
Function& enum_api_helper(Type t,const Token& site,const std::string& operation) {
    const auto key=Type::key(t)+"::"+operation;
    if(auto found=unit.enum_metadata.find(key);found!=unit.enum_metadata.end())return *found->second;
    auto target=enum_api_signature(t,site,operation);
    Function function;function.token=site;function.token.text="$enum_api$"+std::to_string(unit.enum_metadata.size());
    function.symbol=unit.identity+"::"+function.token.text;function.visibility="private";function.result=target.result;function.prepared=true;function.enum_adapter=true;
    function.body_syntax="enum-api-v1:"+key;
    auto token=[&](std::string text){auto value=site;value.kind=TokenKind::name;value.text=std::move(text);return value;};
    auto name=[&](std::string text){return std::make_unique<Expr>(Expr::Kind::name,token(std::move(text)));};
    auto literal=[&](Type type,std::uint64_t bits){auto value=std::make_unique<Expr>(Expr::Kind::literal,site);value->type=type;value->value=bits;return value;};
    auto text=[&](std::string spelling){auto value=literal(Type::string,0);value->aggregate_value.text=std::move(spelling);return value;};
    auto returned=[&](std::unique_ptr<Expr> value){Statement statement(Statement::Kind::returned,site);statement.expression=std::move(value);return statement;};
    auto binary=[&](std::string op,std::unique_ptr<Expr> left,std::unique_ptr<Expr> right){auto value=std::make_unique<Expr>(Expr::Kind::binary,site);value->op=std::move(op);value->parts.push_back(std::move(left));value->parts.push_back(std::move(right));return value;};
    function.parameters.push_back({token("$enum_value"),ir::readonly_view(t)});
    if(!target.parameters.empty()) {
        function.parameters.push_back({token("$enum_mask"),t});
        auto mask=name("$enum_mask");
        auto bits=binary(operation=="with"?"|":operation=="toggled"?"^":"&",name("$enum_value"),std::move(mask));
        if(operation=="without") {auto complement=std::make_unique<Expr>(Expr::Kind::unary,site);complement->op="~";complement->parts.push_back(std::move(bits->parts[1]));bits->parts[1]=std::move(complement);}
        if(operation=="has")bits=binary("==",std::move(bits),name("$enum_mask"));
        if(operation=="has_any")bits=binary("!=",std::move(bits),literal(t,0));
        function.body.push_back(returned(std::move(bits)));
    } else {
        const auto count=t.kind==Type::numeric_enum?t.definition->numeric_members.size():t.definition->cases.size();
        for(unsigned n=0;n<count;++n) {
            const auto spelling=t.kind==Type::numeric_enum?t.definition->numeric_members[n].name:t.definition->cases[n].name;
            function.body_syntax+="|"+spelling;
            Statement branch(Statement::Kind::conditional,site);
            if(t.kind==Type::numeric_enum)branch.expression=binary("==",name("$enum_value"),literal(t,t.definition->numeric_members[n].bits));
            else {
                branch.expression=std::make_unique<Expr>(Expr::Kind::test,site);branch.expression->parts.push_back(name("$enum_value"));
                Pattern pattern;pattern.kind=Pattern::Kind::constructor;pattern.token=token(spelling);pattern.spelling=spelling;pattern.derived_case=true;
                for(unsigned k=0;k<t.definition->cases[n].count;++k){Pattern wildcard;wildcard.token=site;pattern.children.push_back(std::move(wildcard));pattern.fields.push_back(token(""));}
                branch.expression->patterns.push_back(std::move(pattern));
            }
            branch.body.push_back(returned(operation=="name"?text(spelling):literal(Type::boolean,1)));function.body.push_back(std::move(branch));
        }
        if(operation=="is_defined")function.body.push_back(returned(literal(Type::boolean,0)));
        else if(t.kind==Type::numeric_enum) {
            auto conversion=std::make_unique<Expr>(Expr::Kind::conversion,site);conversion->type=*t.definition->enum_backing;conversion->parts.push_back(name("$enum_value"));
            auto display=std::make_unique<Expr>(Expr::Kind::call,token("string"));display->parts.push_back(std::move(conversion));display->names.push_back(token(""));function.body.push_back(returned(std::move(display)));
        } else function.body.push_back(returned(text(""))); // Unreachable for a valid tagged enum.
    }
    unit.functions.push_back(std::move(function));auto& helper=unit.functions.back();unit.enum_metadata.emplace(key,&helper);
    Semantics checker(unit,units,root,evaluate,diagnostics,defaults);checker.analyze_one(helper);return helper;
}
std::optional<Type> enum_value_api(Expr& e,Type type,bool call) {
    if(!enum_value_type(type))return {};
    if(direct_method(type,e.token))return {}; // Preserve explicitly declared enum receiver methods.
    const auto operation=e.token.text;
    const bool property=operation=="name" || (operation=="value" && type.kind==Type::numeric_enum);
    const bool method=operation=="is_defined" || (type.kind==Type::numeric_enum && type.definition->enum_flags &&
        (operation=="has" || operation=="has_any" || operation=="with" || operation=="without" || operation=="toggled"));
    if(!property && !method)return {};
    if(call==property)error(source,e.token,Code::not_callable,property?"Enum name/value are properties; omit parentheses.":"This enum operation requires a method call.");
    auto target=enum_api_signature(type,e.token,operation);e.source_target=target.symbol;
    if(method)call_arguments(e,target,1);
    if(operation=="value") {e.kind=Expr::Kind::conversion;e.type=target.result;e.op="$enum_value";return e.type;}
    auto& helper=enum_api_helper(type,e.token,operation);e.kind=Expr::Kind::call;e.target=helper.symbol;e.op="$enum."+operation;
    e.argument_indexes.clear();for(unsigned n=0;n<e.parts.size();++n)e.argument_indexes.push_back(n);
    return e.type=helper.result;
}
