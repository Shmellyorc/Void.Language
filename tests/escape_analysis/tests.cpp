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
void fixture(const char* name, const std::string& text, int expected, std::size_t constructions, bool optimized, bool baseline) {
    auto analysis=analyze(text); vd::Jit jit({},vd::ir::Emission::entry);
    auto runtime=std::make_shared<vd::Runtime>(1);
    auto module=analysis->lower(vd::ir::Emission::entry);
    auto code=jit.compile(module,module.entry_symbol,optimized,{},runtime);
    // Warm reuse may satisfy a real allocation without any native request.
    // Keep the OOM proof cold rather than forcing an unnecessary native request.
    auto cold_runtime=std::make_shared<vd::Runtime>(1);
    auto cold_code=jit.compile(module,module.entry_symbol,optimized,{},cold_runtime);
    if(std::string(name)=="reference_effects" || std::string(name)=="direct_effects")managed_test::require(cold_code->invoke(0)==0,"cold reference globals");
    allocation_elimination::check(name,[&](int n){return code->invoke(n);},*runtime,expected,constructions,
        [&](int n){return cold_code->invoke(n);});
    escape_analysis::retired_children(name,[&](int n){return code->invoke(n);});
    escape_analysis::remaining_failure(name,[&](int n){return code->invoke(n);},baseline);
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
        managed_test::require(argc==2 || argc==3,"escape fixture directory argument");
        const bool baseline=argc==3 && std::string(argv[2])=="baseline";
        for(bool optimized:{false,true}) {
#define CHECK(name,expected,count,old_count) fixture(#name,read(std::filesystem::path(argv[1])/(#name ".void")),expected,baseline?old_count:count,optimized,baseline);
            VD_ESCAPE_CASES(CHECK)
#undef CHECK

        }
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
