#pragma once
#include "void/gc_schema.hpp"
#include "void/buffer.hpp"
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
    enum class Kind { ordinary, string, buffer } kind = Kind::ordinary;
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
    friend class Runtime;
    void* allocate_validated(const TypeLayout&, LegacyReferenceMap);
    struct BufferState {
        BufferLayoutRef layout;
        mutable const void* owner;
        mutable std::uint64_t valid_prefix=0;
        mutable bool complete=false;
    };
    class PendingBuffer {
    public:
        PendingBuffer()=default;
        explicit PendingBuffer(BufferLayoutRef);
        ~PendingBuffer();
        PendingBuffer(PendingBuffer&&) noexcept;
        PendingBuffer& operator=(PendingBuffer&&) noexcept;
        PendingBuffer(const PendingBuffer&)=delete;
        PendingBuffer& operator=(const PendingBuffer&)=delete;
    private:
        friend class Heap;
        void* data_=nullptr;
        BufferLayoutRef layout_;
    };
    void* adopt_buffer(PendingBuffer&,std::shared_ptr<const BufferState>);
    const BufferState& buffer_state(void*) const;
    bool is_private(void*) const;
    bool private_owner(void*,const void*) const;
    void seal_buffer(void*,const void*,std::uint64_t) noexcept;
    static void release(void*,std::size_t alignment) noexcept;
    struct Object {
        LegacyReferenceMap references;
        std::shared_ptr<const void> metadata; // kind selects immutable nominal/buffer metadata
        TypeLayout::Kind kind;
        bool marked = false;
    };
    std::unordered_map<void*, Object> objects_;
    std::vector<void**> roots_;
    std::size_t private_buffers_=0;
    void destroy(void*,const Object&) noexcept;
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
