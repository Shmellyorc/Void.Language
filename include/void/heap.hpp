#pragma once
#include "void/gc_schema.hpp"
#include "void/buffer.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <map>
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
    enum class Kind { ordinary, string, buffer, weak_cell } kind = Kind::ordinary;
};
// Runtime-owned data only: no code pointers, LLVM types, or generation leases.
struct NominalDescriptor {
    std::string identity, base, shape;
    TypeLayout layout;
    std::string dictionary_key_schema = {}; // Canonical native element identity, no code.
    bool dictionary_key_forbidden = false; // Derived by the shared IR admission rule.
};
// Immutable binding data: no executable addresses, generation leases or permissions.
struct InterfaceWitness {
    std::string key, interface_identity, view_identity, contract;
};
// Existing transactional Runtime catalog. Buffers retain a data-only snapshot;
// the same service admits descriptors, witnesses and schemas atomically.
struct RuntimeMetadata {
    std::map<std::string,std::shared_ptr<const NominalDescriptor>,std::less<>> types;
    std::map<std::string,ElementSchemaRef,std::less<>> schemas;
    std::map<std::string,std::shared_ptr<const InterfaceWitness>,std::less<>> witnesses;
    // The canonical policy index is built/validated before catalog publication.
    // It replaces the discarded admission set and per-operation nominal scan.
    // Views borrow keys from the immutable descriptors retained in types.
    // Catalog copies retain those same owners before copying this index.
    std::map<std::string_view,bool,std::less<>> dictionary_key_policies;
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
    struct MemorySnapshot {std::size_t objects,payload_bytes,buckets,root_capacity; std::size_t cached_bytes=0,cached_blocks=0;};
    MemorySnapshot memory_snapshot() const; // Host diagnostics; excludes allocator overhead.

    void add_root(void** slot);
    void remove_root(void** slot);
    void store(void* object, std::size_t offset, void* value);
private:
    friend class Runtime;
    void validate_output_address(void**) const;
    class ReuseCache;
    void* allocate_validated(const TypeLayout&, LegacyReferenceMap,ReuseCache* = nullptr);
    struct BufferState {
        BufferLayoutRef layout;
        mutable const void* owner;
        mutable std::uint64_t valid_prefix=0;
        mutable bool complete=false;
        mutable std::shared_ptr<const RuntimeMetadata> catalog;
        std::uint64_t requested_live; // immutable construction boundary, not trace extent
        mutable std::size_t readers=0;
        // Heap-owned scan links; metadata nodes stay stable across table rehash.
        mutable const BufferState* previous=nullptr;
        mutable const BufferState* next=nullptr;
        mutable void* data=nullptr;
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
    void validate_reference(const ReferenceEvent&,void*,const void* witness,
                            const RuntimeMetadata&,bool private_nulls) const;
    bool is_private(void*) const;
    bool private_owner(void*,const void*) const;
    void seal_buffer(void*,const void*,std::uint64_t) noexcept;
    static void release(void*,std::size_t alignment) noexcept;
    struct Object {
        LegacyReferenceMap references;
        std::shared_ptr<const void> metadata; // kind selects immutable nominal/buffer metadata
        TypeLayout::Kind kind;
        bool marked = false, released = false;
    };
    std::unordered_map<void*, Object> objects_;
    std::vector<void**> roots_;
    struct WeakCell { void* target; WeakCell* next; };
    // Dead nodes stay outside the live registry. Runtime places this opaque
    // Heap resource at its tail, preserving all existing hot member offsets.
    class ReuseCache {
        friend class Heap;
        friend class Runtime;
        struct Data;
        std::unique_ptr<Data> data_;
        WeakCell* weak_cells_=nullptr;
    public:
        ReuseCache() noexcept;
        ~ReuseCache();
    };
    bool recycle(std::unordered_map<void*,Object>::iterator,ReuseCache::Data*) noexcept;
    MemorySnapshot memory_snapshot(const ReuseCache*) const;
    std::size_t collect(std::span<void* const>,ReuseCache*);
    std::size_t private_buffers_=0,buffer_objects_=0;
    const BufferState* buffers_=nullptr;
    // Conservative inclusive address bounds, widened on adoption. Stale bounds
    // after collection only cause extra scanning; no per-object index is needed.
    std::uintptr_t buffer_low_=0,buffer_high_=0;
    void clear_weak_cells(WeakCell*&) noexcept;
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
