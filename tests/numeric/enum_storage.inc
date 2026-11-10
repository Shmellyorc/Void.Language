// Hand-built typed IR exercises the validator without source analysis.
struct StorageAudit {
    Module module;
    Type enumeration, field, array, field_array, array_field, nested, object, option;
    std::vector<Type> owned;
    unsigned mismatches=0;
    unsigned controls=0;
    unsigned malformed=0;

    StorageAudit() {
        module.identity=module.root_identity="/enum-storage.void";
        module.source_imports[module.identity];
        module.coordinated=true;
        auto d=descriptor("mode");
        d->enum_backing=Type::u8;
        d->numeric_members={{"ready",7}}; // Zero is never a valid member.
        enumeration=Type::nominal(d);
        field=record("field",enumeration);
        array=fixed(enumeration);
        field_array=record("field_array",array);
        array_field=fixed(field);
        nested=fixed(record("middle",fixed(array_field)));
        object=record("holder",nested,true);
        auto sum=std::make_shared<ValueType>();
        sum->name=sum->identity=option_identity(enumeration);
        sum->option_type=true;
        sum->cases={{"None",1,0},{"Some",1,1}};
        sum->fields={{"$tag",Type::i32,sum->identity},{"$some",enumeration,sum->identity}};
        option=Type(sum);
        module.nominal_types={enumeration,field,field_array,object,option};
        Function entry;
        entry.name=module.entry_symbol=module.identity+"::entry";
        entry.source_name="entry";
        entry.module_identity=module.identity;
        entry.blocks={{{integer(Type::i32,0)},returns(0)}};
        module.functions.push_back(std::move(entry));
    }
    std::shared_ptr<ValueType> descriptor(std::string name) {
        auto d=std::make_shared<ValueType>();
        d->name=std::move(name);d->module_identity=module.identity;d->identity=module.identity+"::"+d->name;
        return d;
    }
    Type fixed(Type child) {
        auto d=std::make_shared<ValueType>();
        d->element=child;d->extent=1;d->name=d->identity=array_identity(child,d->extent);
        return owned.emplace_back(d,Type::fixed_array);
    }
    Type record(const char* name,Type member,bool managed=false) {
        auto d=descriptor(name);d->managed=managed;d->fields={{"item",member,d->identity}};
        owned.push_back(Type::nominal(d));
        return owned.back();
    }
    static Instruction integer(Type t,std::int64_t bits) {
        Instruction i{Op::constant};i.type=t;i.immediate=bits;return i;
    }
    static Instruction make(Op op,Type t,std::vector<unsigned> args={}) {
        Instruction i{op};i.type=t;i.arguments=std::move(args);return i;
    }
    static Terminator returns(unsigned id) {return {Terminator::Kind::return_value,id};}
    static Terminator jumps(unsigned block) {return {Terminator::Kind::jump,0,block};}
    static unsigned append(std::vector<Instruction>& code,Instruction i) {
        code.push_back(std::move(i));return static_cast<unsigned>(code.size()-1);
    }
    unsigned complete(std::vector<Instruction>& code,Type t) {
        if(t.kind==Type::numeric_enum)return append(code,integer(t,7));
        if(t==option)return append(code,make(Op::sum,t));
        if(t.kind==Type::fixed_array)return append(code,make(Op::fixed_array,t,{complete(code,array_element(t))}));
        auto child=complete(code,field_type(t,0));
        return append(code,make(t.kind==Type::managed_ref?Op::gc_construct:Op::aggregate,t,{child}));
    }
    unsigned project(std::vector<Instruction>& code,Type t,unsigned value) {
        while(t.kind!=Type::numeric_enum) {
            if(t.kind==Type::fixed_array) {
                auto index=append(code,integer(Type::u64,0));
                value=append(code,{Op::array_get,value,index});t=array_element(t);
            } else {
                value=append(code,{t.kind==Type::managed_ref?Op::gc_read:Op::extract,value});
                t=field_type(t,0);
            }
        }
        return value;
    }
    void run(std::string_view label,Type result,std::vector<Type> locals,std::vector<Block> blocks,std::string_view rejection={},std::optional<unsigned> cleanup={}) {
        auto candidate=module;
        Function f;
        f.source_name="probe";f.module_identity=module.identity;f.name=module.identity+"::probe";
        f.parameters.clear();f.cleanup_entry=cleanup;f.result=result;f.locals=std::move(locals);f.blocks=std::move(blocks);
        candidate.functions.push_back(std::move(f));
        try {
            validate(candidate);
            std::cout<<label<<": accepted\n";
            if(!rejection.empty())++mismatches;
        } catch(const std::invalid_argument& failure) {
            std::cout<<label<<": rejected ("<<failure.what()<<")\n";
            if(rejection.empty() || std::string_view(failure.what()).find(rejection)==std::string_view::npos)++mismatches;
        }
        if(rejection.empty())++controls;else ++malformed;
    }
    void slots() {
        const std::pair<std::string_view,Type> cases[]={{"enum",enumeration},{"field",field},{"array",array},
            {"field-array",field_array},{"array-field",array_field},{"nested",nested},{"managed",object}};
        for(auto [name,type]:cases) {
            std::vector<Instruction> bad{{Op::load,0}};
            auto selected=project(bad,type,0);
            run(name,enumeration,{type},{{std::move(bad),returns(selected)}},
                type.kind==Type::managed_ref?"non-nullable reference storage":"uninitialized numeric enum storage");
            std::vector<Instruction> good;
            auto initial=complete(good,type);
            append(good,{Op::store,0,initial});
            auto value=append(good,{Op::load,0});
            selected=project(good,type,value);
            run(std::string(name)+" initialized",enumeration,{type},{{std::move(good),returns(selected)}});
        }
    }
    void publication() {
        for(Type type:{object,record("legacy_scalar",enumeration,true),record("legacy_array",array,true)}) {
            auto expected=type==object?"nominal source objects require complete construction":"numeric enum fields require complete managed construction";
            run("zero-backed "+type.definition->name,type,{},{{{make(Op::gc_allocate,type)},returns(0)}},expected);
        }
        run("managed missing field",object,{},{{{make(Op::gc_construct,object)},returns(0)}},"invalid complete managed construction");
        run("managed partial buffer",object,{},{{{make(Op::array_allocate,nested),make(Op::gc_construct,object,{0})},returns(1)}},
            "incomplete numeric enum array");
        run("read allocated array before initialization",enumeration,{},
            {{{make(Op::array_allocate,array),integer(Type::u64,0),{Op::array_get,0,1}},returns(2)}},"incomplete numeric enum array");
    }
    void staged_buffers() {
        Instruction capture{Op::array_capture,0};
        run("partial buffer through private home",object,{nested},
            {{{make(Op::array_allocate,nested),capture,{Op::load,0},make(Op::gc_construct,object,{2})},returns(3)}},
            "incomplete numeric enum array");
        run("partial buffer through ordinary store",array,{array},
            {{{make(Op::array_allocate,array),{Op::store,0,0},{Op::load,0}},returns(2)}},
            "incomplete numeric enum array");
        run("forged completion of ordinary value",array,{},
            {{{integer(enumeration,7),make(Op::fixed_array,array,{0}),{Op::array_finish,1}},returns(2)}},
            "invalid numeric enum array completion");
        std::vector<Instruction> code{make(Op::array_allocate,nested),capture,integer(Type::u64,0)};
        auto child=complete(code,array_element(nested));
        auto staged=append(code,{Op::load,0});
        auto initialize=make(Op::array_initialize,Type::none,{child});
        initialize.a=staged;initialize.b=2;
        append(code,initialize);
        auto finished=append(code,{Op::array_finish,staged});
        auto holder=append(code,make(Op::gc_construct,object,{finished}));
        auto selected=project(code,object,holder);
        run("complete private buffer published and projected",enumeration,{nested},{{code,returns(selected)}});
        run("private home absent on one predecessor",nested,{nested},
            {{{integer(Type::boolean,0)},{Terminator::Kind::branch,0,1,2}},
             {{make(Op::array_allocate,nested),capture},jumps(2)},
             {{{Op::load,0},{Op::array_finish,0}},returns(1)}},"uninitialized numeric enum storage");
        run("legacy array initialization of a finished value",array,{},
            {{{integer(enumeration,7),make(Op::fixed_array,array,{0}),integer(Type::u64,0),
               [] {Instruction i{Op::array_initialize,1,2};i.arguments={0};return i;}()},returns(1)}},
            "invalid array element initialization");

        // The completion operation is a checked boundary. A forged attempt
        // can reach it, but the live-prefix check must fail before the value
        // can be published or used, in either optimization mode.
        auto early=module;
        early.functions[0].blocks={{{make(Op::array_allocate,array),{Op::array_finish,0},integer(Type::i32,7)},returns(2)}};
        auto complete_buffer=module;
        Instruction write{Op::array_initialize,0,1};
        write.arguments={2};
        complete_buffer.functions[0].blocks={{{make(Op::array_allocate,array),integer(Type::u64,0),
            integer(enumeration,7),write,{Op::array_finish,0},integer(Type::i32,7)},returns(5)}};
        for(bool optimize:{false,true}) {
            auto ready=vd::Jit{}.compile(complete_buffer,complete_buffer.entry_symbol,optimize,{},std::make_shared<vd::Runtime>());
            check(ready->invoke(0)==7,"complete buffer failed the Runtime completion guard");
            auto entry=vd::Jit{}.compile(early,early.entry_symbol,optimize,{},std::make_shared<vd::Runtime>());
            try {(void)entry->invoke(0);}
            catch(const std::runtime_error& failure) {
                check(std::string_view(failure.what()).find("Invalid fixed value contract")!=std::string_view::npos,failure.what());
                continue;
            }
            throw std::runtime_error("Incomplete array passed the Runtime completion guard");
        }
        std::cout<<"early completion: rejected at the live-prefix guard in normal and O2 JIT\n";
    }
    void managed_staging() {
        Type view=object;
        view.construction=true;
        Function init;
        init.source_name="holder::method::init";
        init.name=module.identity+"::"+init.source_name;
        init.module_identity=module.identity;
        init.method_name="init";
        init.receiver=init.receiver_mutates=init.initializer=true;
        init.parameters=init.locals={view};
        init.result=Type::none;
        init.construction_origins={{0,0}};
        init.blocks={{{},{Terminator::Kind::return_void}}};
        module.functions.push_back(init);
        run("incomplete managed initializer",Type::i32,{},{{{integer(Type::i32,0)},returns(0)}},
            "uninitialized construction field at call/exit/commit");
        std::vector<Instruction> body{{Op::load,0}};
        const auto value=complete(body,nested);
        append(body,{Op::gc_write,0,value});
        module.functions.back().blocks={{std::move(body),{Terminator::Kind::return_void}}};
        run("complete managed initializer",Type::i32,{},{{{integer(Type::i32,0)},returns(0)}});
        module.functions.pop_back();
    }
    void joins() {
        std::vector<Instruction> assigned;
        auto value=complete(assigned,nested);
        append(assigned,{Op::store,0,value});
        std::vector<Instruction> use{{Op::load,0}};
        auto selected=project(use,nested,0);
        const Block branch{{integer(Type::boolean,1)},{Terminator::Kind::branch,0,1,2}};
        const Block set{assigned,jumps(3)}, skip{{},jumps(3)}, join{use,returns(selected)};
        run("join missing true write",enumeration,{nested},{branch,skip,set,join},"uninitialized numeric enum storage");
        run("join missing false write",enumeration,{nested},{branch,set,skip,join},"uninitialized numeric enum storage");
        run("join with both writes",enumeration,{nested},{branch,set,set,join});
        run("unwind before explicit write",enumeration,{nested},{{assigned,jumps(1)},join},
            "uninitialized numeric enum storage",1);
        run("cleanup terminal return load",nested,{nested},
            {{assigned,jumps(1)},{{{Op::load,0}},returns(0)}},{},1);
    }
    void sums() {
        run("unset option",option,{option},{{{{Op::load,0}},returns(0)}},"uninitialized numeric enum storage");
        run("constructed None",option,{option},
            {{{make(Op::sum,option),{Op::store,0,0},{Op::load,0}},returns(2)}});
        Instruction payload{Op::extract,0};payload.immediate=1;
        run("unchecked inactive payload",enumeration,{},{{{make(Op::sum,option),payload},returns(1)}},"sum payload requires checked access");
    }
};
void storage_audit() {
    StorageAudit audit;
    audit.slots();
    audit.publication();
    audit.staged_buffers();
    audit.managed_staging();
    audit.joins();
    audit.sums();
    std::cout<<"storage audit: "<<audit.malformed<<" negative, "<<audit.controls<<" positive, "<<audit.mismatches<<" mismatches\n";
    check(audit.mismatches==0,"typed-IR storage audit failed");
}
