#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
namespace vd {
class Runtime;
struct NominalDescriptor;
struct InterfaceWitness;
enum class ScalarEncoding { i8,i16,i32,i64,u8,u16,u32,u64,f32,f64,boolean };
struct ScalarStorage { std::size_t bytes, alignment; bool operator==(const ScalarStorage&) const = default; };
struct StorageDomain {
    std::size_t pointer_bytes=0, pointer_alignment=0;
    enum class ByteOrder { little, big } byte_order=ByteOrder::little;
    std::uint32_t revision=1;
    std::array<ScalarStorage,11> scalars{};
    bool operator==(const StorageDomain&) const = default;
    static StorageDomain native();
};
struct SchemaScalar { ScalarEncoding encoding; bool operator==(const SchemaScalar&) const = default; };
struct SchemaBytes { std::size_t bytes, alignment; bool operator==(const SchemaBytes&) const = default; };
struct SchemaManaged { std::string target; bool nullable=false; bool operator==(const SchemaManaged&) const = default; };
struct SchemaString { bool operator==(const SchemaString&) const = default; };
struct SchemaInterface {
    std::string target; bool nullable=false;
    std::size_t object_offset, witness_offset, bytes, alignment;
    bool operator==(const SchemaInterface&) const = default;
};
struct SchemaField { std::uint64_t id; std::size_t offset, child; bool operator==(const SchemaField&) const = default; };
struct SchemaRecord { std::size_t bytes, alignment; std::vector<SchemaField> fields; bool operator==(const SchemaRecord&) const = default; };
struct SchemaRepeat {
    std::size_t base, stride; std::uint64_t extent; std::size_t child, bytes, alignment;
    bool operator==(const SchemaRepeat&) const = default;
};
struct SchemaOwnedFixed { bool operator==(const SchemaOwnedFixed&) const = default; };
struct SchemaGrowable { bool operator==(const SchemaGrowable&) const = default; };
using SchemaNode=std::variant<SchemaScalar,SchemaBytes,SchemaManaged,SchemaString,SchemaInterface,SchemaRecord,SchemaRepeat,SchemaOwnedFixed,SchemaGrowable>;
struct ElementSchemaSpec {
    std::string identity, capability_profile, lowering_layout_key;
    StorageDomain storage_domain;
    std::size_t root=0;
    std::vector<SchemaNode> nodes;
};
struct SchemaStep {
    enum class Kind { field, element } kind;
    std::uint64_t value;
};
struct ReferenceEvent {
    enum class Kind { legacy, managed, string, interface } kind;
    std::size_t offset;
    std::optional<std::size_t> witness_offset;
    bool nullable;
    // Borrowed from the owning ReferenceShape/cursor, not caller input specs.
    std::string_view target;
};
class ElementSchema;
using ElementSchemaRef=std::shared_ptr<const ElementSchema>;
// Immutable, factory-created descriptor; callers cannot construct/copy an admitted object.
class ElementSchema {
public:
    ElementSchema(const ElementSchema&)=delete;
    ElementSchema& operator=(const ElementSchema&)=delete;
    const std::string& identity() const { return spec_.identity; }
    std::size_t bytes() const;
    std::size_t alignment() const;
    std::size_t node_count() const { return spec_.nodes.size(); }
    std::size_t edge_count() const;
    std::size_t depth() const;
    bool has_references() const;
    bool equivalent(const ElementSchema&) const;
private:
    friend class Runtime;
    friend class ReferenceShape;
    friend class ReferenceCursor;
    struct Summary {
        std::size_t bytes=0, alignment=1, depth=1;
        bool references=false;
        std::vector<std::size_t> physical, logical;
    };
    explicit ElementSchema(ElementSchemaSpec,bool legacy=false);
    static std::shared_ptr<ElementSchema> prepare(const ElementSchemaSpec&);
    ElementSchemaSpec spec_;
    std::vector<Summary> summaries_;
    bool legacy_=false;
    std::vector<std::shared_ptr<const NominalDescriptor>> nominal_owners_;
    std::vector<std::shared_ptr<const InterfaceWitness>> witness_owners_;
};
class ReferenceShape {
public:
    explicit ReferenceShape(ElementSchemaRef);
    static ReferenceShape legacy(std::size_t bytes,std::span<const std::size_t> offsets);
    std::vector<std::size_t> legacy_offsets() const;
    std::optional<ReferenceEvent> at(std::size_t byte_offset) const;
    std::optional<ReferenceEvent> path(std::span<const SchemaStep>) const;
    bool has_references() const { return schema_->has_references(); }
    std::size_t bytes() const { return schema_->bytes(); }
private:
    friend class ReferenceCursor;
    ElementSchemaRef schema_;
};
class ReferenceCursor {
public:
    ReferenceCursor() = default;
    explicit ReferenceCursor(ReferenceShape);
    void reset(ReferenceShape);
    std::optional<ReferenceEvent> next();
private:
    struct Frame { std::size_t node, base; std::uint64_t next=0; };
    ElementSchemaRef schema_;
    std::vector<Frame> stack_;
};
}
