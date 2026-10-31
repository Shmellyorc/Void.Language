#include "void/frontend.hpp"
#include "void/backend.hpp"
#include "void/cache.hpp"
#include "void/runtime.hpp"
#include "../managed/probe.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
std::filesystem::path directory;
unsigned checks=0,serial=0;
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);++checks;}
auto analyze(const std::string& text) {
    std::ofstream(directory/("snapshot"+std::to_string(++serial)+".void"),std::ios::binary)<<text;
    auto path=directory/"main.void";std::ofstream(path,std::ios::binary)<<text;
    auto result=vd::analyze_project(path);
    if(!result.succeeded())throw vd::CompilationError(result.diagnostics);
    return result.program;
}
struct StandardRoot {
    std::string previous;bool existed=false;
    static void set(const std::string& value,bool present=true) {
#ifdef _WIN32
        _putenv_s("VOID_STDLIB_ROOT",present?value.c_str():"");
#else
        if(present)setenv("VOID_STDLIB_ROOT",value.c_str(),1);else unsetenv("VOID_STDLIB_ROOT");
#endif
    }
    explicit StandardRoot(const std::filesystem::path& path){if(auto* old=std::getenv("VOID_STDLIB_ROOT")){existed=true;previous=old;}set(path.string());}
    ~StandardRoot(){set(previous,existed);}
};
void lifecycle(bool optimize) {
    const std::string text=R"VOID(use math
func entry(n: i32) -> i32:
    var input = n as f64
    return (math.floor(math.sqrt(input * input)) + math.pow(2.0 as f64, 3.0 as f64) + math.atan2(0.0 as f64, 1.0 as f64)) as i32
)VOID";
    auto program=analyze(text);vd::Jit jit;auto code=jit.compile(*program,optimize);
    check(code->invoke(3)==11 && code->invoke(-5)==13,"dynamic Math invocation failed");
    vd::ReloadSlot slot(code);
    auto changed=text;changed.replace(changed.find("return ("),7,"return 1 + ");
    check(jit.replace(slot,*analyze(changed),optimize),"Math caller body reload rejected");
    check(slot.invoke(3)==12 && code->invoke(3)==11,"Math generation lease changed old code");
    auto original_root=vd::standard_library_root();
    auto copy=directory/("stdlib"+std::to_string(serial));std::filesystem::create_directories(copy);
    std::filesystem::copy(original_root,copy,std::filesystem::copy_options::recursive|std::filesystem::copy_options::overwrite_existing);
    std::ifstream stream(original_root/"math.void",std::ios::binary);
    std::string math((std::istreambuf_iterator<char>(stream)),{});
    auto body=math;
    auto body_position=body.find("return _math_floor<t>(input)");check(body_position!=std::string::npos,"floor body mutation site missing");
    body.insert(body_position+std::string("return _math_floor<t>(input)").size()," + zero");
    std::ofstream(copy/"math.void",std::ios::binary)<<body;
    {StandardRoot root(copy);check(jit.replace(slot,*analyze(text),optimize),"Math source body reload required restart");}
    check(slot.invoke(3)==11 && code->invoke(3)==11,"Math library body replacement lost generation lease");
    auto position=math.find("const pi_f64: f64 = ");check(position!=std::string::npos,"pi constant mutation site missing");
    auto end=math.find('\n',position);math.replace(position,end-position,"const pi_f64: f64 = 3.0");
    std::ofstream(copy/"math.void",std::ios::binary)<<math;
    {StandardRoot root(copy);check(!jit.replace(slot,*analyze(text),optimize),"Math constant change admitted compatible reload");}
    auto cache_root=directory/(optimize?"cache-o2":"cache-o0");
    std::filesystem::create_directories(cache_root);
    vd::Jit cached({cache_root,std::string(vd::compiler_build_identity())});
    auto cold=cached.compile(*program,false);auto first=cached.cache_status();
    auto hit=cached.compile(*program,false);
#if defined(__linux__)
    check(first=="miss" || first=="hit","Linux Math cache was unavailable");
    check(cached.cache_status()=="hit","Linux Math cache did not load its native object");
#else
    check(cached.cache_status()==(first=="unavailable"?"unavailable":"hit"),"Math cache missed unchanged input");
#endif
    std::cout<<"Math native cache: first="<<first<<" repeated="<<cached.cache_status()<<'\n';
    check(cold->invoke(9)==17 && hit->invoke(9)==17,"Math cache symbol binding failed");
}
bool collected=true;
void collect(){collected &= vd::Runtime::current().collect()==vd::Runtime::Collection::completed;}
void roots(bool optimize) {
    auto program=analyze(R"VOID(use math
struct box:
    value: i32
func entry(n: i32) -> i32:
    var kept = box(value = n)
    var total: f64 = 0.0
    for i in 0..<20:
        total += math.sqrt(9.0 as f64) + math.sin(0.0 as f64) + math.pow(2.0 as f64, 3.0 as f64)
    return kept.value + (total as i32)
)VOID");
    vd::Jit jit;auto code=jit.compile(*program,optimize);
    managed_test::Probe probe;probe.inspect_roots=true;probe.collect_before=true;probe.frames.reserve(4096);collected=true;
    {managed_test::Observe observe(probe);
     struct Poll {Poll(){managed_test::concurrent_poll.store(collect);}~Poll(){managed_test::concurrent_poll.store(nullptr);}} poll;
     check(code->invoke(7)==227,"managed Math caller changed result");}
    check(collected && probe.polls>0 && probe.roots_checked>0 && probe.roots_valid && probe.commits_valid && probe.frames.empty(),"Math caller roots failed under collection");
    code->runtime_owner()->close();
    check(code->runtime_owner()->memory_snapshot().handles==0,"Math caller Handles remained after close");
}
}
int main(int argc,char** argv) {
    try {
        directory=std::filesystem::absolute(argc>1?argv[1]:".");std::filesystem::create_directories(directory);
        for(bool optimize:{false,true}){lifecycle(optimize);roots(optimize);}
        std::cout<<"Math host: "<<checks<<" checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
