// Display adapters are ordinary compiler-owned functions. Their parameters capture
// values in source order; their bodies reuse ordinary selection, calls and effects.
Function& nominal_display_adapter(Type type,bool formatted,const Token& site) {
    if(!current)error(source,site,Code::constant_call,"Custom display requires runtime evaluation.");
    if(!formatted && (ir::collection_type(type) || !type.definition->cases.empty()) && !type.nullable)return collection_display_adapter(type,site);
    const auto identity=std::to_string(type.kind)+":"+std::to_string(Type::key(type).size())+":"+Type::key(type)+(type.nullable?"?":"")+"|"+ir::capability_key(type);
    const auto key=std::to_string(current->symbol.size())+":"+current->symbol+identity+(formatted?"|format":"|string");
    if(auto found=unit.display_adapters.find(key);found!=unit.display_adapters.end())return *found->second;
    Function function;function.token=site;function.token.text="$display$"+std::to_string(unit.display_adapters.size());function.token.source_width=0;
    function.symbol=current->symbol+"::"+function.token.text;function.concrete_name=function.symbol.substr(unit.identity.size()+2);
    function.visibility="private";function.prepared=true;function.display_adapter=true;function.display_name="<display "+ir::type_name(type)+">";
    function.lexical_owner=lexical_owner();function.lexical_parent=current->symbol;
    function.result=formatted?unit.make_result(Type::string,Type::string):Type(Type::string);
    auto token=[&](std::string name){Token t{TokenKind::name,std::move(name),site.offset};t.source_width=0;return t;};
    function.parameters.push_back({token("$value"),ir::readonly_view(type)});
    if(formatted)function.parameters.push_back({token("$spec"),Type::string});
    auto name=[&](std::string spelling){return std::make_unique<Expr>(Expr::Kind::name,token(std::move(spelling)));};
    auto text=[&](std::string value){auto e=std::make_unique<Expr>(Expr::Kind::literal,site);e->type=Type::string;e->aggregate_value.text=std::move(value);return e;};
    auto call=[&](std::string spelling,std::unique_ptr<Expr> first,std::unique_ptr<Expr> second={}){auto e=std::make_unique<Expr>(Expr::Kind::call,token(std::move(spelling)));e->parts.push_back(std::move(first));e->names.push_back(token(""));if(second){e->parts.push_back(std::move(second));e->names.push_back(token(""));}return e;};
    auto nonnullable=type;nonnullable.nullable=false;
    auto receiver=[&](){auto e=name("$value");if(type.nullable){auto cast=std::make_unique<Expr>(Expr::Kind::conversion,site);cast->type=ir::readonly_view(nonnullable);cast->parts.push_back(std::move(e));return cast;}return e;};
    bool has_string=false,has_format=false;
    auto hook=[&](std::string spelling)->std::unique_ptr<Expr>{
        // Interfaces, resources and callables have opaque displays, never a
        // runtime concrete-type hook search or private-payload reflection.
        if(ir::collection_type(nonnullable) || !nonnullable.definition->cases.empty() || nonnullable.kind==Type::interface_ref || nonnullable.definition->native_resource || nonnullable.definition->callable || nonnullable.definition->task_result)return {};
        auto method=token(spelling);auto selected=ir::select_member(*root.member_catalog,nonnullable,spelling,unit.identity,nullptr,lexical_owner());
        if(auto* generic=direct_method(nonnullable,method);generic || selected.status!=ir::MemberSelection::missing) {
            if(selected.status==ir::MemberSelection::inaccessible || (selected.status==ir::MemberSelection::found && selected.method.empty()))return {};
            if(spelling=="string")has_string=true;else has_format=true;
            // Ordinary semantic checking performs specialization, promotion,
            // original-owner imports, ambiguity and exact signature checks.
            auto e=std::make_unique<Expr>(Expr::Kind::method_call,method);e->parts.push_back(receiver());
            if(spelling=="format"){e->parts.push_back(name("$pattern"));e->names.push_back(token(""));}
            return e;
        }
        return {};
    };
    auto default_value=[&](){if(ir::collection_type(nonnullable) || !nonnullable.definition->cases.empty()){auto& target=collection_display_adapter(nonnullable,site);return call(target.token.text,receiver());}auto e=hook("string");if(e)return e;std::string label;
        if(nonnullable.definition->callable)label="<callable>";
        else if(nonnullable.definition->native_resource || console::endpoint_type(nonnullable))label="<resource>";
        else if(nonnullable.definition->task_result)label="<task>";
        else {auto shown=nonnullable;shown.readonly=false;shown.readonly_references.clear();label="<"+ir::type_name(shown)+">";}
        return text(std::move(label));
    };
    auto equal=[&](std::unique_ptr<Expr> left,std::unique_ptr<Expr> right){auto e=std::make_unique<Expr>(Expr::Kind::binary,site);e->op="==";e->implicit=true;e->parts.push_back(std::move(left));e->parts.push_back(std::move(right));return e;};
    auto choice=[&](std::unique_ptr<Expr> condition,std::unique_ptr<Expr> yes,std::unique_ptr<Expr> no){auto e=std::make_unique<Expr>(Expr::Kind::choice,site);e->parts.push_back(std::move(condition));e->parts.push_back(std::move(yes));e->parts.push_back(std::move(no));return e;};
    std::unique_ptr<Expr> body;
    auto propagated=[&](std::unique_ptr<Expr> value){auto e=std::make_unique<Expr>(Expr::Kind::propagate,site);e->parts.push_back(std::move(value));return e;};
    auto variable=[&](std::string spelling,std::unique_ptr<Expr> value){Statement statement(Statement::Kind::variable,token(spelling));statement.name=std::move(spelling);statement.expression=std::move(value);if(statement.name=="$rendered")statement.annotation=function.result;function.body.push_back(std::move(statement));};
    if(formatted){
        variable("$pattern",propagated(call("$display_pattern",name("$spec"))));
        variable("$alignment",propagated(call("$display_alignment",name("$spec"))));
        auto custom = hook("format");
        if (custom && nonnullable.definition->module_identity == "/@stdlib/time.void") {
            custom = call("$temporal_result", std::move(custom));
        }
        if(!custom)custom=call("Err",text("format at byte 0: Type has no custom format hook"));
        body=choice(equal(name("$pattern"),text("")),call("Ok",default_value()),std::move(custom));
    }
    else body=default_value();
    if(type.nullable){auto null=std::make_unique<Expr>(Expr::Kind::literal,token("null"));null->type=Type::null_value;
        auto missing=formatted?call("$display_format",text("null"),name("$pattern")):text("null");body=choice(equal(name("$value"),std::move(null)),std::move(missing),std::move(body));}
    if(formatted){
        variable("$rendered",std::move(body));
        auto condition=std::make_unique<Expr>(Expr::Kind::test,site);condition->parts.push_back(name("$rendered"));
        Pattern ok;ok.kind=Pattern::Kind::constructor;ok.token=token("Ok");ok.spelling="Ok";ok.fields.push_back(token(""));Pattern captured;captured.kind=Pattern::Kind::capture;captured.token=token("$text");ok.children.push_back(std::move(captured));condition->patterns.push_back(std::move(ok));
        auto aligned=choice(std::move(condition),call("$display_format",name("$text"),name("$alignment")),name("$rendered"));
        body=choice(equal(name("$alignment"),text("")),name("$rendered"),std::move(aligned));
    }
    function.display_has_string=has_string;function.display_has_format=has_format;
    Statement returned(Statement::Kind::returned,site);returned.expression=std::move(body);function.body.push_back(std::move(returned));
    unit.functions.push_back(std::move(function));auto& adapter=unit.functions.back();unit.display_adapters.emplace(key,&adapter);unit.declarations.emplace(adapter.token.text,&adapter);
    Semantics checker(unit,units,root,evaluate,diagnostics,defaults);checker.analyze_one(adapter);
    return adapter;
}
