#pragma once
#include "void/c_dynamic.hpp"
#include "void/ir.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace vd {
// Private linked transport. The trusted host owns the linked code for the
// process lifetime. Project bytes cannot supply a transport address.
extern "C" {
typedef std::uint32_t CPrimitiveThunk(const char*, std::uint32_t, std::uint32_t,
                                     const std::uint64_t*, std::uint32_t, std::uint64_t*);
}
inline constexpr std::string_view c_primitive_transport = "vd_c_primitive_dispatch_v1";
inline constexpr std::string_view c_record_transport = "vd_c_primitive_dispatch_v2";
inline constexpr std::string_view c_resource_transport = "vd_c_primitive_dispatch_v3";
inline constexpr std::string_view c_resource_bridge = "vd_c_resource_dispatch_v3";
inline constexpr std::string_view c_owner_resource_bridge = "vd_c_resource_dispatch_owner_v3";
inline constexpr std::string_view c_resources_bridge = "vd_c_resources_dispatch_v3";
inline constexpr std::string_view c_dynamic_resolve = "vd_c_dynamic_resolve_v1";
inline constexpr std::string_view c_dynamic_call = "vd_c_dynamic_call_v1";
inline constexpr std::string_view c_callback_bridge = "vd_c_callback_dispatch_v1";
struct RuntimeFailure;
extern "C" {
typedef void CCallbackEntry(std::uint64_t, void*, const std::uint64_t*, std::uint64_t*, RuntimeFailure*);
}
extern "C" std::uint32_t vd_c_callback_dispatch_v1(CPrimitiveThunk*, const char*, std::uint32_t,
    std::uint32_t, std::uint64_t*, std::uint32_t, std::uint64_t*, void**, const char*, std::uint32_t,
    std::uint32_t, std::uint64_t, void*, CCallbackEntry*, std::uint32_t, std::uint32_t, RuntimeFailure*) noexcept;
extern "C" CPrimitiveThunk* vd_c_dynamic_resolve_v1(const char*, std::uint32_t) noexcept;
enum class CResourceRole : std::uint32_t { none, acquire, borrow, mutate, close };

extern "C" std::uint32_t vd_c_resource_dispatch_v3(CPrimitiveThunk*, const char*, std::uint32_t,
    std::uint32_t, const std::uint64_t*, std::uint32_t, std::uint64_t*, void**,
    const char*, std::uint32_t, CResourceRole) noexcept;
extern "C" decltype(vd_c_resource_dispatch_v3) vd_c_resource_dispatch_owner_v3;
inline constexpr std::string_view c_span_bridge = "vd_c_span_dispatch_v2";
inline constexpr unsigned c_span_limit = 65536;

struct CSpanRequest {
    void** root;
    std::uint64_t limit;
    std::uint32_t argument, direction; // 1 input, 2 output, 3 input/output
    std::uint32_t offset_argument, length_argument;
};
// Private, rooted borrow operands for mixed conventional calls. Addresses never
// become source values; every operand is checked against the existing ledger.
struct CResourceArgument {
    void** root;
    const char* identity;
    std::uint32_t argument, release, shutdown_owner;
};
extern "C" std::uint32_t vd_c_resources_dispatch_v3(CPrimitiveThunk*, const char*, std::uint32_t,
    std::uint32_t, const std::uint64_t*, std::uint32_t, std::uint64_t*,
    const CResourceArgument*, std::uint32_t, void**, const char*, std::uint32_t, std::uint32_t,
    const CSpanRequest*, std::uint32_t) noexcept;
extern "C" std::uint32_t vd_c_dynamic_call_v1(const char*, std::uint32_t, std::uint32_t,
    const std::uint64_t*, std::uint32_t, std::uint64_t*, const CSpanRequest*, std::uint32_t) noexcept;
extern "C" std::uint32_t vd_c_span_dispatch_v2(CPrimitiveThunk*, const char*, std::uint32_t,
    std::uint32_t, const std::uint64_t*, std::uint32_t, std::uint64_t*,
    const CSpanRequest*, std::uint32_t) noexcept;

inline bool reserved_c_transport(std::string_view symbol) {
    return symbol.starts_with(c_primitive_transport) || symbol.starts_with(c_record_transport) ||
        symbol.starts_with(c_span_bridge) || symbol.starts_with(c_resource_transport) ||
        symbol.starts_with(c_resource_bridge) || symbol.starts_with(c_owner_resource_bridge) || symbol.starts_with(c_dynamic_resolve) ||
        symbol.starts_with(c_resources_bridge) ||
        symbol.starts_with(c_dynamic_call) || symbol.starts_with(c_callback_bridge) ||
        symbol.starts_with("vd_runtime_entry_dynamic_v1");
}
inline constexpr std::uint32_t c_primitive_probe = UINT32_MAX;
inline constexpr unsigned c_primitive_max_arguments = 16;

