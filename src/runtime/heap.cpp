#include "void/heap.hpp"
#include "void/utf8.hpp"
#include <algorithm>
#include <cstring>
#include <new>
#include <utility>
#include <stdexcept>
namespace vd {
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
    if(!pending.data_ || !state || state->layout!=pending.layout_ || state->layout->schema()->has_references() || !state->owner || state->complete || private_buffers_==std::numeric_limits<std::size_t>::max())
        throw std::invalid_argument("invalid private buffer adoption");
    auto map=LegacyReferenceMap::validate(state->layout->bytes(),{});
    auto* data=pending.data_;
    auto [it,added]=objects_.emplace(data,Object{std::move(map),std::move(state),TypeLayout::Kind::buffer});
    (void)it;if(!added) throw std::logic_error("duplicate buffer storage");
    ++private_buffers_;pending.data_=nullptr;return data;
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
    if(b.complete || b.owner!=owner || prefix>b.layout->capacity() || !private_buffers_) std::terminate();
    b.valid_prefix=prefix;b.owner=nullptr;b.complete=true;--private_buffers_;
}
void Heap::destroy(void* object,const Object& record) noexcept {
    if(record.kind==TypeLayout::Kind::buffer) {
        const auto& b=*static_cast<const BufferState*>(record.metadata.get());
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
    std::uint64_t size; std::memcpy(&size,p,sizeof(size));
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
void Heap::store(void* object, std::size_t offset, void* value) {
    auto found = objects_.find(object);
    if (found == objects_.end() || found->second.kind!=TypeLayout::Kind::ordinary || (value && !contains(value)) ||
        !found->second.references.at(offset))
        throw std::invalid_argument("invalid managed reference store");
    std::memcpy(static_cast<char*>(object) + offset, &value, sizeof(value));
}
void Heap::collect() { collect({}); }
void Heap::collect(std::span<void* const> additional_roots) {
    std::vector<void*> pending;
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
            if(found->second.kind==TypeLayout::Kind::buffer) continue; // Stage 2C: admitted reference-free layouts only.
            // Map ownership stays in this stable Heap node until marking ends.
            // No map mutation/sweep or callback occurs during this borrowed scan.
            ReferenceCursor cursor(found->second.references);
            while (auto offset=cursor.next_offset()) {
                void* child = nullptr;
                std::memcpy(&child, static_cast<char*>(p) + *offset, sizeof(child));
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
