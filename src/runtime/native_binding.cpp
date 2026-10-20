#include "void/native_runtime.hpp"
#include "void/native.hpp"
#include "void/callback.hpp"
#include "../native/fixture.h"
#include <atomic>
#include <bit>
#include <cstring>
#include <map>
#include <thread>
#include <type_traits>
#include <stdexcept>
namespace {
std::atomic<std::size_t> live_resources{0};
struct Resource final:vd::CallbackProvider {
    vd::Runtime& runtime;std::thread::id owner=std::this_thread::get_id();
    struct Capture {vd::Runtime::Handle environment;std::shared_ptr<void> lease;std::uint64_t function;vd::NativeCallbackEntry entry;
        Capture(vd::Runtime& r,void* env,std::uint64_t id,vd::NativeCallbackEntry callback):environment(r,env),lease(vd::NativeCode::active_lease()),function(id),entry(callback){}
    };
    vd::Runtime::Handle wrapper;
    std::shared_ptr<Capture> capture;
    std::mutex mutex;std::condition_variable changed;
    vd_fixture_resource* native=nullptr;
    bool provider_closed=false,registered=false;unsigned dispatched=0;
    std::string retained;

    std::mutex error_mutex;vd::OwnedRuntimeFailure callback_error;bool callback_rejected=false;
    vd::CallbackRegistration registration; // Last: no throwing member construction after admission.
    explicit Resource(vd::Runtime& r,void* identity):runtime(r),wrapper(r,identity),registration(r,*this){}
    ~Resource(){close();}
    void own_thread(){if(owner!=std::this_thread::get_id())throw std::runtime_error("Native resource requires its owning thread");}
    void unregister_and_quiesce() noexcept override {
        std::unique_lock lock(mutex);provider_closed=true;registered=false;if(native)vd_fixture_unregister(native);changed.wait(lock,[&]{return !dispatched;});
    }
    void close(){
        if(runtime.is_managed()){vd::Runtime::Native native_safe(runtime);close();return;}
        registration.close();
        std::shared_ptr<Capture> retired;
        {std::lock_guard lock(mutex);if(native){vd_fixture_retain(native,nullptr,0);vd_fixture_destroy(native);native=nullptr;--live_resources;}retired=std::move(capture);retained.clear();}
        // No scheduler/provider locks during Runtime root or ORC lease release.
        retired.reset();wrapper.reset(nullptr);
    }
    static std::int32_t callback(void* context,std::int32_t value) noexcept {
        auto& self=*static_cast<Resource*>(context);std::int32_t result=0;
        bool accepted=self.registration.run([&]{auto captured=self.capture;vd::NativeCode::LeaseScope lease_scope(captured->lease);vd::RuntimeFailure failure;result=captured->entry(captured->function,captured->environment.get(),value,&failure);if(failure.message){std::lock_guard lock(self.error_mutex);self.callback_error.assign(failure);}});
        if(!accepted){std::lock_guard lock(self.error_mutex);self.callback_rejected=true;}
        return result;
    }
    std::int32_t invoke(std::int32_t value,bool foreign,vd::RuntimeFailure* failure){
        {std::lock_guard lock(error_mutex);callback_error.assign({});callback_rejected=false;}
        vd_fixture_callback callback_entry;void* callback_context;
        {std::lock_guard lock(mutex);if(!native || provider_closed || !registered)throw std::runtime_error("Native callback is not registered");callback_entry=vd_fixture_registered(native,&callback_context);++dispatched;}
        struct Dispatch {Resource& resource;~Dispatch(){std::lock_guard lock(resource.mutex);--resource.dispatched;resource.changed.notify_all();}} dispatch{*this};
        std::int32_t result=0;
        if(foreign){std::thread thread([&]{result=vd_fixture_dispatch(callback_entry,callback_context,value);});thread.join();}
        else result=vd_fixture_dispatch(callback_entry,callback_context,value);
        std::lock_guard lock(error_mutex);if(callback_rejected)throw std::runtime_error("Native callback admission/allocation rejected");if(callback_error.value().message)vd::retain_native_failure(callback_error.value(),failure);return result;
    }
};
}
namespace vd {
class NativeLibrary {
    Runtime& runtime_;std::mutex mutex_,shutdown_mutex_;std::map<void*,std::shared_ptr<Resource>> resources_;
public:
    explicit NativeLibrary(Runtime& runtime):runtime_(runtime){}
    static std::shared_ptr<NativeLibrary> get(Runtime& runtime,bool cleanup=false){std::lock_guard lock(runtime.mutex_);if(runtime.closing_ && !cleanup)throw std::runtime_error("Native library admission rejected during shutdown");if(!runtime.native_library_)runtime.native_library_=std::make_shared<NativeLibrary>(runtime);return runtime.native_library_;}
    void publish(void* key,std::shared_ptr<Resource> value){std::lock_guard admission(runtime_.mutex_);if(runtime_.closing_)throw std::runtime_error("Native resource publication rejected during shutdown");std::lock_guard lock(mutex_);if(!resources_.emplace(key,std::move(value)).second)throw std::runtime_error("Duplicate native identity");}
    std::shared_ptr<Resource> find(void* key,bool cleanup=false){std::lock_guard lock(mutex_);auto found=resources_.find(key);if(found==resources_.end()){if(cleanup)return {};throw std::runtime_error("Native resource is closed or invalid");}return found->second;}
    void* identity(){std::lock_guard lock(mutex_);if(resources_.size()!=1)throw std::runtime_error("Fixture inspection requires exactly one resource");return resources_.begin()->first;}
    void erase(void* key){std::shared_ptr<Resource> retired;{std::lock_guard lock(mutex_);auto found=resources_.find(key);if(found!=resources_.end()){retired=std::move(found->second);resources_.erase(found);}}}
    void shutdown(){std::lock_guard drain(shutdown_mutex_);std::map<void*,std::shared_ptr<Resource>> retired;{std::lock_guard lock(mutex_);retired.swap(resources_);}for(auto& [key,resource]:retired)resource->close();}
};
void native_shutdown(Runtime& runtime){std::shared_ptr<NativeLibrary> library;{std::lock_guard lock(runtime.mutex_);library=runtime.native_library_;}if(library)library->shutdown();}
std::size_t native_fixture_live() noexcept{return live_resources.load();}
void* native_fixture_identity(Runtime& runtime){if(!runtime.is_managed())throw std::logic_error("Native fixture identity requires Managed state");return NativeLibrary::get(runtime)->identity();}
}
extern "C" std::uint64_t vd_native_fixture_call(std::uint32_t op,std::uint64_t bits,void* object,void* text,std::uint64_t function,void* environment,vd::NativeCallbackEntry entry,void** out,const char* key,vd::RuntimeFailure* failure) noexcept {
    try {
        auto& runtime=vd::Runtime::current();
        // C++ compilation verifies and applies the actual target C ABI for each
        // exact-width scalar. Private LLVM transport uses only u32/u64/pointers.
#define SCALAR(N,T,INDEX) case INDEX: {using Bits=std::conditional_t<sizeof(T)==1,std::uint8_t,std::conditional_t<sizeof(T)==2,std::uint16_t,std::conditional_t<sizeof(T)==4,std::uint32_t,std::uint64_t>>>;static_assert(std::is_same_v<decltype(&vd_fixture_##N),T(*)(T)>);auto value=std::bit_cast<T>(static_cast<Bits>(bits));return std::bit_cast<Bits>(vd_fixture_##N(value));}
        switch(op){SCALAR(i8,std::int8_t,0) SCALAR(i16,std::int16_t,1) SCALAR(i32,std::int32_t,2) SCALAR(i64,std::int64_t,3) SCALAR(u8,std::uint8_t,4) SCALAR(u16,std::uint16_t,5) SCALAR(u32,std::uint32_t,6) SCALAR(u64,std::uint64_t,7) SCALAR(f32,float,8) SCALAR(f64,double,9)
        case 10:return vd_fixture_bool(bits!=0);default:break;}
#undef SCALAR
        if(op==11 || op==12 || op==13){std::string bytes(runtime.string_bytes(text));
            if(op==11){auto terminated=vd::native_utf8(bytes);vd::Runtime::Native native(runtime);return vd_fixture_terminated(terminated.c_str());}
            if(op==12){vd::Runtime::Native native(runtime);return vd_fixture_length(bytes.data(),bytes.size());}
            char* returned;{vd::Runtime::Native native(runtime);returned=vd_fixture_copy(bytes.data(),bytes.size());}if(!returned)throw std::bad_alloc();
            struct Returned {char* pointer;~Returned(){vd_fixture_free(pointer);}} release{returned};runtime.string_into(out,{returned,bytes.size()});return 0;
        }
        if(op==14){
            auto library=vd::NativeLibrary::get(runtime);auto descriptor=runtime.descriptor(key);vd::TypeLayout layout=descriptor->layout;layout.descriptor=descriptor;runtime.allocate_into(out,layout);
            auto resource=std::make_shared<Resource>(runtime,*out);
            {vd::Runtime::Native native(runtime);resource->native=vd_fixture_create(bits!=0);if(resource->native)++live_resources;}
            if(!resource->native){*out=nullptr;return 0;}library->publish(*out,resource);return 0;
        }
        auto library=vd::NativeLibrary::get(runtime,op==16);auto resource=library->find(object,op==16);if(!resource)return 0;resource->own_thread();
        if(op==15 || op==16){{vd::Runtime::Native native(runtime);resource->close();}library->erase(object);return 0;}
        if(op==20){
            if(!entry || !function)throw std::runtime_error("Invalid native callback identity");auto capture=std::make_shared<Resource::Capture>(runtime,environment,function,entry);
            vd::Runtime::Native native(runtime);std::lock_guard lock(resource->mutex);if(!resource->native || resource->provider_closed || resource->registered)throw std::runtime_error("Native callback registration is one-shot");resource->capture=std::move(capture);vd_fixture_register(resource->native,Resource::callback,resource.get());resource->registered=true;return 0;
        }
        if(op==21 || op==22){vd::Runtime::Native native(runtime);return static_cast<std::uint32_t>(resource->invoke(static_cast<std::int32_t>(bits),op==22,failure));}
        if(op==23){vd::Runtime::Native native(runtime);resource->registration.close();std::shared_ptr<Resource::Capture> retired;{std::lock_guard lock(resource->mutex);retired=std::move(resource->capture);}retired.reset();return 0;}
        std::string bytes;if(op==17)bytes=std::string(runtime.string_bytes(text));
        vd::Runtime::Native native(runtime);std::lock_guard lock(resource->mutex);if(!resource->native)throw std::runtime_error("Native resource is closed");
        if(op==17){resource->retained.swap(bytes);vd_fixture_retain(resource->native,resource->retained.data(),resource->retained.size());return 0;}
        if(op==18)return static_cast<std::uint32_t>(vd_fixture_byte(resource->native,bits));
        if(op==19){vd_fixture_retain(resource->native,nullptr,0);resource->retained.clear();return 0;}
        throw std::runtime_error("Unsupported native operation");
    }catch(const std::exception& error){vd::RuntimeFailure caught;caught.message=error.what();vd::retain_native_failure(caught,failure);}
    catch(...){failure->message="Native binding failed";}
    return 0;
}
