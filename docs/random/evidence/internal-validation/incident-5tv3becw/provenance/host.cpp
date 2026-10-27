#include <algorithm>
#include "void/frontend.hpp"
#include "void/backend.hpp"
#include "void/runtime.hpp"
#include "void/runtime_abi.hpp"
#include "void/cache.hpp"
#include "../managed/probe.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#if defined(__linux__)
#include <cerrno>
#include <cstring>
#include <sys/types.h>
#endif

namespace {
unsigned checks=0, serial=0;
std::filesystem::path directory;
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);++checks;}
auto analyze(const std::string& text) {
    auto snapshot=directory/("host"+std::to_string(++serial)+".void");
    std::ofstream(snapshot,std::ios::binary)<<text;
    auto path=directory/"main.void";
    std::ofstream(path,std::ios::binary)<<text;
    auto result=vd::analyze_project(path);
    if(!result.succeeded())throw vd::CompilationError(result.diagnostics);
    return result.program;
}
void invalid(vd::ir::Module module) {
    bool rejected=false;
    try { vd::ir::validate(module); } catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"forged random IR accepted");
}
void ir_contracts() {
    using namespace vd::ir;
    Module module; module.identity="random-independent";
    Function f; f.name="xor";f.result=Type::u64;
    Instruction a{Op::constant};a.type=Type::u64;a.immediate=-1;
    Instruction b=a;b.immediate=15;
    Instruction operation{Op::random_xor,0,1};operation.type=Type::u64;
    f.blocks={{{a,b,operation},{Terminator::Kind::return_value,2}}};
    module.functions={f};validate(module);++checks;
    for(unsigned mutation=0;mutation<8;++mutation) {
        auto forged=module;auto& i=forged.functions[0].blocks[0].instructions[2];
        switch(mutation){case 0:i.type=Type::i64;break;case 1:i.a=99;break;case 2:i.immediate=1;break;case 3:i.arguments={0};break;case 4:i.callee="user";break;case 5:i.receiver=Place{0,{}};break;case 6:forged.functions[0].blocks[0].instructions[0].type=Type::i64;break;case 7:i.initialization=true;break;}
        invalid(forged);
    }
    auto program=analyze("use random\nfunc entry(n: i32) -> i32:\n    var outcome = random.from_entropy()\n    return n\n");
    auto entropy=program->lower();
    validate(entropy);++checks;
    for(unsigned mutation=0;mutation<5;++mutation) {
        auto forged=entropy;
        if(mutation==0)forged.coordinated=false;
        for(auto& function:forged.functions)for(auto& block:function.blocks)for(auto& i:block.instructions)if(i.op==Op::random_seed) {
            if(mutation==1)i.type=Type::u64;
            if(mutation==2)i.arguments={0};
            if(mutation==3)i.immediate=1;
            if(mutation==4)i.callee="pretend";
        }
        invalid(forged);
    }
    // Independently forge a worker path reaching entropy after valid frontend analysis.
    auto worker=analyze("use random\nfunc work(value: u64) -> u64:\n    var engine = random.generator(seed = value)\n    return engine.rand_u64()\nasync func entry(n: i32) -> i32:\n    var task = spawn work(42)\n    var answer = await task\n    return (answer % 100) as i32\n")->lower();
    Type entropy_type=Type::none;
    for(const auto& fn:entropy.functions)for(const auto& block:fn.blocks)for(const auto& i:block.instructions)if(i.op==Op::random_seed)entropy_type=i.type;
    check(entropy_type!=Type::none,"entropy result type not found");
    for(auto& function:worker.functions)if(function.worker) {
        Instruction seed{Op::random_seed};seed.type=entropy_type;
        auto& block=function.blocks[0];block.instructions.push_back(seed);
    }
    worker.coordinated=true;invalid(worker);
}
void lifecycle(bool optimize) {
    std::string text="use random\nuse random as alias\nvar started = begin_stream()\nfunc begin_stream() -> i32:\n    random.seed(42)\n    return 1\nfunc entry(n: i32) -> i32:\n    return (alias.rand_u64() % 1000000) as i32\n";
    auto program=analyze(text);vd::Jit jit;
    auto code=jit.compile(*program,optimize);
    check(code->invoke(0)==558742 && code->invoke(0)==543102,"default invocation retention/replay failed");
    auto fresh=jit.compile(*program,optimize);
    check(fresh->invoke(0)==558742,"fresh Runtime shared random state");
    auto updated=text;updated.replace(updated.find("return (alias"),7,"return 7 + ");
    vd::ReloadSlot slot(code);
    check(jit.replace(slot,*analyze(updated),optimize),"random body-only reload required restart");
    check(slot.invoke(0)==559016,"compatible reload lost random position");
    auto changed=text;changed.replace(changed.find("seed(42)"),8,"seed(43)");
    check(!jit.replace(slot,*analyze(changed),optimize),"random initializer change admitted reload");
    bool denied=false;std::thread foreign([&]{try{slot.invoke(0);}catch(const std::exception&){denied=true;}});foreign.join();
    check(denied,"foreign thread accessed default random globals");
    auto runtime=code->runtime_owner();
    {vd::Runtime::Attachment attach(*runtime);vd::Runtime::Managed managed(*runtime);check(runtime->collect()==vd::Runtime::Collection::completed,"global collection failed");}
    check(slot.invoke(0)==124200,"global random state did not survive GC");
    runtime->close();check(runtime->memory_snapshot().handles==0,"random global Handles leaked on close");
    vd::Jit cached({directory/"cache",std::string(vd::compiler_build_identity())});
    auto cold=cached.compile(*program,false);auto status=cached.cache_status();
    check(cold->invoke(0)==558742,"cache cold stream mismatch");
    auto hit=cached.compile(*program,false);
    check(cached.cache_status()==(status=="unavailable"?"unavailable":"hit"),"cache did not reuse random code");
    check(hit->invoke(0)==558742 && cold->invoke(0)==543102,"cache serialized live random state");
}
void ownership(bool optimize) {
    auto program=analyze(R"VOID(use random
struct box:
    n: i32
value struct packet:
    refs: [box?, 2]
    values: [i32, 2]
    nested: [i32]
func entry(n: i32) -> i32:
    var engine = random.generator(seed = 42)
    var first = box(n = 11)
    var second = box(n = 19)
    var single = [first]
    var selected = engine.choose(single)
    first.n += 1
    if selected.n != 12:
        return 1
    var data = [packet(refs = [first, second], values = [1, 2], nested = [5]), packet(refs = [second, first], values = [3, 4], nested = [7])]
    var alias = data
    for item in data:
        engine.shuffle(data)
    var copy = engine.shuffled(data)
    copy[0].values[0] = 80
    if data[0].values[0] == 80:
        return 2
    first.n = 31
    var total = 0
    for item in copy:
        total += (item.refs[0] as box).n + (item.refs[1] as box).n
    if total != 100 or alias.count != 2:
        return 3
    return 0
)VOID");
    vd::Jit jit;auto code=jit.compile(*program,optimize);
    managed_test::Probe probe;probe.inspect_roots=true;probe.collect_before=true;probe.frames.reserve(4096);
    {managed_test::Observe observe(probe);check(code->invoke(0)==0,"collection alias/value/identity semantics failed");}
    check(probe.roots_valid && probe.commits_valid && probe.frames.empty() && probe.constructions>5,"collection precise roots/barriers failed");
    unsigned failures=0;
    for(int budget=0;budget<12;++budget) {
        bool failed=false;
        {managed_test::AllocationFailure fault(budget,true);try{check(code->invoke(0)==0,"fault run changed successful result");}catch(const std::runtime_error&){failed=true;}}
        if(failed)++failures;
        check(code->invoke(0)==0,"allocation failure poisoned later random execution");
    }
    check(failures>0,"allocation fault sweep never failed");
}
#if defined(__linux__)
int entropy_mode=0, entropy_calls=0;
#endif
}
#if defined(__linux__)
extern "C" ssize_t __wrap_getrandom(void* buffer,std::size_t count,unsigned flags) {
    ++entropy_calls;
    if(flags!=0)throw std::runtime_error("unexpected entropy flags");
    if(entropy_mode==1){errno=ENOSYS;return -1;}
    if(entropy_mode==2)return 0;
    if(entropy_mode==3 && entropy_calls==1){errno=EINTR;return -1;}
    auto size=entropy_mode==3?std::min(count,std::size_t(3)):count;
    std::memset(buffer,0,size);return static_cast<ssize_t>(size);
}
#endif
namespace {
void entropy_contracts() {
#if defined(__linux__)
    vd::Runtime runtime;vd::Runtime::Attachment attach(runtime);vd::Runtime::Managed managed(runtime);
    for(int mode=0;mode<4;++mode) {
        entropy_mode=mode;entropy_calls=0;std::uint64_t output=123;std::int32_t status=99;vd::RuntimeFailure failure;
        check(vd_random_seed(&output,&status,&failure) && !failure.message,"entropy bridge failed Runtime channel");
        check(status==(mode==1?ENOSYS:mode==2?EIO:0),"entropy OS status lost");
        check(output==(mode==1 || mode==2?123:0),"entropy published partial seed or rejected zero");
        check(entropy_calls==(mode==3?4:1),"entropy interrupted/short read handling changed");
    }
    auto source=analyze(R"VOID(use random
use convert
func entry(n: i32) -> i32:
    if n == 0:
        var invalid = random.try_rand_i32(0)
        var lone = random.rand_range(7, 7)
        var single = [1]
        var picked = random.try_choose(single)
        var copied = random.try_shuffled(single)
        var done = random.try_shuffle(single)
        return lone
    if n == 1:
        var outcome = random.try_initialize()
        if outcome is Err(var reason):
            return reason.native_code
        return 0
    if n == 2:
        return (random.rand_u64() % 1000000) as i32
    if n == 3:
        random.seed(42)
        return (random.rand_u64() % 1000000) as i32
    return 0
)VOID");
    vd::Jit jit;auto code=jit.compile(*source,false);
    entropy_mode=1;entropy_calls=0;
    check(code->invoke(0)==7 && entropy_calls==0,"invalid/singleton call requested entropy");
    check(code->invoke(1)==ENOSYS && entropy_calls==1,"OS failure not recoverable");
    check(code->invoke(1)==ENOSYS && entropy_calls==2,"failed default initialization was published");
    entropy_mode=0;
    check(code->invoke(1)==0 && entropy_calls==3,"default entropy retry failed");
    check(code->invoke(1)==0 && entropy_calls==3,"ready default reseeded");
    check(code->invoke(2)==66420,"zero OS seed did not use deterministic initialization");
    entropy_mode=1;
    check(code->invoke(3)==558742 && entropy_calls==3,"explicit default seed requested entropy");
#endif
}
}
int main(int argc,char** argv) {
    try {
        directory=std::filesystem::absolute(argc>1?argv[1]:".");std::filesystem::create_directories(directory);
        ir_contracts();
        for(bool optimize:{false,true}){lifecycle(optimize);ownership(optimize);}
        entropy_contracts();
        std::cout<<"Random host: "<<checks<<" checks passed\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
