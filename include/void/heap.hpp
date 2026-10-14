#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>
#include <string>
#include <string_view>
namespace vd {
// Single-threaded proof. Type descriptors list every pointer field, including
// references nested inside inline values. Objects contain zeroed payload bytes.
struct NominalDescriptor;
struct TypeLayout {
    std::size_t size;
    std::vector<std::size_t> references;
    std::shared_ptr<const NominalDescriptor> descriptor = {};
    enum class Kind { ordinary, string } kind = Kind::ordinary;
};
// Runtime-owned data only: no code pointers, LLVM types, or generation leases.
struct NominalDescriptor {
    std::string identity, base, shape;
    TypeLayout layout;
};
class Heap {
public:
    Heap() = default;
    Heap(const Heap&) = delete;
    Heap& operator=(const Heap&) = delete;
    ~Heap();
    void* allocate(const TypeLayout&);
    void collect();
    void collect(std::span<void* const> additional_roots);
    bool contains(void* object) const;
    std::string_view nominal_identity(void* object) const;
    std::string_view string_bytes(void* object) const;
    std::size_t size() const { return objects_.size(); }
    void add_root(void** slot);
    void remove_root(void** slot);
    void store(void* object, std::size_t offset, void* value);
private:
    struct Object { TypeLayout layout; bool marked = false; };
    std::unordered_map<void*, Object> objects_;
    std::vector<void**> roots_;
};
class Root {
public:
    Root(Heap& heap, void* object = nullptr);
    ~Root();
    Root(const Root&) = delete;
    Root& operator=(const Root&) = delete;
    void* get() const { return value_; }
    void reset(void* value) { value_ = value; }
private:
    Heap& heap_;
    void* value_;
};
}
