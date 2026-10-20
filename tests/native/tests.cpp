#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/native_runtime.hpp"
#include "void/cache.hpp"
#include "../managed/probe.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <future>
#include <chrono>
#include <cstring>
#include "probe.hpp"
#include <iostream>
#include <stdexcept>
void native_integrated_tests(const std::filesystem::path&,const std::filesystem::path&);
namespace {
std::thread::id collecting_owner;
std::atomic<unsigned> collected_callbacks{0};
void collect_callback(){if(std::this_thread::get_id()==collecting_owner)return;if(vd::Runtime::current().collect()!=vd::Runtime::Collection::completed)std::terminate();++collected_callbacks;}
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
auto analyze(const std::filesystem::path& file){auto a=vd::analyze_project(file,vd::native_fixture_project());if(!a.succeeded())throw vd::CompilationError(a.diagnostics);return a.program;}
}
int main(int argc,char** argv){try{
    if(argc==4 && std::string(argv[1])=="--cached-integrated"){vd::Jit jit({argv[3],std::string(vd::compiler_build_identity())});auto code=jit.compile_async(*analyze(argv[2]),false);auto result=code->invoke(7);require(result==32 && vd::native_fixture_live()==0 && code->runtime_owner()->snapshot().threads==0,"cross-process integrated state leaked");std::cout<<jit.cache_status()<<":"<<result<<"\n";return 0;}
    if(argc==4 && std::string(argv[1])=="--integrated"){native_integrated_tests(argv[2],argv[3]);return 0;}
    if(argc==5 && std::string(argv[1])=="--emit"){auto module=analyze(argv[2])->lower();auto entry=std::find_if(module.functions.begin(),module.functions.end(),[&](const auto& f){return f.name==module.entry_symbol;});if(entry!=module.functions.end() && entry->asynchronous)vd::emit_async_object(module,argv[3],std::string(argv[4])=="optimized");else vd::emit_object(module,argv[3],std::string(argv[4])=="optimized",true);return 0;}
    require(argc==3,"repository and fixture paths required");auto root=std::filesystem::path(argv[1]);auto a=analyze(root/"examples/native/main.void");
    for(bool optimized:{false,true}){vd::Jit jit;auto code=jit.compile(*a,optimized);auto actual=code->invoke(7);if(actual!=19)throw std::runtime_error("native source result: "+std::to_string(actual));require(vd::native_fixture_live()==0,"native resource leaked");}
    auto fixtures=std::filesystem::path(argv[2]);std::filesystem::create_directories(fixtures);auto main=fixtures/"main.void";auto wrappers=fixtures/"wrappers.void";
    const auto wrapper_text=vd::read_source(root/"examples/native/wrappers.void").text();
    auto write=[&](const std::filesystem::path& file,const std::string& text){std::ofstream stream(file);stream<<text;require(bool(stream),"native fixture write");};
    write(wrappers,wrapper_text);
    for(auto [name,expected]:{std::pair{"terminated_nul","native text cannot contain NUL"},std::pair{"closed","Native resource is closed or invalid"},std::pair{"duplicate","Native resource is closed or invalid"}}){auto program=analyze(root/"examples/native"/(std::string(name)+".void"));for(bool optimized:{false,true}){vd::Jit jit;auto code=jit.compile(*program,optimized);bool failed=false;try{code->invoke(7);}catch(const std::runtime_error& error){failed=std::string(error.what()).find(expected)!=std::string::npos;}require(failed,"native checked failure missing");require(vd::native_fixture_live()==0,"failed native call leaked resource");}}
    {auto program=analyze(root/"examples/native/cleanup_failure.void");for(bool optimized:{false,true}){vd::Jit jit;auto code=jit.compile(*program,optimized);bool preserved=false;try{code->invoke(7);}catch(const std::runtime_error& error){auto text=std::string(error.what());preserved=text.starts_with("native text cannot contain NUL") && text.find("Additional failure during defer")!=std::string::npos && text.find("Native resource is closed or invalid")!=std::string::npos;}require(preserved && vd::native_fixture_live()==0,"native primary/cleanup diagnostics were overwritten");}}
    // Foreign generated callbacks collect with captured roots while the owner is
    // NativeSafe. Normal source cleanup unregisters and quiesces before release.
    collecting_owner=std::this_thread::get_id();
    for(bool optimized:{false,true}){vd::Jit jit;auto code=jit.compile(*a,optimized);managed_test::concurrent_poll=collect_callback;auto result=code->invoke(7);managed_test::concurrent_poll=nullptr;require(result==19 && collected_callbacks>0,"foreign callback GC/root result");require(vd::native_fixture_live()==0,"callback GC leaked native resource");}
    // The persisted object contains relocations only: each hit gets fresh native
    // resources, roots and code leases in its isolated generation.
    {auto cache_root=fixtures/"native-cache";std::filesystem::remove_all(cache_root);std::filesystem::create_directories(cache_root);vd::Jit jit({cache_root,std::string(vd::compiler_build_identity())});auto cold=jit.compile(*a,false);bool supported=jit.cache_status()!="unavailable";require(jit.cache_status()==(supported?"miss":"unavailable"),"native cache cold status");require(cold->invoke(7)==19,"native cache cold result");auto hit=jit.compile(*a,false);require(jit.cache_status()==(supported?"hit":"unavailable") && hit->invoke(7)==19 && cold->invoke(7)==19 && vd::native_fixture_live()==0,"native cache hit isolation");}
    // Whole caller-allocation sweep includes callback registration, thread startup,
    // returned C-buffer conversion and transactional resource publication.
    {vd::Jit jit;auto code=jit.compile(*a,false);bool succeeded=false;unsigned failures=0;for(int budget=0;budget<1024 && !succeeded;++budget){try{managed_test::AllocationFailure fault(budget);require(code->invoke(7)==19,"native allocation retry result");succeeded=true;}catch(const std::bad_alloc&){++failures;}catch(const std::runtime_error&){++failures;}require(vd::native_fixture_live()==0,"native allocation failure leaked C resource");auto runtime=code->runtime_owner();vd::Runtime::Attachment attachment(*runtime);vd::Runtime::Managed managed(*runtime);require(runtime->collect()==vd::Runtime::Collection::completed && runtime->snapshot().objects==0,"native allocation failure retained GC roots");}require(succeeded && failures,"native allocation sweep did not finish");}
    const std::string foreground="use \"/wrappers.void\" as library\nfunc entry(value: i32) -> i32:\n    with library.open() as owner:\n        owner.connect(value)\n        return owner.foreign(value)\n";
    write(main,foreground);auto gated=analyze(main);
    // Actual C dispatch reserved by the provider, held before admission. Shutdown
    // reaches unregister but cannot finish while this reserved dispatch remains.
    for(bool optimized:{false,true}){vd::Jit jit;auto code=jit.compile(*gated,optimized);auto runtime=code->runtime_owner();native_test::Gate gate;native_test::gate=&gate;
        auto run=std::async(std::launch::async,[&]{gate.owner=std::this_thread::get_id();try{code->invoke(41);return false;}catch(const std::runtime_error& error){return std::string(error.what()).find("Native callback admission")!=std::string::npos;}});
        gate.reached.acquire();{vd::Runtime::Attachment attachment(*runtime);vd::Runtime::Managed managed(*runtime);require(runtime->collect()==vd::Runtime::Collection::completed,"before-admission callback roots");}
        auto close=std::async(std::launch::async,[&]{runtime->close();});gate.unregistered.acquire();bool waiting=close.wait_for(std::chrono::seconds(0))==std::future_status::timeout;auto second_close=std::async(std::launch::async,[&]{runtime->close();});gate.release.release();close.get();second_close.get();require(waiting && run.get(),"provider did not quiesce a dispatched pre-admission callback");native_test::gate=nullptr;require(vd::native_fixture_live()==0 && runtime->snapshot().threads==0,"native shutdown leaked participant/resource");
    }
    // Fail the foreign callback's allocations independently of its caller.
    write(wrappers,wrapper_text);auto allocated=wrapper_text;auto method=allocated.find("    return owner.bias + value");allocated.replace(method,std::string("    return owner.bias + value").size(),"    var allocated = \"callback-owned native bytes\"\n    return owner.bias + value");write(wrappers,allocated);auto callback_alloc=analyze(main);
    {vd::Jit jit;auto code=jit.compile(*callback_alloc,false);native_test::Gate gate;gate.owner=std::this_thread::get_id();gate.fail_callback=true;native_test::gate=&gate;bool failed=false;try{code->invoke(7);}catch(const std::runtime_error&){failed=true;}native_test::gate=nullptr;require(failed && vd::native_fixture_live()==0,"foreign callback allocation failure escaped/leaked");}
    // Same-thread self-drain fails rather than deadlocking; a foreign callback
    // cannot use owning-thread resource operations. Both retain checked failures.
    for(bool foreign:{false,true}){auto changed=wrapper_text;auto at=changed.find("    return owner.bias + value");changed.replace(at,strlen("    return owner.bias + value"),foreign?"    return owner.byte(0 as u64)":"    owner.disconnect()\n    return value");write(wrappers,changed);write(main,foreign?foreground:std::string("use \"/wrappers.void\" as library\nfunc entry(value: i32) -> i32:\n    with library.open() as owner:\n        owner.connect(value)\n        return owner.invoke(value)\n"));auto program=analyze(main);for(bool optimized:{false,true}){vd::Jit jit;auto code=jit.compile(*program,optimized);bool rejected=false;try{code->invoke(7);}catch(const std::runtime_error& error){rejected=std::string(error.what()).find(foreign?"owning thread":"cannot drain itself")!=std::string::npos;}require(rejected && vd::native_fixture_live()==0,"native callback affinity/self-drain failure missing");}}
    write(wrappers,wrapper_text);
    // A native registration retains its exact old generation independently of
    // the original NativeCode or source activation. Release breaks that lease.
    const std::string persistent="use \"/wrappers.void\" as library\nfunc entry(value: i32) -> i32:\n    var owner = library.open()\n    owner.connect(value)\n    return owner.invoke(1)\n";
    write(main,persistent);auto old_analysis=analyze(main);
    {vd::Jit jit({fixtures,std::string(vd::compiler_build_identity())});auto old=jit.compile(*old_analysis,false);require(old->invoke(7)==8,"native persistent callback setup");auto runtime=old->runtime_owner();vd::ReloadSlot slot(old);
        auto changed=wrapper_text;auto at=changed.find("return owner.bias + value");changed.replace(at,strlen("return owner.bias + value"),"return owner.bias + value + 100");write(wrappers,changed);auto edited=analyze(main);require(jit.replace(slot,*edited,false),"native compatible reload rejected");old.reset();require(jit.live_generations()==2,"native callback old generation retired early");
        {vd::Runtime::Attachment attachment(*runtime);vd::Runtime::Managed managed(*runtime);vd::Runtime::Handle identity(*runtime,vd::native_fixture_identity(*runtime));vd::RuntimeFailure failure;auto result=vd_native_fixture_call(21,2,identity.get(),nullptr,0,nullptr,nullptr,nullptr,"",&failure);require(!failure.message && result==9,"native callback migrated to replacement generation");vd_native_fixture_call(23,0,identity.get(),nullptr,0,nullptr,nullptr,nullptr,"",&failure);require(!failure.message && jit.live_generations()==1,"native unregister did not release generation lease");vd_native_fixture_call(15,0,identity.get(),nullptr,0,nullptr,nullptr,nullptr,"",&failure);require(!failure.message,"persistent native close failed");}
        require(slot.invoke(7)==108,"replacement native callback result");runtime->close();require(vd::native_fixture_live()==0,"native shutdown leaked persistent registration");
    }
    // GC has no native finalizer. A dropped source owner stays a real live native
    // resource until explicit Runtime shutdown drains its provider and owner.
    write(wrappers,wrapper_text);write(main,"use \"/wrappers.void\" as library\nfunc entry(value: i32) -> i32:\n    var owner = library.open()\n    return value\n");
    {vd::Jit jit;auto code=jit.compile(*analyze(main),false);require(code->invoke(7)==7 && vd::native_fixture_live()==1,"native resource setup");auto runtime=code->runtime_owner();{vd::Runtime::Attachment attachment(*runtime);vd::Runtime::Managed managed(*runtime);runtime->collect();require(vd::native_fixture_live()==1,"GC implicitly finalized native resource");}runtime->close();require(vd::native_fixture_live()==0,"shutdown did not release dropped native resource");}
    // Explicit descriptors fail closed on missing symbols, ABI mismatch and unsafe
    // retention changes. No dynamic symbol lookup or public pointer type exists.
    for(int kind=0;kind<3;++kind){auto descriptor=vd::native_fixture_project();if(kind==0)descriptor.bindings[0].symbol="missing_symbol";if(kind==1)descriptor.bindings[0].parameters={"string"};if(kind==2)descriptor.bindings[0].callbacks=true;bool rejected=false;try{vd::analyze_project(main,descriptor);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"unverified native descriptor accepted");}
    require(!vd::analyze_project(root/"examples/native/main.void").succeeded(),"native project silently admitted without explicit trust descriptor");
    for(const auto& text:{
        "use \"/native_bridge.void\" as bridge\nvalue struct copied_owner:\n    handle: bridge.resource? = null\nfunc (owner copied_owner) deinit():\n    pass\nfunc entry(value: i32) -> i32:\n    return value\n",
        "use \"/native_bridge.void\" as bridge\nasync func transfer(handle: bridge.resource) -> i32:\n    return 1\nfunc entry(value: i32) -> i32:\n    return value\n",
        "use \"/native_bridge.void\" as bridge\nfunc entry(value: i32) -> i32:\n    var forged = bridge.resource()\n    return value\n",
        "use \"/native_bridge.void\" as bridge\nstruct forged extends bridge.resource:\n    number: i32 = 0\nfunc entry(value: i32) -> i32:\n    return value\n",
        "use \"/native_bridge.void\" as bridge\nfunc entry(value: i32) -> i32:\n    var handle = bridge.acquire(true) as bridge.resource\n    handle.token = 12 as u64\n    return value\n",
        "use \"/native_bridge.void\" as bridge\nfunc work(value: i32) -> i32:\n    return bridge.scalar_i32(value)\nasync func entry(value: i32) -> i32:\n    return await spawn work(value)\n",
        "use \"/native_bridge.void\" as bridge\nfunc entry(value: i32) -> i32:\n    var handle = bridge.acquire(true) as bridge.resource\n    var callback = func(x: i32) -> i32:\n        return x\n    bridge.register(handle, callback)\n    return value\n"
    }){write(main,text);require(!vd::analyze_project(main,vd::native_fixture_project()).succeeded(),"unsafe native source accepted");}
    // Independent typed IR checks cannot be bypassed by removing source guards.
    auto module=a->lower();auto reject=[&](auto mutate){auto bad=module;mutate(bad);bool failed=false;try{vd::ir::validate(bad);}catch(const std::invalid_argument&){failed=true;}require(failed,"malformed native IR accepted");};
    reject([](auto& m){auto resource=*std::find_if(m.nominal_types.begin(),m.nominal_types.end(),[](auto t){return t.definition && t.definition->native_resource;});auto& block=m.functions.front().blocks.front();vd::ir::Instruction zero{vd::ir::Op::constant};zero.type=vd::ir::Type::u64;unsigned value=block.instructions.size();block.instructions.push_back(zero);vd::ir::Instruction forged{vd::ir::Op::gc_construct};forged.type=resource;forged.arguments={value};block.instructions.push_back(forged);});
    reject([](auto& m){for(auto& f:m.functions)for(auto& b:f.blocks)for(unsigned n=0;n<b.instructions.size();++n)if(b.instructions[n].op==vd::ir::Op::native_call && b.instructions[n].callee=="vd_native_fixture_call#register"){b.instructions[n].arguments[1]=n-1;return;}});
    reject([](auto& m){for(auto& f:m.functions)for(auto& b:f.blocks)for(auto& i:b.instructions)if(i.op==vd::ir::Op::native_call){i.type=vd::ir::Type::fixed_array;return;}});
    reject([](auto& m){m.native_library.clear();});
    reject([](auto& m){m.imports.push_back("vd_native_fixture_call");});
    reject([](auto& m){for(auto& f:m.functions)for(auto& b:f.blocks)for(auto& i:b.instructions)if(i.op==vd::ir::Op::native_call){i.callee="missing_symbol";return;}});
    reject([](auto& m){for(auto& f:m.functions)for(auto& b:f.blocks)for(auto& i:b.instructions)if(i.op==vd::ir::Op::native_call && i.callee=="vd_native_fixture_call#register"){i.arguments[1]=i.arguments[0];return;}});

    {auto source=vd::analyze_project(root/"examples/async/spawn.void");require(source.succeeded(),"native worker validation fixture");auto bad=source.program->lower();bad.native_library="void.fixture.v1";bool changed=false;for(auto& f:bad.functions)if(f.source_name=="pair"){vd::ir::Instruction native{vd::ir::Op::native_call};native.callee="vd_native_fixture_call#i32";native.type=vd::ir::Type::i32;native.arguments={0};f.blocks={{{{vd::ir::Op::argument,0},native},{vd::ir::Terminator::Kind::return_value,1}}};changed=true;}bool rejected=false;try{vd::ir::validate(bad);}catch(const std::invalid_argument& error){rejected=std::string(error.what()).find("worker")!=std::string::npos;}require(changed && rejected,"verified scalar native effect escaped independent worker IR validation");}
    std::cout<<"native source passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
