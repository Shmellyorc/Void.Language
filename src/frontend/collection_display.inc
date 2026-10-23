// One ordinary generated function drives a heap work stack. Each concrete owner
// kind (including sum payload continuations) has typed owner/cursor stacks; no boxed payloads, runtime type
// dispatch, recursive collection calls or executable collection metadata.
Function& collection_display_adapter(Type type,const Token& site) {
    const auto key=current->symbol+"|collections|"+std::to_string(type.kind)+":"+Type::key(type)+"|"+ir::capability_key(type);
    if(auto found=unit.display_adapters.find(key);found!=unit.display_adapters.end())return *found->second;
    Function f;f.token=site;f.token.text="$display$collections$"+std::to_string(unit.display_adapters.size());f.token.source_width=0;
    f.symbol=current->symbol+"::"+f.token.text;f.concrete_name=f.symbol.substr(unit.identity.size()+2);f.visibility="private";
    f.prepared=true;f.display_adapter=true;f.collection_adapter=true;f.display_name="<display "+ir::type_name(type)+">";f.result=Type::string;
    f.lexical_owner=lexical_owner();f.lexical_parent=current->symbol;
    auto token=[&](std::string name){Token t{TokenKind::name,std::move(name),site.offset};t.source_width=0;return t;};
    using E=std::unique_ptr<Expr>;using Body=std::vector<Statement>;
    auto name=[&](std::string text){return std::make_unique<Expr>(Expr::Kind::name,token(std::move(text)));};
    auto number=[&](std::uint64_t value,Type t=Type::i64){auto e=std::make_unique<Expr>(Expr::Kind::literal,site);e->type=t;e->value=e->aggregate_value.bits=value;return e;};
    auto text=[&](std::string value){auto e=std::make_unique<Expr>(Expr::Kind::literal,site);e->type=Type::string;e->aggregate_value.text=std::move(value);return e;};
    auto call=[&](std::string spelling,E a={},E b={}){auto e=std::make_unique<Expr>(Expr::Kind::call,token(std::move(spelling)));if(a){e->parts.push_back(std::move(a));e->names.push_back(token(""));}if(b){e->parts.push_back(std::move(b));e->names.push_back(token(""));}return e;};
    auto method=[&](std::string owner,std::string spelling,E value){auto e=std::make_unique<Expr>(Expr::Kind::method_call,token(std::move(spelling)));e->parts.push_back(name(std::move(owner)));if(value){e->parts.push_back(std::move(value));e->names.push_back(token(""));}return e;};
    auto binary=[&](std::string op,E a,E b){auto e=std::make_unique<Expr>(Expr::Kind::binary,site);e->op=std::move(op);e->implicit=true;e->parts.push_back(std::move(a));e->parts.push_back(std::move(b));return e;};
    auto count=[&](std::string owner){auto e=std::make_unique<Expr>(Expr::Kind::field,token("count"));e->parts.push_back(name(std::move(owner)));return e;};
    auto last=[&](std::string owner){return binary("-",count(owner),number(1));};
    auto index=[&](std::string owner,E at){auto e=std::make_unique<Expr>(Expr::Kind::array_index,site);e->parts.push_back(name(std::move(owner)));e->parts.push_back(std::move(at));return e;};
    auto top=[&](std::string owner){return index(owner,last(owner));};
    auto expression=[&](Body& body,E e){Statement s(Statement::Kind::expression,site);s.expression=std::move(e);body.push_back(std::move(s));};
    auto append=[&](Body& body,E e){expression(body,call("$collection_append",std::move(e)));};
    auto variable=[&](Body& body,std::string id,E value,std::optional<Type> type={}){Statement s(Statement::Kind::variable,token(id));s.name=id;s.expression=std::move(value);s.annotation=type;body.push_back(std::move(s));};
    auto assign=[&](Body& body,E place,E value){Statement s(Statement::Kind::assignment,site);s.name=place->parts.at(0)->token.text;s.selection=std::move(place);s.expression=std::move(value);s.op="=";body.push_back(std::move(s));};
    auto condition=[&](Body& body,E test)->Statement&{Statement s(Statement::Kind::conditional,site);s.expression=std::move(test);body.push_back(std::move(s));return body.back();};
    auto stack_type=[&](Type element){auto d=std::make_shared<ir::ValueType>();auto plain=element;plain.readonly=false;plain.readonly_references.clear();d->identity=d->name=ir::array_identity(plain,{});d->element=ir::FieldType(plain);Type result(d,Type::growable_array);ir::ReferencePaths paths;for(auto p:ir::readonly_paths(element)){p.insert(p.begin(),ir::array_element_path);paths.insert(std::move(p));}return ir::with_readonly_paths(result,paths);};
    auto stack=[&](std::string id,Type element){auto e=std::make_unique<Expr>(Expr::Kind::array_literal,site);variable(f.body,id,std::move(e),stack_type(element));};
    std::vector<Type> types;std::map<std::string,unsigned> tags;
    auto identity=[](Type t){return std::to_string(t.kind)+":"+Type::key(t)+"|"+ir::capability_key(ir::readonly_view(t));};
    auto composite=[](Type t){return ir::collection_type(t) || (t.kind==Type::aggregate && t.definition && !t.definition->cases.empty());};
    auto discover=[&](Type t){t.nullable=false;t=ir::readonly_view(t);if(composite(t) && !tags.contains(identity(t))){tags.emplace(identity(t),types.size());types.push_back(t);}};
    discover(type);for(std::size_t n=0;n<types.size();++n){auto t=types[n];if(!t.definition->cases.empty()){for(unsigned field=1;field<t.definition->fields.size();++field)discover(ir::field_type(t,field));}else if(t.kind==Type::dictionary){discover(ir::dictionary_key(t));discover(ir::dictionary_value(t));}else discover(ir::array_element(t));}
    auto id=[](std::string part,unsigned tag){return "$"+part+std::to_string(tag);};
    f.parameters.push_back({token("$value"),ir::readonly_view(type)});stack("$work",Type::i64);
    for(unsigned n=0;n<types.size();++n){stack(id("owners",n),types[n]);stack(id("cursor",n),Type::i64);if(ir::collection_type(types[n]))stack(id("version",n),Type::u64);if(types[n].kind==Type::dictionary){stack(id("phase",n),Type::i64);stack(id("keys",n),ir::dictionary_key(types[n]));stack(id("values",n),ir::dictionary_value(types[n]));}}
    auto push=[&](Body& body,Type t,E value){
        // Capture once, before allocation/hooks; ordinary values retain their copy
        // rules and reference leaves retain the source readonly projection.
        const auto capture="$next"+std::to_string(body.size());variable(body,capture,std::move(value));
        Body* nonnull=&body;
        if(t.nullable){auto nil=std::make_unique<Expr>(Expr::Kind::literal,token("null"));nil->type=Type::null_value;auto& branch=condition(body,binary("==",name(capture),std::move(nil)));append(branch.body,text("null"));nonnull=&branch.otherwise;}
        auto raw=t;raw.nullable=false;
        auto receiver=[&](){auto e=name(capture);if(t.nullable){auto cast=std::make_unique<Expr>(Expr::Kind::conversion,site);cast->type=ir::readonly_view(raw);cast->parts.push_back(std::move(e));return cast;}return e;};
        if(!composite(raw)){append(*nonnull,call("string",receiver()));return;}
        auto tag=tags.at(identity(raw));Body* entered=nonnull;
        if(raw.kind==Type::growable_array || raw.kind==Type::dictionary){auto& branch=condition(*nonnull,call("$collection_enter",receiver()));append(branch.otherwise,text("<cycle>"));entered=&branch.body;}
        expression(*entered,method(id("owners",tag),"push",receiver()));expression(*entered,method(id("cursor",tag),"push",number(0)));
        if(ir::collection_type(raw))expression(*entered,method(id("version",tag),"push",call("$collection_version",receiver())));
        if(raw.kind==Type::dictionary)expression(*entered,method(id("phase",tag),"push",number(0)));
        expression(*entered,method("$work","push",number(tag)));if(ir::collection_type(raw))append(*entered,text(raw.kind==Type::dictionary?"{":"["));
    };
    push(f.body,type,name("$value"));
    Statement loop(Statement::Kind::loop,site);loop.expression=binary(">",count("$work"),number(0));
    for(unsigned n=0;n<types.size();++n){auto t=types[n];auto owners=id("owners",n),cursor=id("cursor",n),versions=id("version",n),phase=id("phase",n);auto owner=id("owner",n),position=id("position",n);
        auto& branch=condition(loop.body,binary("==",top("$work"),number(n)));auto& body=branch.body;
        variable(body,owner,top(owners));variable(body,position,top(cursor));
        if(!t.definition->cases.empty()) {
            const auto selected=id("case",n);variable(body,selected,call("$collection_tag",name(owner)));
            auto& invalid=condition(body,binary("or",binary("<",name(selected),number(0,Type::i32)),binary(">=",name(selected),number(t.definition->cases.size(),Type::i32))));expression(invalid.body,call("$collection_invalid"));
            for(unsigned c=0;c<t.definition->cases.size();++c){const auto& variant=t.definition->cases[c];
                auto& arm=condition(body,binary("==",name(selected),number(c,Type::i32)));auto& opened=condition(arm.body,binary("==",name(position),number(0)));
                const auto label=t.definition->result_type?variant.name:t.definition->identity+"."+variant.name;
                append(opened.body,text(label+(variant.count?"(":"")));assign(opened.body,top(cursor),number(1));
                Body* pending=&opened.otherwise;
                for(unsigned field=0;field<variant.count;++field){auto& payload=condition(*pending,binary("==",name(position),number(field+1)));if(field)append(payload.body,text(", "));
                    assign(payload.body,top(cursor),number(field+2));push(payload.body,ir::field_type(t,variant.begin+field),call("$collection_payload",name(owner),number(variant.begin+field,Type::i32)));pending=&payload.otherwise;
                }
                if(variant.count)append(*pending,text(")"));for(auto key:{owners,cursor,std::string("$work")})expression(*pending,method(key,"remove_at",last(key)));
            }
            Statement again(Statement::Kind::continued,site);body.push_back(std::move(again));continue;
        }
        expression(body,call("$collection_check",name(owner),top(versions)));
        auto& ended=condition(body,binary("==",name(position),count(owner)));
        if(t.kind!=Type::fixed_array)expression(ended.body,call("$collection_leave",name(owner)));
        for(auto key:{owners,cursor,versions,std::string("$work")})expression(ended.body,method(key,"remove_at",last(key)));
        if(t.kind==Type::dictionary)expression(ended.body,method(phase,"remove_at",last(phase)));
        append(ended.body,text(t.kind==Type::dictionary?"}":"]"));
        auto& next=ended.otherwise;
        if(t.kind==Type::dictionary){
            auto keys=id("keys",n),values=id("values",n);
            auto& key=condition(next,binary("==",top(phase),number(0)));auto& separator=condition(key.body,binary(">",name(position),number(0)));append(separator.body,text(", "));
            variable(key.body,"$entry",call("$collection_entry",name(owner),name(position)));expression(key.body,method(keys,"push",call("$collection_key",name("$entry"))));expression(key.body,method(values,"push",call("$collection_value",name("$entry"))));assign(key.body,top(phase),number(1));push(key.body,ir::dictionary_key(t),top(keys));
            auto& value=condition(key.otherwise,binary("==",top(phase),number(1)));append(value.body,text(": "));assign(value.body,top(phase),number(2));push(value.body,ir::dictionary_value(t),top(values));
            for(auto key:{keys,values})expression(value.otherwise,method(key,"remove_at",last(key)));assign(value.otherwise,top(cursor),binary("+",name(position),number(1)));assign(value.otherwise,top(phase),number(0));
        }else{auto& separator=condition(next,binary(">",name(position),number(0)));append(separator.body,text(", "));assign(next,top(cursor),binary("+",name(position),number(1)));push(next,ir::array_element(t),index(owner,name(position)));}
        // Exactly one state transition per dispatch, even if it pushes/pops a tag.
        Statement again(Statement::Kind::continued,site);body.push_back(std::move(again));
    }
    f.body.push_back(std::move(loop));Statement returned(Statement::Kind::returned,site);returned.expression=call("$collection_finish");f.body.push_back(std::move(returned));
    unit.functions.push_back(std::move(f));auto& adapter=unit.functions.back();unit.display_adapters.emplace(key,&adapter);unit.declarations.emplace(adapter.token.text,&adapter);
    Semantics checker(unit,units,root,evaluate,diagnostics,defaults);checker.analyze_one(adapter);return adapter;
}
