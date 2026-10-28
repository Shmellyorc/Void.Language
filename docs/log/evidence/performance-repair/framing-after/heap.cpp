#include "performance_metrics.hpp"
#include "void/heap.hpp"
#include "void/utf8.hpp"
#include <algorithm>
#include <cstring>
#include <new>
#include <utility>
#include <unordered_set>
#include <stdexcept>
namespace vd {
Heap::MemorySnapshot Heap::memory_snapshot() const {
    std::size_t bytes=0;
    for(const auto& [address,object]:objects_) {(void)address;bytes+=object.references.bytes();}
    return {objects_.size(),bytes,objects_.bucket_count(),roots_.capacity()};
}
Heap::~Heap() { for (auto& [p, o] : objects_) { (void)o; destroy(p,o); } }
void* Heap::allocate(const TypeLayout& layout) {
    return allocate_validated(layout,LegacyReferenceMap::validate(layout.size,layout.references));
}
void* Heap::allocate_validated(const TypeLayout& layout,LegacyReferenceMap shape) {
    if (!layout.size || layout.size!=shape.bytes()) throw std::invalid_argument("empty or mismatched object layout");
    if (layout.kind!=TypeLayout::Kind::ordinary && layout.kind!=TypeLayout::Kind::string) throw std::invalid_argument("unknown object layout kind");
    if (layout.kind == TypeLayout::Kind::string &&
        (layout.size <= sizeof(std::uint64_t) || !layout.references.empty() || layout.descriptor))
        throw std::invalid_argument("invalid intrinsic string layout");
    auto* p = ::operator new(layout.size);
    std::memset(p, 0, layout.size);
    try { objects_.emplace(p, Object{std::move(shape),layout.descriptor,layout.kind}); }
    catch (...) { ::operator delete(p); throw; }
    return p;
}
std::size_t BufferLayout::element_offset(std::uint64_t index) const {
    if(index>=capacity_) throw std::out_of_range("buffer element outside capacity");
    if(index && stride_>(std::numeric_limits<std::size_t>::max()-base_)/index) throw std::length_error("buffer offset overflow");
    return base_+stride_*static_cast<std::size_t>(index);
}
BufferReferenceCursor::BufferReferenceCursor(BufferLayoutRef layout)
    :layout_(std::move(layout)),shape_(layout_ ? layout_->schema() : ElementSchemaRef{}),cursor_(shape_) {}
std::optional<BufferReferenceEvent> BufferReferenceCursor::next() {
    if(!shape_.has_references())return {};
    while(element_<layout_->capacity()) {
        if(auto event=cursor_.next()) {
            const auto base=layout_->element_offset(element_);
            // Admitted child geometry plus factory-checked capacity guarantees
            // containment. Check again before forming any payload address.
            auto offset=[&](std::size_t child) {
                if(base>layout_->bytes() || child>layout_->bytes()-base || sizeof(void*)>layout_->bytes()-base-child)
                    throw std::logic_error("buffer reference outside allocation");
                return base+child;
            };
            event->offset=offset(event->offset);
            if(event->witness_offset)event->witness_offset=offset(*event->witness_offset);
            return BufferReferenceEvent{element_,*event};
        }
        ++element_;if(element_<layout_->capacity())cursor_.reset(shape_);
    }
    return {};
}
void Heap::validate_reference(const ReferenceEvent& event,void* object,const void* witness,
                              const RuntimeMetadata& catalog,bool private_nulls) const {
    if(!object) {
        if(witness || (!private_nulls && !event.nullable))throw std::invalid_argument("incomplete required buffer reference");
        return;
    }
    auto found=objects_.find(object);
    if(found==objects_.end() || (found->second.kind==TypeLayout::Kind::buffer && !static_cast<const BufferState*>(found->second.metadata.get())->complete))
        throw std::invalid_argument("untracked or private buffer reference target");
    if(event.kind==ReferenceEvent::Kind::buffer) {
        if(witness || found->second.kind!=TypeLayout::Kind::buffer)throw std::invalid_argument("nested fixed buffer kind mismatch");
        const auto& state=buffer_state(object);
        if(state.layout->schema()->identity()!=event.target || state.layout->capacity()!=event.extent || state.valid_prefix!=event.extent)throw std::invalid_argument("nested fixed buffer type mismatch");
        return;
    }
    if(event.kind==ReferenceEvent::Kind::string) {
        if(witness || found->second.kind!=TypeLayout::Kind::string)throw std::invalid_argument("buffer string kind mismatch");
        (void)string_bytes(object);return;
    }
    if(found->second.kind!=TypeLayout::Kind::ordinary || !found->second.metadata)throw std::invalid_argument("buffer nominal kind mismatch");
    const auto* actual=static_cast<const NominalDescriptor*>(found->second.metadata.get());
    auto current=catalog.types.find(actual->identity);
    if(current==catalog.types.end() || current->second.get()!=actual)throw std::invalid_argument("foreign buffer nominal descriptor");
    auto target=event.target;
    if(event.kind==ReferenceEvent::Kind::interface) {
        const InterfaceWitness* admitted=nullptr;
        for(const auto& [key,w]:catalog.witnesses) { (void)key;if(w.get()==witness) {admitted=w.get();break;} }
        if(!admitted || admitted->interface_identity!=event.target)throw std::invalid_argument("invalid buffer interface witness");
        target=admitted->view_identity;
    } else if(event.kind!=ReferenceEvent::Kind::managed || witness)throw std::invalid_argument("incorrect buffer reference kind");
    for(unsigned depth=0;current!=catalog.types.end() && depth<128;++depth) {
        if(current->first==target)return;
        if(current->second->base.empty())break;
        current=catalog.types.find(current->second->base);
    }
    throw std::invalid_argument("buffer reference target mismatch");
}
void Heap::release(void* data,std::size_t alignment) noexcept {
    if(!data) return;
    if(alignment>__STDCPP_DEFAULT_NEW_ALIGNMENT__) ::operator delete(data,std::align_val_t(alignment));
    else ::operator delete(data);
}
Heap::PendingBuffer::PendingBuffer(BufferLayoutRef layout) :layout_(std::move(layout)) {
    if(!layout_->bytes()) return;
    auto alignment=layout_->alignment();
    data_=alignment>__STDCPP_DEFAULT_NEW_ALIGNMENT__ ? ::operator new(layout_->bytes(),std::align_val_t(alignment)) : ::operator new(layout_->bytes());
    if(reinterpret_cast<std::uintptr_t>(data_)%alignment) {
        Heap::release(data_,alignment);data_=nullptr;throw std::logic_error("native allocator violated buffer alignment");
    }
    std::memset(data_,0,layout_->bytes()); // NativeSafe, no Runtime/collector lock.
}
Heap::PendingBuffer::~PendingBuffer() {if(data_) Heap::release(data_,layout_->alignment());}
Heap::PendingBuffer::PendingBuffer(PendingBuffer&& p) noexcept : data_(std::exchange(p.data_,nullptr)),layout_(std::move(p.layout_)) {}
Heap::PendingBuffer& Heap::PendingBuffer::operator=(PendingBuffer&& p) noexcept {
    if(this!=&p) {if(data_)Heap::release(data_,layout_->alignment());data_=std::exchange(p.data_,nullptr);layout_=std::move(p.layout_);}return *this;
}
void* Heap::adopt_buffer(PendingBuffer& pending,std::shared_ptr<const BufferState> state) {
    if(!pending.data_ || !state || state->layout!=pending.layout_ || state->requested_live>state->layout->capacity() || !state->catalog || !state->owner || state->complete || private_buffers_==std::numeric_limits<std::size_t>::max() || buffer_objects_==std::numeric_limits<std::size_t>::max())
        throw std::invalid_argument("invalid private buffer adoption");
    auto map=LegacyReferenceMap::validate(state->layout->bytes(),{});
    auto* data=pending.data_;
    auto [it,added]=objects_.emplace(data,Object{std::move(map),std::move(state),TypeLayout::Kind::buffer});
    (void)it;if(!added) throw std::logic_error("duplicate buffer storage");
    const auto& adopted=*static_cast<const BufferState*>(it->second.metadata.get());
    adopted.data=data;adopted.next=buffers_;
    if(buffers_)buffers_->previous=&adopted;
    buffers_=&adopted;
    const auto base=reinterpret_cast<std::uintptr_t>(data);
    const auto tail=pending.layout_->bytes()-1;
    const auto high=tail>std::numeric_limits<std::uintptr_t>::max()-base ? std::numeric_limits<std::uintptr_t>::max() : base+tail;
    if(!buffer_objects_) {buffer_low_=base;buffer_high_=high;}
    else {buffer_low_=std::min(buffer_low_,base);buffer_high_=std::max(buffer_high_,high);}
    ++private_buffers_;++buffer_objects_;pending.data_=nullptr;return data;
}
const Heap::BufferState& Heap::buffer_state(void* object) const {
    auto found=objects_.find(object);
    if(found==objects_.end() || found->second.kind!=TypeLayout::Kind::buffer || !found->second.metadata) throw std::invalid_argument("not a buffer object");
    return *static_cast<const BufferState*>(found->second.metadata.get());
}
bool Heap::is_private(void* object) const {
    auto found=objects_.find(object);
    return found!=objects_.end() && found->second.kind==TypeLayout::Kind::buffer && !static_cast<const BufferState*>(found->second.metadata.get())->complete;
}
bool Heap::private_owner(void* object,const void* owner) const {const auto& b=buffer_state(object);return !b.complete && b.owner==owner;}
void Heap::seal_buffer(void* object,const void* owner,std::uint64_t prefix) noexcept {
    auto found=objects_.find(object);
    if(found==objects_.end() || found->second.kind!=TypeLayout::Kind::buffer)std::terminate();
    const auto& b=*static_cast<const BufferState*>(found->second.metadata.get());
    if(b.complete || b.owner!=owner || prefix!=b.requested_live || prefix>b.layout->capacity() || !private_buffers_) std::terminate();
    b.valid_prefix=prefix;b.owner=nullptr;b.complete=true;--private_buffers_;
}
void Heap::destroy(void* object,const Object& record) noexcept {
    if(record.kind==TypeLayout::Kind::buffer) {
        const auto& b=*static_cast<const BufferState*>(record.metadata.get());
        if(!buffer_objects_)std::terminate();--buffer_objects_;
        if(b.previous)b.previous->next=b.next;else buffers_=b.next;
        if(b.next)b.next->previous=b.previous;
        b.previous=b.next=nullptr;b.data=nullptr;
        if(!b.complete) {if(!private_buffers_)std::terminate();--private_buffers_;}
        release(object,b.layout->alignment());
    } else ::operator delete(object);
}
bool Heap::contains(void* p) const {
    auto found=objects_.find(p);
    return found!=objects_.end() && (found->second.kind!=TypeLayout::Kind::buffer || static_cast<const BufferState*>(found->second.metadata.get())->complete);
}
std::string_view Heap::string_bytes(void* p) const {
    if (!p) return {};
    auto found=objects_.find(p);
    if (found==objects_.end() || found->second.kind!=TypeLayout::Kind::string)
        throw std::invalid_argument("invalid string object");
    std::uint64_t size; perf::copy(&size,p,sizeof(size));
    if (size!=found->second.references.bytes()-sizeof(size)) throw std::logic_error("corrupt string extent");
    return {static_cast<const char*>(p)+sizeof(size),static_cast<std::size_t>(size)};
}
std::string_view Heap::nominal_identity(void* p) const {
    auto found=objects_.find(p);
    if(found==objects_.end() || found->second.kind!=TypeLayout::Kind::ordinary || !found->second.metadata) return {};
    return static_cast<const NominalDescriptor*>(found->second.metadata.get())->identity;
}
void Heap::add_root(void** slot) {
    if (!slot || std::find(roots_.begin(), roots_.end(), slot) != roots_.end())
        throw std::invalid_argument("invalid or duplicate root slot");
    roots_.push_back(slot);
}
void Heap::remove_root(void** slot) {
    auto found = std::find(roots_.begin(), roots_.end(), slot);
    if (found == roots_.end()) throw std::invalid_argument("unregistered root slot");
    roots_.erase(found);
}
// A registered root may not serve as an untyped write alias into buffer data.
// Ordinary/string-only heaps take the constant-time fast path.
void Heap::validate_output_address(void** slot) const { perf::Timer perf_timer(perf::heap_alias); 
    if(!buffer_objects_)return;
    const auto address=reinterpret_cast<std::uintptr_t>(slot);
    // Pointer-sized writes beginning just below the lowest buffer may overlap.
    // Use subtraction to avoid wrapping address+sizeof(void*) near UINTPTR_MAX.
    if(address>buffer_high_ || (address<buffer_low_ && buffer_low_-address>=sizeof(void*)))return;
    // Include private and published buffers, without revisiting unrelated
    // ordinary objects retained by a long-lived async root.
    for(auto* buffer=buffers_;perf::test(buffer,perf::heap_alias_visits);buffer=buffer->next) {
        const auto& state=*buffer;
        const auto base=reinterpret_cast<std::uintptr_t>(state.data);
        if((address>=base && address-base<state.layout->bytes()) || (address<base && base-address<sizeof(void*))) {
            if(state.readers)throw BufferBusy("output aliases leased buffer storage");
            throw std::invalid_argument("untyped output aliases buffer storage");
        }
    }
}
void Heap::store(void* object, std::size_t offset, void* value) {
    auto found = objects_.find(object);
    if(found!=objects_.end() && found->second.kind==TypeLayout::Kind::buffer) {
        const auto& state=*static_cast<const BufferState*>(found->second.metadata.get());
        if(state.readers)throw BufferBusy("buffer storage has active read leases");
        if(!state.complete || offset<state.layout->base())throw std::invalid_argument("invalid buffer store");
        auto relative=offset-state.layout->base();
        if(relative/state.layout->stride()>=state.valid_prefix)throw std::out_of_range("buffer store outside live prefix");
        auto event=ReferenceShape(state.layout->schema()).at(relative%state.layout->stride());
        if(!event || event->kind==ReferenceEvent::Kind::interface)throw std::invalid_argument("store requires one typed reference word");
        // Stage 2 embedding mutation is restricted to the explicit mutable profile.
        // Other profiles cannot gain write authority through this legacy entry.
        if(!state.layout->schema()->mutable_data())throw std::invalid_argument("buffer profile does not permit mutation");
        validate_reference(*event,value,nullptr,*state.catalog,false);
        perf::copy(static_cast<char*>(object)+offset,&value,sizeof(value));return;
    }
    if (found == objects_.end() || found->second.kind!=TypeLayout::Kind::ordinary || (value && !contains(value)) ||
        !found->second.references.at(offset))
        throw std::invalid_argument("invalid managed reference store");
    perf::copy(static_cast<char*>(object) + offset, &value, sizeof(value));
}
void Heap::collect() { collect({}); }
void Heap::collect(std::span<void* const> additional_roots) {
    std::vector<void*> pending;
    std::optional<std::unordered_set<void*>> buffer_queued; // do not construct even an empty set on legacy paths
    for (auto* root : additional_roots) if (root) pending.push_back(root);
    for (auto* root : roots_) if (*root) pending.push_back(*root);
    // Validate the entire graph before sweeping. A bad slot cannot silently
    // turn a reachable object into reclaimed memory.
    try {
        while (!pending.empty()) {
            void* p = pending.back(); pending.pop_back();
            auto found = objects_.find(p);
            if (found == objects_.end()) throw std::logic_error("untracked managed reference");
            if (found->second.marked) continue;
            found->second.marked = true;
            if(found->second.kind==TypeLayout::Kind::buffer) {
                const auto& state=*static_cast<const BufferState*>(found->second.metadata.get());
                if(!state.layout->schema()->has_references())continue;
                BufferReferenceCursor references(state.layout);
                while(auto e=references.next()) {
                    void* child=nullptr;const void* witness=nullptr;
                    perf::copy(&child,static_cast<char*>(p)+e->reference.offset,sizeof(child));
                    if(e->reference.witness_offset)perf::copy(&witness,static_cast<char*>(p)+*e->reference.witness_offset,sizeof(witness));
                    if(e->element>=(state.complete ? state.valid_prefix : state.requested_live) && (child || witness))
                        throw std::logic_error("nonzero reference in spare capacity");
                    validate_reference(e->reference,child,witness,*state.catalog,!state.complete || e->element>=state.valid_prefix);
                    if(child && !objects_.find(child)->second.marked) {
                        if(!buffer_queued)buffer_queued.emplace();
                        if(buffer_queued->insert(child).second)pending.push_back(child);
                    }
                }
                continue;
            }
            // Map ownership stays in this stable Heap node until marking ends.
            // No map mutation/sweep or callback occurs during this borrowed scan.
            ReferenceCursor cursor(found->second.references);
            while (auto offset=cursor.next_offset()) {
                void* child = nullptr;
                perf::copy(&child, static_cast<char*>(p) + *offset, sizeof(child));
                if (child) {
                    if(private_buffers_ && is_private(child)) throw std::logic_error("heap edge to private buffer");
                    pending.push_back(child);
                }
            }
        }
    } catch (...) {
        for (auto& [p, o] : objects_) { (void)p; o.marked = false; }
        throw;
    }
    for (auto it = objects_.begin(); it != objects_.end();) {
        if (!it->second.marked) { destroy(it->first,it->second); it = objects_.erase(it); }
        else { it->second.marked = false; ++it; }
    }
}
Root::Root(Heap& heap, void* object) : heap_(heap), value_(object) { heap_.add_root(&value_); }
Root::~Root() { heap_.remove_root(&value_); }
}
