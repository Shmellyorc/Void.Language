#include "void/native_runtime.hpp"
#include "void/native_resource.hpp"
#include "void/native.hpp"
#include "void/callback.hpp"
#include "../native/fixture.h"
#include "../native/sink.h"
#include <atomic>
#include <bit>
#include <cstring>
#include <map>
#include <thread>
#include <type_traits>
#include <stdexcept>
namespace {
struct Provider {
    const char* name;
    void* (*create)(bool); void (*destroy)(void*);
    char* (*copy)(const char*,std::size_t); void (*free_text)(char*);
    void (*retain)(void*,const char*,std::size_t);std::int32_t (*byte)(void*,std::uint64_t);
    void (*register_callback)(void*,vd_fixture_callback,void*);void (*unregister)(void*);
    vd_fixture_callback (*registered)(void*,void**);
    std::int32_t (*dispatch)(vd_fixture_callback,void*,std::int32_t);
    std::atomic<std::size_t> live{0};
};
// Only these statically compiled operations can enter the shared lifecycle.
#define PROVIDER(N,ID) Provider N##_provider{ID, \
    [](bool success)->void*{return vd_##N##_create(success);}, \
    [](void* p){vd_##N##_destroy(static_cast<vd_##N##_resource*>(p));}, \
    vd_##N##_copy,vd_##N##_free, \
    [](void* p,const char* s,std::size_t n){vd_##N##_retain(static_cast<vd_##N##_resource*>(p),s,n);}, \
    [](void* p,std::uint64_t n){return vd_##N##_byte(static_cast<vd_##N##_resource*>(p),n);}, \
    [](void* p,vd_fixture_callback cb,void* context){vd_##N##_register(static_cast<vd_##N##_resource*>(p),cb,context);}, \
    [](void* p){vd_##N##_unregister(static_cast<vd_##N##_resource*>(p));}, \
    [](void* p,void** context){return vd_##N##_registered(static_cast<vd_##N##_resource*>(p),context);}, \
    vd_##N##_dispatch};
PROVIDER(fixture,"void.fixture.v1")
PROVIDER(sink,"void.sink.v1")
#undef PROVIDER

struct Resource final : vd::CallbackProvider, vd::NativeResource {
    struct Capture {vd::Runtime::Handle environment;std::shared_ptr<void> lease;std::uint64_t function;vd::NativeCallbackEntry entry;
        Capture(vd::Runtime& r,void* env,std::uint64_t id,vd::NativeCallbackEntry callback):environment(r,env),lease(vd::NativeCode::active_lease()),function(id),entry(callback){}
    };
    std::shared_ptr<Capture> capture;
    std::mutex mutex;std::condition_variable changed;
    Provider& provider;void* native=nullptr;
    bool provider_closed=false,registered=false;unsigned dispatched=0;
    std::string retained;

    std::mutex error_mutex;vd::OwnedRuntimeFailure callback_error;bool callback_rejected=false;
    vd::CallbackRegistration registration; // Last: no throwing member construction after admission.
    explicit Resource(vd::Runtime& r, void* identity, Provider& p)
        : NativeResource(r, identity), provider(p), registration(r, *this) {}
    ~Resource(){close();}
    const void* kind() const noexcept override {
        return &provider;
    }
    void unregister_and_quiesce() noexcept override {
        std::unique_lock lock(mutex);provider_closed=true;registered=false;if(native)provider.unregister(native);changed.wait(lock,[&]{return !dispatched;});
    }
    std::error_code close() override {
        if (runtime.is_managed()) {
            vd::Runtime::Native native_safe(runtime);
            return close();
        }
        registration.close();
        std::shared_ptr<Capture> retired;
        {
            std::lock_guard lock(mutex);
            if (native) {
                provider.retain(native, nullptr, 0);
                provider.destroy(native);
                native = nullptr;
                --provider.live;
            }
            retired = std::move(capture);
            retained.clear();
        }
        // No scheduler/provider locks during Runtime root or ORC lease release.
        retired.reset();
        wrapper.reset(nullptr);
        return {};
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
        {std::lock_guard lock(mutex);if(!native || provider_closed || !registered)throw std::runtime_error("Native callback is not registered");callback_entry=provider.registered(native,&callback_context);++dispatched;}
        struct Dispatch {Resource& resource;~Dispatch(){std::lock_guard lock(resource.mutex);--resource.dispatched;resource.changed.notify_all();}} dispatch{*this};
        std::int32_t result=0;
        if(foreign){std::thread thread([&]{result=provider.dispatch(callback_entry,callback_context,value);});thread.join();}
        else result=provider.dispatch(callback_entry,callback_context,value);
        std::lock_guard lock(error_mutex);if(callback_rejected)throw std::runtime_error("Native callback admission/allocation rejected");if(callback_error.value().message)vd::retain_native_failure(callback_error.value(),failure);return result;
    }
};
}
namespace vd {
std::size_t native_fixture_live() noexcept{return fixture_provider.live.load();}
void* native_fixture_identity(Runtime& runtime){if(!runtime.is_managed())throw std::logic_error("Native fixture identity requires Managed state");return NativeLibrary::get(runtime)->identity(&fixture_provider);}
}
static std::uint64_t native_call(Provider& provider,std::uint32_t op,std::uint64_t bits,void* object,void* text,std::uint64_t function,void* environment,vd::NativeCallbackEntry entry,void** out,const char* key,vd::RuntimeFailure* failure) noexcept {
    try {
        auto& runtime=vd::Runtime::current();
        if(&provider==&sink_provider && !(op==2 || op==13 || (op>=14 && op<=21) || op==23))throw std::runtime_error("Unsupported sink operation");
        // C++ compilation verifies and applies the actual target C ABI for each
        // exact-width scalar. Private LLVM transport uses only u32/u64/pointers.
#define SCALAR(N,T,INDEX) case INDEX: {using Bits=std::conditional_t<sizeof(T)==1,std::uint8_t,std::conditional_t<sizeof(T)==2,std::uint16_t,std::conditional_t<sizeof(T)==4,std::uint32_t,std::uint64_t>>>;static_assert(std::is_same_v<decltype(&vd_fixture_##N),T(*)(T)>);auto value=std::bit_cast<T>(static_cast<Bits>(bits));return std::bit_cast<Bits>(vd_fixture_##N(value));}
        static_assert(std::is_same_v<decltype(&vd_sink_i32),std::int32_t(*)(std::int32_t)>);
        if(&provider==&sink_provider && op==2)return static_cast<std::uint32_t>(vd_sink_i32(static_cast<std::int32_t>(bits)));
        if(&provider==&fixture_provider)switch(op){SCALAR(i8,std::int8_t,0) SCALAR(i16,std::int16_t,1) SCALAR(i32,std::int32_t,2) SCALAR(i64,std::int64_t,3) SCALAR(u8,std::uint8_t,4) SCALAR(u16,std::uint16_t,5) SCALAR(u32,std::uint32_t,6) SCALAR(u64,std::uint64_t,7) SCALAR(f32,float,8) SCALAR(f64,double,9)
        case 10:return vd_fixture_bool(bits!=0);default:break;}
#undef SCALAR
        if(op==11 || op==12 || op==13){std::string bytes(runtime.string_bytes(text));
            if(op==11){auto terminated=vd::native_utf8(bytes);vd::Runtime::Native native(runtime);return vd_fixture_terminated(terminated.c_str());}
            if(op==12){vd::Runtime::Native native(runtime);return vd_fixture_length(bytes.data(),bytes.size());}
            char* returned;{vd::Runtime::Native native(runtime);returned=provider.copy(bytes.data(),bytes.size());}if(!returned)throw std::bad_alloc();
            struct Returned {Provider& provider;char* pointer;~Returned(){provider.free_text(pointer);}} release{provider,returned};runtime.string_into(out,{returned,bytes.size()});return 0;
        }
        if(op==14){
            auto library=vd::NativeLibrary::get(runtime);auto descriptor=runtime.descriptor(key);vd::TypeLayout layout=descriptor->layout;layout.descriptor=descriptor;runtime.allocate_into(out,layout);
            auto resource=std::make_shared<Resource>(runtime,*out,provider);
            {vd::Runtime::Native native(runtime);resource->native=provider.create(bits!=0);if(resource->native)++provider.live;}
            if(!resource->native){*out=nullptr;return 0;}library->publish(*out,resource);return 0;
        }
        auto library = vd::NativeLibrary::get(runtime, op == 16);
        auto found = library->find(object, op == 16);
        if (!found) return 0;
        if (found->kind() != &provider) {
            throw std::runtime_error("Native resource belongs to another provider");
        }
        auto resource = std::static_pointer_cast<Resource>(found);
        resource->own_thread();
        if(op==15 || op==16){{vd::Runtime::Native native(runtime);resource->close();}library->erase(object);return 0;}
        if(op==20){
            if(!entry || !function)throw std::runtime_error("Invalid native callback identity");auto capture=std::make_shared<Resource::Capture>(runtime,environment,function,entry);
            vd::Runtime::Native native(runtime);std::lock_guard lock(resource->mutex);if(!resource->native || resource->provider_closed || resource->registered)throw std::runtime_error("Native callback registration is one-shot");resource->capture=std::move(capture);provider.register_callback(resource->native,Resource::callback,resource.get());resource->registered=true;return 0;
        }
        if(op==21 || op==22){vd::Runtime::Native native(runtime);return static_cast<std::uint32_t>(resource->invoke(static_cast<std::int32_t>(bits),op==22,failure));}
        if(op==23){vd::Runtime::Native native(runtime);resource->registration.close();std::shared_ptr<Resource::Capture> retired;{std::lock_guard lock(resource->mutex);retired=std::move(resource->capture);}retired.reset();return 0;}
        std::string bytes;if(op==17)bytes=std::string(runtime.string_bytes(text));
        vd::Runtime::Native native(runtime);std::lock_guard lock(resource->mutex);if(!resource->native)throw std::runtime_error("Native resource is closed");
        if(op==17){resource->retained.swap(bytes);provider.retain(resource->native,resource->retained.data(),resource->retained.size());return 0;}
        if(op==18)return static_cast<std::uint32_t>(provider.byte(resource->native,bits));
        if(op==19){provider.retain(resource->native,nullptr,0);resource->retained.clear();return 0;}
        throw std::runtime_error("Unsupported native operation");
    }catch(const std::exception& error){vd::RuntimeFailure caught;caught.message=error.what();vd::retain_native_failure(caught,failure);}
    catch(...){failure->message="Native binding failed";}
    return 0;
}

namespace vd {
std::size_t native_sink_live() noexcept{return sink_provider.live.load();}
void* native_sink_identity(Runtime& runtime){if(!runtime.is_managed())throw std::logic_error("Native sink identity requires Managed state");return NativeLibrary::get(runtime)->identity(&sink_provider);}
}
#define BRIDGE(N) extern "C" std::uint64_t vd_native_##N##_call(std::uint32_t op,std::uint64_t bits,void* object,void* text,std::uint64_t function,void* environment,vd::NativeCallbackEntry entry,void** out,const char* key,vd::RuntimeFailure* failure) noexcept {return native_call(N##_provider,op,bits,object,text,function,environment,entry,out,key,failure);}
BRIDGE(fixture)
BRIDGE(sink)
#undef BRIDGE
