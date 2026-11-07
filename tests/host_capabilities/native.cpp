#include "void/application.hpp"
#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/host_abi.hpp"
#include "void/host_ops.hpp"
#include "void/native_resource.hpp"
#include "../managed/probe.hpp"
#include "../../src/runtime/host_internal.hpp"
#include <array>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
namespace fs=std::filesystem;
namespace {
unsigned checks=0;
thread_local int sync_failure=-1,close_failure=-1;
thread_local bool second_sync=false;
thread_local int poison_after_sync=-1;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);++checks;}
struct Reply {std::int64_t kind=0,code=0,number=0,detail=0;bool flag=false;void* text=nullptr;void* bytes=nullptr;};
constexpr vd::host::ReplyLayout geometry{sizeof(Reply),{offsetof(Reply,kind),offsetof(Reply,code),offsetof(Reply,number),offsetof(Reply,detail),offsetof(Reply,flag),offsetof(Reply,text),offsetof(Reply,bytes)},nullptr};
const vd::NativeArrayField string_field{"",0,0,12,0};
const vd::NativeArrayElement string_element{"host-test-string",sizeof(void*),alignof(void*),1,&string_field};
struct Boundary {
    vd::Runtime runtime{1};
    vd::Runtime::Attachment attached{runtime};
    vd::Runtime::Managed managed{runtime};
    std::array<void*,12> keys{};
    std::array<std::size_t,12> offsets{};
    vd::Runtime::Slot key_slot;
    vd::Runtime::Frame key_roots;
    Reply reply;
    std::array<std::size_t,2> reply_offsets{offsetof(Reply,text),offsetof(Reply,bytes)};
    vd::Runtime::Slot reply_slot{&reply,sizeof(reply),reply_offsets.data(),reply_offsets.size()};
    vd::Runtime::Frame reply_roots{runtime,&reply_slot,1};
    vd::RuntimeFailure failure;
    Boundary():key_slot{keys.data(),sizeof(keys),offsets.data(),offsets.size()},key_roots(runtime,&key_slot,1) {
        for(unsigned n=0;n<offsets.size();++n)offsets[n]=n*sizeof(void*);
        for(unsigned n=0;n<keys.size();++n) {bool payload=false;runtime.construct_into(&keys[n],vd::TypeLayout{sizeof(bool),{}},&payload);}
    }
    void text(unsigned n,const std::string& value){runtime.string_into(&keys[n],value);}
    void array(unsigned n,std::initializer_list<std::string> values) {
        auto layout=runtime.native_array_layout(string_element,0);runtime.array_new(&keys[n],layout,true);
        for(const auto& value:values) {text(11,value);runtime.array_push(runtime.root_word(&keys[n]),layout->schema(),&keys[11]);}
    }
    Reply call(unsigned op,std::initializer_list<void*> values={},std::initializer_list<std::int64_t> numbers={},bool strings=false) {
        std::array<void*,8> refs{};std::array<std::int64_t,8> nums{};std::copy(values.begin(),values.end(),refs.begin());std::copy(numbers.begin(),numbers.end(),nums.begin());
        auto layout=geometry;if(strings)layout.strings=&string_element;reply={};failure={};
        vd_host_call(&reply,&layout,op,refs.data(),nums.data(),&failure);return reply;
    }
    void cleanup(unsigned op,unsigned key) {failure={};vd_host_cleanup(op,keys[key],&failure);check(!failure.message,"checked resource cleanup failed");}
};
unsigned descriptors(){unsigned n=0;for(const auto& entry:fs::directory_iterator("/proc/self/fd")){(void)entry;++n;}return n;}
void arguments() {
    using namespace vd;
    auto a=checked_application_arguments({"","snow 雪😀","--app","tail\\"});check(!a.error && a.values[1]=="snow 雪😀","Unicode arguments changed");
    check(checked_application_arguments({std::string("a\0b",3)}).error==2,"NUL argument admitted");
    check(checked_application_arguments({std::string("\xff",1)}).error==4,"invalid UTF-8 admitted");
    check(checked_application_arguments(std::vector<std::string>(4097)).error==2,"argument count not bounded");
    check(checked_application_arguments({std::string(1048577,'x')}).error==2,"argument bytes not bounded");
    Boundary b;auto r=b.call(host::args_count);check(!b.failure.message && r.kind==6,"embedding arguments silently inferred");
    b.runtime.application_arguments(std::move(a));r=b.call(host::args_count);check(!b.failure.message && !r.kind && r.number==4,"per-Runtime argument context missing");
    r=b.call(host::arg,{}, {1});check(!b.failure.message && b.runtime.string_bytes(r.text)=="snow 雪😀","argument GC publication");
    bool rejected=false;try{b.runtime.application_arguments(checked_application_arguments({}));}catch(const std::logic_error&){rejected=true;}check(rejected,"mutable argument context");
}
void allocation_and_gc() {
    using namespace vd;using namespace vd::host;
    Boundary b;unsigned failed=0,succeeded=0;
    for(int budget=0;budget<250;++budget) {
        {managed_test::AllocationFailure fault(budget,true);b.call(hash_open,{b.keys[0]});}
        if(b.failure.message) {
            ++failed;auto ledger=NativeLibrary::existing(b.runtime);check(!ledger || !ledger->find(b.keys[0],true),"failed acquisition escaped into native ledger");
        } else {check(!b.reply.kind,"hash acquisition typed failure");++succeeded;b.cleanup(hash_cleanup,0);}
        if(failed && succeeded)break;
    }
    check(failed && succeeded,"allocation sweep never crossed acquisition publication");
    auto opened=b.call(hash_open,{b.keys[0]});check(!b.failure.message && !opened.kind,"hash open after allocation failure");
    b.runtime.bytes_into(&b.keys[1],"abc");b.call(hash_update,{b.keys[0],b.keys[1]});check(!b.failure.message && !b.reply.kind,"byte snapshot forced GC");
    auto result=b.call(hash_final,{b.keys[0]});check(!b.failure.message && !result.kind,"hash finish under forced GC");
    auto bytes=b.runtime.byte_array_bytes(b.runtime.root_word(&b.reply.bytes));check(bytes.size()==32 && static_cast<unsigned char>(bytes[0])==0xba && static_cast<unsigned char>(bytes[31])==0xad,"digest publication roots lost");
    b.cleanup(hash_cleanup,0);b.cleanup(hash_cleanup,0);
    for(unsigned mutation=0;mutation<10;++mutation) {
        auto layout=geometry;std::array<void*,8> refs{};std::array<std::int64_t,8> nums{};RuntimeFailure failure;
        auto op=static_cast<unsigned>(args_count);
        if(mutation<7)++layout.fields[mutation];else if(mutation==7)++layout.size;else if(mutation==8)op=999;else op=hash_cleanup;
        vd_host_call(&b.reply,&layout,op,refs.data(),nums.data(),&failure);check(failure.message,"forged host geometry admitted");
    }
    check(b.runtime.collect()==Runtime::Collection::completed,"forced collection failed");
}
void publication(const fs::path& work) {
    using namespace vd::host;
    Request root_request;root_request.operation=dir_create;root_request.texts={(work/"private").string()};check(!directory_execute(root_request).kind,"private root create");
    auto root=root_request.acquired;
    auto child=[&](const char* name){Request q;q.operation=dir_child;q.objects={root};q.texts={name};check(!directory_execute(q).kind,"private child create");return q.acquired;};
    auto before=child("before");Request q;q.operation=dir_publish;q.objects={before,root};q.texts={"blocked"};sync_failure=0;
    auto r=directory_execute(q);sync_failure=-1;check(r.kind==17 && !r.flag && fs::exists(work/"private/before") && !fs::exists(work/"private/blocked"),"pre-rename failure mutated namespace");
    auto uncertain=child("uncertain");q.objects={uncertain,root};q.texts={"published"};sync_failure=1;
    r=directory_execute(q);sync_failure=-1;check(r.kind==17 && r.flag && r.code==EIO && fs::exists(work/"private/published") && !fs::exists(work/"private/uncertain"),"post-rename durability failure reported old state");
    auto both=child("both");q.objects={both,root};q.texts={"both-published"};sync_failure=1;second_sync=true;r=directory_execute(q);sync_failure=-1;second_sync=false;
    check(r.flag && r.code==EIO && r.detail==EIO,"secondary namespace durability failure lost");
    auto conflict=child("conflict");q.objects={conflict,root};q.texts={"published"};r=directory_execute(q);check(r.kind==3 && !r.flag && fs::exists(work/"private/conflict"),"no-replace publication overwrote existing directory");
    struct stat local_volume{},shared_volume{};
    if(!::stat(work.c_str(),&local_volume) && !::stat("/dev/shm",&shared_volume) && local_volume.st_dev!=shared_volume.st_dev) {
        std::array<char,64> pattern{};std::strcpy(pattern.data(),"/dev/shm/void-host-XXXXXX");
        auto* temporary=::mkdtemp(pattern.data());check(temporary,"cross-volume fixture admission");fs::path holder=temporary;
        Request other;other.operation=dir_create;other.texts={(holder/"root").string()};check(!directory_execute(other).kind,"cross-volume private root");
        auto crossing=child("crossing");q.objects={crossing,other.acquired};q.texts={"target"};r=directory_execute(q);
        check(r.kind==13 && !r.flag && fs::exists(work/"private/crossing") && !fs::exists(holder/"root/target"),"cross-volume publication copied or mutated its source");
        crossing->close();other.acquired->close();fs::remove(holder/"root");fs::remove(holder);
    }
    // Held ancestry remains authoritative after external namespace substitution.
    fs::create_directory(work/"outside");fs::rename(work/"private",work/"moved");fs::create_directory_symlink(work/"outside",work/"private");
    auto held_child=child("held-child");check(fs::exists(work/"moved/held-child") && !fs::exists(work/"outside/held-child"),"changed ancestor redirected a capability");held_child->close();
    Request file;file.operation=dir_file;file.objects={root};file.texts={"sentinel"};file.numbers={1};check(!directory_execute(file).kind,"exclusive capability file");
    check(vd::filesystem::private_mode(*file.acquired_file,true).error==vd::filesystem::ErrorKind::success,"held new file mode failed");
    auto fd=vd::filesystem::descriptor(*file.acquired_file);struct stat stat{};check(!fstat(fd,&stat) && (stat.st_mode&0777)==0700,"execute classification not private");
    check(vd::filesystem::private_mode_matches(*file.acquired_file,true).number==1,"held executable classification query failed");
    check(vd::filesystem::private_mode_matches(*file.acquired_file,false).number==0,"non-executable classification falsely matched");
    check(!fchmod(fd,0600),"mode corruption fixture failed");
    check(vd::filesystem::private_mode_matches(*file.acquired_file,true).number==0,"permission corruption was not detected");
    check(vd::filesystem::private_mode_matches(*file.acquired_file,false).number==1,"held data classification query failed");
    file.acquired_file.reset();
    Request attack;attack.operation=dir_file;attack.objects={root};attack.numbers={0};
    std::ofstream(work/"outside/data")<<"KEEP";chmod((work/"outside/data").c_str(),0600);fs::create_symlink(work/"outside/data",work/"moved/link");fs::create_hard_link(work/"outside/data",work/"moved/hard");mkfifo((work/"moved/fifo").c_str(),0600);
    for(const char* name:{"link","hard","fifo","../outside/data"}){attack.texts={name};check(directory_execute(attack).kind!=0,"unsafe child admitted");}
    check(fs::file_size(work/"outside/data")==4,"unrelated sentinel changed");
    // Native close errors remain observable on an explicitly released owner.
    auto close_case=child("close-case");close_failure=0;auto closed=close_case->close();close_failure=-1;check(closed.value()==EIO,"directory close error discarded");
    root->close();before->close();uncertain->close();both->close();conflict->close();
}
void process_lifetime(const fs::path& helper) {
    using namespace vd;using namespace vd::host;
    const auto initial=descriptors();
    for(int budget=0;budget<120;++budget) {
        Boundary b;b.text(1,helper.string());b.text(2,"/tmp");b.array(3,{"--sleep"});b.array(4,{});
        {managed_test::AllocationFailure fault(budget,true);b.call(spawn,{b.keys[0],b.keys[1],b.keys[3],b.keys[4],b.keys[2]}, {},true);}
        const bool success=!b.failure.message && !b.reply.kind;
        if(success)b.cleanup(child_cleanup,0);
        {Runtime::Native native(b.runtime);int status=0;check(waitpid(-1,&status,WNOHANG)==-1 && errno==ECHILD,"child acquisition failure or cleanup left a child");}
        if(success && budget>40)break;
    }
    check(descriptors()==initial,"child descriptors leaked across fault sweep");
    {Boundary b;b.text(1,helper.string());b.text(2,"/tmp");b.array(3,{"--sleep"});b.array(4,{});
    b.call(spawn,{b.keys[0],b.keys[1],b.keys[3],b.keys[4],b.keys[2]}, {},true);check(!b.failure.message && !b.reply.kind,"owned child startup");
    auto ledger=NativeLibrary::existing(b.runtime);
    {Runtime::Native native(b.runtime);ledger->request_stop();check(!ledger->shutdown(),"child stop/drain failed");int status=0;check(waitpid(-1,&status,WNOHANG)==-1 && errno==ECHILD,"shutdown did not reap child");}
    }
    Boundary waiting;waiting.text(1,helper.string());waiting.text(2,"/tmp");waiting.array(3,{"--sleep"});waiting.array(4,{});
    waiting.call(spawn,{waiting.keys[0],waiting.keys[1],waiting.keys[3],waiting.keys[4],waiting.keys[2]}, {},true);
    check(!waiting.failure.message && !waiting.reply.kind,"concurrent collection child startup");
    auto owner=NativeLibrary::existing(waiting.runtime);bool collected=false;
    std::thread collector([&]{Runtime::Attachment attached(waiting.runtime);Runtime::Managed managed(waiting.runtime);
        collected=waiting.runtime.collect(std::chrono::seconds(5))==Runtime::Collection::completed;owner->request_stop();});
    waiting.call(vd::host::wait,{waiting.keys[0]});
    {Runtime::Native native(waiting.runtime);collector.join();}
    check(collected && !waiting.failure.message && !waiting.reply.kind && waiting.reply.flag && waiting.reply.detail==SIGKILL,"native wait failed concurrent GC/stop/drain");
    waiting.cleanup(child_cleanup,0);
    for(unsigned iteration=0;iteration<20;++iteration) {
        waiting.call(spawn,{waiting.keys[0],waiting.keys[1],waiting.keys[3],waiting.keys[4],waiting.keys[2]}, {},true);
        check(!waiting.failure.message && !waiting.reply.kind,"stop/close race child startup");
        std::atomic<bool> done=false;
        std::thread stopper([&]{while(!done.load()){owner->request_stop();std::this_thread::yield();}});
        waiting.call(child_close,{waiting.keys[0]});done=true;
        {Runtime::Native native(waiting.runtime);stopper.join();}
        check(!waiting.failure.message && !waiting.reply.kind,"concurrent stop/close lost resource ownership");
    }
}
void publication_boundary(const fs::path& work) {
    using namespace vd;using namespace vd::host;
    Boundary b;b.text(1,(work/"boundary-private").string());
    b.call(dir_create,{b.keys[0],b.keys[1]});check(!b.failure.message && !b.reply.kind,"publication boundary root");
    b.text(1,"stage");b.call(dir_child,{b.keys[0],b.keys[2],b.keys[1]});check(!b.failure.message && !b.reply.kind,"publication boundary stage");
    b.text(1,"final");poison_after_sync=1;
    b.call(dir_publish,{b.keys[2],b.keys[0],b.keys[1]});managed_test::allocation_budget(-1);poison_after_sync=-1;
    check(!b.failure.message && !b.reply.kind && b.reply.flag,"publication status allocated after rename");
    // Exercise the separate single-file control-record publication boundary.
    b.text(1,"control.new");b.call(dir_file,{b.keys[0],b.keys[3],b.keys[1]}, {1});check(!b.failure.message && !b.reply.kind,"control file acquisition");
    filesystem::ErrorKind borrowed=filesystem::ErrorKind::success;
    {auto file=filesystem::borrow_file(b.runtime,b.keys[3],borrowed);check(file && borrowed==filesystem::ErrorKind::success,"control file authority");
        filesystem::Request write(filesystem::Operation::write_bytes);write.file=file->file();write.bytes="new";check(filesystem::run(write).error==filesystem::ErrorKind::success,"control file data");}
    std::ofstream(work/"boundary-private/control.json")<<"old";chmod((work/"boundary-private/control.json").c_str(),0600);
    b.text(4,"control.json");sync_failure=0;b.call(dir_replace,{b.keys[0],b.keys[3],b.keys[1],b.keys[0],b.keys[4]}, {1});sync_failure=-1;
    check(!b.failure.message && b.reply.kind==17 && !b.reply.flag && fs::exists(work/"boundary-private/control.new"),"failed control flush published a new record");
    sync_failure=1;b.call(dir_replace,{b.keys[0],b.keys[3],b.keys[1],b.keys[0],b.keys[4]}, {1});sync_failure=-1;
    std::string selected;std::ifstream(work/"boundary-private/control.json")>>selected;
    check(!b.failure.message && b.reply.kind==17 && b.reply.flag && selected=="new" && !fs::exists(work/"boundary-private/control.new"),"uncertain control replacement reported no commit");
    b.cleanup(dir_cleanup,2);b.cleanup(dir_cleanup,0);
}
void archive_shutdown(const fs::path& package) {
    using namespace vd;using namespace vd::host;
    Boundary b;b.text(1,package.string());filesystem::Result opened;
    auto file=filesystem::open(package.string(),filesystem::OpenMode::read,opened);check(file && opened.error==filesystem::ErrorKind::success,"archive shutdown fixture open");
    filesystem::publish_file(b.runtime,b.keys[2],file);
    unsigned failures=0,successes=0;
    for(int budget=0;budget<300;++budget) {
        {managed_test::AllocationFailure fault(budget,true);b.call(zip_open,{b.keys[0],b.keys[2]}, {1073741824,10000,1073741824});}
        if(b.failure.message) {
            ++failures;check(!NativeLibrary::existing(b.runtime)->find(b.keys[0],true),"failed ZIP acquisition escaped into ledger");
        } else {check(!b.reply.kind,"ZIP fault sweep typed failure");++successes;b.cleanup(zip_cleanup,0);}
        filesystem::ErrorKind borrow_error=filesystem::ErrorKind::success;
        auto borrow=filesystem::borrow_file(b.runtime,b.keys[2],borrow_error);check(borrow && borrow_error==filesystem::ErrorKind::success,"failed ZIP acquisition retained its file borrow");borrow.reset();
        if(failures && successes)break;
    }
    check(failures && successes,"ZIP allocation sweep missed publication boundary");
    b.call(zip_open,{b.keys[0],b.keys[2]}, {1073741824,10000,1073741824});check(!b.failure.message && !b.reply.kind,"archive acquisition under forced GC");
    {Runtime::Native native(b.runtime);auto ledger=NativeLibrary::existing(b.runtime);check(!ledger->shutdown(),"archive borrow did not drain before file close");}
}
void ir_authority(const fs::path& work) {
    auto file=work/"ir.void";std::ofstream(file)<<"use crypto\nfunc main():\n    var outcome = crypto.sha256()\n";
    auto analyzed=vd::analyze_project(file);check(analyzed.succeeded(),"host source analysis failed");auto valid=analyzed.program->lower();vd::ir::validate(valid);
    for(unsigned mutation=0;mutation<11;++mutation) {
        auto changed=valid;bool found=false;
        for(auto& fn:changed.functions)for(auto& block:fn.blocks)for(auto& i:block.instructions) {
            if(found || i.op!=vd::ir::Op::host_native)continue;found=true;
            if(mutation==0)changed.coordinated=false;if(mutation==1)fn.module_identity="/crypto.void";
            if(mutation==2)i.immediate=999;if(mutation==3)i.arguments.clear();if(mutation==4)i.a=1;if(mutation==5)i.b=1;
            if(mutation==6)i.type=vd::ir::Type::string;if(mutation==7)i.callee="forged";if(mutation==8)i.initialization=true;
            if(mutation==9)i.receiver=vd::ir::Place{0,{}};if(mutation==10)i.array_repeat=true;
        }
        check(found,"host primitive missing in canonical IR");bool rejected=false;try{vd::ir::validate(changed);}catch(const std::exception&){rejected=true;}check(rejected,"forged host IR admitted");
    }
}
}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd) {
    if(sync_failure==0){if(!second_sync)sync_failure=-1;errno=EIO;return -1;}
    if(sync_failure>0)--sync_failure;
    auto result=__real_fsync(fd);
    if(poison_after_sync==0){poison_after_sync=-1;managed_test::allocation_budget(0);}else if(poison_after_sync>0)--poison_after_sync;
    return result;
}
extern "C" int __real_close(int);
extern "C" int __wrap_close(int fd) {
    auto result=__real_close(fd);if(close_failure==0){close_failure=-1;errno=EIO;return -1;}
    if(close_failure>0)--close_failure;return result;
}
int main(int argc,char** argv) {
    try {
        if(argc!=4)throw std::runtime_error("work, helper and ZIP fixture required");fs::path work=fs::absolute(argv[1]);fs::create_directories(work);
        arguments();allocation_and_gc();publication(work);publication_boundary(work);process_lifetime(fs::absolute(argv[2]));archive_shutdown(fs::absolute(argv[3]));ir_authority(work);
        std::cout<<"Host boundary/GC/allocation/publication/process/ZIP/IR: "<<checks<<" checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
