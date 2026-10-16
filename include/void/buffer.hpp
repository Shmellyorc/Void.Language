#pragma once
#include "void/gc_schema.hpp"
#include <limits>
namespace vd {
class BufferLayout;
using BufferLayoutRef=std::shared_ptr<const BufferLayout>;
// Capacity-specific, immutable, data-only. Only Runtime can bind it to admission.
class BufferLayout {
public:
    BufferLayout(const BufferLayout&)=delete;
    BufferLayout& operator=(const BufferLayout&)=delete;
    const ElementSchemaRef& schema() const {return schema_;}
    std::uint64_t capacity() const {return capacity_;}
    std::size_t base() const {return base_;}
    std::size_t stride() const {return stride_;}
    std::size_t bytes() const {return bytes_;}
    std::size_t alignment() const {return schema_->alignment();}
    std::size_t element_offset(std::uint64_t) const;
    bool equivalent(const BufferLayout& other) const noexcept {
        return schema_==other.schema_ && capacity_==other.capacity_ && base_==other.base_ && stride_==other.stride_ && bytes_==other.bytes_;
    }
private:
    friend class Runtime;
    BufferLayout(ElementSchemaRef,std::uint64_t,std::size_t base,std::size_t stride);
    ElementSchemaRef schema_;
    std::uint64_t capacity_;
    std::size_t base_,stride_,bytes_;
};
struct BufferWorkQuota {std::size_t bytes,reference_events,node_events;};
// Physical construction events, over the SAME admitted nodes/physical field index.
// Runtime-internal: prepare precedes all traversal/reset calls.
// This cursor rejects all reference roles, even inside an empty repetition.
class DataCursor {
public:
    struct Event {std::size_t offset,bytes; bool scalar; ScalarEncoding encoding=ScalarEncoding::u8; bool u8_run=false;};
    void prepare(ElementSchemaRef); // NativeSafe: validates subset, reserves depth.
    void reset() noexcept;
    std::optional<Event> next(std::size_t& node_work);
    bool done() const {return stack_.empty();}
    bool has_data() const;
    bool dense_u8() const;
    std::size_t atomic_bytes() const {return atomic_;}
private:
    struct Frame {std::size_t node,base;std::uint64_t next=0;};
    ElementSchemaRef schema_;
    std::vector<Frame> stack_;
    std::size_t atomic_=1;
};
}
