#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/runtime.hpp"
#include "../managed/probe.hpp"
#include <atomic>
#include <future>
#include <latch>
void async_ir_tests();
void async_parallel_tests(const std::filesystem::path&,const std::filesystem::path&);
void async_lifecycle_tests(const std::filesystem::path&,const std::filesystem::path&);
void async_lifetime_tests(const std::filesystem::path&);
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::shared_ptr<const vd::ValidatedAnalysis> analyze(const std::filesystem::path& file){auto result=vd::analyze_project(file);if(!result.succeeded())throw vd::CompilationError(result.diagnostics);return result.program;}
}
int main(int argc,char** argv){try{
    if(argc==4 && std::string(argv[1])=="--parallel"){async_parallel_tests(argv[2],argv[3]);return 0;}
    if(argc==4 && std::string(argv[1])=="--lifecycle"){async_lifecycle_tests(argv[2],argv[3]);return 0;}
    if(argc==5 && std::string(argv[1])=="--emit"){auto analysis=analyze(argv[2]);vd::emit_async_object(analysis->lower(),argv[3],std::string(argv[4])=="optimized");return 0;}
    if(argc==2 && std::string(argv[1])=="--ir"){async_ir_tests();return 0;}
    require(argc==3,"repository root and fixture root required");auto file=std::filesystem::path(argv[1])/"examples/async/activation.void";auto analysis=analyze(file);
    for(bool optimize:{false,true}){vd::Jit jit;auto code=jit.compile_async(*analysis,optimize);require(code->invoke(7)==17,"async await result");}
    for(auto [name,answer]:{std::pair{"lifecycle_exits",14},std::pair{"lifecycle_values",17},std::pair{"lifecycle_resources",30},std::pair{"lifecycle",8},std::pair{"managed",343},std::pair{"large",27},std::pair{"closures",16},std::pair{"loops",25},std::pair{"values",39},std::pair{"methods",129}}){auto a=analyze(std::filesystem::path(argv[1])/"examples/async"/(std::string(name)+".void"));for(bool optimize:{false,true}){vd::Jit jit;auto code=jit.compile_async(*a,optimize);auto actual=code->invoke(7);if(actual!=answer)throw std::runtime_error(std::string(name)+": expected "+std::to_string(answer)+", got "+std::to_string(actual));}}
    auto fixtures=std::filesystem::path(argv[2]);std::filesystem::create_directories(fixtures);std::filesystem::remove_all(fixtures/".void");
    auto source=fixtures/"main.void";
    auto write=[&](const std::string& text){std::ofstream out(source);out<<text;require(bool(out),"fixture write failed");};
    write("func worker(left: i32, right: f64 = 2.0d) -> i32:\n    return left + (right as i32)\nasync func child(left: i32, right: i64 = 3) -> i32:\n    return left + (right as i32)\nasync func entry(value: i32) -> i32:\n    var direct = await child(right = 4, left = value)\n    var job = spawn worker(right = 5.0d, left = value)\n    return direct + await job\n");
    auto named=analyze(source);for(bool optimize:{false,true}){vd::Jit jit;require(jit.compile_async(*named,optimize)->invoke(7)==23,"named async/spawn argument order or typed wrapper");}
    write("async func child(value: i32) -> i32:\n    return 10 / value\nasync func entry(value: i32) -> i32:\n    var ignored = child(0)\n    return 7\n");
    auto unobserved=analyze(source);for(bool optimize:{false,true}){vd::Jit jit;auto code=jit.compile_async(*unobserved,optimize);bool failed=false;try{code->invoke(7);}catch(const std::runtime_error& error){failed=std::string(error.what()).find("child")!=std::string::npos;}require(failed,"unobserved failure was lost");}
    write("async func child(value: i32) -> i32:\n    return (value - value - 1) as u8 as i32\nasync func entry(value: i32) -> i32:\n    var ignored = child(value)\n    return 10 / (value - value)\n");
    auto primary=analyze(source);{vd::Jit jit;auto code=jit.compile_async(*primary,false);bool failed=false;try{code->invoke(7);}catch(const std::runtime_error& error){failed=std::string(error.what()).find("::entry")!=std::string::npos;}require(failed,"root failure did not remain primary");}
    auto managed=analyze(std::filesystem::path(argv[1])/"examples/async/managed.void");
    for(bool optimize:{false,true}){vd::Jit jit;auto code=jit.compile_async(*managed,optimize);auto runtime=code->runtime_owner();managed_test::Probe probe;probe.inspect_roots=true;probe.collect_before=true;probe.frames.reserve(1024);{managed_test::Observe observe(probe);require(code->invoke(7)==343,"GC async result");}require(probe.frames.empty() && probe.roots_valid && probe.commits_valid,"async root imbalance");
        bool complete=false;unsigned failures=0;for(int budget=0;budget<1024 && !complete;++budget){managed_test::Probe p;p.inspect_roots=true;p.frames.reserve(1024);{managed_test::Observe observe(p);try{managed_test::AllocationFailure failure(budget);require(code->invoke(7)==343,"allocation retry result");complete=true;}catch(const std::bad_alloc&){++failures;}catch(const std::runtime_error&){++failures;}}require(p.frames.empty() && p.roots_valid,"allocation failure leaked frame");{vd::Runtime::Attachment attachment(*runtime);vd::Runtime::Managed active(*runtime);require(runtime->collect()==vd::Runtime::Collection::completed && runtime->snapshot().objects==0,"async allocation retained roots");}}
        require(complete && failures>0,"async allocation sweep incomplete");
    }
    // Explicit adapters share the persistent source cache and old-generation leases.
    write(vd::read_source(std::filesystem::path(argv[1])/"examples/async/activation.void").text());auto cached_analysis=analyze(source);
    const auto identity=std::string(vd::compiler_build_identity());{vd::Jit jit({fixtures,identity});auto old=jit.compile_async(*cached_analysis,false);require(old->invoke(7)==17,"cached miss async result");bool supported=jit.cache_status()=="miss";auto hit=jit.compile_async(*cached_analysis,false);require(hit->invoke(7)==17 && (!supported || jit.cache_status()=="hit"),"cached hit async result");vd::ReloadSlot slot(old);
        auto text=vd::read_source(source).text();auto at=text.find("return a + b");text.replace(at,12,"return a + b + 1");write(text);auto edited=analyze(source);require(jit.replace(slot,*edited,false) && slot.invoke(7)==18 && old->invoke(7)==17,"async leased reload");
    }
    // No new public task, cancellation, group or callable escape surface.
    for(const auto& text:{"func entry(value: i32) -> i32:\n    return await value\n", "async func entry(value: i32) -> func() -> i32:\n    return func() -> i32:\n        return value\n", "async func child(value: i32) -> i32:\n    return value\nasync func entry(value: i32) -> i32:\n    var f = child\n    return value\n"})require(!vd::analyze_source({"/invalid-async.void",text}).succeeded(),"invalid async source accepted");
    for(const auto& text:{
        "struct box:\n    number: i32 = 0\nasync func (p box) deinit():\n    pass\nfunc entry(value: i32) -> i32:\n    return value\n",
        "value struct point:\n    number: i32 = 0\nasync func (p point) change() -> i32:\n    return p.number\nfunc entry(value: i32) -> i32:\n    return value\n",
        "struct box:\n    number: i32 = 0\nasync virtual func (p box) change() -> i32:\n    return p.number\nfunc entry(value: i32) -> i32:\n    return value\n",
        "interface reader:\n    async func read() -> i32\nfunc entry(value: i32) -> i32:\n    return value\n",
        "struct box:\n    number: i32 = 0\nasync func read(const owner: box) -> box:\n    return owner\nasync func entry(value: i32) -> i32:\n    var owner = box()\n    var alias = await read(owner)\n    alias.number = 9\n    return value\n"
    })require(!vd::analyze_source({"/invalid-async.void",text}).succeeded(),"async calling restriction or readonly escape accepted");
    {
        auto rejected=vd::analyze_source({"/borrowed-async.void",
            "interface reader:\n    func plus(amount: i32) -> i32\nstruct box:\n    number: i32 = 1\nfunc (owner box) plus(amount: i32) -> i32:\n    return owner.number + amount\nasync func child(value: i32) -> i32:\n    return value\nasync func entry(value: i32) -> i32:\n    var owner = box()\n    var view: reader = owner\n    return view.plus(await child(value))\n"});
        require(!rejected.succeeded(),"prepared interface receiver survived await source checking");
    }
    require(!vd::analyze_source({"/task-construction.void",
        "func construct<t>(value: t) -> t:\n    return t()\nasync func child(value: i32) -> i32:\n    return value\nasync func entry(value: i32) -> i32:\n    var job = child(value)\n    var invalid = construct(job)\n    return value\n"}).succeeded(),"inferred task constructor accepted");
    async_lifetime_tests(source);
    std::cout<<"async activations passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
