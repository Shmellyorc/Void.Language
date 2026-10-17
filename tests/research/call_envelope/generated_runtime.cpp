#include "void/runtime_abi.hpp"
#include <array>
#include <iostream>
#include <optional>
#include <stdexcept>
using namespace vd;
extern "C" std::uint64_t void_fixed_caller(void**,void**,const BufferLayoutRef*,const BufferWorkQuota*,RuntimeFailure*) noexcept;
namespace {
void check(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
enum Mode {normal,unregistered_source,unregistered_result,source_result_alias,prefailed,quota_failure,
    wrong_scalar_layout,wrong_extent,missing_layout,missing_quota,empty_result,null_source,
    before_poll,after_poll,publication_failure};
constexpr const char* expected[]={nullptr,"Invalid fixed value contract","Invalid fixed value contract","Invalid aliased context",
    "original failure","Invalid fixed value contract","Invalid fixed value contract","Invalid fixed value contract",
    "Invalid fixed value contract","Invalid fixed value contract",nullptr,"Invalid fixed value contract",
    "injected before poll","injected after poll","Invalid fixed value contract"};
struct Observation {
    Runtime& runtime;
    const BufferLayoutRef& layout;
    BufferWorkQuota& quota;
    RuntimeFailure& failure;
    Mode mode;
    void** source;
    std::optional<Runtime::RegisteredRootWord> caller,callee;
    bool ready=false,callee_left=false,caller_left=false;
};
thread_local Observation* observation;
#ifdef VD_FIXED_OBSERVE
bool expired(Runtime::RegisteredRootWord token) {
    try {observation->runtime.fixed_scalar(token,observation->layout,0);}
    catch(const std::invalid_argument&) {return true;}return false;
}
void inspect(void** homes) {
    auto& a=*observation;
    for(unsigned i=0;i<8;++i) {
        check(a.runtime.fixed_scalar(a.runtime.root_word(homes+i),a.layout,0)==7+i,"snapshot order or callee argument transport");
        check(homes[i]!=*a.source,"snapshot aliases source");
        for(unsigned j=0;j<i;++j) check(homes[i]!=homes[j],"snapshot payload alias");
    }
}
#endif
}
// Observation only: these hooks cannot authorize a call or supply metadata.
// The no-observer targets provide empty hooks for the identical generated object.
extern "C" void research_fixed_snapshots_ready(void** homes) noexcept {
#ifdef VD_FIXED_OBSERVE
    inspect(homes);auto& a=*observation;
    check(a.runtime.fixed_scalar(a.runtime.root_word(a.source),a.layout,0)==15,"later source effects");
    a.caller=a.runtime.root_word(homes);
#else
    (void)homes;
#endif
}
extern "C" void research_fixed_callee_ready(void** homes) noexcept {
#ifdef VD_FIXED_OBSERVE
    inspect(homes);auto& a=*observation;a.callee=a.runtime.root_word(homes);a.ready=true;
    if(a.mode==before_poll) a.failure.message="injected before poll";
#else
    (void)homes;
#endif
}
extern "C" void research_fixed_after_poll() noexcept {
#ifdef VD_FIXED_OBSERVE
    auto& a=*observation;check(a.ready,"safepoint before callee roots");
    check(a.runtime.snapshot().completed_epoch>0,"callee did not collect");
    check(a.runtime.fixed_scalar(*a.callee,a.layout,0)==7,"callee root lost across collection");
    if(a.mode==after_poll) a.failure.message="injected after poll";
    if(a.mode==publication_failure) a.quota={};
#endif
}
extern "C" void research_fixed_frame_left(std::int32_t callee) noexcept {
#ifdef VD_FIXED_OBSERVE
    auto& a=*observation;
    if(callee) {check(a.callee && expired(*a.callee),"callee roots not expired");a.callee_left=true;}
    else {if(a.caller) check(expired(*a.caller),"caller roots not expired");a.caller_left=true;}
#else
    (void)callee;
#endif
}
int main(int argc,char** argv) {
    if(argc!=3) return 1;
    try {
        const auto returned=std::stoul(argv[1]);const auto extent=std::stoull(argv[2]);
        check(returned<8 && extent>1,"invalid test arguments");
        unsigned cases=0;
#ifdef VD_FIXED_OBSERVE
        constexpr unsigned last=publication_failure;
#else
        constexpr unsigned last=null_source;
#endif
        for(unsigned m=0;m<=last;++m) {
            auto mode=static_cast<Mode>(m);
            Runtime r(1);
            ElementSchemaSpec schema{"private-u64","mutable","private-call-test",StorageDomain::native(),0,{SchemaScalar{mode==wrong_scalar_layout?ScalarEncoding::i64:ScalarEncoding::u64}}};
            r.admit_buffer_schemas({&schema,1});auto layout=r.buffer_layout(r.buffer_schema(schema.identity),mode==wrong_extent?extent+1:extent);
            Runtime::Attachment attachment(r);Runtime::Managed managed(r);
            void* source=nullptr;void* result=nullptr;constexpr std::size_t zero=0;
            Runtime::Slot slots[]={{&source,sizeof(void*),&zero,1},{&result,sizeof(void*),&zero,1}};
            Runtime::Frame roots(r,slots,2);RuntimeFailure failure;BufferWorkQuota quota{4096,16,32};
            vd_fixed_default(&source,&layout,&quota,&failure);
            vd_fixed_write(&source,&layout,0,7,&failure);vd_fixed_write(&source,&layout,layout->capacity()-1,73,&failure);
            vd_fixed_default(&result,&layout,&quota,&failure);
            check(!failure.message,"test initialization failed");
            if(mode==empty_result) result=nullptr;
            if(mode==null_source) source=nullptr;
            auto* sentinel=result;void* fake=source;
            Observation active{r,layout,quota,failure,mode,&source,{},{}};observation=&active;
            if(mode==prefailed) failure.message="original failure";
            if(mode==quota_failure) quota={};
            const auto value=void_fixed_caller(mode==unregistered_source?&fake:&source,
                mode==unregistered_result?&fake:mode==source_result_alias?&source:&result,
                mode==missing_layout?nullptr:&layout,mode==missing_quota?nullptr:&quota,&failure);
            const bool success=mode==normal || mode==empty_result;
            check(success?!failure.message:failure.message!=nullptr,"wrong failure status");
            if(!success && std::string(failure.message)!=expected[m]) throw std::runtime_error(std::string("wrong failure: ")+failure.message);
            check(value==(success?8+returned:0),"post-call continuation result");
#ifdef VD_FIXED_OBSERVE
            check(active.caller_left,"caller cleanup missing");
            if(active.ready) check(active.callee_left,"callee cleanup missing");
#endif
            if(success) {
                check(result!=sentinel && result!=source,"independent result storage");
                vd_fixed_write(&source,&layout,0,51,&failure);
                check(vd_fixed_read(&result,&layout,0,&failure)==7+returned,"result aliases source");
                check(vd_fixed_read(&result,&layout,extent-1,&failure)==73,"incomplete array snapshot");
                check(!failure.message,"success continuation failed");
            } else check(result==sentinel,"failure changed result");
            // Collection with only the two host roots must discard all call
            // temporaries; unlinking host values must then leave no objects.
            check(r.collect()==Runtime::Collection::completed,"post-call collection failed");
            check(r.snapshot().objects==(source?1u:0u)+(result?1u:0u),"call retained temporary buffers");
            source=result=nullptr;
            check(r.collect()==Runtime::Collection::completed && !r.snapshot().objects,"root or temporary leak");
            ++cases;observation=nullptr;
        }
        std::cout<<cases<<" private-call cases passed; returned argument "<<returned<<", extent "<<extent<<'\n';
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
