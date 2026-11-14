#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/tooling.hpp"
#include "cases.hpp"
#include "check.hpp"
#include <fstream>
#include <iterator>
namespace {
std::shared_ptr<const vd::ValidatedAnalysis> analyze(const std::string& text) {
    auto analysis=vd::analyze_source({"/elision.void",text});
    if(!analysis.succeeded())throw vd::CompilationError(analysis.diagnostics);
    return analysis.program;
}
std::string read(const std::filesystem::path& file) {
    std::ifstream stream(file); managed_test::require(bool(stream),"elision fixture missing");
    return {std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
}
void fixture(const char* name, const std::string& text, int expected, std::size_t constructions, bool optimized) {
    auto analysis=analyze(text); vd::Jit jit({},vd::ir::Emission::entry);
    auto runtime=std::make_shared<vd::Runtime>(1);
    auto module=analysis->lower(vd::ir::Emission::entry);
    auto code=jit.compile(module,module.entry_symbol,optimized,{},runtime);
    allocation_elimination::check(name,[&](int n){return code->invoke(n);},*runtime,expected,constructions);
}
// These are deliberately outside A's proof. Counts protect the conservative
// fallback as well as values: equal results alone do not prove escape safety.
void boundaries(bool optimized) {
    const std::string box="struct box:\n    value: i32\n";
    fixture("identity",box+"func entry(n: i32) -> i32:\n    var a = box(value = 7)\n    var b = box(value = 7)\n    if a == b:\n        return 1\n    return 0\n",0,2,optimized);
    fixture("published-field",box+"struct holder:\n    item: box\nfunc entry(n: i32) -> i32:\n    var a = box(value = 7)\n    var h = holder(item = a)\n    h.item.value = 9\n    return a.value\n",9,2,optimized);
    fixture("captured",box+"func entry(n: i32) -> i32:\n    var a = box(value = 7)\n    var get = func() -> i32:\n        return a.value\n    return get()\n",7,3,optimized);
    fixture("alias-rebind",box+"func entry(n: i32) -> i32:\n    var a = box(value = 7)\n    var b = a\n    a = box(value = 9)\n    return a.value + b.value\n",16,2,optimized);
    fixture("loop-instance",box+"func entry(n: i32) -> i32:\n    var previous = box(value = -1)\n    var total = 0\n    for index in 0..<n:\n        var current = box(value = index)\n        total += previous.value\n        previous = current\n    return total\n",498500,1001,optimized);
    std::string large="struct large:\n";
    for(unsigned i=0;i<33;++i)large+="    field"+std::to_string(i)+": i64\n";
    fixture("over-256",large+"func entry(n: i32) -> i32:\n    var item = large(field0 = 7)\n    return item.field0 as i32\n",7,1,optimized);
    std::string bounded="struct full:\n";
    for(unsigned i=0;i<32;++i)bounded+="    field"+std::to_string(i)+": i64\n";
    bounded+="func entry(n: i32) -> i32:\n    var total = 0\n";
    for(unsigned i=0;i<65;++i)bounded+="    var b"+std::to_string(i)+" = full(field0 = 1)\n    total += b"+std::to_string(i)+".field0 as i32\n";
    bounded+="    return total\n";
    fixture("16KiB-budget",bounded,65,1,optimized);
    // A scalar division remains observable after eliminating its owner. Do not
    // treat every thrown error as the permitted disappearance of allocation OOM.
    auto analysis=analyze(box+"func entry(n: i32) -> i32:\n    var item = box(value = 7)\n    return item.value / (n - n)\n");
    vd::Jit jit; auto code=jit.compile(*analysis,optimized); managed_test::Probe p; bool failed=false;
    { managed_test::Observe observe(p); try { code->invoke(1000); }
      catch(const std::runtime_error& e) { failed=std::string(e.what()).find("zero")!=std::string::npos; }
    }
    managed_test::require(failed && p.constructions==0,"elision lost remaining arithmetic failure");
}
}
int main(int argc,char**argv) {
    try {
        if(argc==6 && std::string(argv[1])=="--emit") {
            auto analysis=analyze(read(argv[3]));
            vd::emit_runtime_object(analysis->lower(vd::ir::Emission::entry),argv[4],
                                    std::string(argv[5])=="optimized",std::string("_")+argv[2]);
            return 0;
        }
        managed_test::require(argc==2,"elision fixture directory argument");
        for(bool optimized:{false,true}) {
#define CHECK(name,expected,count) fixture(#name,read(std::filesystem::path(argv[1])/(#name ".void")),expected,count,optimized);
            VD_ELISION_CASES(CHECK)
#undef CHECK
            boundaries(optimized);
        }
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
