    void condition(Expr& e) {
        if(e.kind==Expr::Kind::binary && (e.op=="and" || e.op=="or")) {
            const bool isolated=e.op=="or";
            if(isolated)scopes.emplace_back();
            condition(*e.parts[0]);
            if(isolated)scopes.pop_back();
            const bool previous=suppress_zero;
            if(e.parts[0]->kind==Expr::Kind::literal &&
               ((e.op=="and" && !e.parts[0]->value) || (e.op=="or" && e.parts[0]->value)))suppress_zero=true;
            if(isolated)scopes.emplace_back();
            condition(*e.parts[1]);
            if(isolated)scopes.pop_back();
            suppress_zero=previous;e.type=Type::boolean;return;
        }
        if(e.kind==Expr::Kind::unary && e.op=="not") {
            scopes.emplace_back();condition(*e.parts[0]);scopes.pop_back();e.type=Type::boolean;return;
        }
        pattern_bindings=e.kind==Expr::Kind::test;expect(expression(e),Type::boolean,e.token);pattern_bindings=false;
    }
    bool irrefutable(const Pattern& p) const {
        if(p.kind==Pattern::Kind::wildcard || p.kind==Pattern::Kind::capture)return true;
        if(p.kind==Pattern::Kind::constructor && p.narrowed==p.type && !p.type.nullable)return std::all_of(p.children.begin(),p.children.end(),[&](const auto& child){return irrefutable(child);});
        return false;
    }
    template<class Visit> bool warn_match(const Token& token,Type type,Visit patterns) {
        bool complete=false,present=false,absent=false;std::set<unsigned> covered;std::set<std::uint64_t> booleans;
        patterns([&](const Pattern& p){
            complete|=irrefutable(p);
            present|=p.nullable && p.children.size()==1 && irrefutable(p.children[0]);
            absent|=p.kind==Pattern::Kind::value && p.values[0]->token.text=="null";
            if(p.kind==Pattern::Kind::constructor && type.definition && !type.definition->cases.empty() && std::all_of(p.children.begin(),p.children.end(),[&](const auto& child){return irrefutable(child);}))covered.insert(p.tag);
            if(p.kind==Pattern::Kind::value && p.values[0]->kind==Expr::Kind::sum && p.values[0]->parts.empty())covered.insert(p.values[0]->slot);
            if(type==Type::boolean && p.kind==Pattern::Kind::value && p.values[0]->kind==Expr::Kind::literal)booleans.insert(p.values[0]->value);
        });
        if(type.definition && !type.definition->cases.empty())complete|=covered.size()==type.definition->cases.size();
        if(type==Type::boolean)complete|=booleans.size()==2;
        if(type.nullable)complete|=present && absent;
        if(!complete)diagnostics.warn(source,token,Code::incomplete_match,"Match may leave values unmatched.");return complete;
    }
    void check_pattern(Pattern& p,Type type) {
        Depth guard(diagnostics.semantic_depth(),source,p.token,128);p.type=type;
        using K=Pattern::Kind;
        switch(p.kind) {
        case K::wildcard: return;
        case K::capture:p.slot=declare(p.token,type);return;
        case K::value:{
            auto spelling=[&](const auto& self,const Expr& expression)->std::string{if(expression.kind==Expr::Kind::name)return expression.token.text;if(expression.kind==Expr::Kind::field){auto prefix=self(self,*expression.parts[0]);if(!prefix.empty())return prefix+"."+expression.token.text;}return {};};
            const auto name=spelling(spelling,*p.values[0]);
            if(!name.empty() && !lookup(name.substr(0,name.find('.')))){auto [owner,structure]=structure_lookup(unit,root,units,name,p.token);if(structure){p.kind=K::constructor;p.spelling=name;p.values.clear();check_pattern(p,type);return;}}
            expect(expression(*p.values[0],type),type,p.token);
            if(!(p.values[0]->kind==Expr::Kind::sum && p.values[0]->parts.empty()) && !ir::pattern_comparable(type))error(source,p.token,Code::type_mismatch,"Use explicit list/dictionary patterns for collection contents.");return;
        }
        case K::range:
            if(!numeric::integer(type))error(source,p.token,Code::type_mismatch,"Range patterns require an integer subject.");
            for(auto& value:p.values)expect(expression(*value,type),type,value->token);return;
        case K::list: {
            if(!ir::array_type(type))error(source,p.token,Code::type_mismatch,"List patterns require a fixed or growable array.");
            for(unsigned n=0;n<p.children.size();++n) {
                auto element=ir::array_element(type);
                if(p.rest && n+1==p.children.size())element=slice_type(type);
                check_pattern(p.children[n],element);
            }
            return;
        }
        case K::dictionary:
            if(type.kind!=Type::dictionary)error(source,p.token,Code::type_mismatch,"Dictionary patterns require a dictionary.");
            for(unsigned n=0;n<p.children.size();++n){expect(expression(*p.values[n],ir::dictionary_key(type)),ir::dictionary_key(type),p.values[n]->token);check_pattern(p.children[n],ir::dictionary_value(type));}return;
        case K::constructor: {
            auto first=p.spelling.substr(0,p.spelling.find('.'));bool call=!p.annotated_type && (lookup(first) || unit.declarations.contains(first));
            if(auto qualified=unit.qualifiers.find(first);qualified!=unit.qualifiers.end()){auto rest=p.spelling.substr(first.size()+1);auto function=rest.substr(0,rest.find('.'));for(auto* owner:qualified->second)call|=owner->declarations.contains(function);}
            if(call){
                std::function<std::unique_ptr<Expr>(Pattern&)> value=[&](Pattern& part)->std::unique_ptr<Expr>{
                    if(part.kind==K::value)return std::move(part.values[0]);
                    if(part.kind==K::list){auto expression=std::make_unique<Expr>(Expr::Kind::array_literal,part.token);for(auto& child:part.children)expression->parts.push_back(value(child));return expression;}
                    if(part.kind==K::dictionary){auto expression=std::make_unique<Expr>(Expr::Kind::dictionary_literal,part.token);for(unsigned n=0;n<part.children.size();++n){expression->parts.push_back(std::move(part.values[n]));expression->parts.push_back(value(part.children[n]));}return expression;}
                    if(part.kind!=K::constructor)error(source,part.token,Code::syntax,"A value-pattern call needs ordinary argument expressions.");
                    auto dot=part.spelling.rfind('.');auto token=part.token;token.text=dot==std::string::npos?part.spelling:part.spelling.substr(dot+1);auto expression=std::make_unique<Expr>(dot==std::string::npos?Expr::Kind::call:Expr::Kind::method_call,token);expression->type_arguments=part.type_arguments;
                    if(dot!=std::string::npos){auto prefix=part.spelling.substr(0,dot);auto end=prefix.find('.');Token name=part.token;name.text=prefix.substr(0,end);auto receiver=std::make_unique<Expr>(Expr::Kind::name,name);while(end!=std::string::npos){auto begin=end+1;end=prefix.find('.',begin);name.text=prefix.substr(begin,end-begin);auto field=std::make_unique<Expr>(Expr::Kind::field,name);field->parts.push_back(std::move(receiver));receiver=std::move(field);}expression->parts.push_back(std::move(receiver));}
                    for(unsigned n=0;n<part.children.size();++n){expression->parts.push_back(value(part.children[n]));expression->names.push_back(part.fields[n]);}return expression;
                };
                auto expression=value(p);p.children.clear();p.values.push_back(std::move(expression));p.kind=K::value;expect(this->expression(*p.values[0],type),type,p.token);if(!ir::pattern_comparable(type))error(source,p.token,Code::type_mismatch,"Use explicit collection patterns.");return;
            }

            if((p.spelling=="Ok" || p.spelling=="Err") && type.nullable && (ir::reference_value(type) || (type.definition && type.definition->callable))) {
                if(p.spelling=="Err" || p.children.size()!=1)error(source,p.token,Code::type_mismatch,"Nullable presence uses Ok(pattern); Err is a result variant.");
                p.nullable=true;auto present=type;present.nullable=false;check_pattern(p.children[0],present);return;
            }
            if(type.definition && !type.definition->cases.empty() && p.spelling.substr(p.spelling.rfind('.')==std::string::npos?0:p.spelling.rfind('.')+1)==type.definition->name){
                auto [owner,structure]=structure_lookup(unit,root,units,p.spelling,p.token);if(structure && structure->definition==type.definition){if(p.children.size()>1)error(source,p.token,Code::type_mismatch,"A whole enum type pattern accepts one pattern.");if(p.children.empty()){p.kind=K::wildcard;return;}auto inner=std::move(p.children[0]);p=std::move(inner);check_pattern(p,type);return;}
            }
            if(type.definition && !type.definition->cases.empty()) {
                const auto& cases=type.definition->cases;std::string variant=p.spelling;
                if(!type.definition->result_type) {
                    const auto dot=variant.rfind('.');
                    if(dot==std::string::npos)error(source,p.token,Code::type_mismatch,"Enum patterns require type.variant spelling.");
                    auto [owner,structure]=structure_lookup(unit,root,units,variant.substr(0,dot),p.token);
                    if(!structure || structure->definition!=type.definition)error(source,p.token,Code::type_mismatch,"Enum pattern has a different nominal type.");
                    variant=variant.substr(dot+1);
                }
                auto selected=std::find_if(cases.begin(),cases.end(),[&](const auto& c){return c.name==variant;});
                if(selected==cases.end() || selected->count!=p.children.size())error(source,p.token,Code::type_mismatch,"Enum/result pattern payload does not match its variant.");
                p.tag=static_cast<unsigned>(selected-cases.begin());
                for(unsigned n=0;n<p.children.size();++n){if(!p.fields[n].text.empty())error(source,p.fields[n],Code::syntax,"Variant patterns use positional payloads.");p.indexes.push_back(selected->begin+n);check_pattern(p.children[n],ir::field_type(type,selected->begin+n));}return;
            }
            auto resolved=p.annotated_type?resolve_type(p.narrowed,unit,root,units,p.token):Type(Type::none);
            if(p.annotated_type && (numeric::number(resolved) || resolved==Type::boolean || resolved==Type::string)){
                expect(type,resolved,p.token);p.narrowed=resolved;if(p.children.size()>1)error(source,p.token,Code::type_mismatch,"Scalar type patterns accept one whole-value pattern.");if(!p.children.empty()){p.rest=true;check_pattern(p.children[0],resolved);}return;
            }
            auto [owner,structure]=p.annotated_type?owned_structure(resolved):structure_lookup(unit,root,units,p.spelling,p.token);
            if(structure && !structure->generics.empty()){
                if(!p.type_arguments.empty()){std::vector<Type> arguments;for(auto argument:p.type_arguments)arguments.push_back(resolve_type(argument,unit,root,units,p.token));structure=&unit.specialize_structure(unit,*owner,*structure,arguments,p.token);}
                else {Structure* inferred=nullptr;for(auto& [id,source]:units)for(auto& candidate:source->structures)if(candidate.generic_origin==structure && candidate.definition==type.definition)inferred=&candidate;if(!inferred)error(source,p.token,Code::type_mismatch,"This type pattern needs explicit generic arguments.");structure=inferred;}
            }else if(!p.type_arguments.empty())error(source,p.token,Code::unsupported_type,"This pattern type is not generic.");
            if(!structure || !structure->definition->cases.empty())error(source,p.token,Code::unsupported_type,"Unknown struct type in pattern.");
            auto target=Type(structure->definition,structure->definition->managed?Type::managed_ref:structure->definition->interface_type?Type::interface_ref:Type::aggregate);
            if(target.kind==Type::aggregate && type!=target)error(source,p.token,Code::type_mismatch,"Value type pattern has a different nominal type.");
            if(target.kind==Type::interface_ref) {
                if(!ir::reference_value(type))error(source,p.token,Code::type_mismatch,"Interface patterns require a managed reference or interface subject.");
                for(const auto& [id,u]:units)for(const auto& candidate:u->structures)if(candidate.definition->managed) {
                    Type view(candidate.definition,Type::managed_ref);
                    try {
                        auto binding=ir::structural_binding(*root.member_catalog,view,target,unit.identity);
                        auto& bindings=root.member_catalog->interface_bindings;
                        if(std::none_of(bindings.begin(),bindings.end(),[&](const auto& existing){return existing.key==binding.key;}))bindings.push_back(std::move(binding));
                    }catch(const std::invalid_argument&){}
                }
            }
            if(target.kind==Type::managed_ref && !ir::reference_value(type))error(source,p.token,Code::type_mismatch,"Managed type patterns require a reference subject.");
            p.narrowed=target;
            if(p.children.size()==1 && p.fields[0].text.empty()){p.rest=true;check_pattern(p.children[0],target);return;}
            for(unsigned n=0;n<p.children.size();++n) {
                if(p.fields[n].text.empty())error(source,p.token,Code::syntax,"Struct patterns use named fields or one whole-value pattern.");
                auto found=std::find_if(target.definition->fields.begin(),target.definition->fields.end(),[&](const auto& field){return field.name==p.fields[n].text;});
                if(found==target.definition->fields.end())error(source,p.fields[n],Code::unknown_identifier,"Unknown struct pattern field.");
                const auto index=static_cast<unsigned>(found-target.definition->fields.begin());p.indexes.push_back(index);check_pattern(p.children[n],ir::field_type(target,index));
            }
            return;
        }
        }
    }
    Type check_match(Expr& e,std::optional<Type> expected) {
        auto type=expression(*e.parts[0]);std::optional<Type> result=expected;
        for(unsigned n=0;n<e.patterns.size();++n) {
            scopes.emplace_back();check_pattern(e.patterns[n],type);auto value=expression(*e.parts[n+1],result);
            if(result)expect(value,*result,e.parts[n+1]->token);else result=value;
            scopes.pop_back();
        }
        if(!result || *result==Type::none)error(source,e.token,Code::invalid_value,"A match expression requires value-producing arms.");
        warn_match(e.token,type,[&](auto visit){for(const auto& p:e.patterns)visit(p);});return e.type=*result;
    }
