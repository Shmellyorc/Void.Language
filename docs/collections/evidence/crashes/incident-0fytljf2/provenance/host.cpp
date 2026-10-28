#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/cache.hpp"
#include "../managed/probe.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <fstream>
using managed_test::require;
namespace {
void empty(vd::Runtime& runtime) {
    vd::Runtime::Attachment attachment(runtime); vd::Runtime::Managed managed(runtime);
    require(runtime.collect() == vd::Runtime::Collection::completed && !runtime.snapshot().objects,
            "collections retained objects after local roots expired");
}
auto compile(vd::Jit& jit, const std::filesystem::path& file, bool optimized) {
    auto analysis = vd::analyze_project(file);
    if (!analysis.succeeded()) throw vd::CompilationError(analysis.diagnostics);
    auto module = analysis.program->lower(); vd::ir::validate(module);
    return jit.compile(module, module.entry_symbol, optimized, {}, std::make_shared<vd::Runtime>(1));
}
void roots(const std::filesystem::path& root, bool optimized) {
    for (const auto* name : {"semantics", "types", "numeric", "large"}) {
        vd::Jit jit; auto code = compile(jit, root / "tests/collections_stdlib" / (std::string(name) + ".void"), optimized);
        managed_test::Probe probe; probe.inspect_roots = true; probe.collect_before = true; probe.frames.reserve(4096);
        { managed_test::Observe observe(probe); require(code->invoke(std::string(name)=="large"?0:7) == 7, "collections result under forced GC"); }
        require(probe.roots_valid && probe.commits_valid && probe.frames.empty() && probe.roots_checked,
                "collections precise roots/publication/frame cleanup failed");
        empty(*code->runtime_owner());
    }
}
void guard_ir(const std::filesystem::path& root) {
    auto analysis=vd::analyze_project(root/"tests/collections_stdlib/semantics.void");
    if(!analysis.succeeded())throw vd::CompilationError(analysis.diagnostics);
    const auto original=analysis.program->lower();
    for(unsigned variant=0;variant<2;++variant) {
        auto forged=original;bool changed=false;
        for(auto& function:forged.functions)for(auto& block:function.blocks)for(auto& instruction:block.instructions)
            if(!changed && instruction.op==vd::ir::Op::list_check){
                if(variant==0)instruction.a=instruction.b;else instruction.b=instruction.a;
                changed=true;
            }
        bool rejected=false;try{vd::ir::validate(forged);}catch(const std::invalid_argument&){rejected=true;}
        require(changed && rejected,"malformed list guard operands accepted by independent IR");
    }
}
void faults(const std::filesystem::path& root, bool optimized) {
    vd::Jit jit; auto code = compile(jit, root / "tests/collections_stdlib/faults.void", optimized);
    for (int operation = 1; operation <= 8; ++operation) {
        unsigned failures = 0; bool success = false;
        for (int budget = 0; budget < 4096 && !success; ++budget) {
            managed_test::Probe probe; probe.inspect_roots = true; probe.frames.reserve(4096);
            bool failed = false;
            { managed_test::Observe observe(probe);
              try { managed_test::AllocationFailure inject(budget); require(code->invoke(operation) == 7, "allocation sweep result"); }
              catch (const std::bad_alloc&) { failed = true; }
              catch (const std::runtime_error& error) {
                  if (std::string(error.what()).find("allocation") == std::string::npos &&
                      std::string(error.what()).find("bad_alloc") == std::string::npos) throw;
                  failed = true;
              }
            }
            require(probe.roots_valid && probe.commits_valid && probe.frames.empty(), "allocation failure corrupted roots");
            empty(*code->runtime_owner());
            require(code->invoke(operation) == 7, "allocation failure poisoned retry");
            empty(*code->runtime_owner());
            if (failed) ++failures; else success = true;
        }
        require(success && failures, "allocation sweep did not reach success and failures");
        std::cout << "operation=" << operation << " optimized=" << optimized << " injected_allocation_failures=" << failures << '\n';
    }
}
auto analyze(const std::filesystem::path& path,const vd::SourceOverrides& overrides={}) {
    auto analysis=vd::analyze_project(path,overrides);
    if(!analysis.succeeded())throw vd::CompilationError(analysis.diagnostics);
    return analysis.program;
}
void publication(const std::filesystem::path& root,bool optimized) {
    vd::Jit jit;auto code=compile(jit,root/"tests/collections_stdlib/publication.void",optimized);
    unsigned partial=0;bool success=false;
    for(int budget=0;budget<4096 && !success;++budget) {
        code->invoke(0);auto runtime=code->runtime_owner();
        {vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);runtime->collect();}
        const auto before=runtime->snapshot().objects;
        managed_test::Probe probe;probe.inspect_roots=true;probe.frames.reserve(4096);
        bool failed=false;
        {managed_test::Observe observe(probe);
         try{managed_test::AllocationFailure inject(budget);require(code->invoke(1)==7,"publication success value");}
         catch(const std::bad_alloc&){failed=true;}
         catch(const std::runtime_error& error){if(std::string(error.what()).find("allocation")==std::string::npos && std::string(error.what()).find("bad_alloc")==std::string::npos)throw;failed=true;}}
        require(probe.roots_valid && probe.commits_valid && probe.frames.empty(),"publication fault corrupted roots");
        const int visited=code->invoke(99);require(visited>=0 && visited<=8 && code->invoke(101)==8,"map callback effects/input corrupted");
        if(failed){require(code->invoke(100)==1,"failed map published an incomplete outer result");if(visited)++partial;}
        else{success=true;require(visited==8 && code->invoke(100)==0,"completed map did not publish");}
        {vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);runtime->collect();}
        if(failed)require(runtime->snapshot().objects==before,"failed private map retained heap objects");
    }
    require(success && partial,"publication sweep missed completed callback effects before failure");
    code->invoke(0);bool callback_failed=false;
    try{code->invoke(2);}catch(const std::runtime_error& error){callback_failed=std::string(error.what()).find("zero")!=std::string::npos;}
    require(callback_failed && code->invoke(99)==1 && code->invoke(100)==1 && code->invoke(101)==9,
            "failed sort lost callback effects or published a partial result");
    std::cout<<"map publication optimized="<<optimized<<" partial_effect_failures="<<partial<<" passed\n";
}
void reload(const std::filesystem::path& root,const std::filesystem::path& work,bool optimized) {
    const auto path=root/"tests/collections_stdlib/reload.void";
    const auto source=vd::read_source(path);
    std::filesystem::create_directories(work);
    std::ofstream(work/"reload-original.void",std::ios::binary)<<source.text();
    auto changed=source.text();auto position=changed.find("calls += step");
    require(position!=std::string::npos,"reload fixture marker");changed.replace(position,13,"calls += step * 10");
    changed.replace(changed.find("return left.number == right.number"),34,"return left.number != right.number");
    std::ofstream(work/"reload-changed.void",std::ios::binary)<<changed;
    const auto replacement=analyze(path,{{"/reload.void",vd::SourceFile(source.path(),changed)}});
    const auto cache_root=work/(optimized?"cache-o2":"cache-o0");
    std::filesystem::create_directories(cache_root);
    vd::Jit jit({cache_root,std::string(vd::compiler_build_identity())});
    auto original=analyze(path);auto old=jit.compile(*original,optimized);
    std::ofstream(work/"original-contract.txt",std::ios::binary)<<original->compatibility_contract();
    std::ofstream(work/"changed-contract.txt",std::ios::binary)<<replacement->compatibility_contract();
    require(jit.cache_status()=="miss","collections cache did not miss");
    auto fresh=jit.compile(*original,optimized);require(jit.cache_status()=="hit","collections cache did not hit");
    require(old->invoke(0)==0 && old->invoke(1)==301 && fresh->invoke(3)==0 && old->invoke(4)==7,"cached application state not independent");
    vd::ReloadSlot slot(old);require(jit.replace(slot,*replacement,optimized),"compatible captured comparator reload rejected");
    require(slot.invoke(1)==3301 && old->invoke(1)==3601 && slot.invoke(4)==0 && old->invoke(4)==7,"captured comparator/equality lost new/old generation behavior");
    unsigned mutation_index=0;
    for(const auto& mutation:{std::pair{"var calls = 0","var calls = 5"},std::pair{"number: i32","number: i64"}}) {
        auto invalid=changed;invalid.replace(invalid.find(mutation.first),std::char_traits<char>::length(mutation.first),mutation.second);
        std::ofstream(work/("reload-incompatible-"+std::to_string(++mutation_index)+".void"),std::ios::binary)<<invalid;
        require(!jit.replace(slot,*analyze(path,{{"/reload.void",vd::SourceFile(source.path(),invalid)}}),optimized),"incompatible reload admitted");
    }
    auto capture=changed;capture.insert(capture.find("    var step = 1"),"    var extra = 2\n");
    capture.replace(capture.find("calls += step * 10"),18,"calls += step * 10 + extra");
    std::ofstream(work/"reload-capture.void",std::ios::binary)<<capture;
    require(!jit.replace(slot,*analyze(path,{{"/reload.void",vd::SourceFile(source.path(),capture)}}),optimized),"changed callback capture admitted");
    auto instances=changed;instances.insert(instances.find("    if saved is"),"    c.sum<i64>([1])\n");
    std::ofstream(work/"reload-instances.void",std::ios::binary)<<instances;
    require(!jit.replace(slot,*analyze(path,{{"/reload.void",vd::SourceFile(source.path(),instances)}}),optimized),"changed generic instance set admitted");
    fresh.reset();old.reset();require(jit.live_generations()==1,"captured callable pinned old generation");
    require(slot.invoke(1)==6601,"stored comparator did not survive old generation retirement");
    vd::Jit active;auto original_code=active.compile(*original,optimized);vd::ReloadSlot active_slot(original_code);
    managed_test::Probe pause;
    pause.before_construct=[&](std::size_t,const void*,std::size_t){if(!pause.gated){pause.gated=true;pause.reached.release();pause.release.acquire();}};
    auto lease=original_code;int before=-1,after=-1;std::exception_ptr error;
    std::thread owner([&,lease]()mutable{try{lease->invoke(0);{managed_test::Observe observe(pause);before=lease->invoke(1);}lease.reset();after=active_slot.invoke(1);}catch(...){error=std::current_exception();}});
    pause.reached.acquire();bool replaced=active.replace(active_slot,*replacement,optimized);
    original_code.reset();lease.reset();bool retained=active.live_generations()==2;
    pause.release.release();owner.join();if(error)std::rethrow_exception(error);
    require(replaced && retained && before==301 && after==3301 && active.live_generations()==1,"active algorithm/callback lease failed");
    std::cout<<"collections cache/reload/captures/active leases optimized="<<optimized<<" passed\n";
}
}
int main(int argc, char** argv) {
    try { require(argc == 3 || argc == 4, "repository and disposable work paths required");
        guard_ir(argv[1]);
        if(argc==4 && std::string(argv[3])=="--ir-only") {std::cout<<"Malformed collection guard IR rejected\n";return 0;}
        for (bool optimized : {false, true}) { if(argc==3){roots(argv[1], optimized); faults(argv[1], optimized);publication(argv[1],optimized);} reload(argv[1],argv[2],optimized); }
        std::cout << "Collections forced-GC and allocation sweeps passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