class CPrimitiveAdmission final {
public:
    struct Shape {
        ir::Type scalar = ir::Type::none;
        enum class Alias { none, c_int, c_long, c_size, c_bool, c_char } alias = Alias::none;
        unsigned record = 0; // 1-based index, zero for primitives
        unsigned extent = 0; // inline C array of scalars only
        unsigned direction = 0; // C byte span only
        unsigned offset_argument = 0, length_argument = 0, limit = 0;
        unsigned resource = 0; // 1-based resource identity
        unsigned callback = 0; // 1-based callback signature
        bool retained = false;
        CResourceRole role = CResourceRole::none;
    };
    struct Callback {
        std::string name;
        Shape result;
        std::vector<Shape> parameters;
    };
    struct Field {
        std::string name, c_name;
        Shape shape;
        unsigned offset;
    };
    struct Record {
        std::string name, c_name;
        unsigned size, alignment;
        std::vector<Field> fields;
    };
    struct Resource {
        std::string name, c_name, acquire, release;
        unsigned release_ordinal = 0;
        bool shutdown_owner = false;
    };
    struct Binding {
        std::string name, export_name;
        std::vector<ir::Type> parameters;
        ir::Type result;
        std::vector<Shape> abi_parameters;
        Shape abi_result;
    };
    using Owner = std::shared_ptr<const CPrimitiveAdmission>;
    CPrimitiveAdmission(const CPrimitiveAdmission&) = delete;
    CPrimitiveAdmission& operator=(const CPrimitiveAdmission&) = delete;

    // Parse independently before asking the trusted C transport to attest its
    // exact descriptor and independently compiled fixture.
    static Owner admit(std::string_view descriptor, CPrimitiveThunk* linked_thunk);
    static Owner admit_dynamic(std::string_view closure, std::filesystem::path host_root);
    static Owner admit_conventional(std::string_view descriptor, std::filesystem::path host_root,
                                    std::string_view library, std::string_view adapter,
                                    const std::vector<DynamicClosure::Payload>& payload = {});
    static std::string generate_adapter(std::string_view descriptor, std::string_view header);
    static std::string generate_declarations(std::string_view descriptor);
    const std::string& descriptor() const { return descriptor_; }
    const std::string& provider() const { return provider_; }
    const std::string& target() const { return target_; }
    const std::string& module() const { return module_; }
    bool physical_source() const { return physical_source_; }
    // Rebase a descriptor-selected physical declaration onto its verified
    // project/package source identity. The native descriptor remains exact.
    Owner at_source_module(std::string identity) const;
    const std::vector<Binding>& bindings() const { return bindings_; }
    const std::vector<Callback>& callbacks() const { return callbacks_; }
    CPrimitiveThunk* thunk() const { return thunk_; }
    const DynamicClosure::Owner& dynamic() const { return dynamic_; }
    const std::filesystem::path& dynamic_root() const { return dynamic_root_; }
    std::string dynamic_contract() const;
    unsigned revision() const { return revision_; }
    unsigned abi_revision() const { return revision_ > 3 ? 3 : revision_; }
    std::string_view transport() const {
        if (revision_ >= 3) return c_resource_transport;
        return revision_ == 1 ? c_primitive_transport : c_record_transport;
    }
    const Resource& resource(unsigned index) const { return resources_.at(index - 1); }
    const std::vector<Resource>& resources() const { return resources_; }
    bool resource_type(ir::Type) const;
    const Record& record(unsigned index) const { return records_.at(index - 1); }
    const std::vector<Record>& records() const { return records_; }
    bool inline_array(ir::Type) const;
    bool matches(const Shape&, ir::Type) const;
    bool signature(const Binding&, const std::vector<ir::Type>&, ir::Type) const;
    static unsigned scalar_bytes(ir::Type);
    std::string call_identity(const Binding& binding) const { return provider_ + "::" + binding.export_name; }
    std::string source() const;
private:
    CPrimitiveAdmission() = default;
    static std::shared_ptr<CPrimitiveAdmission> parse(std::string_view, bool conventional = false);
    std::string descriptor_, provider_, target_, module_;
    unsigned revision_ = 1;
    bool physical_source_ = false;
    std::vector<Binding> bindings_;
    std::vector<Callback> callbacks_;
    std::vector<Record> records_;
    std::vector<Resource> resources_;
    std::string source_type(const Shape&) const;
    CPrimitiveThunk* thunk_ = nullptr;
    std::string thunk_identity_;
    DynamicClosure::Owner dynamic_;
    std::filesystem::path dynamic_root_;
};
}
