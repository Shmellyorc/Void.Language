#include "void/frontend.hpp"
#include "void/backend.hpp"
#include "void/cache.hpp"
#include "../managed/probe.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>

namespace {
std::filesystem::path work;
unsigned checks=0, serial=0;
void check(bool condition,const char* why){if(!condition)throw std::runtime_error(why);++checks;}
auto analyze(const std::string& source){
    std::ofstream(work/("snapshot"+std::to_string(++serial)+".void"))<<source;
    auto path=work/"main.void";std::ofstream(path)<<source;
    auto analysis=vd::analyze_project(path);
    if(!analysis.succeeded())throw vd::CompilationError(analysis.diagnostics);
    return analysis.program;
}
const std::string source=R"VOID(use json as json
struct data:
    score: i32
    label: string
    levels: [i32]
func entry(n: i32) -> i32:
    if n < 0:
        if json.parse("[1,]") is Err(var failure):
            return n
        return -999
    var input = data(score = n, label = "rooted", levels = [1,2,3])
    if json.stringify(input) is Ok(var saved):
        if json.parse_as<data>(saved) is Ok(var restored):
            return restored.score + restored.levels[1]
    return -999
)VOID";
bool collected;
void collect(){collected &= vd::Runtime::current().collect()==vd::Runtime::Collection::completed;}
void wide_roots(bool optimize){
    std::string wide="use json as json\nstruct leaf:\n    const score: i32\nstruct wide:\n";
    for(unsigned n=0;n<32;++n)wide+="    const slot"+std::to_string(n)+": leaf\n";
    wide+="func entry(n: i32) -> i32:\n    var input = wide(";
    for(unsigned n=0;n<32;++n){if(n)wide+=", ";wide+="slot"+std::to_string(n)+" = leaf(score = n)";}
    wide+=")\n    if json.to_value(input) is Ok(var node):\n        if json.from_value<wide>(node) is Ok(var restored):\n            return restored.slot31.score + 2\n    return -999\n";
    vd::Jit jit;auto code=jit.compile(*analyze(wide),optimize);
    managed_test::Probe probe;probe.inspect_roots=true;probe.collect_before=true;probe.frames.reserve(4096);collected=true;
    {managed_test::Observe observe(probe);
     struct Poll {Poll(){managed_test::concurrent_poll.store(collect);}~Poll(){managed_test::concurrent_poll.store(nullptr);}} polling;
     check(code->invoke(7)==9,"wide const-reference construction changed under forced GC");}
    check(collected && probe.roots_checked>0 && probe.roots_valid && probe.commits_valid && probe.frames.empty(),"wide JSON construction lost roots");
    {auto& runtime=*code->runtime_owner();vd::Runtime::Attachment attachment(runtime);vd::Runtime::Managed managed(runtime);
     check(runtime.collect()==vd::Runtime::Collection::completed && runtime.snapshot().objects==0,"wide JSON construction retained temporary objects");}
    std::cout<<"JSON wide const GC "<<(optimize?"O2":"O0")<<": "<<probe.roots_checked<<" root observations, "<<probe.constructions<<" managed constructions, "<<probe.polls<<" polls\n";
    code->runtime_owner()->close();check(code->runtime_owner()->memory_snapshot().handles==0,"wide JSON Runtime handles survived close");
}
void roots_and_failures(bool optimize){
    auto program=analyze(source);vd::Jit jit;auto code=jit.compile(*program,optimize);
    managed_test::Probe probe;probe.inspect_roots=true;probe.collect_before=true;probe.frames.reserve(4096);collected=true;
    {managed_test::Observe observe(probe);
     struct Poll {Poll(){managed_test::concurrent_poll.store(collect);}~Poll(){managed_test::concurrent_poll.store(nullptr);}} polling;
     check(code->invoke(7)==9,"JSON result changed under forced GC");
     check(code->invoke(-3)==-3 && code->invoke(8)==10,"parse error prevented retry");}
    check(collected && probe.roots_checked>0 && probe.roots_valid && probe.commits_valid && probe.frames.empty(),"JSON roots or publication failed");
    std::cout<<"JSON forced GC "<<(optimize?"O2":"O0")<<": "<<probe.roots_checked<<" root observations, "<<probe.constructions<<" managed constructions, "<<probe.polls<<" polls\n";
    unsigned failures=0;bool success=false;
    auto attempt=[&](int budget){
        std::optional<int> result;
        try{managed_test::AllocationFailure inject(budget);result=code->invoke(7);}
        catch(const std::bad_alloc&){++failures;}
        catch(const std::runtime_error&){++failures;}
        // An incorrect returned value is never classified as an injected failure.
        if(result){check(*result==9,"allocation sweep changed JSON result");success=true;}
        check(code->invoke(7)==9,"allocation failure prevented subsequent JSON operation");
        auto& runtime=*code->runtime_owner();vd::Runtime::Attachment attachment(runtime);vd::Runtime::Managed managed(runtime);
        check(runtime.collect()==vd::Runtime::Collection::completed,"JSON failure collection did not finish");
        check(runtime.snapshot().objects==0,"JSON temporary objects survived invocation cleanup");
    };
    for(int budget=0;budget<2048 && !success;++budget)attempt(budget);
    for(int budget=2048;budget<=131072 && !success;budget*=2)attempt(budget);
    check(success && failures>10,"JSON allocation sweep did not cover failures and success");
    std::cout<<"JSON allocation sweep "<<(optimize?"O2":"O0")<<": "<<failures<<" injected failures\n";
    code->runtime_owner()->close();check(code->runtime_owner()->memory_snapshot().handles==0,"JSON Runtime handles survived close");
}
void reload_and_cache(bool optimize){
    auto program=analyze(source);vd::Jit jit;auto code=jit.compile(*program,optimize);vd::ReloadSlot slot(code);
    check(code->invoke(7)==9,"initial JSON generation failed");
    auto changed=source;auto pos=changed.find("restored.score + restored.levels[1]");changed.replace(pos,std::string("restored.score").size(),"1 + restored.score");
    check(jit.replace(slot,*analyze(changed),optimize),"JSON body-only reload rejected");
    check(slot.invoke(7)==10 && code->invoke(7)==9,"JSON old generation lease changed");
    auto schema=source;schema.replace(schema.find("score: i32"),10,"score: i64");
    // Keep the changed source valid without changing entry's i32 contract.
    schema.replace(schema.find("score = n"),9,"score = n as i64");
    schema.replace(schema.find("restored.score +"),16,"(restored.score as i32) +");
    check(!jit.replace(slot,*analyze(schema),optimize),"JSON schema change admitted compatible reload");
    auto cache=work/(optimize?"cache-o2":"cache-o0");std::filesystem::create_directories(cache);
    vd::Jit cached({cache,std::string(vd::compiler_build_identity())});
    auto first=cached.compile(*program,false);auto second=cached.compile(*program,false);
    check(cached.cache_status()=="hit","JSON did not obtain an actual native object cache hit");
    check(first->invoke(3)==5 && second->invoke(4)==6,"JSON cache symbols or fresh Runtime state failed");
    vd::Jit application({cache,std::string(vd::compiler_build_identity())});
    auto fresh=application.compile(*program,false);
    check(application.cache_status()=="hit" && fresh->invoke(5)==7,"fresh JSON application failed to reuse the native object");
    check(first->runtime_owner()!=fresh->runtime_owner(),"separate JSON applications shared Runtime state");
}
}
int main(int argc,char** argv){
    try{check(argc==2,"expected evidence directory");work=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(work);
        for(bool optimize:{false,true}){wide_roots(optimize);roots_and_failures(optimize);reload_and_cache(optimize);}
        std::cout<<"JSON host: "<<checks<<" checks passed\n";return 0;
    }catch(const std::exception& failure){std::cerr<<failure.what()<<'\n';return 1;}
}
