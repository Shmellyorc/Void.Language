#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/runtime.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
using namespace vd::ir;
void check(bool condition,std::string_view message) {
    if(!condition)throw std::runtime_error(std::string(message));
}
void rejects(const Module& module,std::string_view message) {
    try {validate(module);}
    catch(const std::invalid_argument& error) {
        check(std::string_view(error.what()).find(message)!=std::string_view::npos,error.what());
        return;
    }
    throw std::runtime_error("Malformed IR accepted");
}
std::shared_ptr<const vd::ValidatedAnalysis> analysis(const std::string& text) {
    auto result=vd::analyze_source({"enum-reload.void",text});
    if(!result.succeeded())throw vd::CompilationError(std::move(result.diagnostics));
    return result.program;
}
std::string source(std::string_view backing,unsigned high,int answer) {
    return "enum mode("+std::string(backing)+"):\n    low = 3\n    high = "+std::to_string(high)+R"(

struct state:
    current: option<mode>
    payload: [mode]

func entry(raw: i32) -> i32:
    var held = state(current = raw as? mode, payload = [mode.low])
    for index in 0..<1024:
        var temp = state(current = None, payload = [mode.high])
        if not (temp.current is None):
            return 2
    if held.current is Some(mode.high) and held.payload[0] is mode.low:
        return )"+std::to_string(answer)+"\n    return 1\n";
}
void malformed() {
    const auto valid=analysis(source("u8",255,17))->lower();
    auto bad=valid;
    auto& checked=*std::find_if(bad.functions.begin(),bad.functions.end(),[](const auto& function) {
        return std::any_of(function.blocks.begin(),function.blocks.end(),[](const auto& block) {
            return std::any_of(block.instructions.begin(),block.instructions.end(),[](const auto& i){return i.op==Op::try_convert;});
        });
    });
    Type option;
    for(auto& block:checked.blocks)for(auto& i:block.instructions)if(i.op==Op::try_convert) {
        option=i.type;i.type=Type::i32;
    }
    rejects(bad,"invalid fallible integer conversion");

    bad=valid;
    const auto enumeration=field_type(option,1);
    auto probe=Function{};
    probe.name=valid.identity+"::invalid_enum_ir";
    probe.module_identity=valid.identity;
    probe.source_name="invalid_enum_ir";
    probe.parameters.clear();
    probe.result=option;
    Instruction boolean{Op::constant};boolean.type=Type::boolean;boolean.immediate=1;
    Instruction attempt{Op::try_convert};attempt.type=option;
    probe.blocks={{{boolean,attempt},{Terminator::Kind::return_value,1}}};
    bad.functions.push_back(probe);
    rejects(bad,"invalid fallible integer conversion");

    probe.result=enumeration;
    Instruction raw{Op::constant};raw.type=enumeration;raw.immediate=4;
    probe.blocks={{{raw},{Terminator::Kind::return_value,0}}};
    bad.functions.back()=probe;
    rejects(bad,"undeclared numeric enum constant");
    probe.locals={enumeration};
    probe.blocks={{{Instruction{Op::load,0}},{Terminator::Kind::return_value,0}}};
    bad.functions.back()=probe;
    rejects(bad,"uninitialized numeric enum storage");

    auto duplicate=std::make_shared<ValueType>();
    duplicate->identity=valid.identity+"::bad_members";
    duplicate->module_identity=valid.identity;
    duplicate->name="bad_members";
    duplicate->enum_backing=Type::i8;
    duplicate->numeric_members={{"a",127},{"b",127}};
    duplicate->self=duplicate;
    bad=valid;
    bad.nominal_types.push_back(Type::nominal(duplicate));
    rejects(bad,"duplicate numeric enum discriminant");
    duplicate->numeric_members[1].bits=256;
    rejects(bad,"numeric enum discriminant");
    duplicate->enum_backing=Type::f64;
    rejects(bad,"invalid numeric enum descriptor");

    auto forged=std::make_shared<ValueType>(*option.definition);
    forged->identity=forged->name="$option:wrong";
    forged->self=forged;
    bad=valid;
    bad.nominal_types.emplace_back(forged);
    rejects(bad,"invalid canonical option identity");
}
#include "enum_storage.inc"
void literal_source_types() {
    const auto module=analysis(R"(func probe():
    var i32_min = -2147483648 as? i64
    var i32_max = 2147483647 as? i64
    var below_i32 = -2147483649 as? i64
    var above_i32 = 2147483648 as? i64
    var i64_min = -9223372036854775808 as? i64
    var i64_max = 9223372036854775807 as? i64
    var above_i64 = 9223372036854775808 as? u64
    var u64_max = 18446744073709551615 as? u64
)" )->lower();
    std::vector<Type> sources;
    for(const auto& f:module.functions)for(const auto& block:f.blocks)for(const auto& op:block.instructions) {
        if(op.op!=Op::try_convert)continue;
        const auto& source=block.instructions.at(op.a);
        check(source.op==Op::constant,"literal cast lowered a nonconstant operand");
        sources.push_back(source.type);
    }
    check(sources==std::vector<Type>{Type::i32,Type::i32,Type::i64,Type::i64,Type::i64,Type::i64,Type::u64,Type::u64},
          "literal fallible-cast source types disagree at an integer boundary");
}
void reload_and_gc() {
    auto initial=analysis(source("u8",255,17));
    auto body=analysis(source("u8",255,23));
    auto value_edit=analysis(source("u8",254,23));
    auto backing_edit=analysis(source("u16",255,23));
    for(bool optimize:{false,true}) {
        auto runtime=std::make_shared<vd::Runtime>(16);
        vd::Jit jit;
        auto code=jit.compile(initial->lower(),"entry",optimize,{},runtime);
        check(code->invoke(255)==17 && code->invoke(4)==1,"checked option changed across GC allocations");
        check(runtime->snapshot().completed_epoch>0,"no collection took place during live enum/option use");
        {
            vd::Runtime::Attachment attached(*runtime);
            vd::Runtime::Managed managed(*runtime);
            check(runtime->collect()==vd::Runtime::Collection::completed,"forced collection failed");
        }
        check(code->invoke(255)==17,"forced GC invalidated enum or option");
        // Use the ordinary validated-analysis path for generation replacement.
        vd::ReloadSlot slot(jit.compile(*initial,optimize));
        check(jit.replace(slot,*body,optimize) && slot.invoke(255)==23,"body-only edit refused");
        check(!jit.replace(slot,*value_edit,optimize),"discriminant edit reused a generation");
        check(!jit.replace(slot,*backing_edit,optimize),"backing edit reused a generation");
        check(slot.invoke(255)==23 && slot.invoke(254)==1,"rejected reload changed live enum semantics");
    }
}
}
int main() {
    try {
        malformed();
        storage_audit();
        literal_source_types();
        reload_and_gc();
        std::cout<<"numeric enum IR rejection, normal/O2 GC and reload passed\n";
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
