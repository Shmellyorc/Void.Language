#include "void/runtime_abi.hpp"
#include "void/async.hpp"
#include "void/format.hpp"
#include "void/native_runtime.hpp"
#include <new>
#include <algorithm>
#include <stdexcept>
namespace vd {
void Runtime::admit_globals(std::string_view contract, bool replacement, bool commit) {
    std::lock_guard lock(mutex_);
    if (closing_ || (global_admitted_ && (!replacement || global_contract_ != contract)))
        throw std::invalid_argument("Runtime already owns an independent or incompatible global program");
    if (commit) { global_contract_ = contract; global_admitted_ = true; }
}
int Runtime::globals_begin(std::string_view contract, std::size_t count, RuntimeFailure& failure) {
    require_managed();
    {
        std::lock_guard lock(mutex_);
        if (global_admitted_ && global_contract_ != contract) throw std::logic_error("Incompatible global program admission");
        if (global_owner_ != std::thread::id{} && global_owner_ != std::this_thread::get_id()) throw std::logic_error("Runtime globals require their application owner thread");
        if (global_state_ == 3) { failure = global_failure_.value(); return -1; }
        if (global_state_ == 2) return 0;
        if (global_state_ == 1) throw std::logic_error("Reentrant entry during global initialization");
        global_contract_ = contract; global_admitted_ = true;
        global_owner_ = std::this_thread::get_id(); global_state_ = 1;
    }
    try {
        std::vector<std::unique_ptr<Handle>> roots;
        roots.reserve(count);
        for (std::size_t n = 0; n < count; ++n) roots.push_back(std::make_unique<Handle>(*this));
        std::lock_guard lock(mutex_);
        if (closing_) throw std::logic_error("Runtime closed during global root setup");
        globals_.swap(roots);
    } catch (...) {
        failure.message = "Global root allocation failed"; globals_finish(failure); return -1;
    }
    return 1;
}
void Runtime::globals_finish(RuntimeFailure& failure) {
    require_managed();
    std::lock_guard lock(mutex_);
    if (global_state_ != 1 || global_owner_ != std::this_thread::get_id()) throw std::logic_error("Invalid global initialization completion");
    if (failure.message) { global_failure_.assign(failure); failure = global_failure_.value(); global_state_ = 3; }
    else {
        for (const auto& cell : globals_) if (!cell->value_) throw std::logic_error("Incomplete global initialization publication");
        global_state_ = 2;
    }
}
void Runtime::global_publish(std::size_t id, void* cell) {
    require_managed();
    {
        std::lock_guard lock(mutex_);
        if (global_owner_ != std::this_thread::get_id() || global_state_ != 1 || id >= globals_.size() || globals_[id]->value_ || !cell || !heap_.contains(cell))
            throw std::logic_error("Invalid global publication");
        globals_[id]->value_ = cell;
    }
}
void* Runtime::global_cell(std::size_t id) {
    require_managed();
    std::lock_guard lock(mutex_);
    if (global_owner_ != std::this_thread::get_id()) throw std::logic_error("Runtime globals require their application owner thread");
    if ((global_state_ != 1 && global_state_ != 2) || id >= globals_.size() || !globals_[id]->value_)
        throw std::logic_error("Runtime global is not initialized");
    return globals_[id]->value_;
}
void Runtime::require_globals_ready() const {
    require_managed();
    std::lock_guard lock(mutex_);
    if (global_state_ != 2 || global_owner_ != std::this_thread::get_id())
        throw std::logic_error("Selected entry cannot run before global initialization completes on its owner thread");
}
std::error_code Runtime::cleanup_failed_globals() {
    if (current_runtime_ == this && is_managed()) throw std::logic_error("Global failure cleanup requires NativeSafe state");
    { std::lock_guard lock(mutex_); if (global_state_ != 3) return {}; }
    auto error = native_shutdown(*this);
    std::vector<std::unique_ptr<Handle>> roots;
    { std::lock_guard lock(mutex_); roots.swap(globals_); }
    roots.clear();
    return error;
}
void Runtime::display_enter(std::string_view hook,void* identity) {
    auto& active=thread().display_hooks;
    if(active.size()>=64)throw std::length_error("Display hook nesting limit exceeded");
    if(identity)for(const auto& invocation:active)if(invocation.first==hook && invocation.second==identity)throw std::logic_error("Recursive display hook");
    active.emplace_back(hook,identity);
}
void Runtime::display_leave() noexcept {
    if(!current_thread_ || current_thread_->display_hooks.empty())std::terminate();
    current_thread_->display_hooks.pop_back();
}
void Runtime::collection_scope() {auto& t=thread();t.collection_displays.push_back({{},t.collection_path.size()});}
void Runtime::collection_scope_leave() noexcept {
    auto& t=thread();if(t.collection_displays.empty())std::terminate();
    t.collection_path.resize(t.collection_displays.back().checkpoint);t.collection_displays.pop_back();
}
bool Runtime::collection_enter(void* identity) {
    auto& t=thread();if(!identity || t.collection_displays.empty())throw std::logic_error("Invalid collection display scope");
    if(std::find(t.collection_path.begin(),t.collection_path.end(),identity)!=t.collection_path.end())return false;
    t.collection_path.push_back(identity);return true;
}
void Runtime::collection_leave(void* identity) {
    auto& t=thread();if(t.collection_displays.empty() || t.collection_path.size()<=t.collection_displays.back().checkpoint || t.collection_path.back()!=identity)throw std::logic_error("Invalid collection display path");t.collection_path.pop_back();
}
void Runtime::collection_append(void* text) {auto& t=thread();if(t.collection_displays.empty())throw std::logic_error("Invalid collection display scope");t.collection_displays.back().bytes.append(string_bytes(text));}
void Runtime::collection_finish(void** out) {auto& t=thread();if(t.collection_displays.empty())throw std::logic_error("Invalid collection display scope");auto bytes=std::move(t.collection_displays.back().bytes).finish();string_into(out,bytes);}
Runtime& Runtime::current() {
    if (!current_runtime_) throw std::logic_error("generated code has no runtime attachment");
    return *current_runtime_;
}
}
namespace {
#define VD_DISPLAY_ENCODING(name) static_assert(static_cast<unsigned>(vd::ScalarEncoding::name)==static_cast<unsigned>(vd::formatting::NumberKind::name))
VD_DISPLAY_ENCODING(i8);VD_DISPLAY_ENCODING(i16);VD_DISPLAY_ENCODING(i32);VD_DISPLAY_ENCODING(i64);
VD_DISPLAY_ENCODING(u8);VD_DISPLAY_ENCODING(u16);VD_DISPLAY_ENCODING(u32);VD_DISPLAY_ENCODING(u64);
VD_DISPLAY_ENCODING(f32);VD_DISPLAY_ENCODING(f64);
#undef VD_DISPLAY_ENCODING
static_assert(static_cast<unsigned>(vd::ScalarEncoding::boolean)==10);
std::int32_t runtime_entry(vd::NativeCode::CheckedEntry entry,std::int32_t value,vd::RuntimeFailure* failure,const vd::RuntimeTypeRecord* records,std::size_t count,const vd::RuntimeWitnessRecord* witnesses=nullptr,std::size_t witness_count=0) noexcept {
    try {
        vd::Runtime runtime(256);
        vd::Runtime::Attachment attachment(runtime);
        if(count || witness_count) {vd_runtime_admit_metadata(&runtime,records,count,witnesses,witness_count,failure);if(failure->message) return 0;}
        auto checkpoint=runtime.console_checkpoint();vd::NativeCode::LeaseScope native_failures(vd::NativeCode::active_lease());std::int32_t result;
        {vd::Runtime::Managed managed(runtime);result=entry(value,failure);}
        vd::finish_console(runtime,checkpoint,failure);if(failure->message)vd::export_native_failure(failure);return result;
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
void vd_display_enter(const char* hook,void* identity,vd::RuntimeFailure* failure) noexcept {
    try{vd::Runtime::current().display_enter(hook,identity);}
    catch(const std::length_error&){failure->message="Display hook nesting limit exceeded";}
    catch(const std::logic_error&){failure->message="Recursive display hook";}
    catch(const std::bad_alloc&){failure->message="Display hook tracking allocation failed";}
    catch(...){std::terminate();}
}
bool vd_collection_scope(vd::RuntimeFailure* failure) noexcept {
    try {vd::Runtime::current().collection_scope();return true;}catch(const std::bad_alloc&){failure->message="Collection display allocation failed";}catch(const std::exception&){failure->message="Collection display extent is unrepresentable";}return false;
}
void vd_collection_scope_leave() noexcept {vd::Runtime::current().collection_scope_leave();}
bool vd_collection_enter(void* value,vd::RuntimeFailure* failure) noexcept {
    try{return vd::Runtime::current().collection_enter(value);}catch(const std::bad_alloc&){failure->message="Collection display allocation failed";}catch(const std::exception&){failure->message="Invalid collection display path";}return false;
}
void vd_collection_leave(void* value,vd::RuntimeFailure* failure) noexcept {try{vd::Runtime::current().collection_leave(value);}catch(const std::exception&){failure->message="Invalid collection display path";}}
void vd_collection_append(void* value,vd::RuntimeFailure* failure) noexcept {try{vd::Runtime::current().collection_append(value);}catch(const std::bad_alloc&){failure->message="Collection display allocation failed";}catch(const std::length_error&){failure->message="Collection display extent is unrepresentable";}catch(const std::exception&){failure->message="Invalid collection display text";}}
void vd_collection_finish(void** out,vd::RuntimeFailure* failure) noexcept {try{vd::Runtime::current().collection_finish(out);}catch(const std::bad_alloc&){failure->message="Collection display allocation failed";}catch(const std::length_error&){failure->message="Collection display extent is unrepresentable";}catch(const std::exception&){failure->message="Invalid collection display scope";}}
void vd_display_leave() noexcept {vd::Runtime::current().display_leave();}
void vd_display_error(void* text,vd::RuntimeFailure* failure) noexcept {
    try {auto bytes=vd::Runtime::current().string_bytes(text);std::string message="Format rejected: ";
        for(char byte:bytes)if(byte)message+=byte;else message+="\\0";
        vd::RuntimeFailure rejected;rejected.message=message.c_str();vd::retain_native_failure(rejected,failure);
    }catch(const std::bad_alloc&){failure->message="Format error retention allocation failed";}
    catch(const std::length_error&){failure->message="Format error extent is unrepresentable";}
    catch(...){std::terminate();}
}
bool vd_scalar_display(void** out,unsigned encoding,std::uint64_t bits,void* source,void* specification,bool formatted,vd::RuntimeFailure* failure) noexcept {
    try {
        auto& runtime=vd::Runtime::current();std::string rendered;bool valid=true;
        try {
            // All borrows are consumed into owned bytes before managed allocation.
            auto spec=formatted?runtime.string_bytes(specification):std::string_view{};
            if(encoding<10)rendered=vd::formatting::number({static_cast<vd::formatting::NumberKind>(encoding),bits},spec);
            else if(encoding==10)rendered=vd::formatting::text(bits?"true":"false",spec);
            else if(encoding==11)rendered=vd::formatting::text(runtime.string_bytes(source),spec);
            else throw std::invalid_argument("Invalid display scalar encoding");
        }catch(const vd::formatting::Error& error){if(!formatted)throw;valid=false;rendered=error.what();}
        runtime.string_into(out,rendered);return valid;
    }catch(const std::bad_alloc&){failure->message="Display allocation failed";}
    catch(const std::length_error&){failure->message="Display extent is unrepresentable";}
    catch(const std::exception&){failure->message="Invalid display operation";}
    catch(...){std::terminate();}return false;
}
bool vd_format_layout(void** out,void* specification,unsigned part,vd::RuntimeFailure* failure) noexcept {
    try {auto& runtime=vd::Runtime::current();std::string result;bool valid=true;
        try{auto spec=runtime.string_bytes(specification);auto layout=vd::formatting::split_layout(spec);
            auto view=part?spec.substr(0,layout.pattern_offset):layout.pattern;if(part && view.ends_with('|'))view.remove_suffix(1);result=view;
        }catch(const vd::formatting::Error& error){valid=false;result=error.what();}
        runtime.string_into(out,result);return valid;
    }catch(const std::bad_alloc&){failure->message="Format layout allocation failed";}
    catch(const std::length_error&){failure->message="Format layout extent is unrepresentable";}
    catch(...){std::terminate();}return false;
}
void vd_string_join(void** out,void*** roots,std::size_t count,vd::RuntimeFailure* failure) noexcept {
    try {auto& runtime=vd::Runtime::current();vd::formatting::Builder builder;
        for(std::size_t i=0;i<count;++i)builder.append(runtime.string_bytes(*roots[i]));
        auto owned=std::move(builder).finish();runtime.string_into(out,owned);
    }catch(const std::bad_alloc&){failure->message="Interpolation allocation failed";}
    catch(const std::length_error&){failure->message="Interpolation extent is unrepresentable";}
    catch(const std::exception&){failure->message="Invalid interpolation storage";}
    catch(...){std::terminate();}
}
std::uint64_t vd_string_count(void* source,unsigned bytes,vd::RuntimeFailure* failure) noexcept {
    try{return vd::Runtime::current().string_count(source,bytes!=0);}
    catch(const std::exception&){failure->message="Invalid UTF-8 string storage";}catch(...){std::terminate();}return 0;
}
void vd_string_index(void** out,void* source,unsigned encoding,std::uint64_t index,vd::RuntimeFailure* failure) noexcept {
    try{vd::Runtime::current().string_index_into(out,source,static_cast<vd::ScalarEncoding>(encoding),index);}
    catch(const std::out_of_range&){failure->message="String index outside extent";}
    catch(const std::bad_alloc&){failure->message="String allocation failed";}
    catch(const std::exception&){failure->message="Invalid UTF-8 string storage";}catch(...){std::terminate();}
}
void vd_string_encode(void** out,void* source,vd::RuntimeFailure* failure) noexcept {
    try{vd::Runtime::current().string_encode_into(out,source);}
    catch(const std::bad_alloc&){failure->message="UTF-8 byte allocation failed";}
    catch(const std::exception&){failure->message="UTF-8 byte conversion failed";}catch(...){std::terminate();}
}
bool vd_string_decode(void** out,void** source,vd::RuntimeFailure* failure) noexcept {
    try{auto& runtime=vd::Runtime::current();auto valid=runtime.string_decode_into(out,runtime.root_word(source));if(!valid)runtime.string_into(out,"invalid UTF-8");return valid;}
    catch(const std::bad_alloc&){failure->message="String allocation failed";}
    catch(const std::exception&){failure->message="UTF-8 byte conversion failed";}catch(...){std::terminate();}return false;
}

void vd_string_literal(void** slot,const char* bytes,std::size_t size,vd::RuntimeFailure* failure) noexcept {
    try { vd::Runtime::current().string_into(slot,{bytes,size}); }
    catch(const std::bad_alloc&) { failure->message="String allocation failed"; }
    catch(const std::length_error&) { failure->message="String size limit exceeded"; }
    catch(const std::invalid_argument&) { failure->message="Invalid UTF-8 string storage"; }
    catch(...) { std::terminate(); }
}
void vd_string_concat(void** slot,void* left,void* right,vd::RuntimeFailure* failure) noexcept {
    try { vd::Runtime::current().concat_into(slot,left,right); }
    catch(const std::bad_alloc&) { failure->message="String allocation failed"; }
    catch(const std::length_error&) { failure->message="String size limit exceeded"; }
    catch(const std::invalid_argument&) { failure->message="Invalid UTF-8 string storage"; }
    catch(...) { std::terminate(); }
}
bool vd_string_equal(void* left,void* right,vd::RuntimeFailure* failure) noexcept {
    try { auto& runtime=vd::Runtime::current(); return runtime.string_bytes(left)==runtime.string_bytes(right); }
    catch(const std::invalid_argument&) { failure->message="Invalid UTF-8 string storage"; }
    catch(...) { std::terminate(); }
    return false;
}
void vd_gc_frame_enter(void* frame, const vd::Runtime::Slot* slots, std::size_t count) noexcept {
    try { new (frame) vd::Runtime::Frame(vd::Runtime::current(), slots, count); }
    catch (...) { std::terminate(); }
}
void vd_gc_frame_leave(void* frame) noexcept { static_cast<vd::Runtime::Frame*>(frame)->~Frame(); }
void vd_cleanup_failure(vd::RuntimeFailure* primary,const vd::RuntimeFailure* cleanup) noexcept {
    if(!cleanup->message)return;
    if(vd_worker_cleanup_failure(primary,cleanup))return;
    if(!primary->message){*primary=*cleanup;return;}
    auto append=[&](const char* message){if(primary->count<32)primary->frames[primary->count++]=message;else primary->count=33;};
    append("Additional failure during defer:\n");append(cleanup->message);
    for(unsigned n=0;n<cleanup->count && n<32;++n)append(cleanup->frames[n]);
}
bool vd_gc_release(void* object) noexcept {try{return vd::Runtime::current().begin_release(object);}catch(...){std::terminate();}}
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
            if(r.dictionary_key_forbidden>1)throw std::invalid_argument("invalid dictionary key policy");
            d.dictionary_key_schema=r.dictionary_key_schema?r.dictionary_key_schema:"";d.dictionary_key_forbidden=r.dictionary_key_forbidden!=0;
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

extern "C" int vd_globals_begin(const char* contract, std::size_t bytes, std::size_t count, vd::RuntimeFailure* failure) noexcept {
    try { return vd::Runtime::current().globals_begin(std::string_view(contract, bytes), count, *failure); }
    catch (const std::exception& error) { vd::RuntimeFailure value; value.message = error.what(); vd::retain_native_failure(value, failure); return -1; }
}
extern "C" void vd_globals_finish(vd::RuntimeFailure* failure) noexcept {
    try { vd::Runtime::current().globals_finish(*failure); }
    catch (const std::exception& error) { vd::RuntimeFailure value; value.message = error.what(); vd::retain_native_failure(value, failure); }
}
extern "C" void vd_global_publish(std::size_t id, void* cell, vd::RuntimeFailure* failure) noexcept {
    try { vd::Runtime::current().global_publish(id, cell); }
    catch (const std::exception& error) { vd::RuntimeFailure value; value.message = error.what(); vd::retain_native_failure(value, failure); }
}
extern "C" void vd_global_cell(void** out, std::size_t id, const char* declaration, vd::RuntimeFailure* failure) noexcept {
    try { *out = vd::Runtime::current().global_cell(id); }
    catch (const std::exception& error) {
        try { std::string message = std::string(error.what()) + ": " + declaration; vd::RuntimeFailure value; value.message = message.c_str(); vd::retain_native_failure(value, failure); }
        catch (...) { failure->message = "Runtime global access failed"; }
    }
}

extern "C" void vd_globals_ready(vd::RuntimeFailure* failure) noexcept {
    try { vd::Runtime::current().require_globals_ready(); }
    catch (const std::exception& error) { vd::RuntimeFailure value; value.message = error.what(); vd::retain_native_failure(value, failure); }
}
