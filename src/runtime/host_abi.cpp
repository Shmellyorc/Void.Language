#include "host_internal.hpp"
#include "void/host_abi.hpp"
#include <cerrno>
#include <charconv>
#include <cstring>
#include <stdexcept>
namespace vd::host {
namespace {
std::array<char,256> kinds{};
struct Rejected {std::int64_t kind;};
class Resource final : public NativeResource {
public:
    char type;
    std::shared_ptr<Object> object;
    Resource(Runtime& r,void* key,char kind):NativeResource(r,key),type(kind){}
    const void* kind() const noexcept override {return &kinds[static_cast<unsigned char>(type)];}
    std::error_code close() override {
        if(runtime.is_managed()){Runtime::Native safe(runtime);return close();}
        // Keep the published shared_ptr immutable. request_stop may read it on
        // another thread while the owner closes; Object::stop coordinates with
        // close, and ledger retirement supplies the final lifetime barrier.
        auto result=object?object->close():std::error_code{};wrapper.reset(nullptr);return result;
    }
    std::error_code quiesce() override {return object?object->quiesce():std::error_code{};}
    void request_stop() noexcept override {if(object)object->stop();}
};
struct Reply {std::int64_t kind=0,code=0,number=0,detail=0;bool flag=false;void* text=nullptr;void* bytes=nullptr;};
constexpr std::array<std::size_t,7> fields{offsetof(Reply,kind),offsetof(Reply,code),offsetof(Reply,number),offsetof(Reply,detail),offsetof(Reply,flag),offsetof(Reply,text),offsetof(Reply,bytes)};
std::shared_ptr<Resource> lookup(Runtime& runtime,void* key,char kind) {
    auto ledger=NativeLibrary::existing(runtime);if(!ledger)return {};
    auto r=ledger->find(key,true);if(!r)return {};
    if(r->kind()!=&kinds[static_cast<unsigned char>(kind)])throw std::logic_error("Invalid host resource identity");
    auto value=std::static_pointer_cast<Resource>(r);value->own_thread();return value;
}
void close_failure(std::error_code code,RuntimeFailure* failure) noexcept {
    if(!code || !failure)return;
    std::array<char,120> message{};constexpr char prefix[]="Host resource cleanup failed; native code=";
    std::memcpy(message.data(),prefix,sizeof(prefix)-1);
    auto end=std::to_chars(message.data()+sizeof(prefix)-1,message.data()+message.size()-1,code.value());*end.ptr=0;
    RuntimeFailure detail;detail.message=message.data();retain_native_failure(detail,failure);
}
struct Admission {
    Runtime& r;
    explicit Admission(Runtime& runtime):r(runtime){r.begin_native_job();}
    ~Admission(){r.end_native_job();}
};
struct AcquisitionRollback {
    Runtime& runtime;
    Request& request;
    std::error_code& error;
    bool committed=false;
    ~AcquisitionRollback() {
        if(committed)return;
        try {
            Runtime::Native safe(runtime);
            if(request.acquired)error=request.acquired->close();
            if(request.acquired_file) {
                auto result=filesystem::close(*request.acquired_file);
                if(!error && result.os_code)error={static_cast<int>(result.os_code),std::system_category()};
            }
        } catch(...) {error=std::make_error_code(std::errc::io_error);}
    }
};
bool factory(unsigned op) {return op==hash_open || op==zip_open || op==dir_create || op==dir_admit || op==dir_child || op==dir_open || op==dir_file || op==dir_lock || op==spawn;}
bool closing(unsigned op) {return op==hash_close || op==zip_close || op==dir_close || op==lock_close || op==child_close;}
std::vector<std::string> strings(Runtime& r,void* input,const NativeArrayElement* element) {
    if(!element || element->bytes!=sizeof(void*) || element->count!=1 || element->fields[0].kind!=12 || element->fields[0].offset)
        throw std::logic_error("Invalid host string array ABI");
    std::size_t zero=0;Runtime::Slot slot{&input,sizeof(input),&zero,1};Runtime::Frame root(r,&slot,1);
    auto layout=r.native_array_layout(*element,0);auto count=r.array_count(r.root_word(&input),layout,true);
    if(count>4096)throw Rejected{12};
    std::vector<std::string> result;result.reserve(count);std::size_t bytes=0;
    for(std::uint64_t n=0;n<count;++n) {
        void* value=nullptr;Runtime::Slot item{&value,sizeof(value),&zero,1};Runtime::Frame item_root(r,&item,1);
        r.array_read(&value,r.root_word(&input),layout,true,n);auto text=r.string_bytes(value);
        if(text.size()>1048576-bytes)throw Rejected{12};
        bytes+=text.size();result.emplace_back(text);
    }
    return result;
}
}
}
extern "C" void vd_host_call(void* output,const vd::host::ReplyLayout* layout,unsigned op,void* const* refs,const std::int64_t* nums,vd::RuntimeFailure* failure) noexcept {
    using namespace vd;using namespace vd::host;
    std::error_code rollback_error;
    try {
        if(!failure || !output || !layout || !refs || !nums || op>=operations.size() || operations[op].cleanup || layout->size!=sizeof(Reply) || layout->fields!=fields)
            throw std::logic_error("Invalid host boundary ABI");
        auto& runtime=Runtime::current();Admission admission(runtime);
        Reply reply;constexpr std::array<std::size_t,2> offsets{offsetof(Reply,text),offsetof(Reply,bytes)};
        Runtime::Slot slot{&reply,sizeof(reply),offsets.data(),offsets.size()};Runtime::Frame roots(runtime,&slot,1);
        // Allocate the empty owned byte reply before entering any namespace
        // mutation. Publication status can then be returned without allocation.
        runtime.bytes_into(&reply.bytes,{});
        Request request;request.operation=op;
        AcquisitionRollback rollback{runtime,request,rollback_error};
        std::vector<std::shared_ptr<Resource>> resources;
        void* created_key=nullptr;char created_kind=0;unsigned reference=0,number=0;
        Result result;
        try {
        for(char c:operations[op].arguments) {
            if(c=='s') {request.numbers.push_back(nums[number++]);continue;}
            void* value=refs[reference++];
            if(c=='t') {
                auto text=runtime.string_bytes(value);
                if(text.size()>1048576){result=error(12);break;}
                request.texts.emplace_back(text);
            } else if(c=='b') {
                std::size_t zero=0;Runtime::Slot input{&value,sizeof(value),&zero,1};Runtime::Frame root(runtime,&input,1);
                auto extent=runtime.list_count(runtime.root_word(&value),ScalarEncoding::u8);
                if(extent>16*1024*1024){result=error(12);break;}
                request.bytes.push_back(runtime.byte_array_bytes(runtime.root_word(&value)));
            } else if(c=='a') {
                request.arrays.push_back(strings(runtime,value,layout->strings));
            } else if(factory(op) && ((op==dir_child || op==dir_open || op==dir_file || op==dir_lock)?reference==2:reference==1)) {
                created_key=value;created_kind=c;
                if(!value)throw std::logic_error("Null host acquisition identity");
                if(auto ledger=NativeLibrary::existing(runtime);ledger && ledger->find(value,true))throw std::logic_error("Duplicate host acquisition");
            } else if(c=='F') {
                filesystem::ErrorKind code=filesystem::ErrorKind::success;
                request.file=filesystem::borrow_file(runtime,value,code);
                if(!request.file){result=error(static_cast<std::int64_t>(code));break;}
            } else {
                auto resource=lookup(runtime,value,c);
                if(!resource) {if(!closing(op))result=error(7);break;}
                request.objects.push_back(resource->object);resources.push_back(resource);
            }
        }
        } catch(const Rejected& rejected) {result=error(rejected.kind);}
        std::shared_ptr<Resource> acquisition;
        if(!result.kind && created_key && created_kind!='F')acquisition=std::make_shared<Resource>(runtime,created_key,created_kind);
        if(!result.kind) {
            Runtime::Native safe(runtime);
            if(closing(op)) {
                if(!resources.empty()) {auto code=resources[0]->close();if(code)result=native_error(code.value());}
            } else if(op==args_count || op==arg) {
                const auto& context=runtime.application_arguments();result.kind=context.error;
                if(!result.kind) {
                    if(op==args_count)result.number=static_cast<std::int64_t>(context.values.size());
                    else if(request.numbers[0]<0 || static_cast<std::size_t>(request.numbers[0])>=context.values.size())result=error(2);
                    else result.text=context.values[request.numbers[0]];
                }
            } else if(op<=verify)result=crypto_execute(request);
            else if(op<=zip_cleanup)result=zip_execute(request);
            else if(op<=lock_cleanup || op==file_mode_matches)result=directory_execute(request);
            else result=process_execute(request);
        }
        if(closing(op) && !resources.empty())NativeLibrary::existing(runtime)->erase(refs[0],resources[0].get());
        reply.kind=result.kind;reply.code=result.code;reply.number=result.number;reply.detail=result.detail;reply.flag=result.flag;
        if(!result.text.empty())runtime.string_into(&reply.text,result.text);
        if(!result.bytes.empty())runtime.bytes_into(&reply.bytes,result.bytes);
        if(!result.kind && created_key) {
            if(created_kind=='F')filesystem::publish_file(runtime,created_key,request.acquired_file);
            else {acquisition->object=request.acquired;NativeLibrary::get(runtime)->publish(created_key,acquisition);}
        }
        std::memcpy(output,&reply,sizeof(reply));
        rollback.committed=true;
    } catch(const std::bad_alloc&) {if(failure)failure->message="Host boundary allocation failed";}
      catch(const std::length_error&) {if(failure)failure->message="Host boundary extent exceeds its limit";}
      catch(...) {if(failure)failure->message="Host boundary invariant failed";}
    if(rollback_error && failure) {
        // Preserve the primary failure first; cleanup is additional diagnostic.
        std::array<char,240> message{};
        auto length=std::strlen(failure->message);std::memcpy(message.data(),failure->message,length);
        constexpr char suffix[]="; acquisition rollback close failed; native code=";
        std::memcpy(message.data()+length,suffix,sizeof(suffix)-1);length+=sizeof(suffix)-1;
        auto end=std::to_chars(message.data()+length,message.data()+message.size()-1,rollback_error.value());*end.ptr=0;
        RuntimeFailure detail;detail.message=message.data();retain_native_failure(detail,failure);
    }
}
extern "C" void vd_host_cleanup(unsigned op,void* key,vd::RuntimeFailure* failure) noexcept {
    using namespace vd;using namespace vd::host;
    try {
        if(!failure || op>=operations.size() || !operations[op].cleanup)throw std::logic_error("Invalid host cleanup ABI");
        auto& runtime=Runtime::current();auto resource=lookup(runtime,key,operations[op].arguments[0]);if(!resource)return;
        std::error_code result;{Runtime::Native safe(runtime);result=resource->close();}
        NativeLibrary::existing(runtime)->erase(key,resource.get());close_failure(result,failure);
    } catch(...) {if(failure)failure->message="Host resource cleanup invariant failed";}
}
