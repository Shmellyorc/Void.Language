#include "void/runtime.hpp"
#include "void/native_runtime.hpp"
#include "void/callback.hpp"
#include "void/utf8.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <set>
#include <stdexcept>

namespace vd {
thread_local Runtime* Runtime::current_runtime_ = nullptr;
thread_local Runtime::Thread* Runtime::current_thread_ = nullptr;
Runtime::~Runtime() {
    close();
    if (!threads_.empty() || !handles_.empty() || requested_ || callbacks_ || async_owner_!=std::thread::id{}) std::terminate();
}
Runtime::Thread& Runtime::thread() const {
    if (current_runtime_ != this || !current_thread_ || current_thread_->owner != std::this_thread::get_id())
        throw std::logic_error("thread is not attached to this runtime");
    return *current_thread_;
}
void Runtime::require_managed() const {
    if (thread().state != State::managed) throw std::logic_error("operation requires Managed state");
}
Runtime::Attachment::Attachment(Runtime& r) : Attachment(r, false) {}
Runtime::Attachment::Attachment(Runtime& r, bool admitted_callback) : runtime_(r), callback_(admitted_callback) {
    if (current_runtime_) {
        if (current_runtime_ != &r) throw std::logic_error("cross-runtime attachment");
        ++current_thread_->depth;
        if (callback_) ++current_thread_->admissions;
        return;
    }
    std::unique_lock lock(r.mutex_);
    r.changed_.wait(lock, [&] { return !r.requested_; });
    if (r.closing_ && !admitted_callback) throw std::logic_error("runtime is closing");
    r.threads_.push_back(&record_);
    record_.depth = 1;
    record_.admissions = callback_ ? 1 : 0;
    current_runtime_ = &r;
    current_thread_ = &record_;
    outer_ = true;
}
Runtime::Attachment::~Attachment() {
    auto& t = runtime_.thread();
    if (callback_) { if (!t.admissions) std::terminate(); --t.admissions; }
    if (!outer_) { if (t.depth < 2) std::terminate(); --t.depth; return; }
    if (t.depth != 1 || t.frames || t.private_roots || t.managed_depth || t.admissions || t.state != State::native_safe) std::terminate();
    std::unique_lock lock(runtime_.mutex_);
    runtime_.changed_.wait(lock, [&] { return !runtime_.requested_; });
    auto& records = runtime_.threads_;
    records.erase(std::find(records.begin(), records.end(), &t));
    current_thread_ = nullptr;
    current_runtime_ = nullptr;
}
void Runtime::enter(Thread& t) {
    std::unique_lock lock(mutex_);
    changed_.wait(lock, [&] { return !requested_; });
    t.state = State::managed;
}
Runtime::Managed::Managed(Runtime& r) : runtime_(r), previous_(r.thread().state) {
    if (previous_ == State::parked) throw std::logic_error("cannot enter from Parked state");
    std::unique_lock lock(r.mutex_);
    if (previous_ == State::native_safe) r.changed_.wait(lock,[&] { return !r.requested_; });
    auto& t = r.thread();
    if (r.closing_ && !t.managed_depth && !t.admissions) throw std::logic_error("runtime rejects new Managed entries while closing");
    ++t.managed_depth;
    t.state = State::managed;
}
Runtime::Managed::~Managed() {
    std::lock_guard lock(runtime_.mutex_);
    auto& t = runtime_.thread();
    if (!t.managed_depth || t.state != State::managed) std::terminate();
    --t.managed_depth;
    if (previous_ == State::native_safe) {
        t.state = State::native_safe;
        runtime_.changed_.notify_all();
    }
}
Runtime::Native::Native(Runtime& r) : runtime_(r) {
    r.require_managed();
    std::lock_guard lock(r.mutex_);
    r.thread().state = State::native_safe;
    r.changed_.notify_all();
}
Runtime::Native::~Native() { runtime_.enter(runtime_.thread()); }
void Runtime::park(Thread& t, std::unique_lock<std::mutex>& lock) {
    ++cooperative_parks_;
    // The lock is released by wait. A new epoch may start before this waiter
    // wakes: it must remain frozen until the CURRENT request is also complete.
    t.state = State::parked;
    while (requested_) {
        t.epoch = epoch_;
        changed_.notify_all();
        changed_.wait(lock);
    }
    t.state = State::managed;
}
void Runtime::poll() {
    require_managed();
    std::unique_lock lock(mutex_);
    if (requested_) park(thread(), lock);
}
Runtime::Collection Runtime::collect(std::chrono::milliseconds timeout) {
    require_managed();
    if (timeout.count() < 0) throw std::invalid_argument("negative collection timeout");
    std::unique_lock lock(mutex_);
    auto& self = thread();
    if (requested_) {
        self.collecting_epoch = epoch_;
        park(self, lock);
        self.collecting_epoch = 0;
        return self.result;
    }
    if (epoch_ == std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("collection epoch exhausted");
    requested_ = true;
    ++epoch_;
    self.state = State::parked;
    self.epoch = epoch_;
    changed_.notify_all();
    const auto safe = [&] {
        return std::all_of(threads_.begin(), threads_.end(), [](auto* t) { return t->state != State::managed; });
    };
    bool ready = true;
    if (timeout == std::chrono::milliseconds::max()) changed_.wait(lock, safe);
    else ready = changed_.wait_for(lock, timeout, safe);
    Collection result = ready ? Collection::completed : Collection::timed_out;
    if (ready) {
        try {
            std::vector<void*> roots;
            for (auto* t : threads_) for (auto* f = t->frames; f; f = f->previous_)
                for (std::size_t i = 0; i < f->count_; ++i) {
                    const auto& slot = f->slots_[i];
                    for (std::size_t j = 0; j < slot.count; ++j) {
                        void* value = nullptr;
                        std::memcpy(&value, static_cast<const char*>(slot.storage) + slot.offsets[j], sizeof(value));
                        if (value) {
                            if(heap_.private_buffers_ && heap_.is_private(value)) {
                                bool authorized=false;
                                for(auto* root=t->private_roots;root;root=root->previous_)
                                    if(&root->frame_==f && static_cast<const char*>(slot.storage)+slot.offsets[j]==reinterpret_cast<const char*>(&root->value_) && root->value_==value && heap_.private_owner(value,root)) authorized=true;
                                if(!authorized) throw std::logic_error("private buffer escaped its registered root");
                            }
                            roots.push_back(value);
                        }
                    }
                }
            for (auto* h : handles_) if (h->value_) roots.push_back(h->value_);
            heap_.collect(roots);
        } catch (const std::bad_alloc&) { result = Collection::allocation_failure; }
        catch (...) { std::terminate(); } // Invalid precise roots are a broken compiler/runtime invariant.
    }
    if (result == Collection::completed) allocations_ = 0;
    for (auto* t : threads_) if (t->collecting_epoch == epoch_) t->result = result;
    completed_epoch_ = epoch_;
    requested_ = false;
    self.state = State::managed;
    changed_.notify_all();
    return result;
}
Runtime::Frame* Runtime::require_output(void** slot) const {
    require_managed();
    if (!slot) throw std::invalid_argument("null allocation output");
    for (auto* f = thread().frames; f; f = f->previous_)
        for (std::size_t i = 0; i < f->count_; ++i)
            for (std::size_t j = 0; j < f->slots_[i].count; ++j)
                if (static_cast<const char*>(f->slots_[i].storage) + f->slots_[i].offsets[j] == reinterpret_cast<const char*>(slot)) return f;
    throw std::invalid_argument("allocation output is not a registered precise root");
}
std::string_view Runtime::string_bytes(void* object) const {
    require_managed();
    std::lock_guard lock(mutex_);
    return heap_.string_bytes(object);
}
void Runtime::string_into(void** slot,std::string_view text) {
    string_parts_into(slot,text,{});
}
std::size_t Runtime::string_payload_size(std::size_t left,std::size_t right) {
    constexpr auto maximum=std::numeric_limits<std::size_t>::max()-sizeof(std::uint64_t);
    if (left>maximum || right>maximum-left) throw std::length_error("string extent overflow");
    return sizeof(std::uint64_t)+left+right;
}
void Runtime::string_parts_into(void** slot,std::string_view left,std::string_view right) {
    require_output(slot);
    {std::lock_guard lock(mutex_);heap_.validate_output_address(slot);}
    auto bytes=string_payload_size(left.size(),right.size());
    if (!valid_utf8(left) || !valid_utf8(right)) throw std::invalid_argument("invalid UTF-8 string");
    if (left.empty() && right.empty()) { std::lock_guard lock(mutex_);heap_.validate_output_address(slot);*slot=nullptr; return; }
    std::vector<char> payload(bytes);
    std::uint64_t size=left.size()+right.size(); std::memcpy(payload.data(),&size,sizeof(size));
    if(!left.empty()) std::memcpy(payload.data()+sizeof(size),left.data(),left.size());
    if(!right.empty()) std::memcpy(payload.data()+sizeof(size)+left.size(),right.data(),right.size());
    TypeLayout layout{payload.size(),{}}; layout.kind=TypeLayout::Kind::string;
    construct_into(slot,layout,payload.data());
}
void Runtime::concat_into(void** slot,void* left,void* right) {
    auto a=string_bytes(left),b=string_bytes(right);
    string_parts_into(slot,a,b);
}
void Runtime::allocate_into(void** slot, const TypeLayout& layout) {
    require_output(slot);
    {std::lock_guard lock(mutex_);heap_.validate_output_address(slot);}
    if (layout.kind!=TypeLayout::Kind::ordinary) throw std::invalid_argument("intrinsic allocation requires complete construction");
    // Before allocation, the caller's registered slot contains null/its old
    // value. There is no safepoint between allocation and publication.
    poll();
    std::lock_guard lock(mutex_);
    validate_descriptor(layout);
    heap_.validate_output_address(slot);
    *slot = heap_.allocate(layout);
}
void Runtime::construct_into(void** slot, const TypeLayout& layout, const void* payload) {
    require_output(slot);
    {std::lock_guard lock(mutex_);heap_.validate_output_address(slot);}
    if (!payload) throw std::invalid_argument("missing prepared object");
    if (layout.kind==TypeLayout::Kind::string) {
        if (layout.size<=sizeof(std::uint64_t) || !layout.references.empty() || layout.descriptor)
            throw std::invalid_argument("invalid intrinsic string layout");
        std::uint64_t size; std::memcpy(&size,payload,sizeof(size));
        if (size!=layout.size-sizeof(size) || !valid_utf8({static_cast<const char*>(payload)+sizeof(size),static_cast<std::size_t>(size)}))
            throw std::invalid_argument("invalid intrinsic string payload");
    }
    auto references=LegacyReferenceMap::validate(layout.size,layout.references);
    ReferenceCursor cursor(references);
    bool due;
    { std::lock_guard lock(mutex_); due=allocation_budget_ && allocations_>=allocation_budget_; }
    // Neither collection nor parking occurs with a collector-required lock held.
    if (due && collect()!=Collection::completed) throw std::bad_alloc();
    poll();
    std::lock_guard lock(mutex_);
    while (auto offset=cursor.next_offset()) {
        void* value=nullptr; std::memcpy(&value,static_cast<const char*>(payload)+*offset,sizeof(value));
        if (value && !heap_.contains(value)) throw std::logic_error("prepared reference belongs to another heap");
    }
    // Heap::allocate completes every fallible descriptor/map operation. The
    // copy and final root publication cannot throw or invoke user code/GC.
    validate_descriptor(layout);
    heap_.validate_output_address(slot);
    void* object=heap_.allocate_validated(layout,std::move(references));
    std::memcpy(object,payload,layout.size);
    *slot=object;
    if (allocations_ != static_cast<std::size_t>(-1)) ++allocations_;
}
void Runtime::store(void* object, std::size_t offset, void* value) {
    require_managed();
    std::lock_guard lock(mutex_);
    heap_.store(object, offset, value);
}
bool Runtime::begin_release(void* object) {
    require_managed();std::lock_guard lock(mutex_);auto found=heap_.objects_.find(object);
    if(found==heap_.objects_.end() || found->second.kind!=TypeLayout::Kind::ordinary || !found->second.metadata)throw std::invalid_argument("Invalid resource receiver");
    if(found->second.released)return false;found->second.released=true;return true;
}
bool Runtime::contains(void* p) const { std::lock_guard lock(mutex_); return heap_.contains(p); }
Runtime::Snapshot Runtime::snapshot() const {
    std::lock_guard lock(mutex_);
    return {epoch_, completed_epoch_, threads_.size(), static_cast<std::size_t>(std::count_if(threads_.begin(), threads_.end(), [](auto* t) { return t->state == State::parked; })), heap_.size(), requested_, cooperative_parks_};
}
bool Runtime::wait_for_request(std::chrono::milliseconds timeout) {
    if (current_runtime_ == this && thread().state == State::managed) throw std::logic_error("Managed thread cannot wait for a request");
    std::unique_lock lock(mutex_);
    return changed_.wait_for(lock, timeout, [&] { return requested_; });
}
void Runtime::async_worker_limit(unsigned count) {
    if(!count || count>64)throw std::invalid_argument("worker limit must be 1..64");
    std::lock_guard lock(mutex_);if(async_owner_!=std::thread::id{} || closing_)throw std::logic_error("worker configuration requires an idle Runtime");async_worker_limit_=count;
}
unsigned Runtime::async_worker_limit() const {std::lock_guard lock(mutex_);return async_worker_limit_;}
bool Runtime::enter_async(){require_managed();std::lock_guard lock(mutex_);if(async_owner_!=std::thread::id{} || closing_)return false;async_owner_=std::this_thread::get_id();return true;}
void Runtime::leave_async() noexcept {std::lock_guard lock(mutex_);if(async_owner_!=std::this_thread::get_id())std::terminate();async_owner_={};}
void Runtime::close() {
    if(CallbackRegistration::active_on(*this))throw std::logic_error("Runtime cannot quiesce its own callback");
    if (current_runtime_ == this && thread().state == State::managed) throw std::logic_error("close requires NativeSafe state");
    {std::lock_guard lock(mutex_);closing_ = true;changed_.notify_all();}
    native_shutdown(*this);
}
Runtime::Frame::Frame(Runtime& r, const Slot* slots, std::size_t count)
    : runtime_(r), previous_(nullptr), slots_(slots), count_(count) {
    r.require_managed();
    if (count && !slots) throw std::invalid_argument("null root frame slots");
    for (std::size_t i = 0; i < count; ++i) {
        const auto& s = slots[i];
        if (!s.storage || (s.count && !s.offsets)) throw std::invalid_argument("invalid root storage");
        for (std::size_t j = 0; j < s.count; ++j)
            if (s.offsets[j] > s.size || s.size - s.offsets[j] < sizeof(void*)) throw std::invalid_argument("invalid root offset");
    }
    previous_ = r.thread().frames;
    r.thread().frames = this;
}
Runtime::Frame::~Frame() {
    runtime_.require_managed();
    auto& t = runtime_.thread();
    if (t.frames != this) std::terminate();
    t.frames = previous_;
}
Runtime::Handle::Handle(Runtime& r, void* value) : runtime_(r), value_(value) {
    std::lock_guard lock(r.mutex_);
    if (r.closing_ || (value && !r.heap_.contains(value))) throw std::invalid_argument("invalid persistent handle");
    r.handles_.push_back(this);
}
Runtime::Handle::~Handle() {
    std::lock_guard lock(runtime_.mutex_);
    auto& handles = runtime_.handles_;
    handles.erase(std::find(handles.begin(), handles.end(), this));
}
void Runtime::Handle::reset(void* value) {
    std::lock_guard lock(runtime_.mutex_);
    if (value && !runtime_.heap_.contains(value)) throw std::invalid_argument("invalid persistent handle value");
    value_ = value;
}
void* Runtime::Handle::get() const {
    runtime_.require_managed();
    std::lock_guard lock(runtime_.mutex_);
    return value_;
}
}

namespace vd {
void Runtime::admit_types(std::span<const NominalDescriptor> incoming) {admit_metadata(incoming,{});}
void Runtime::admit_metadata(std::span<const NominalDescriptor> incoming,std::span<const InterfaceWitness> witnesses) {admit_metadata(incoming,witnesses,{});}
void Runtime::admit_buffer_schemas(std::span<const ElementSchemaSpec> schemas) {admit_metadata({},{},schemas);}
void Runtime::admit_metadata(std::span<const NominalDescriptor> incoming,std::span<const InterfaceWitness> incoming_witnesses,std::span<const ElementSchemaSpec> incoming_schemas) {
    if(current_thread_ && current_thread_->state!=State::native_safe)
        throw std::logic_error("type admission requires NativeSafe state");
    if(incoming.empty() && incoming_witnesses.empty() && incoming_schemas.empty()) return;
    NominalCatalog candidates;
    std::map<std::string,std::shared_ptr<const InterfaceWitness>,std::less<>> witness_candidates;
    std::map<std::string,std::shared_ptr<ElementSchema>,std::less<>> schema_candidates;
    for(const auto& input:incoming_schemas) {
        auto schema=ElementSchema::prepare(input);
        if(!schema_candidates.emplace(input.identity,std::move(schema)).second) throw std::invalid_argument("duplicate element schema");
    }
    for(const auto& input:incoming_witnesses) {
        if(input.key.empty() || input.interface_identity.empty() || input.view_identity.empty() || input.contract.empty() ||
            input.key.find('\0')!=std::string::npos || input.interface_identity.find('\0')!=std::string::npos || input.view_identity.find('\0')!=std::string::npos || input.contract.find('\0')!=std::string::npos)
            throw std::invalid_argument("invalid interface witness");
        const auto encoded=[](std::string_view value){return std::to_string(value.size())+":"+std::string(value);};
        if(!input.key.starts_with("interface:"+encoded(input.interface_identity)+encoded(input.view_identity))) throw std::invalid_argument("witness key/domain mismatch");
        if(!witness_candidates.emplace(input.key,std::make_shared<InterfaceWitness>(input)).second) throw std::invalid_argument("duplicate interface witness");
    }
    for(const auto& input:incoming) {
        if(input.identity.empty() || input.shape.empty() || !input.layout.size || input.layout.descriptor || input.layout.kind!=TypeLayout::Kind::ordinary)
            throw std::invalid_argument("invalid nominal descriptor");
        auto value=std::make_shared<NominalDescriptor>(input);
        value->layout.references=ReferenceShape::legacy(value->layout.size,value->layout.references).legacy_offsets();
        if(!candidates.emplace(value->identity,std::move(value)).second) throw std::invalid_argument("duplicate nominal descriptor");
    }
    for(;;) {
        std::shared_ptr<const TypeCatalog> snapshot;
        {std::lock_guard lock(mutex_); if(closing_) throw std::logic_error("runtime is closing"); snapshot=types_;}
        auto prepared=std::make_shared<TypeCatalog>(snapshot ? *snapshot : TypeCatalog{});
        for(const auto& [key,value]:candidates) {
            auto [found,added]=prepared->types.emplace(key,value);
            if(!added) {
                const auto& a=*found->second;const auto& b=*value;
                if(a.base!=b.base || a.shape!=b.shape || a.layout.size!=b.layout.size || a.layout.references!=b.layout.references)
                    throw std::invalid_argument("incompatible nominal descriptor admission");
            }
        }
        for(const auto& [key,value]:prepared->types) {
            std::set<std::string_view> seen{key};auto current=value;
            while(!current->base.empty()) {
                auto found=prepared->types.find(current->base);
                if(found==prepared->types.end() || seen.size()>=128 || !seen.insert(current->base).second)
                    throw std::invalid_argument("missing, recursive or excessive nominal ancestry");
                const auto& parent=found->second;
                if(current->layout.size<parent->layout.size || !std::includes(current->layout.references.begin(),current->layout.references.end(),parent->layout.references.begin(),parent->layout.references.end()))
                    throw std::invalid_argument("incompatible base payload prefix");
                current=parent;
            }
        }
        for(const auto& [key,value]:witness_candidates) {
            if(!prepared->types.contains(value->view_identity)) throw std::invalid_argument("unadmitted witness static view");
            auto [found,added]=prepared->witnesses.emplace(key,value);
            if(!added) {
                const auto& old=*found->second;
                if(old.interface_identity!=value->interface_identity || old.view_identity!=value->view_identity || old.contract!=value->contract)
                    throw std::invalid_argument("incompatible interface witness admission");
            }
        }
        for(const auto& [key,schema]:schema_candidates) {
            std::map<std::string,std::shared_ptr<const NominalDescriptor>> targets;
            std::map<std::string,std::shared_ptr<const InterfaceWitness>> interfaces;
            for(const auto& node:schema->spec_.nodes) {
                if(auto* ref=std::get_if<SchemaManaged>(&node)) {
                    auto target=prepared->types.find(ref->target);
                    if(target==prepared->types.end()) throw std::invalid_argument("unadmitted schema reference target");
                    targets.emplace(ref->target,target->second);
                }
                if(auto* ref=std::get_if<SchemaGrowable>(&node)) {
                    auto target=prepared->types.find(ref->header);if(target==prepared->types.end())throw std::invalid_argument("unadmitted nested list header type");targets.emplace(ref->header,target->second);
                }
                if(auto* ref=std::get_if<SchemaInterface>(&node)) {
                    auto witness=std::find_if(prepared->witnesses.begin(),prepared->witnesses.end(),[&](const auto& entry){return entry.second->interface_identity==ref->target;});
                    if(witness==prepared->witnesses.end()) {
                        auto declaration=prepared->types.find(ref->target);
                        if(declaration==prepared->types.end() || !declaration->second->shape.starts_with("interface:"))throw std::invalid_argument("unadmitted schema interface target");
                        targets.emplace(ref->target,declaration->second);
                    } else interfaces.emplace(ref->target,witness->second);
                }
            }
            schema->nominal_owners_.clear();schema->witness_owners_.clear();
            for(const auto& [id,target]:targets) { (void)id;schema->nominal_owners_.push_back(target); }
            for(const auto& [id,witness]:interfaces) { (void)id;schema->witness_owners_.push_back(witness); }
            auto [found,added]=prepared->schemas.emplace(key,schema);
            if(!added && !found->second->equivalent(*schema)) throw std::invalid_argument("incompatible element schema admission");
        }
        {std::lock_guard lock(mutex_);
            if(closing_) throw std::logic_error("runtime is closing");
            if(types_!=snapshot) continue;
            types_=std::move(prepared); // All allocations/validation precede publication.
            return;
        }
    }
}
ReferenceShape Runtime::reference_shape(ElementSchemaRef schema) const {
    std::lock_guard lock(mutex_);
    if(schema && types_) {
        auto found=types_->schemas.find(schema->identity());
        if(found!=types_->schemas.end() && found->second==schema) return ReferenceShape(std::move(schema));
    }
    throw std::invalid_argument("foreign or unadmitted element schema handle");
}
ElementSchemaRef Runtime::buffer_schema(std::string_view key) const {
    std::lock_guard lock(mutex_);
    if(types_) {auto found=types_->schemas.find(key);if(found!=types_->schemas.end()) return found->second;}
    throw std::invalid_argument("element schema is not admitted");
}
std::shared_ptr<const NominalDescriptor> Runtime::descriptor(std::string_view key) const {
    std::lock_guard lock(mutex_);
    if(!types_) throw std::invalid_argument("nominal type is not admitted");
    auto found=types_->types.find(key);
    if(found==types_->types.end()) throw std::invalid_argument("nominal type is not admitted");
    return found->second;
}
const InterfaceWitness* Runtime::witness(std::string_view key) const {
    require_managed();std::lock_guard lock(mutex_);
    if(!types_) return nullptr;
    auto found=types_->witnesses.find(key);return found==types_->witnesses.end() ? nullptr : found->second.get();
}
std::int64_t Runtime::interface_binding(void* object,const void* pointer,std::span<const char* const> keys) const {
    require_managed();std::lock_guard lock(mutex_);
    if(!object || !pointer || !types_) return -1;
    // Validate opaque membership before dereferencing; metadata is not a GC root.
    const InterfaceWitness* admitted=nullptr;
    for(const auto& [key,value]:types_->witnesses) if(value.get()==pointer) {admitted=value.get();break;}
    if(!admitted) return -1;
    auto key=heap_.nominal_identity(object);if(key.empty()) return -1;
    auto current=types_->types.find(key);
    bool matches=false;
    for(unsigned depth=0;current!=types_->types.end() && depth<128;++depth) {
        if(current->first==admitted->view_identity) {matches=true;break;}
        if(current->second->base.empty()) break;
        current=types_->types.find(current->second->base);
    }
    if(!matches) return -1;
    auto found=std::lower_bound(keys.begin(),keys.end(),admitted->key,[](const char* a,std::string_view b){return std::string_view(a)<b;});
    return found==keys.end() || std::string_view(*found)!=admitted->key ? -1 : found-keys.begin();
}
std::int64_t Runtime::dynamic_type(void* object,std::span<const char* const> catalog) const {
    require_managed();
    std::lock_guard lock(mutex_);
    const auto key=heap_.nominal_identity(object);
    if(key.empty()) return -1;
    auto found=std::lower_bound(catalog.begin(),catalog.end(),key,[](const char* a,std::string_view b){return std::string_view(a)<b;});
    if(found==catalog.end() || std::string_view(*found)!=key) return -1;
    return found-catalog.begin();
}
}

namespace vd {
void Runtime::validate_descriptor(const TypeLayout& layout) const {
    if(!layout.descriptor) return;
    if(!types_) throw std::invalid_argument("unadmitted nominal layout");
    const auto& descriptor=*layout.descriptor;
    auto found=types_->types.find(descriptor.identity);
    if(found==types_->types.end() || found->second!=layout.descriptor || descriptor.layout.size!=layout.size || descriptor.layout.references!=layout.references)
        throw std::invalid_argument("nominal layout does not match admitted descriptor");
}
}
