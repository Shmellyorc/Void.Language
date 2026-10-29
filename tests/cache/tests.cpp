#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/tooling.hpp"
#include "void/runtime.hpp"
#include "../managed/probe.hpp"
#include "../../src/backend/llvm/native_cache.hpp"
#include <fstream>
#include <iostream>
namespace {
void require(bool yes,const char* text){if(!yes)throw std::runtime_error(text);}
void write(const std::filesystem::path& file,const std::string& text){std::ofstream out(file);out<<text;require(bool(out),"fixture write");}
std::shared_ptr<const vd::ValidatedAnalysis> analyze(const std::filesystem::path& path){auto a=vd::analyze_project(path);if(!a.succeeded())throw vd::CompilationError(a.diagnostics);return a.program;}
void empty(vd::Runtime& runtime){vd::Runtime::Attachment a(runtime);vd::Runtime::Managed m(runtime);require(runtime.collect()==vd::Runtime::Collection::completed,"collection failed");require(!runtime.snapshot().objects,"root lifetime leak");}
std::int32_t identity_import(std::int32_t value){return value;}
}
int main(int argc,char**argv){try{
    require(argc==3,"cache test root and repository required");auto root=std::filesystem::path(argv[1]);
    std::filesystem::remove_all(root);std::filesystem::create_directories(root);
    {
        vd::llvm_backend::NativeCache storage(root,"allocation-write");std::string object(256,'x');
        managed_test::AllocationFailure failure(0);
        require(!storage.write(false,object),"optional publication allocation escaped");
    }
    const auto identity=std::string(vd::compiler_build_identity());require(identity.empty() || identity.size()==64,"invalid embedded content ID");
    {
        auto equality=vd::analyze_source({"/equality.void",R"VOID(value struct counter:
    amount: i32
    operator ==(const left: counter, const right: counter) -> bool:
        return left.amount == right.amount
func unused() -> i32:
    return 99
func entry(n: i32) -> i32:
    var item = counter(amount = n)
    return item.amount
)VOID"});
        require(equality.succeeded(),"equality retention fixture analysis");
        auto entry=equality.program->lower(vd::ir::Emission::entry);
        vd::ir::validate(entry);
        unsigned operators=0;
        for(const auto& function:entry.functions){
            operators+=function.operator_glyph=="==";
            require(function.source_name!="unused","entry emission retained unrelated function");
        }
        require(operators==1,"entry emission lost custom equality declaration");
        vd::Jit emitted({},vd::ir::Emission::entry);
        for(bool optimized:{false,true})require(emitted.compile(*equality.program,optimized)->invoke(7)==7,
                                               "custom equality metadata broke entry execution");
    }
    auto path=root/"main.void";auto source=vd::read_source(std::filesystem::path(argv[2])/"examples/advanced/combined.void").text();write(path,source);
    auto a=analyze(path);vd::Jit jit({root,identity});auto old=jit.compile(*a,false);require(old->invoke(7)==47,"cold result");
    bool supported=jit.cache_status()!="unavailable";
    require(jit.cache_status()==(supported?"miss":"unavailable"),"first cache status");
    auto hit=jit.compile(*a,false);require(jit.cache_status()==(supported?"hit":"unavailable"),"second cache status");
    require(hit->invoke(7)==47 && old->invoke(7)==47 && jit.live_generations()==2,"isolated generations");
    auto runtime=hit->runtime_owner();managed_test::Probe probe;probe.inspect_roots=true;probe.collect_before=true;probe.frames.reserve(256);
    {managed_test::Observe observe(probe);require(hit->invoke(7)==47,"cached GC result");}
    require(probe.frames.empty() && probe.roots_valid && probe.commits_valid,"cache roots/publication");empty(*runtime);
    bool complete=false;unsigned failures=0;
    for(int budget=0;budget<1536 && !complete;++budget){managed_test::Probe failed;failed.inspect_roots=true;failed.frames.reserve(256);
        {managed_test::Observe observe(failed);try{managed_test::AllocationFailure failure(budget);require(hit->invoke(7)==47,"failure retry result");complete=true;}catch(const std::bad_alloc&){++failures;}catch(const std::runtime_error&){++failures;}}
        require(failed.frames.empty() && failed.roots_valid && failed.commits_valid,"cached allocation cleanup");empty(*runtime);
    }
    require(complete && failures>0,"cached allocation sweep");
    vd::ReloadSlot slot(hit);auto changed=source;auto pos=changed.find("return a.amount + value");changed.insert(pos+std::string("return a.amount + value").size()," + 1");write(path,changed);auto next=analyze(path);
    require(jit.replace(slot,*next,false) && slot.invoke(7)==48 && old->invoke(7)==47,"body reload");
    require(jit.replace(slot,*next,false) && jit.cache_status()==(supported?"hit":"unavailable"),"forced-coordination reload hit");
    auto layout=changed;layout.insert(layout.find("    stored: i32"),"    extra: i32\n");write(path,layout);require(!jit.replace(slot,*analyze(path),false),"incompatible layout accepted");
    write(path,changed);vd::ToolingSession session(path);auto revision=vd::ToolingSession::analyze(session.select());require(session.accept(revision),"revision accept");
    require(jit.replace(slot,session,*revision,false) && slot.invoke(7)==48,"current revision publication");
    session.refresh();require(!jit.replace(slot,session,*revision,false),"stale revision published");
    require(slot.invoke(7)==48,"stale publication changed state");
    if(supported){
        auto directory=root/".void/cache/v1";for(auto& file:std::filesystem::directory_iterator(directory)){if(file.path().filename().string().starts_with("tmp-"))continue;write(file.path(),"corrupt");}
        auto retry=jit.compile(*a,false);require(jit.cache_status()=="miss" && retry->invoke(7)==47,"corrupt fallback");
        std::filesystem::remove_all(root/".void");write(root/".void","blocked");auto unavailable=jit.compile(*a,false);require(jit.cache_status()=="unavailable" && unavailable->invoke(7)==47,"filesystem fallback");std::filesystem::remove(root/".void");
    }
    {vd::Jit changed_build({root,identity+"different"});require(changed_build.compile(*a,false)->invoke(7)==47 && changed_build.cache_status()!=("hit"),"build invalidation");}
    {auto optimized=jit.compile(*a,true);require(optimized->invoke(7)==47 && jit.cache_status()==(supported?"miss":"unavailable"),"optimized mode cache separation");
     vd::Jit fresh({root,identity});auto reused=fresh.compile(*a,true);
     require(reused->invoke(7)==47 && fresh.cache_status()==(supported?"hit":"unavailable"),"optimized fresh JIT cache hit");
     require(reused->runtime_owner()!=optimized->runtime_owner(),"cached object shared Runtime state");
     vd::Jit changed_build({root,identity+"different"});
     require(changed_build.compile(*a,true)->invoke(7)==47 && changed_build.cache_status()!="hit","optimized build invalidation");
     vd::Jit entry_only({root,identity},vd::ir::Emission::entry);
     require(entry_only.compile(*a,true)->invoke(7)==47 && entry_only.cache_status()==(supported?"miss":"unavailable"),"entry/export cache separation");
     require(entry_only.compile(*a,true)->invoke(7)==47 && entry_only.cache_status()==(supported?"hit":"unavailable"),"entry-emission warm cache");}
    {vd::Jit custom({root,identity});custom.bind("cache_unused_import",identity_import);require(custom.compile(*a,false)->invoke(7)==47 && custom.cache_status()==(identity.empty()?"unavailable":"disabled"),"custom import persistence bypass");}
    old.reset();hit.reset();require(slot.invoke(7)==48,"retired old generation");empty(*runtime);
    std::cout<<"cache generations, source/build invalidation, revisions, roots, failures and fallback passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
