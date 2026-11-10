#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/os.hpp"
#include "void/os_abi.hpp"
#include "void/path.hpp"
#include "void/runtime_abi.hpp"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <new>
#include <stdexcept>
#include <utility>

namespace {
thread_local bool fail_next_allocation = false;
}
void* operator new(std::size_t size) {
    if (std::exchange(fail_next_allocation,false)) throw std::bad_alloc();
    if (auto* pointer=std::malloc(size?size:1)) return pointer;
    throw std::bad_alloc();
}
void operator delete(void* pointer) noexcept {std::free(pointer);}
void operator delete(void* pointer,std::size_t) noexcept {std::free(pointer);}
void* operator new(std::size_t size,const std::nothrow_t&) noexcept {
    try {return ::operator new(size);} catch (const std::bad_alloc&) {return nullptr;}
}
void operator delete(void* pointer,const std::nothrow_t&) noexcept {::operator delete(pointer);}

namespace {
namespace fs=std::filesystem;
unsigned checks=0, startup_calls=0;
void check(bool value,const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
template<class Exception=std::invalid_argument,class Action>
void rejects(Action action,const char* message) {
    try {action();} catch (const Exception&) {check(true,message);return;}
    check(false,message);
}

vd::ApplicationMetadata::Owner metadata(std::string_view app="org.test.application") {
    return vd::ApplicationMetadata::make({"org.test.project","Project","2.3.4",app,"App","5.6.7-beta+build"});
}
std::int32_t bootstrap_entry(std::int32_t value,vd::RuntimeFailure*) {
    ++startup_calls;
    auto context=vd::Runtime::current().application_metadata();
    return context && context->application_id()=="org.test.application"?value:-1;
}
void admission() {
    auto record=metadata();
    auto decoded=vd::ApplicationMetadata::decode(record->bytes());
    check(decoded!=record && *decoded==*record,"metadata decode owns the bytes");
    rejects([&]{metadata("con.device");},"invalid host ID accepted");
    rejects([&]{vd::ApplicationMetadata::make({"ok","\x01","0.0.0","ok","App","0.0.0"});},"invalid name accepted");
    rejects([&]{vd::ApplicationMetadata::make({"ok","Project","0.0.0","ok","App","0.0.0-01"});},"invalid SemVer accepted");

    vd::Runtime first,second;
    first.admit_application(record);
    rejects<std::logic_error>([&]{first.admit_application(record);},"duplicate admission accepted");
    first.start_application(decoded,false);
    first.start_application(decoded);
    first.start_application(decoded);
    rejects<std::logic_error>([&]{first.admit_application(record);},"late admission accepted");
    rejects([&]{first.start_application(metadata("org.other.app"));},"conflicting context accepted");
    second.admit_application({});
    rejects([&]{second.start_application(record);},"explicit absence was overwritten");
    second.start_application();
    check(first.application_metadata()==record && !second.application_metadata(),"independent admission states");

    auto boot=[&](std::string_view bytes,vd::RuntimeFailure& failure) {
        return vd_runtime_entry_context_v1(bootstrap_entry,17,&failure,nullptr,0,nullptr,0,0,nullptr,false,bytes.data(),bytes.size());
    };
    auto broken=record->bytes();broken[0]=2;
    vd::RuntimeFailure failure;
    check(boot(broken,failure)==0 && failure.message && startup_calls==0,"unsupported revision ran entry");
    failure={};broken=record->bytes();broken[4]=static_cast<char>(255);
    check(boot(broken,failure)==0 && failure.message && startup_calls==0,"corrupt extent ran entry");
    failure={};
    check(boot(std::string_view(record->bytes()).substr(0,record->bytes().size()-1),failure)==0 && failure.message && startup_calls==0,"truncated metadata ran entry");
    failure={};fail_next_allocation=true;
    auto status=boot(record->bytes(),failure);
    fail_next_allocation=false;
    check(status==0 && failure.message && startup_calls==0,"metadata allocation failure ran entry");
    failure={};
    check(boot(record->bytes(),failure)==17 && !failure.message && startup_calls==1,"startup did not see admitted metadata");
}

void bridge_allocation() {
    struct Reply {std::int64_t kind,code,number;bool flag;void* text;void* domain;};
    const vd::os::ReplyLayout geometry{sizeof(Reply),{offsetof(Reply,kind),offsetof(Reply,code),offsetof(Reply,number),offsetof(Reply,flag),offsetof(Reply,text),offsetof(Reply,domain)}};
    const std::size_t refs[]={offsetof(Reply,text),offsetof(Reply,domain)};
    auto context=metadata("org.test.long-application-identity");
    vd::Runtime runtime(1);runtime.start_application(context);
    vd::Runtime::Attachment attachment(runtime);
    vd::Runtime::Managed managed(runtime);
    Reply reply{};
    vd::Runtime::Slot slot{&reply,sizeof(reply),refs,2};
    vd::Runtime::Frame roots(runtime,&slot,1);
    vd::RuntimeFailure failure;
    fail_next_allocation=true;
    vd_os_call(&reply,&geometry,static_cast<unsigned>(vd::os::Operation::application_id),nullptr,&failure);
    fail_next_allocation=false;
    check(failure.message && std::string_view(failure.message)=="OS boundary allocation failed" && !reply.text,"OS identity fault became an ordinary result");
    failure={};
    vd_os_call(&reply,&geometry,static_cast<unsigned>(vd::os::Operation::application_id),nullptr,&failure);
    check(!failure.message && reply.kind==0 && runtime.string_bytes(reply.text)==context->application_id(),"rooted OS reply did not recover");
    check(runtime.collect()==vd::Runtime::Collection::completed && runtime.string_bytes(reply.text)==context->application_id(),"OS identity reply lost its precise root");
}

struct Fixture {
    fs::path root=fs::temp_directory_path()/vd::utf8_path("VOID S4 application 雪 "+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Fixture(){fs::create_directories(root);}
    ~Fixture(){std::error_code error;fs::remove_all(root,error);}
    void put(const fs::path& file,std::string_view text) {
        auto path=root/file;
        fs::create_directories(path.parent_path());
        std::ofstream out(path,std::ios::binary);out<<text;
        if (!out) throw std::runtime_error("application fixture write failed");
    }
    void manifest(std::string_view id,std::string_view name="Game",std::string_view version="1.0.0") {
        put("void.project.toml","schema=1\n[project]\nid='org.test.project'\nname='Project'\nversion='2.0.0'\nkind='application'\n"
            "[application]\nid='"+std::string(id)+"'\nname='"+std::string(name)+"'\nversion='"+std::string(version)+"'\n");
    }
    std::shared_ptr<const vd::ValidatedAnalysis> analysis() const {
        auto result=vd::analyze_manifest_project(root);
        if(!result.succeeded())throw vd::CompilationError(std::move(result.diagnostics));
        return result.project->program();
    }
};
std::string body(std::string_view id,int value,bool globals) {
    std::string text="use \"./locations.void\" as lib\nuse os\n";
    if(globals)text+="var snapshot = lib.directory()\nvar state = 0\n";
    text+="func main() -> i32:\n";
    if(globals)text+="    state += 1\n    var actual = snapshot\n";
    else text+="    var actual = lib.directory()\n";
    text+="    if actual is Ok(var observed):\n        if os.data_dir(app_id = \""+std::string(id)+"\") is Ok(var expected):\n"
        "            if observed == expected:\n                return "+std::string(globals?"state + ":"")+std::to_string(value)+"\n    return 99\n";
    return text;
}
void generation(bool optimize) {
    constexpr auto a="org.test.alpha",b="org.test.beta";
    const auto library="use os as system\nfunc directory() -> result<string, system.error>:\n    return system.data_dir()\n";
    Fixture first,second;
    first.manifest(a);second.manifest(b);
    first.put("locations.void",library);second.put("locations.void",library);
    first.put("main.void",body(a,10,true));second.put("main.void",body(b,30,false));
    auto original=first.analysis(),other=second.analysis();
    vd::Jit jit({},vd::ir::Emission::entry);
    auto one=jit.compile(*original,optimize),two=jit.compile(*other,optimize);
    check(one->runtime_owner() && two->runtime_owner() && one->runtime_owner()!=two->runtime_owner(),"independent applications share a Runtime");
    check(one->invoke(0)==11 && two->invoke(0)==30,"application identity missing in global or shared library code");
    check(one->runtime_owner()->application_metadata()->application_id()==a && two->runtime_owner()->application_metadata()->application_id()==b,"Runtime ID leakage");

    auto slot=std::make_unique<vd::ReloadSlot>(one);
    first.put("main.void",body(a,20,true));
    auto edited=first.analysis();
    check(jit.replace(*slot,*edited,optimize),"body-only application replacement rejected");
    auto next=slot->acquire();
    check(next->runtime_owner()==one->runtime_owner() && next->invoke(0)==22,"global cells or application context reset on replacement");
    check(next->compatibility().build_provenance!=one->compatibility().build_provenance,"generation provenance did not notice changed bytes");
    check(one->invoke(0)==13,"retained old generation stopped working");
    first.manifest(a,"Renamed","1.0.1");
    check(!jit.replace(*slot,*first.analysis(),optimize),"metadata name/version changed without restart");
    first.manifest(b);
    check(!jit.replace(*slot,*first.analysis(),optimize),"application ID changed without restart");
    first.manifest(a);
    auto changed=body(a,20,true);changed.replace(changed.find("state = 0"),9,"state = 1");
    first.put("main.void",changed);
    check(!jit.replace(*slot,*first.analysis(),optimize),"global initializer change retained incompatible cells");
    first.put("main.void",body(a,20,true));
    fs::rename(first.root/"main.void",first.root/"renamed.void");
    check(!jit.replace(*slot,*first.analysis(),optimize),"entry rename reused incompatible application state");
    fs::rename(first.root/"renamed.void",first.root/"main.void");
    {
        std::ofstream manifest(first.root/"void.project.toml",std::ios::app);
        manifest<<"[sources]\nexclude=['elsewhere']\n";
    }
    check(!jit.replace(*slot,*first.analysis(),optimize),"source policy change reused selected application");
    std::weak_ptr<vd::Runtime> closed=one->runtime_owner();
    slot.reset();next.reset();one.reset();
    check(closed.expired() && two->invoke(0)==30,"closing one application affected another Runtime");

    auto ir=other->lower(vd::ir::Emission::entry);
    auto explicit_owner=std::make_shared<vd::Runtime>(1);
    explicit_owner->admit_application(other->application_metadata());
    auto matching=jit.compile(ir,ir.entry_symbol,optimize,{},explicit_owner);
    check(matching->invoke(0)==30,"matching host context or GC-budgeted query failed");
    auto standalone=vd::analyze_project(second.root/"main.void");
    check(standalone.succeeded() && !standalone.program->application_metadata(),"bare source inherited project metadata");
    auto standalone_ir=standalone.program->lower(vd::ir::Emission::entry);
    auto hosted=jit.compile(standalone_ir,standalone_ir.entry_symbol,optimize,{},explicit_owner);
    check(hosted->invoke(0)==30,"standalone embedding discarded its admitted host context");
    auto conflict=std::make_shared<vd::Runtime>();
    conflict->admit_application(other->application_metadata());
    auto eager=original->lower(vd::ir::Emission::entry);
    rejects([&]{jit.compile(eager,eager.entry_symbol,optimize,{},conflict);},"conflicting host metadata reached eager program");
    check(conflict->application_metadata()==other->application_metadata(),"failed admission changed host metadata");

    auto bad=ir;
    bad.application_metadata.reset();
    rejects([&]{vd::ir::validate(bad);},"typed IR allowed compiled metadata to be stripped");
    bad=ir;
    bool found=false;
    for(auto& f:bad.functions)for(auto& block:f.blocks)for(auto& i:block.instructions)if(i.os_location_adapter) {
        i.os_location_adapter=false;found=true;
    }
    check(found,"adapter proof missing from typed IR");
    rejects([&]{vd::ir::validate(bad);},"typed IR allowed forged private adapter call");
}
void cache_and_scalar() {
    Fixture fixture;fixture.manifest("org.test.cached");
    fixture.put("main.void","func main() -> i32:\n    return 7\n");
    vd::Jit scalar;
    auto code=scalar.compile(*fixture.analysis());
    check(!code->runtime_owner() && code->invoke(0)==7 && code->compatibility().application_metadata,"metadata forced a Runtime for scalar code");

    fixture.put("locations.void","use os\nfunc directory() -> result<string, os.error>:\n    return os.data_dir()\n");
    fixture.put("main.void",body("org.test.cached",9,false));
    auto program=fixture.analysis();
    vd::Jit cache({fixture.root,"slice4-application-test"},vd::ir::Emission::entry);
    auto first=cache.compile(*program,false);
    auto cold=cache.cache_status();
    auto second=cache.compile(*program,false);
    auto warm=cache.cache_status();
    check(first->runtime_owner()!=second->runtime_owner() && first->invoke(0)==9 && second->invoke(0)==9,"cache reused Runtime state");
    check((cold=="miss" && warm=="hit") || (cold=="unavailable" && warm=="unavailable"),"cache cold/warm result");
    std::cout<<"Application cache: "<<cold<<"/"<<warm<<'\n';

    Fixture library;
    library.put("void.project.toml","schema=1\n[project]\nid='org.test.lib'\nname='Lib'\nversion='1.0.0'\n"
        "kind='library'\n[package]\nid='org.test.locations'\nname='locations'\n");
    library.put("locations.void","use os\nfunc directory() -> result<string, os.error>:\n    return os.data_dir()\n");
    auto checked=vd::analyze_manifest_project(library.root);
    check(checked.succeeded() && !checked.project->program() && !checked.project->semantic_analysis()->application_metadata(),
        "library package acquired application metadata");
}
}
int main() {
    try {
        admission();bridge_allocation();generation(false);generation(true);cache_and_scalar();
        std::cout<<"project_application: "<<checks<<" checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        fail_next_allocation=false;std::cerr<<error.what()<<'\n';return 1;
    }
}
