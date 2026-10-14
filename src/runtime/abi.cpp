#include "void/runtime_abi.hpp"
#include <new>
#include <stdexcept>
namespace vd {
Runtime& Runtime::current() {
    if (!current_runtime_) throw std::logic_error("generated code has no runtime attachment");
    return *current_runtime_;
}
}
namespace {
std::int32_t runtime_entry(vd::NativeCode::CheckedEntry entry,std::int32_t value,vd::RuntimeFailure* failure,const vd::RuntimeTypeRecord* records,std::size_t count,const vd::RuntimeWitnessRecord* witnesses=nullptr,std::size_t witness_count=0) noexcept {
    try {
        vd::Runtime runtime(256);
        vd::Runtime::Attachment attachment(runtime);
        if(count || witness_count) {vd_runtime_admit_metadata(&runtime,records,count,witnesses,witness_count,failure);if(failure->message) return 0;}
        vd::Runtime::Managed managed(runtime);
        return entry(value,failure);
    } catch (const std::bad_alloc&) {failure->message="Runtime startup allocation failed";}
      catch (...) {std::terminate();}
    return 0;
}
void construct(void** slot,std::size_t size,const std::size_t* offsets,std::size_t count,const void* payload,const char* key,vd::RuntimeFailure* failure) noexcept {
    try {
        vd::TypeLayout layout{size,{}};
        if(count) layout.references.assign(offsets,offsets+count);
        if(key) {
            try {
                layout.descriptor=vd::Runtime::current().descriptor(key);
                if(layout.size!=layout.descriptor->layout.size || layout.references!=layout.descriptor->layout.references)
                    throw std::invalid_argument("typed construction layout mismatch");
            } catch(const std::invalid_argument&) {failure->message="Invalid managed construction descriptor";return;}
        }
        vd::Runtime::current().construct_into(slot,layout,payload);
    } catch(const std::bad_alloc&) {failure->message="Managed construction allocation failed";}
      catch(...) {std::terminate();}
}
}
extern "C" {
void vd_gc_frame_enter(void* frame, const vd::Runtime::Slot* slots, std::size_t count) noexcept {
    try { new (frame) vd::Runtime::Frame(vd::Runtime::current(), slots, count); }
    catch (...) { std::terminate(); }
}
void vd_gc_frame_leave(void* frame) noexcept { static_cast<vd::Runtime::Frame*>(frame)->~Frame(); }
void vd_gc_poll() noexcept {
    try { vd::Runtime::current().poll(); } catch (...) { std::terminate(); }
}
void vd_gc_collect(vd::RuntimeFailure* failure) noexcept {
    try {
        auto status = vd::Runtime::current().collect();
        if (status != vd::Runtime::Collection::completed) failure->message = "GC collection could not complete";
    } catch (...) { std::terminate(); }
}
std::int32_t vd_runtime_entry(vd::NativeCode::CheckedEntry entry, std::int32_t value, vd::RuntimeFailure* failure) noexcept {
    return runtime_entry(entry,value,failure,nullptr,0);
}
std::int32_t vd_runtime_entry_typed(vd::NativeCode::CheckedEntry entry,std::int32_t value,vd::RuntimeFailure* failure,const vd::RuntimeTypeRecord* records,std::size_t count) noexcept {
    return runtime_entry(entry,value,failure,records,count);
}
void vd_gc_construct(void** slot,std::size_t size,const std::size_t* offsets,std::size_t count,const void* payload,vd::RuntimeFailure* failure) noexcept {
    construct(slot,size,offsets,count,payload,nullptr,failure);
}
void vd_gc_construct_typed(void** slot,std::size_t size,const std::size_t* offsets,std::size_t count,const void* payload,const char* key,vd::RuntimeFailure* failure) noexcept {
    construct(slot,size,offsets,count,payload,key,failure);
}
void vd_runtime_admit(vd::Runtime* runtime,const vd::RuntimeTypeRecord* records,std::size_t count,vd::RuntimeFailure* failure) noexcept {vd_runtime_admit_metadata(runtime,records,count,nullptr,0,failure);}
void vd_runtime_admit_metadata(vd::Runtime* runtime,const vd::RuntimeTypeRecord* records,std::size_t count,const vd::RuntimeWitnessRecord* records_w,std::size_t count_w,vd::RuntimeFailure* failure) noexcept {
    try {
        std::vector<vd::NominalDescriptor> types;
        types.reserve(count);
        for(std::size_t n=0;n<count;++n) {
            const auto& r=records[n];
            if(!r.identity || !r.base || !r.shape || (r.count && !r.references)) throw std::invalid_argument("invalid runtime catalog");
            vd::NominalDescriptor d{r.identity,r.base,r.shape,{r.size,{}}};
            if(r.count) d.layout.references.assign(r.references,r.references+r.count);
            types.push_back(std::move(d));
        }
        std::vector<vd::InterfaceWitness> witnesses;witnesses.reserve(count_w);
        for(std::size_t n=0;n<count_w;++n) {
            const auto& r=records_w[n];if(!r.key || !r.interface_identity || !r.view_identity || !r.contract) throw std::invalid_argument("invalid runtime witness catalog");
            witnesses.push_back({r.key,r.interface_identity,r.view_identity,r.contract});
        }
        runtime->admit_metadata(types,witnesses);
    } catch(const std::bad_alloc&) {failure->message="Runtime descriptor admission allocation failed";}
      catch(const std::invalid_argument&) {failure->message="Incompatible runtime descriptor admission";}
      catch(...) {std::terminate();}
}
const void* vd_gc_witness(const char* key) noexcept {
    try {return vd::Runtime::current().witness(key);}catch(...) {std::terminate();}
}
std::int64_t vd_gc_interface_binding(void* object,const void* witness,const char* const* keys,std::size_t count) noexcept {
    try {return vd::Runtime::current().interface_binding(object,witness,{keys,count});}catch(...) {std::terminate();}
}
std::int32_t vd_runtime_entry_metadata(vd::NativeCode::CheckedEntry entry,std::int32_t value,vd::RuntimeFailure* failure,const vd::RuntimeTypeRecord* records,std::size_t count,const vd::RuntimeWitnessRecord* witnesses,std::size_t witness_count) noexcept {
    return runtime_entry(entry,value,failure,records,count,witnesses,witness_count);
}
std::int64_t vd_gc_dynamic_type(void* object,const char* const* catalog,std::size_t count) noexcept {
    try {return vd::Runtime::current().dynamic_type(object,{catalog,count});}
    catch(...) {std::terminate();}
}
void vd_gc_allocate(void** slot, std::size_t size, const std::size_t* offsets, std::size_t count, vd::RuntimeFailure* failure) noexcept {
    try {
        vd::TypeLayout layout{size, {}};
        if (count) layout.references.assign(offsets, offsets + count);
        vd::Runtime::current().allocate_into(slot, layout);
    } catch (const std::bad_alloc&) { failure->message = "Managed allocation failed"; }
    catch (...) { std::terminate(); }
}
bool vd_gc_alive(void* object) noexcept {
    try { return vd::Runtime::current().contains(object); } catch (...) { std::terminate(); }
}
void vd_gc_native_enter(void* guard) noexcept {
    try { new (guard) vd::Runtime::Native(vd::Runtime::current()); } catch (...) { std::terminate(); }
}
void vd_gc_native_leave(void* guard) noexcept { static_cast<vd::Runtime::Native*>(guard)->~Native(); }
}
