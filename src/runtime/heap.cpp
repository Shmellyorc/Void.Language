#include "void/heap.hpp"
#include <algorithm>
#include <cstring>
#include <new>
#include <stdexcept>
namespace vd {
Heap::~Heap() { for (auto& [p, o] : objects_) { (void)o; ::operator delete(p); } }
void* Heap::allocate(const TypeLayout& layout) {
    if (!layout.size) throw std::invalid_argument("empty object layout");
    std::vector<std::size_t> offsets = layout.references;
    std::sort(offsets.begin(), offsets.end());
    for (std::size_t i = 0; i < offsets.size(); ++i)
        if (offsets[i] > layout.size || layout.size - offsets[i] < sizeof(void*) ||
            (i && offsets[i] - offsets[i - 1] < sizeof(void*)))
            throw std::invalid_argument("invalid reference layout");
    auto* p = ::operator new(layout.size);
    std::memset(p, 0, layout.size);
    try { objects_.emplace(p, Object{layout}); }
    catch (...) { ::operator delete(p); throw; }
    return p;
}
bool Heap::contains(void* p) const { return p && objects_.contains(p); }
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
        std::find(found->second.layout.references.begin(), found->second.layout.references.end(), offset) == found->second.layout.references.end())
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
            for (auto offset : found->second.layout.references) {
                void* child = nullptr;
                std::memcpy(&child, static_cast<char*>(p) + offset, sizeof(child));
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
