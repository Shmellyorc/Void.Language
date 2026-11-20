#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/runtime.hpp"
#include "void/cache.hpp"
#include "../managed/probe.hpp"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <thread>

namespace {
unsigned checks=0;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);++checks;}
auto analyze(const std::string& text){auto r=vd::analyze_source({"/statics.void",text});if(!r.succeeded())throw vd::CompilationError(r.diagnostics);return r.program;}
void rejects(vd::ir::Module module){bool denied=false;try{vd::ir::validate(module);}catch(const std::invalid_argument&){denied=true;}check(denied,"forged static metadata accepted");}
const std::string source=R"(struct node:
    value: i32
value struct state:
    static number = 10
    static object = node(value = 4)
    static const frozen = node(value = 8)
static func state.bump(amount: i32) -> i32:
    state.number += amount
    state.object.value += 1
    return state.number + state.object.value
func entry(n: i32) -> i32:
    return state.bump(n)
)";
void exercise(bool optimize,const std::filesystem::path& directory){
    auto program=analyze(source);auto module=program->lower();
    check(module.globals.size()==3,"static fields escaped global cells");
    bool found=false;for(const auto& f:module.functions)if(f.name=="/statics.void::state::static::bump"){
        found=true;check(f.static_member && !f.receiver && f.parameters.size()==1 && f.parameters[0]==vd::ir::Type::i32,"static method acquired a hidden receiver");
    }
    check(found,"static method missing");
    vd::Jit jit({directory,std::string(vd::compiler_build_identity())});
    auto code=jit.compile(*program,optimize);auto runtime=code->runtime_owner();
    managed_test::Probe probe;probe.inspect_roots=true;probe.collect_before=true;probe.frames.reserve(2048);
    {managed_test::Observe observe(probe);check(code->invoke(2)==17,"initial static values incorrect");}
    check(probe.roots_valid && probe.commits_valid && probe.frames.empty(),"static initialization lost precise roots");
    check(code->invoke(0)==18,"static state did not persist");
    {vd::Runtime::Attachment attachment(*runtime);vd::Runtime::Managed managed(*runtime);check(runtime->collect()==vd::Runtime::Collection::completed && runtime->snapshot().objects>=3,"static roots did not survive GC");}
    auto independent=jit.compile(*program,optimize);check(independent->invoke(0)==15,"cache hit reused live static state");
    check(independent->runtime_owner()!=runtime,"independent program reused Runtime");
    bool denied=false;std::thread other([&]{try{code->invoke(0);}catch(const std::exception&){denied=true;}});other.join();check(denied,"foreign owner thread accessed static state");
    vd::ReloadSlot slot(code);auto body=source;auto at=body.find("return state.bump(n)");body.replace(at,20,"return state.bump(n) + 20");
    check(jit.replace(slot,*analyze(body),optimize),"body-only static reload rejected");check(slot.invoke(0)==39,"reload replayed static initialization");
    auto initializer=source;initializer.replace(initializer.find("= 10"),4,"= 30");check(!jit.replace(slot,*analyze(initializer),optimize),"changed static initializer did not require restart");
    auto visibility=source;visibility.replace(visibility.find("static number"),13,"private static number");check(!jit.replace(slot,*analyze(visibility),optimize),"changed static visibility did not require restart");
    auto frozen=source;frozen.replace(frozen.find("static object"),13,"static const object");check(!vd::analyze_source({"/statics.void",frozen}).succeeded(),"static const allowed mutation");
    auto forged=module;for(auto& f:forged.functions)if(f.static_member){f.lexical_owner="/statics.void::missing";break;}rejects(std::move(forged));
    forged=module;for(auto& f:forged.functions)if(f.static_member){f.static_member=false;break;}rejects(std::move(forged));
    forged=module;forged.globals[0].static_owner="/statics.void::missing";rejects(std::move(forged));
    forged=module;for(auto& f:forged.functions)if(f.name=="/statics.void::state::static::bump")f.visibility="private";rejects(std::move(forged));
    runtime->close();check(runtime->memory_snapshot().handles==0,"static roots retained handles after shutdown");
    std::ifstream input(std::filesystem::path(VOID_STATIC_SOURCE_DIR)/"async.void");
    std::string async_source{std::istreambuf_iterator<char>(input),{}};check(!async_source.empty(),"async fixture missing");
    auto async_code=jit.compile_async(*analyze(async_source),optimize);check(async_code->invoke(0)==0,"static async/private worker context failed");
    auto async_runtime=async_code->runtime_owner();async_runtime->close();check(async_runtime->memory_snapshot().handles==0,"async static roots retained handles");
}
}
int main(int argc,char** argv){try{if(argc!=2)throw std::runtime_error("expected evidence cache directory");for(bool optimize:{false,true})exercise(optimize,std::filesystem::path(argv[1])/(optimize?"o2":"o0"));std::cout<<checks<<" G80 static lifecycle/GC/reload/IR checks passed\n";return 0;}catch(const std::exception& failure){std::cerr<<failure.what()<<'\n';return 1;}}
