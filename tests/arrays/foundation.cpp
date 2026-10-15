#include "void/frontend.hpp"
#include "void/native_storage.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <tuple>
#include <stdexcept>
namespace {
using namespace vd;
using ir::Type;
void check(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::shared_ptr<const ValidatedAnalysis> analyze(const std::string& text) {
    auto result=analyze_source({"arrays.void",text});
    if(!result.succeeded()) throw CompilationError(result.diagnostics);
    return result.program;
}
void negative(const std::string& text,DiagnosticCode code) {
    auto result=analyze_source({"negative.void",text});
    check(!result.succeeded(),"negative source unexpectedly accepted: "+text);
    check(std::any_of(result.diagnostics.begin(),result.diagnostics.end(),[&](const auto& d){return d.code==code;}),
          "wrong diagnostic: "+render_diagnostic(result.diagnostics.front()));
    for(const auto& d:result.diagnostics) check(d.primary.source && d.primary.end<=text.size(),"lost source span");
}
void reject_ir(ir::Module module) {
    try {ir::validate(module);} catch(const std::invalid_argument&) {return;}
    throw std::runtime_error("forged array IR accepted");
}
void source_and_ir() {
    auto program=analyze(
        "const last = data[-1]\nconst data: [i64, size] = [20, 42]\nconst size = 1 + 1\n"
        "func echo(values: [i64, 2]) -> [i64, size]:\n    return values\n"
        "func entry(x: i32) -> i32:\n    var numbers: [i64, size] = [20, 22]\n"
        "    numbers[-1] += 1\n    var empty: [bool, 0] = []\n    var defaults: [i32, 2]\n"
        "    return numbers[0] as i32\n");
    bool folded=false;
    for(const auto& declaration:program->declarations()) if(declaration.identity.ends_with("::last"))
        folded=declaration.type==Type::i64 && declaration.value==42;
    check(folded,"forward extent/constant indexing did not fold exactly");
    auto module=program->lower();ir::validate(module);
    const auto& echo=module.functions.front();
    check(echo.parameters[0]==echo.result && echo.parameters[0].definition==echo.result.definition,"array type not canonical");
    check(ir::type_name(echo.result)=="[i64, 2]" && echo.result.definition->fields.empty(),"structural formatting or fake field layout");
    bool zero=false, defaults=false, get=false, set=false;
    for(const auto& f:module.functions) for(const auto& b:f.blocks) for(const auto& i:b.instructions) {
        if(i.op==ir::Op::fixed_array && i.arguments.empty()) zero=*i.type.definition->extent==0;
        if(i.op==ir::Op::fixed_array && ir::array_element(i.type)==Type::i32 && i.arguments.size()==2) defaults=true;
        if(i.op==ir::Op::array_get) {get=true;check(!i.source_excerpt.empty(),"missing checked access snapshot");}
        if(i.op==ir::Op::array_set) set=true;
    }
    check(zero && defaults && get && set,"missing scalar fixed IR/default/access");
    auto bad=module;
    for(auto& f:bad.functions) for(auto& b:f.blocks) for(auto& i:b.instructions)
        if(i.op==ir::Op::fixed_array && !i.arguments.empty()) {i.arguments.pop_back();reject_ir(bad);goto wrong_extent_done;}
wrong_extent_done:
    bad=module;
    for(auto& f:bad.functions) for(auto& b:f.blocks) for(auto& i:b.instructions)
        if(i.op==ir::Op::fixed_array) {
            auto definition=std::make_shared<ir::ValueType>(*i.type.definition);
            definition->extent=65537;definition->identity=ir::array_identity(ir::array_element(i.type),65537);
            definition->self.reset();definition->arena.reset();
            i.type=Type(definition,Type::fixed_array);reject_ir(bad);goto forged_done;
        }
forged_done:
    bad=module;
    for(auto& f:bad.functions) for(auto& b:f.blocks) for(auto& i:b.instructions)
        if(i.op==ir::Op::array_get) {i.b=i.a;reject_ir(bad);goto wrong_index_done;}
wrong_index_done:
    bad=module;
    for(auto& f:bad.functions) for(auto& b:f.blocks) for(auto& i:b.instructions)
        if(i.op==ir::Op::array_set) {i.arguments[0]=i.a;reject_ir(bad);goto wrong_value_done;}
wrong_value_done:
    bad=module;
    for(auto& f:bad.functions) for(auto& b:f.blocks) for(auto& i:b.instructions)
        if(i.op==ir::Op::array_get) {i.immediate=8;reject_ir(bad);goto stride_done;}
stride_done:
    // Public/query Types retain their structural arena after the analysis dies.
    auto retained=echo.result;program.reset();module={};bad={};
    check(ir::array_element(retained)==Type::i64,"array element outlived its arena");
    auto order=analyze("func indexer() -> i32:\n    return 0\nfunc rhs() -> i32:\n    return 7\n"
                       "func entry(x: i32) -> i32:\n    var items: [i32, 1] = [x]\n"
                       "    items[indexer()] = rhs()\n    return items[0]\n")->lower();
    const auto& instructions=order.functions.back().blocks.front().instructions;
    std::size_t index_call=0,bounds=0,rhs_call=0,update=0;unsigned indexes=0,rhss=0;
    for(std::size_t n=0;n<instructions.size();++n) {
        const auto& i=instructions[n];
        if(i.op==ir::Op::call && i.callee.ends_with("::indexer")) {index_call=n;++indexes;}
        if(i.op==ir::Op::call && i.callee.ends_with("::rhs")) {rhs_call=n;++rhss;}
        if(i.op==ir::Op::array_get && !bounds) bounds=n;
        if(i.op==ir::Op::array_set) update=n;
    }
    check(indexes==1 && rhss==1 && index_call<bounds && bounds<rhs_call && rhs_call<update,"owner/index/check/RHS order or duplicate evaluation");
    auto fields=analyze("value struct point:\n    width: i32 = 2\nconst dimensions = point()\n"
                        "const size = dimensions.width\nvalue struct holder:\n    items: [i32, size]\n"
                        "func entry(x: i32) -> i32:\n    var value = holder()\n    value.items[0] = x\n    return value.items[0]\n");
    ir::validate(fields->lower());
    SourceOverrides overlays;
    overlays.emplace("/main.void",SourceFile("main.void","use \"/sizes.void\" as sizes\n"
        "func entry(x: i32) -> i32:\n    var values: [i32, sizes.width] = [1, 2]\n    return values[0]\n"));
    overlays.emplace("/sizes.void",SourceFile("sizes.void","const width = 2\n"));
    auto imported=analyze_project(std::filesystem::current_path()/"main.void",overlays);
    if(!imported.succeeded()) throw CompilationError(imported.diagnostics);
    ir::validate(imported.program->lower());
    check(std::any_of(imported.program->references().begin(),imported.program->references().end(),
        [](const auto& edge){return edge.to=="/sizes.void::width" && edge.source.source;}),"imported extent dependency missing");
    overlays["/sizes.void"]=SourceFile("sizes.void","private const width = 2\n");
    check(!analyze_project(std::filesystem::current_path()/"main.void",overlays).succeeded(),"private extent escaped ownership");
    for(const auto& type:{"i8","i16","i32","i64","u8","u16","u32","u64","f32","f64"})
        ir::validate(analyze(std::string("func scalar() -> ")+type+":\n    var a: ["+type+", 1] = [42]\n    return a[-1]\n")->lower());
    ir::validate(analyze("func scalar() -> bool:\n    var a: [bool, 1] = [true]\n    return a[0]\n")->lower());
    for(const auto& index_type:{"i8","i16","i32","i64","u8","u16","u32","u64"})
        ir::validate(analyze(std::string("func at(values: [i32, 1], index: ")+index_type+") -> i32:\n    return values[index]\n")->lower());
    ir::validate(analyze("func yes() -> bool:\n    return true\nfunc cfg() -> bool:\n"
        "    var a: [bool, 2] = [yes(), yes() and yes()]\n"
        "    a[0] = yes() and yes()\n    return a[0]\n")->lower());
    auto selected_read=analyze("struct holder:\n    items: [i32, 1]\n"
        "func select(owner: holder) -> i32:\n    owner.items[0] = 7\n    return 0\n"
        "func read(owner: holder) -> i32:\n    return owner.items[select(owner)]\n")->lower();
    std::size_t selector=0,field_read=0;
    const auto& selected_instructions=selected_read.functions.back().blocks.front().instructions;
    for(std::size_t n=0;n<selected_instructions.size();++n) {
        const auto& i=selected_instructions[n];
        if(i.op==ir::Op::call) selector=n;
        if(i.op==ir::Op::gc_read) field_read=n;
    }
    check(selector && field_read>selector,"array bytes selected before index-expression effects");
    auto readonly=analyze("func consume(const values: [i32, 1]):\n    pass\n");
    auto writable=analyze("func consume(values: [i32, 1]):\n    pass\n");
    check(readonly->lower().functions.front().parameters.front().readonly &&
          readonly->compatibility_contract()!=writable->compatibility_contract(),"const fixed-formal contract lost before borrow planning");
}
void negatives() {
    const std::string header="func entry(x: i32) -> i32:\n";
    negative(header+"    var a: [, 2]\n    return x\n",DiagnosticCode::unsupported_type);
    for(const auto& type:{"[i32,]","[i32 2]"}) negative(header+"    var a: "+type+"\n    return x\n",DiagnosticCode::syntax);
    negative(header+"    var a: [i32, -1]\n    return x\n",DiagnosticCode::integer_range);
    for(const auto& extent:{"true","2.5"}) negative(header+"    var a: [i32, "+extent+"]\n    return x\n",DiagnosticCode::unsupported_type);
    negative(header+"    var a: [i32, 65537]\n    return x\n",DiagnosticCode::nesting);
    analyze(header+"    var a: [i8, 65536]\n    return x\n"); // Existing shape limit, not native-budget approval.
    std::string deep="i32";for(unsigned i=0;i<160;++i) deep="["+deep+", 0]";
    negative(header+"    var a: "+deep+"\n    return x\n",DiagnosticCode::nesting);
    std::string casts="0";for(unsigned i=0;i<600;++i) casts+=" as [i32, 1]";
    auto bounded=analyze_source({"casts.void",header+"    var a = "+casts+"\n    return x\n"});
    check(!bounded.succeeded() && std::any_of(bounded.diagnostics.begin(),bounded.diagnostics.end(),[](const auto& d) {
        return d.code==DiagnosticCode::nesting && d.description=="Expression size limit exceeded.";
    }),"type extents reset the enclosing expression work budget");

    negative(header+"    var a: [i32, x]\n    return x\n",DiagnosticCode::unknown_identifier);
    negative(header+"    const n = 2\n    var a: [i32, n]\n    return x\n",DiagnosticCode::unknown_identifier);
    negative(header+"    var a: [i32, 2] = [1]\n    return x\n",DiagnosticCode::type_mismatch);
    negative(header+"    var a: [i32, 0] = [1]\n    return x\n",DiagnosticCode::type_mismatch);
    negative(header+"    var a: [bool, 1] = [1]\n    return x\n",DiagnosticCode::type_mismatch);
    negative(header+"    var a: [i8, 1] = [128]\n    return x\n",DiagnosticCode::integer_range);
    negative(header+"    var a: [i32, 1] = [1]\n    return a[true]\n",DiagnosticCode::type_mismatch);
    negative(header+"    var a: [i32, 1] = [1]\n    return a[0.5]\n",DiagnosticCode::type_mismatch);
    const std::string located="func at() -> i32:\n    var items: [i32, 1] = [1]\n    return items[true]\n";
    auto location=analyze_source({"location.void",located});
    check(!location.succeeded() && location.diagnostics.front().primary.begin==located.find("true") &&
          location.diagnostics.front().primary.source->location(location.diagnostics.front().primary.begin).line==3,
          "index diagnostic did not retain the exact offending source operand");
    negative(header+"    const a: [i32, 1] = [1]\n    a[0] = 2\n    return x\n",DiagnosticCode::readonly);
    negative("func consume(const values: [i32, 1]):\n    values[0] = 2\n",DiagnosticCode::readonly);
    negative("const a: [i32, 0] = []\nconst b = a[0]\n",DiagnosticCode::invalid_value);
    negative("const a: [i32, 1] = [42]\nconst b = a[-2]\n",DiagnosticCode::invalid_value);
    negative("const a: [i32, 1] = [42]\nconst b = a[-9223372036854775808 as i64]\n",DiagnosticCode::invalid_value);
    negative("const a: [i32, 1] = [42]\nconst b = a[18446744073709551615 as u64]\n",DiagnosticCode::invalid_value);
    negative("const a: [i32, n] = [1]\nconst n = a[0]\n",DiagnosticCode::constant_cycle);
    negative("const a = b\nconst b = a\n"+header+"    var v: [i32, a]\n    return x\n",DiagnosticCode::constant_cycle);
    negative("value struct holder:\n    a: [i32, 32768]\n    b: [i32, 32768]\n",DiagnosticCode::nesting);
    negative("value struct recursive:\n    a: [recursive, 1]\n",DiagnosticCode::unsupported_type);
    negative("value struct point:\n    width: i32\nfunc (self point) init():\n    pass\n"
             "const dimensions = point()\nconst size = dimensions.width\n"+header+
             "    var a: [i32, size]\n    return x\n",DiagnosticCode::constant_call);
    // Later checkpoints stay explicit, including future string indexing.
    negative(header+"    var a = []\n    return x\n",DiagnosticCode::unsupported_type);
    negative(header+"    var a: [i32]\n    return x\n",DiagnosticCode::unsupported_type);
    negative(header+"    var a: [string, 1] = [\"x\"]\n    return x\n",DiagnosticCode::unsupported_type);
    negative(header+"    return \"x\"[0]\n",DiagnosticCode::unsupported_type);
    negative("struct holder:\n    items: [i32, 1]\nfunc frozen(owner: holder):\n"
             "    const ro = owner\n    var alias = ro\n    alias.items[0] = 3\n",DiagnosticCode::readonly);
}
void indexes_and_budget() {
    check(ir::checked_array_index(~std::uint64_t{0},Type::i64,3)==2,"negative last");
    check(ir::checked_array_index(253,Type::i8,3)==0,"negative first");
    for(auto [bits,type,extent]:{std::tuple{std::uint64_t{1}<<63,Type::i64,std::uint64_t{3}},
                              std::tuple{~std::uint64_t{0},Type::u64,std::uint64_t{3}},
                              std::tuple{std::uint64_t{0},Type::i32,std::uint64_t{0}}}) {
        bool rejected=false;try {ir::checked_array_index(bits,type,extent);} catch(const std::out_of_range&) {rejected=true;}
        check(rejected,"index extrema/empty accepted");
    }
    StorageTarget target{"test-only-x64","explicit-test-layout",std::numeric_limits<std::uint64_t>::max(),16,
                         [](Type){return StorageExtent{8,8};}};
    bool missing_target=false;try {NativeStoragePlanner absent({});} catch(const std::invalid_argument&) {missing_target=true;}
    check(missing_target,"native planner fell back to a host layout");
    const std::set<StorageRole> all{StorageRole::parameter,StorageRole::result,StorageRole::local,StorageRole::instruction,
                                  StorageRole::copy,StorageRole::roots,StorageRole::staging,StorageRole::known_abi};
    NativeStoragePlanner planner(target);planner.value(StorageRole::local,"array-home",Type::i64,8192);
    check(planner.finish(all,true).bytes==65536,"approved boundary");
    bool rejected=false;try {planner.finish({},true);} catch(const std::invalid_argument&) {rejected=true;}
    check(rejected,"incomplete ABI inventory accepted");
    planner.storage(StorageRole::known_abi,"actual-known-overhead",{8,8});
    rejected=false;try {planner.finish(all,true);} catch(const std::length_error&) {rejected=true;}
    check(rejected,"ABI overhead excluded from budget");
    check(planner.finish(all,false).bytes>65536,"array-free functions acquired a new cap");
    rejected=false;try {planner.value(StorageRole::copy,"overflow",Type::i64,~std::uint64_t{0});} catch(const std::length_error&) {rejected=true;}
    check(rejected,"storage multiplication overflow accepted");
}
}
int main() {
    try {source_and_ir();negatives();indexes_and_budget();std::cout<<"P23-1 scalar fixed-array analysis/IR and storage-contract checks passed\n";return 0;}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
