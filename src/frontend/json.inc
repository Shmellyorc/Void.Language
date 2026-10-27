// JSON adapters are finite, ordinary checked functions. Each concrete type has
// a typed heap stack; one dispatch loop supplies postorder continuations. No
// runtime object metadata, recursive generated calls or access exceptions exist.
VOID_SEMANTIC_HANDLER Type json_call(Expr& e) {
    if(!current || e.type_arguments.size()!=1)
        error(source,e.token,Code::argument_count,"Private JSON conversion requires one concrete type argument.");
    const bool decoding=e.token.text=="_json_from_value";
    const auto requested=resolve_type(e.type_arguments[0],unit,root,units,e.token);
    const auto json_value=Type(unit.structure_names.at("value")->definition,Type::managed_ref);
    const auto context=Type(unit.structure_names.at("_context")->definition,Type::managed_ref);
    auto raw=[](Type t){t.readonly=false;t.readonly_references.clear();return t;};
    auto identity=[&](Type t){return std::to_string(t.kind)+":"+Type::key(t)+(t.nullable?"?":"");};
    std::vector<Type> types;std::vector<std::string> paths;std::map<std::string,unsigned> tags;
    std::vector<std::optional<RelatedLocation>> field_origins;
    std::size_t discovery_work=0;
    auto field_origin=[&](const ir::ValueField& member)->std::optional<RelatedLocation>{
        for(const auto& [id,u]:units)for(const auto& declaration:u->structures)
            if(declaration.definition->identity==member.owner)
                return RelatedLocation{span(u->source,Token{TokenKind::name,member.name,member.source_offset}),"Offending stored field declared here."};
        return {};
    };
    auto discover=[&](Type t,std::string path,std::optional<RelatedLocation> origin={}){t=raw(t);auto key=identity(t);
        if(tags.contains(key))return;
        if(types.size()>=4096 || key.size()>8388608-discovery_work)
            error(source,e.token,Code::limit,"JSON concrete type discovery work limit exceeded.");
        discovery_work+=key.size();tags.emplace(std::move(key),types.size());types.push_back(t);paths.push_back(std::move(path));field_origins.push_back(std::move(origin));};
    discover(requested,"root");
    for(std::size_t n=0;n<types.size();++n) {
        auto t=types[n];const auto path=paths[n];
        auto reject=[&](std::string reason,std::optional<unsigned> field={}) {
            std::vector<RelatedLocation> related;
            if(field_origins[n])related.push_back(*field_origins[n]);
            if(t.definition)for(const auto& [id,u]:units)for(const auto& declaration:u->structures)
                if(declaration.definition==t.definition)related.push_back({span(u->source,declaration.token),"Requested data type declared here."});
            if(field)if(auto origin=field_origin(t.definition->fields.at(*field)))related.push_back(*origin);
            error(source,e.token,Code::unsupported_type,"JSON does not support "+path+" ("+ir::type_name(t)+"): "+reason+". Use an explicit public-data DTO.",{},std::move(related));
        };
        if(t.nullable){if(t.kind!=Type::managed_ref)reject("only nullable managed data is supported");t.nullable=false;discover(t,path+"?",field_origins[n]);continue;}
        if(numeric::number(t) || t==Type::boolean || t==Type::string || t==json_value)continue;
        if(ir::array_type(t)){discover(ir::array_element(t),path+"[]",field_origins[n]);continue;}
        if(t.kind==Type::dictionary){if(ir::dictionary_key(t)!=Type::string)reject("dictionary keys must be strings");discover(ir::dictionary_value(t),path+"{value}",field_origins[n]);continue;}
        if(!t.definition || (t.kind!=Type::aggregate && t.kind!=Type::managed_ref))reject("unsupported value category");
        if(t.definition->result_type || t.definition->callable || t.definition->task_result || t.definition->native_resource || !t.definition->native_provider.empty())reject("result, callable, task and resource storage is excluded");
        if(!t.definition->cases.empty()){
            if(std::any_of(t.definition->cases.begin(),t.definition->cases.end(),[](const auto& c){return c.count!=0;}))reject("enum payloads have no automatic wire schema");
            continue;
        }
        for(auto chain=t.definition;chain;chain=chain->base.lock()) {
            auto [owner,structure]=owned_structure(Type(chain,chain->managed?Type::managed_ref:Type::aggregate));
            if(structure->initializer)reject("custom initializers are excluded");
            bool property=false;
            for(const auto& f:owner->functions)if(f.receiver && f.receiver->type.definition==chain){if(f.token.text=="deinit")reject("custom deinitializers are excluded");property|=f.accessor!=0;}
            if(t.definition->fields.empty() && property)reject("property-only types have no public stored-data schema");
        }
        std::set<std::string> names;
        for(unsigned field=0;field<t.definition->fields.size();++field) {
            const auto& member=t.definition->fields[field];
            if(member.visibility!="public" || member.event)reject("inaccessible or event field '"+member.name+"'",field);
            if(!names.insert(member.name).second)reject("duplicate effective field name '"+member.name+"'",field);
            discover(member.type,path+"."+member.name,field_origin(member));
        }
    }
    // Admission precedes cache lookup, including calls with unsupported fields
    // that runtime input could otherwise avoid.
    const auto key=std::string(decoding?"decode|":"encode|")+identity(requested)+"|"+ir::capability_key(requested)+"|json-v1";
    if(auto found=unit.json_adapters.find(key);found!=unit.json_adapters.end()) {
        e.target=found->second->symbol;return e.type=call_arguments(e,*found->second,0);
    }
    auto tag=[&](Type t){return tags.at(identity(raw(t)));};
    auto scalar=[&](Type t){return t.nullable || numeric::number(t) || t==Type::boolean || t==Type::string || t==json_value || (t.definition && !t.definition->cases.empty());};
    auto array=[](Type element){auto plain=element;plain.readonly=false;plain.readonly_references.clear();auto d=std::make_shared<ir::ValueType>();d->identity=d->name=ir::array_identity(plain,{});d->element=ir::FieldType(plain);Type result(d,Type::growable_array);ir::ReferencePaths paths;for(auto p:ir::readonly_paths(element)){p.insert(p.begin(),ir::array_element_path);paths.insert(std::move(p));}return ir::with_readonly_paths(result,paths);};
    std::map<std::string,Type> bindings{{"jn",ir::readonly_view(json_value)},{"jc",context}};
    auto alias=[&](std::string name,Type t){bindings.emplace(name,t);return name;};
    for(unsigned n=0;n<types.size();++n) {
        auto id=std::to_string(n);alias("jt"+id,types[n]);
        alias("jo"+id,array(ir::readonly_view(types[n])));alias("jr"+id,array(types[n]));
        if(ir::collection_type(types[n])) {
            auto child=types[n].kind==Type::dictionary?ir::dictionary_value(types[n]):ir::array_element(types[n]);
            alias("jl"+id,array(child));alias("jb"+id,array(array(child)));
            auto views=array(ir::readonly_view(child));alias("jv"+id,views);alias("js"+id,array(views));
        }
    }
    auto quote=[](const std::string& text){std::string out="\"";for(unsigned char c:text){if(c=='"' || c=='\\')out+='\\';if(c=='{' || c=='}')out+=c;out+=c;}return out+'"';};
    std::string code;auto line=[&](unsigned indent,const std::string& text){code.append(indent*4,' ');code+=text+'\n';if(code.size()>8388608)error(source,e.token,Code::limit,"JSON adapter source work limit exceeded.");};
    const auto local_name="_json_generated_"+std::to_string(unit.json_adapters.size());
    line(0,"private func "+local_name+"("+(decoding?std::string("const input: jn, context: jc, unknown: unknown_fields"):"const input: jt0, context: jc")+") -> result<"+(decoding?std::string("jt0"):"value")+", error>:");
    line(1,"var work: [i64] = []");
    if(!decoding) {
        line(1,"var frames: [_encode_frame] = []");line(1,"var results: [value] = []");
        for(unsigned n=0;n<types.size();++n)if(!scalar(types[n])){auto id=std::to_string(n);line(1,"var owners"+id+": jo"+id+" = []");if(types[n].kind==Type::dictionary || types[n].kind==Type::growable_array)line(1,"var snapshots"+id+": js"+id+" = []");}
        unsigned serial=0;
        std::function<void(Type,const std::string&,const std::string&,unsigned)> push;
        push=[&](Type t,const std::string& value,const std::string& depth,unsigned level) {
            auto id=std::to_string(tag(t)),next="next"+std::to_string(serial++);line(level,"var "+next+" = "+value);
            if(t.nullable){line(level,"if "+next+" == null:");line(level+1,"results.push(_new_null(context, "+depth+")?)");line(level,"else:");t.nullable=false;push(t,next+" as jt"+std::to_string(tag(t)),depth,level+1);return;}
            if(t==json_value){line(level,"results.push(_clone("+next+", context, "+depth+")?)");return;}
            if(numeric::number(t)){line(level,"results.push(_new_number<jt"+id+">("+next+", context, "+depth+")?)");return;}
            if(t==Type::boolean || t==Type::string){line(level,"results.push("+std::string(t==Type::boolean?"_new_bool":"_new_text")+"("+next+", context, "+depth+")?)");return;}
            if(!t.definition->cases.empty()){
                for(const auto& c:t.definition->cases){line(level,"if "+next+" is jt"+id+"."+c.name+":");line(level+1,"results.push(_new_text("+quote(c.name)+", context, "+depth+")?)");}return;
            }
            const bool object=!ir::array_type(t);auto count=ir::collection_type(t)?next+".count":std::to_string(t.definition->fields.size());
            const auto frame="frame"+std::to_string(serial++);line(level,"var "+frame+" = _begin(context, "+depth+", "+count+", "+(object?"true":"false")+")?");
            if(t.kind==Type::dictionary || t.kind==Type::growable_array){
                const auto snapshot="snapshot"+std::to_string(serial++);line(level,"var "+snapshot+": jv"+id+" = []");
                line(level,"for item"+std::string(t.kind==Type::dictionary?", key":"")+" in "+next+":");
                if(t.kind==Type::dictionary){line(level+1,"if key.byte_length > context.budget.max_string_bytes:");line(level+2,"return Err(context.fail(error_kind.limit_exceeded, \"max_string_bytes\"))");line(level+1,"var charged = context.charge(key.byte_length)?");line(level+1,frame+".names.push(key)");}
                line(level+1,snapshot+".push(item)");line(level,"snapshots"+id+".push("+snapshot+")");
            }
            line(level,"owners"+id+".push("+next+")");line(level,"frames.push("+frame+")");line(level,"work.push("+id+" as i64)");
        };
        push(requested,"input","0",1);
        line(1,"for work.count > 0:");
        for(unsigned n=0;n<types.size();++n)if(!scalar(types[n])) {
            auto t=types[n],child=ir::collection_type(t)?(t.kind==Type::dictionary?ir::dictionary_value(t):ir::array_element(t)):Type::none;auto id=std::to_string(n);
            line(2,"if work[-1] == "+id+":");line(3,"var frame = frames[-1]");line(3,"var owner = owners"+id+"[-1]");line(3,"var transition = context.charge(1)?");
            line(3,"if frame.awaiting:");line(4,"frame.attach(results.remove_at(results.count - 1))");line(4,"var segment = context.path.remove_at(context.path.count - 1)");
            line(3,"if frame.cursor >= frame.count:");line(4,"var owner_done = owners"+id+".remove_at(owners"+id+".count - 1)");
            if(t.kind==Type::dictionary || t.kind==Type::growable_array)line(4,"var snapshot_done = snapshots"+id+".remove_at(snapshots"+id+".count - 1)");
            line(4,"var frame_done = frames.remove_at(frames.count - 1)");line(4,"var work_done = work.remove_at(work.count - 1)");line(4,"results.push(frame.finish())");line(4,"continue");
            line(3,"var cursor = frame.cursor");line(3,"frame.cursor += 1");line(3,"frame.awaiting = true");
            if(ir::collection_type(t)){
                if(t.kind==Type::dictionary){line(3,"frame.key = frame.names[cursor]");line(3,"context.path.push(frame.key)");push(child,"snapshots"+id+"[-1][cursor]","frame.depth",3);}
                else {line(3,"var indexed = _push_index(context, cursor)?");push(child,t.kind==Type::growable_array?"snapshots"+id+"[-1][cursor]":"owner[cursor]","frame.depth",3);}
            } else {
                // The measured wide encoder repeats the same fallible scalar
                // conversion for every field. Select names and typed values,
                // then share conversion/descent per concrete child type.
                std::map<unsigned,std::vector<unsigned>> fields_by_tag;
                line(3,"var child_tag: i64 = -1");
                for(unsigned field=0;field<t.definition->fields.size();++field){const auto& member=t.definition->fields[field];
                    fields_by_tag[tag(member.type)].push_back(field);
                    line(3,"if cursor == "+std::to_string(field)+":");line(4,"frame.key = "+quote(member.name));line(4,"child_tag = "+std::to_string(tag(member.type)));}
                line(3,"context.path.push(frame.key)");
                for(const auto& [child_tag,fields]:fields_by_tag) {
                    line(3,"if child_tag == "+std::to_string(child_tag)+":");
                    const auto field_type=Type(t.definition->fields[fields.front()].type);
                    // A choice stores its value at every join. Keep the
                    // original path for fixed/value data so selecting a field
                    // cannot introduce extra owned-array snapshots.
                    if(field_type.kind==Type::fixed_array || field_type.kind==Type::aggregate) {
                        for(auto field:fields){line(4,"if cursor == "+std::to_string(field)+":");push(t.definition->fields[field].type,"owner."+t.definition->fields[field].name,"frame.depth",5);}
                        continue;
                    }
                    if(fields.size()==1){push(t.definition->fields[fields.front()].type,"owner."+t.definition->fields[fields.front()].name,"frame.depth",4);continue;}
                    // Balanced ordinary if-expressions keep parser/checker
                    // depth logarithmic even for very wide repeated schemas.
                    std::function<void(unsigned,unsigned,unsigned,const std::string&)> select;
                    select=[&](unsigned first,unsigned last,unsigned level,const std::string& prefix) {
                        if(last-first==1){line(level,prefix+"owner."+t.definition->fields[fields[first]].name);return;}
                        auto middle=first+(last-first)/2;
                        line(level,prefix+"if cursor < "+std::to_string(fields[middle])+":");select(first,middle,level+1,"");
                        line(level,"else:");select(middle,last,level+1,"");
                    };
                    select(0,fields.size(),4,"var selected = ");push(t.definition->fields[fields.front()].type,"selected","frame.depth",4);
                }
            }
            line(3,"continue");
        }
        // A scalar-only graph still has a syntactically valid, unreachable loop.
        line(2,"return Err(context.fail(error_kind.limit_exceeded, \"Invalid encoder continuation\"))");
        line(1,"return Ok(results.remove_at(results.count - 1))");
    } else {
        line(1,"var frames: [_decode_frame] = []");
        for(unsigned n=0;n<types.size();++n){auto id=std::to_string(n);line(1,"var results"+id+": jr"+id+" = []");if(ir::collection_type(types[n])){if(types[n].kind==Type::dictionary)line(1,"var builders"+id+": jr"+id+" = []");else line(1,"var builders"+id+": jb"+id+" = []");}}
        // Every pending source is stored in a declared readonly field. No DOM
        // reference enters the mutable typed destination stacks.
        line(1,"frames.push(_decode_frame(node = input))");line(1,"work.push(0 as i64)");line(1,"for work.count > 0:");
        for(unsigned n=0;n<types.size();++n) {
            auto t=types[n];auto id=std::to_string(n);auto done=[&](unsigned level){line(level,"var frame_done = frames.remove_at(frames.count - 1)");line(level,"var work_done = work.remove_at(work.count - 1)");line(level,"continue");};
            auto pop=[&](Type child){auto c=std::to_string(tag(child));return "results"+c+".remove_at(results"+c+".count - 1)";};
            auto push=[&](Type child,const std::string& node,const std::string& depth,unsigned level){line(level,"frames.push(_decode_frame(node = "+node+", depth = "+depth+"))");line(level,"work.push("+std::to_string(tag(child))+" as i64)");};
            line(2,"if work[-1] == "+id+":");line(3,"var frame = frames[-1]");line(3,"var node = frame.node");line(3,"context.location = node.source_offset()");line(3,"var transition = context.charge(1)?");
            if(t.nullable){auto child=t;child.nullable=false;
                line(3,"if frame.awaiting:");line(4,"results"+id+".push("+pop(child)+")");line(3,"else if node.kind is value_kind.null_value:");line(4,"var admitted = context.node(frame.depth)?");line(4,"results"+id+".push(null)");line(3,"else:");line(4,"frame.awaiting = true");push(child,"node","frame.depth",4);line(4,"continue");done(3);continue;
            }
            if(scalar(t)) {
                if(t==json_value)line(3,"results"+id+".push(_clone(node, context, frame.depth)?)");
                else if(numeric::number(t))line(3,"results"+id+".push(_read_number<jt"+id+">(node, context, frame.depth)?)");
                else if(t==Type::string || t==Type::boolean)line(3,"results"+id+".push("+std::string(t==Type::string?"_read_text":"_read_bool")+"(node, context, frame.depth)?)");
                else {line(3,"var text = _read_text(node, context, frame.depth)?");line(3,"var found = false");for(const auto& c:t.definition->cases){line(3,"if _same(context, text, "+quote(c.name)+")?:");line(4,"results"+id+".push(jt"+id+"."+c.name+")");line(4,"found = true");}line(3,"if not found:");line(4,"return Err(context.fail(error_kind.invalid_enum, \"Unknown enum case\"))");}
                done(3);continue;
            }
            const bool object=!ir::array_type(t);auto child=ir::collection_type(t)?(t.kind==Type::dictionary?ir::dictionary_value(t):ir::array_element(t)):Type::none;
            line(3,"if frame.cursor == 0 and not frame.awaiting:");
            const auto extent=t.kind==Type::fixed_array?std::to_string(*t.definition->extent):"-1";
            line(4,"var checked = _read_container(node, context, frame.depth, "+std::string(object?"true":"false")+", "+extent+")?");line(4,"frame.depth = checked.depth");line(4,"frame.count = checked.count");
            if(ir::array_type(t)){line(4,"var builder: jl"+id+" = []");line(4,"builders"+id+".push(builder)");}
            else if(t.kind==Type::dictionary){line(4,"var builder: jt"+id+" = {}");line(4,"builders"+id+".push(builder)");}
            else {
                line(4,"if unknown is unknown_fields.reject:");line(5,"for key, index in node.keys()?:");line(6,"var known = false");
                // Selection is cheap; the fallible comparison and its roots
                // have one body, including comparisons after a successful match.
                line(6,"var field_index: i64 = 0");
                line(6,"for field_index < "+std::to_string(t.definition->fields.size())+":");
                line(7,"var field_name = \"\"");
                for(unsigned field=0;field<t.definition->fields.size();++field){line(7,"if field_index == "+std::to_string(field)+":");line(8,"field_name = "+quote(t.definition->fields[field].name));}
                line(7,"if _same(context, key, field_name)?:");line(8,"known = true");
                line(7,"field_index += 1");
                line(6,"if not known:");line(7,"return Err(context.fail(error_kind.unknown_property, \"Unknown object property\", node.member_offset(index), \"/\" + _segment(key)))");
                line(4,"frame.count = "+std::to_string(t.definition->fields.size()));
            }
            // Mark entry independently of cursor, which remains zero while the
            // first nested value is decoded. -1 is a private awaiting sentinel.
            line(3,"if frame.awaiting:");
            if(ir::array_type(t))line(4,"builders"+id+"[-1].push("+pop(child)+")");
            else if(t.kind==Type::dictionary){line(4,"var key = node.member_name(frame.cursor)");line(4,"var charged = frame.dictionary_charge(context, key)?");line(4,"builders"+id+"[-1][key] = "+pop(child));}
            else line(4,"var finished = context.charge(1)?");
            line(4,"frame.awaiting = false");line(4,"frame.cursor += 1");line(4,"var segment = context.path.remove_at(context.path.count - 1)");
            line(3,"if frame.cursor >= frame.count:");
            if(t.kind==Type::dictionary)line(4,"results"+id+".push(builders"+id+".remove_at(builders"+id+".count - 1))");
            else if(ir::array_type(t)) {
                line(4,"var builder = builders"+id+".remove_at(builders"+id+".count - 1)");
                if(t.kind==Type::growable_array)line(4,"results"+id+".push(builder)");
                else {line(4,"var reversed: jl"+id+" = []");line(4,"for builder.count > 0:");line(5,"var charged = context.charge(2)?");line(5,"reversed.push(builder.remove_at(builder.count - 1))");line(4,"var built = _json_fixed_recipe<jt"+id+">(reversed)");line(4,"results"+id+".push(built)");}
            } else {
                // Keep wide postorder construction out of the continuation's
                // large root frame. An ordinary closure captures only the
                // completed typed stacks, then returns before traversal resumes.
                // This adds constant call depth, never a call per document edge.
                const bool outlined=t.definition->fields.size()>=32;
                const unsigned level=outlined?5:4;
                if(outlined)line(4,"var construct = func() -> jt"+id+":");
                for(unsigned field=t.definition->fields.size();field>0;--field){line(level,"var field"+std::to_string(field-1)+" = "+pop(t.definition->fields[field-1].type));}
                std::string construction="jt"+id+"(";for(unsigned field=0;field<t.definition->fields.size();++field){if(field)construction+=", ";construction+=t.definition->fields[field].name+" = field"+std::to_string(field);}construction+=")";
                if(outlined){line(5,"return "+construction);line(4,"results"+id+".push(construct())");}
                else line(4,"results"+id+".push("+construction+")");
            }
            done(4);
            if(ir::collection_type(t)) {
                line(3,"frame.awaiting = true");
                if(t.kind==Type::dictionary){line(3,"context.path.push(node.member_name(frame.cursor))");push(child,"node.member_child(frame.cursor)?","frame.depth",3);}
                else {line(3,"var indexed = _push_index(context, frame.cursor)?");push(child,"node.at(frame.cursor)?","frame.depth",3);}
                line(3,"continue");
            } else {
                line(3,"var field_name = \"\"");line(3,"var child_tag: i64 = -1");
                std::set<unsigned> nullable_tags;
                for(unsigned field=0;field<t.definition->fields.size();++field) {
                    const auto& member=t.definition->fields[field];
                    line(3,"if frame.cursor == "+std::to_string(field)+":");
                    line(4,"field_name = "+quote(member.name));line(4,"child_tag = "+std::to_string(tag(member.type)));
                    if(Type(member.type).nullable)nullable_tags.insert(tag(member.type));
                }
                // _find must precede the path push, just as in every original
                // field arm. Missing nullable values retain their concrete stack.
                line(3,"var child = _find(node, field_name, context)?");line(3,"context.path.push(field_name)");
                line(3,"if child == null:");
                for(auto nullable:nullable_tags){line(4,"if child_tag == "+std::to_string(nullable)+":");
                    line(5,"var admitted = context.node(frame.depth)?");line(5,"results"+std::to_string(nullable)+".push(null)");
                    line(5,"frame.cursor += 1");line(5,"var segment = context.path.remove_at(context.path.count - 1)");line(5,"continue");}
                line(4,"return Err(context.fail(error_kind.missing_property, \"Missing required object property\"))");
                line(3,"frame.awaiting = true");line(3,"frames.push(_decode_frame(node = child as value, depth = frame.depth))");
                line(3,"work.push(child_tag)");line(3,"continue");
            }
        }
        line(2,"return Err(context.fail(error_kind.limit_exceeded, \"Invalid decoder continuation\"))");line(1,"return Ok(results0.remove_at(results0.count - 1))");
    }
    SourceFile generated(source.path(),code);auto syntax=lex(generated,diagnostics);
    for(auto& token:syntax){token.offset=e.token.offset;token.source_width=0;}
    Unit parsed(unit.source,unit.identity);Parser(unit.source,diagnostics,std::move(syntax),std::move(bindings)).parse(parsed);
    if(parsed.parse_failed || parsed.functions.size()!=1)throw Poison{};
    unit.function_types.splice(unit.function_types.end(),parsed.function_types);unit.results.splice(unit.results.end(),parsed.results);unit.arrays.splice(unit.arrays.end(),parsed.arrays);unit.applications.splice(unit.applications.end(),parsed.applications);
    unit.functions.splice(unit.functions.end(),parsed.functions);auto& adapter=unit.functions.back();
    adapter.json_adapter=true;adapter.prepared=true;adapter.token.source_width=0;adapter.visibility="private";
    adapter.symbol=unit.identity+"::$json$"+std::to_string(unit.json_adapters.size());adapter.concrete_name=adapter.symbol.substr(unit.identity.size()+2);adapter.display_name="<JSON "+std::string(decoding?"decode ":"encode ")+ir::type_name(requested)+">";
    adapter.result=resolve_type(adapter.result,unit,root,units,e.token);for(auto& p:adapter.parameters)p.type=resolve_type(p.type,unit,root,units,p.token);
    // A fixed array recipe already exists for ordinary per-element defaults.
    // Here its independently checked element expression pops a completed typed
    // value, so no uninitialized reference or extent-sized native frame escapes.
    std::vector<Statement*> statements;for(auto& s:adapter.body)statements.push_back(&s);std::vector<Expr*> expressions;
    while(!statements.empty()){auto* s=statements.back();statements.pop_back();if(s->expression)expressions.push_back(s->expression.get());for(auto& child:s->body)statements.push_back(&child);for(auto& child:s->otherwise)statements.push_back(&child);}
    while(!expressions.empty()) {
        auto* x=expressions.back();expressions.pop_back();
        if(x->kind==Expr::Kind::field && x->parts.size()==1 && x->parts[0]->kind==Expr::Kind::name)
            if(auto alias=adapter.substitutions.find(x->parts[0]->token.text);alias!=adapter.substitutions.end() && alias->second.definition && !alias->second.definition->cases.empty()) {
                const auto& cases=alias->second.definition->cases;
                auto found=std::find_if(cases.begin(),cases.end(),[&](const auto& c){return c.name==x->token.text;});
                if(found!=cases.end()){x->kind=Expr::Kind::sum;x->type=alias->second;x->slot=found-cases.begin();x->parts.clear();}
            }
        if(x->kind==Expr::Kind::call && x->token.text=="_json_fixed_recipe") {
            const auto target=x->type_arguments.at(0);auto owner=std::move(x->parts.at(0));
            auto remove=std::make_unique<Expr>(Expr::Kind::method_call,x->token);remove->token.text="remove_at";remove->parts.push_back(std::move(owner));
            auto last=std::make_unique<Expr>(Expr::Kind::literal,x->token);last->type=Type::i64;last->value=last->aggregate_value.bits=~std::uint64_t{0};remove->parts.push_back(std::move(last));remove->names.push_back(Token{TokenKind::name,"",x->token.offset});
            x->kind=Expr::Kind::array_default;x->type=target;x->type_arguments.clear();x->parts.clear();x->names.clear();x->parts.push_back(std::move(remove));
        }
        for(auto& child:x->parts)expressions.push_back(child.get());for(auto& child:x->omitted)expressions.push_back(child.get());
        std::vector<Pattern*> patterns;for(auto& p:x->patterns)patterns.push_back(&p);
        while(!patterns.empty()){auto* p=patterns.back();patterns.pop_back();for(auto& value:p->values)expressions.push_back(value.get());for(auto& child:p->children)patterns.push_back(&child);}
    }
    unit.json_adapters.emplace(key,&adapter);unit.declarations.emplace(adapter.token.text,&adapter);
    Semantics checker(unit,units,root,evaluate,diagnostics,defaults);checker.analyze_one(adapter);
    e.target=adapter.symbol;return e.type=call_arguments(e,adapter,0);
}
