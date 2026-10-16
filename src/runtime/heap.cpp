#include "void/heap.hpp"
#include "void/utf8.hpp"
#include <algorithm>
#include <cstring>
#include <new>
#include <stdexcept>
namespace vd {
Heap::~Heap() { for (auto& [p, o] : objects_) { (void)o; ::operator delete(p); } }
void* Heap::allocate(const TypeLayout& layout) {
    return allocate_validated(layout,ReferenceShape::legacy(layout.size,layout.references));
}
void* Heap::allocate_validated(const TypeLayout& layout,ReferenceShape shape) {
    if (!layout.size || layout.size!=shape.bytes()) throw std::invalid_argument("empty or mismatched object layout");
    if (layout.kind!=TypeLayout::Kind::ordinary && layout.kind!=TypeLayout::Kind::string) throw std::invalid_argument("unknown object layout kind");
    if (layout.kind == TypeLayout::Kind::string &&
        (layout.size <= sizeof(std::uint64_t) || !layout.references.empty() || layout.descriptor))
        throw std::invalid_argument("invalid intrinsic string layout");
    auto* p = ::operator new(layout.size);
    std::memset(p, 0, layout.size);
    try { objects_.emplace(p, Object{layout,std::move(shape)}); }
    catch (...) { ::operator delete(p); throw; }
    return p;
}
bool Heap::contains(void* p) const { return p && objects_.contains(p); }
std::string_view Heap::string_bytes(void* p) const {
    if (!p) return {};
    auto found=objects_.find(p);
    if (found==objects_.end() || found->second.layout.kind!=TypeLayout::Kind::string)
        throw std::invalid_argument("invalid string object");
    std::uint64_t size; std::memcpy(&size,p,sizeof(size));
    if (size!=found->second.layout.size-sizeof(size)) throw std::logic_error("corrupt string extent");
    return {static_cast<const char*>(p)+sizeof(size),static_cast<std::size_t>(size)};
}
std::string_view Heap::nominal_identity(void* p) const {
    auto found=objects_.find(p);
    if(found==objects_.end() || !found->second.layout.descriptor) return {};
    return found->second.layout.descriptor->identity;
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
    if (found == objects_.end() || (value && !contains(value)) ||
        !found->second.references.at(offset))
        throw std::invalid_argument("invalid managed reference store");
    std::memcpy(static_cast<char*>(object) + offset, &value, sizeof(value));
}
void Heap::collect() { collect({}); }
void Heap::collect(std::span<void* const> additional_roots) {
    std::vector<void*> pending;
    for (auto* root : additional_roots) if (root) pending.push_back(root);
    for (auto* root : roots_) if (*root) pending.push_back(*root);
    ReferenceCursor cursor;
    // Validate the entire graph before sweeping. A bad slot cannot silently
    // turn a reachable object into reclaimed memory.
    try {
        while (!pending.empty()) {
            void* p = pending.back(); pending.pop_back();
            auto found = objects_.find(p);
            if (found == objects_.end()) throw std::logic_error("untracked managed reference");
            if (found->second.marked) continue;
            found->second.marked = true;
            cursor.reset(found->second.references);
            while (auto reference=cursor.next()) {
                void* child = nullptr;
                std::memcpy(&child, static_cast<char*>(p) + reference->offset, sizeof(child));
                if (child) pending.push_back(child);
            }
        }
    } catch (...) {
        for (auto& [p, o] : objects_) { (void)p; o.marked = false; }
        throw;
    }
    for (auto it = objects_.begin(); it != objects_.end();) {
        if (!it->second.marked) { ::operator delete(it->first); it = objects_.erase(it); }
        else { it->second.marked = false; ++it; }
    }
}
Root::Root(Heap& heap, void* object) : heap_(heap), value_(object) { heap_.add_root(&value_); }
Root::~Root() { heap_.remove_root(&value_); }
}
