#include <algorithm>
#include <chrono>
#include <cstdlib>
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
void invalid(vd::ir::Module module,const std::string& reason={}) {
    bool rejected=false;
    try { vd::ir::validate(module); } catch(const std::invalid_argument& e){rejected=true;if(!reason.empty())check(std::string(e.what()).find(reason)!=std::string::npos,e.what());}
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
    auto calls=analyze("use random as rng\nfunc entry(n: i32) -> i32:\n    var a = rng.rand_i32()\n    var b = rng.rand_i32(maximum = n)\n    var c = rng.rand_i32(upper = n, lower = 1)\n    return a + b + c\n")->lower();
    unsigned forms=0;
    for(const auto& fn:calls.functions)if(fn.module_identity=="/main.void")for(const auto& block:fn.blocks)for(const auto& i:block.instructions)if(i.op==Op::call) {
        if(i.callee=="/@stdlib/random.void::rand_i32"){check(i.arguments.empty(),"raw arity IR changed");forms|=1;}
        if(i.callee=="/@stdlib/random.void::_arity_rand_i32_1"){check(i.arguments.size()==1,"maximum arity IR changed");forms|=2;}
        if(i.callee=="/@stdlib/random.void::_arity_rand_i32_2"){check(i.arguments.size()==2,"two-bound arity IR changed");forms|=4;}
    }
    check(forms==7,"frontend did not select three distinct ordinary IR signatures");
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
    auto worker=analyze("use random\nfunc work(value: u64) -> u64:\n    var engine = random.generator(seed = value)\n    return engine.rand_u64()\nasync func entry(n: i32) -> i32:\n    var task = spawn work(42)\n    var answer = await task\n    if n < 0:\n        var unused = random.from_entropy()\n    return (answer % 100) as i32\n")->lower();
    Type entropy_type=Type::none;
    for(const auto& fn:worker.functions)for(const auto& block:fn.blocks)for(const auto& i:block.instructions)if(i.op==Op::random_seed)entropy_type=i.type;
    check(entropy_type!=Type::none,"entropy result type not found");
    for(auto& function:worker.functions)if(function.worker) {
        Instruction seed{Op::random_seed};seed.type=entropy_type;
        auto& block=function.blocks[0];block.instructions.push_back(seed);
    }
    worker.coordinated=true;invalid(worker,"worker cannot acquire OS entropy");
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
    auto random_path=vd::standard_library_root()/"random.void";
    std::ifstream random_stream(random_path,std::ios::binary);
    std::string random_source((std::istreambuf_iterator<char>(random_stream)),{});
    for(const auto& edit:std::vector<std::pair<std::string,std::string>>{
            {"const algorithm_version: i32 = 1","const algorithm_version: i32 = 2"},
            {"    private s3: u64","    private s3: u64\n    private extra: u64"},
            {"13787848793156543929","13787848793156543927"}}) {
        auto modified=random_source;auto at=modified.find(edit.first);check(at!=std::string::npos,"reload mutation site missing");modified.replace(at,edit.first.size(),edit.second);
        auto snapshot=directory/("random_override"+std::to_string(++serial)+".void");std::ofstream(snapshot,std::ios::binary)<<modified;
        auto path=directory/"main.void";std::ofstream(path,std::ios::binary)<<text;
        auto copied=directory/"stdlib-reload";
        std::filesystem::create_directories(copied);
        std::filesystem::copy(vd::standard_library_root(),copied,std::filesystem::copy_options::recursive|std::filesystem::copy_options::overwrite_existing);
        std::ofstream(copied/"random.void",std::ios::binary)<<modified;
        vd::AnalysisResult candidate;
        {StandardRoot selected(copied);candidate=vd::analyze_project(path);}
        if(!candidate.succeeded())throw vd::CompilationError(candidate.diagnostics);
        check(!jit.replace(slot,*candidate.program,optimize),"random algorithm/layout/initializer mutation did not require restart");
    }
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
bool collections_collected=true;
void collection_poll() {
    collections_collected &= vd::Runtime::current().collect()==vd::Runtime::Collection::completed;
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
    collections_collected=true;
    {managed_test::Observe observe(probe);
     struct PollCollection { PollCollection(){managed_test::concurrent_poll.store(collection_poll);} ~PollCollection(){managed_test::concurrent_poll.store(nullptr);} } collect_each_poll;
     check(code->invoke(0)==0,"collection alias/value/identity semantics failed");}
    check(collections_collected && probe.polls>0 && probe.roots_checked>0,"collection did not survive forced GC between swaps");
    if(!probe.roots_valid || !probe.commits_valid || !probe.frames.empty() || probe.constructions<2)
        std::cerr<<"roots="<<probe.roots_valid<<" commits="<<probe.commits_valid<<" frames="<<probe.frames.size()<<" constructions="<<probe.constructions<<'\n';
    check(probe.roots_valid && probe.commits_valid && probe.frames.empty() && probe.constructions>=2,"collection precise roots/barriers failed");
    unsigned failures=0;
    for(int budget=0;budget<12;++budget) {
        bool failed=false;
        {managed_test::AllocationFailure fault(budget,true);try{check(code->invoke(0)==0,"fault run changed successful result");}catch(const std::runtime_error&){failed=true;}}
        if(failed)++failures;
        check(code->invoke(0)==0,"allocation failure poisoned later random execution");
    }
    check(failures>0,"allocation fault sweep never failed");
}
void swap_failures(bool optimize) {
    auto program=analyze(R"VOID(use random
value struct row:
    data: [i32, 2]
    nested: [i32]
func initial() -> [row]:
    return [row(data = [1, 2], nested = [5]), row(data = [3, 4], nested = [7]), row(data = [5, 6], nested = [9])]
var source = initial()
func entry(n: i32) -> i32:
    if n == -1:
        return source[0].data[0] * 100 + source[1].data[0] * 10 + source[2].data[0]
    if n == -2:
        return source[0].data[0] + source[1].data[0] + source[2].data[0]
    if n == -3:
        return source.count as i32
    if n == -4:
        source = initial()
        return 0
    var engine = random.generator(seed = 42)
    if n == 0:
        var copy = engine.shuffled(source)
    else:
        engine.shuffle(source)
    return 0
)VOID");
    vd::Jit jit;auto code=jit.compile(*program,optimize);check(code->invoke(-1)==135,"swap source initialization changed");
    for(int operation=0;operation<2;++operation) {
        unsigned failures=0, successes=0;
        for(int budget=0;budget<256;++budget) {
            bool failed=false;
            {managed_test::AllocationFailure fault(budget,true);try{check(code->invoke(operation)==0,"swap fault success changed");}catch(const std::runtime_error&){failed=true;}}
            failures+=failed;successes+=!failed;
            check(code->invoke(-3)==3,"swap failure changed list length");
            if(operation==0 || !failed)check(code->invoke(-2)==9,"successful/copy shuffle lost elements");
            if(operation==0)check(code->invoke(-1)==135,"copy failure changed source outer order");
            check(code->invoke(-4)==0,"swap failure prevented source reset");
            if(failures && successes)break;
        }
        std::cout<<"Swap fault sweep: operation="<<operation<<" failures="<<failures<<" successes="<<successes<<'\n';
        check(failures && successes,"complex swap fault sweep missed failure or success");
    }
}
void function_values(bool optimize) {
    auto program=analyze(R"VOID(use random
use convert
func entry(n: i32) -> i32:
    random.seed(42)
    var raw = random.rand_i32
    if raw() != -1787294930:
        return 1
    var engine = random.generator(seed = 42)
    var snapshot_draw = engine.rand_u64
    if snapshot_draw() != (1546998764402558742 as u64):
        return 2
    if snapshot_draw() != (6990951692964543102 as u64):
        return 3
    if engine.rand_u64() != (1546998764402558742 as u64):
        return 4
    var bounded = engine.try_rand_i32
    if convert.unwrap(bounded(10)) != 2:
        return 5
    return 0
)VOID");
    vd::Jit jit;auto code=jit.compile(*program,optimize);
    check(code->invoke(0)==0,"single-signature random function values changed");
}
void benchmark() {
    auto program=analyze("use random\nfunc entry(n: i32) -> i32:\n    var engine = random.generator(seed = 42)\n    var total: u64 = 0\n    var items = [0, 1, 2, 3]\n    for index in 0..<n:\n        total += engine.rand_u64()\n        total += ((engine.rand_i32() as i64) + (2147483648 as i64)) as u64\n        total += engine.rand_i32(1000) as u64\n        engine.shuffle(items)\n    return (total % 1000000) as i32\n");
    vd::Jit jit;auto code=jit.compile(*program,true);code->invoke(0);
    auto start=std::chrono::steady_clock::now();auto sum=code->invoke(20000);
    auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    check(sum==201598,"benchmark independent checksum changed");
    std::cout<<"Benchmark O2: 20000 iterations (raw, typed, bounded, four-item shuffle), "<<seconds<<" s, "<<(20000/seconds)<<" iterations/s\n";
}
#if defined(__linux__)
int entropy_mode=0, entropy_calls=0;
bool native_gc_ok=false;
#endif
}
#if defined(__linux__)
extern "C" ssize_t __wrap_getrandom(void* buffer,std::size_t count,unsigned flags) {
    ++entropy_calls;
    if(flags!=0)throw std::runtime_error("unexpected entropy flags");
    if(entropy_mode==1){errno=ENOSYS;return -1;}
    if(entropy_mode==2)return 0;
    if(entropy_mode==4 && entropy_calls>1){errno=ENOSYS;return -1;}
    if(entropy_mode==3 && entropy_calls==1){errno=EINTR;return -1;}
    if(entropy_mode==5) {
        auto& runtime=vd::Runtime::current();
        std::thread collector([&]{vd::Runtime::Attachment attach(runtime);vd::Runtime::Managed managed(runtime);native_gc_ok=runtime.collect()==vd::Runtime::Collection::completed;});
        collector.join();
    }
    auto size=(entropy_mode==3 || entropy_mode==4)?std::min(count,std::size_t(3)):count;
    std::memset(buffer,0,size);return static_cast<ssize_t>(size);
}
#endif
namespace {
void entropy_contracts() {
#if defined(__linux__)
    {vd::Runtime runtime;vd::Runtime::Attachment attach(runtime);vd::Runtime::Managed managed(runtime);
    for(int mode=0;mode<6;++mode) {
        entropy_mode=mode;entropy_calls=0;std::uint64_t output=123;std::int32_t status=99;vd::RuntimeFailure failure;
        check(vd_random_seed(&output,&status,&failure) && !failure.message,"entropy bridge failed Runtime channel");
        check(status==((mode==1 || mode==4)?ENOSYS:mode==2?EIO:0),"entropy OS status lost");
        check(output==(mode==1 || mode==2 || mode==4?123:0),"entropy published partial seed or rejected zero");
        check(entropy_calls==(mode==3?4:mode==4?2:1),"entropy interrupted/short read handling changed");
    }
    check(native_gc_ok,"entropy did not enter NativeSafe for concurrent GC");}
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
        for(bool optimize:{false,true}){lifecycle(optimize);ownership(optimize);swap_failures(optimize);function_values(optimize);}
        entropy_contracts();
        benchmark();
        std::cout<<"Random host: "<<checks<<" checks passed\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
