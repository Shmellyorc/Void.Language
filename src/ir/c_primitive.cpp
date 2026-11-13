#include "void/c_primitive.hpp"
#include "void/path.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <initializer_list>
#include <set>
#include <span>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace vd {
namespace {
using Type = ir::Type;
constexpr std::size_t descriptor_limit = 16384;
constexpr std::size_t binding_limit = 64;

[[noreturn]] void reject(std::size_t line, const char* why) {
    throw std::invalid_argument("C primitive descriptor line " + std::to_string(line) + ": " + why);
}
bool lower(char c) { return c >= 'a' && c <= 'z'; }
bool digit(char c) { return c >= '0' && c <= '9'; }
bool identifier(std::string_view name, bool export_name = false) {
    auto letter = [=](char c) { return lower(c) || (export_name && c >= 'A' && c <= 'Z'); };
    return !name.empty() && name.size() <= 64 && (letter(name.front()) || (export_name && name.front() == '_')) &&
        std::all_of(name.begin() + 1, name.end(), [&](char c) { return letter(c) || digit(c) || c == '_'; });
}
bool package(std::string_view name) {
    if (name.empty() || name.size() > 96 || !lower(name.front()) || name.back() == '.' || name.find("..") != name.npos)
        return false;
    return name.find('.') != name.npos && std::all_of(name.begin(), name.end(), [](char c) {
        return lower(c) || digit(c) || c == '.' || c == '-';
    });
}
bool digest(std::string_view text) {
    return text.size() == 64 && std::all_of(text.begin(), text.end(), [](char c) {
        return digit(c) || (c >= 'a' && c <= 'f');
    });
}
class DescriptorReader {
    std::string_view input_;
public:
    std::size_t line = 0;
    explicit DescriptorReader(std::string_view input) : input_(input) {}
    bool done() const { return input_.empty(); }
    std::vector<std::string_view> take() {
        ++line;
        const auto end = input_.find('\n');
        if (end == input_.npos || end > 1024) reject(line, "expected a bounded LF-terminated record");
        auto record = input_.substr(0, end);
        input_.remove_prefix(end + 1);
        std::vector<std::string_view> fields;
        while (true) {
            const auto separator = record.find(' ');
            auto token = record.substr(0, separator);
            if (token.empty() || !std::all_of(token.begin(), token.end(), [](char c) { return c >= '!' && c <= '~'; }))
                reject(line, "expected canonical ASCII fields separated by a single space");
            fields.push_back(token);
            if (separator == record.npos) return fields;
            record.remove_prefix(separator + 1);
        }
    }
    std::string_view value(std::string_view key) {
        auto fields = take();
        if (fields.size() != 2 || fields[0] != key) reject(line, "unexpected descriptor field or field count");
        return fields[1];
    }
    void exact(std::initializer_list<std::string_view> expected) {
        if (!std::ranges::equal(take(), expected)) reject(line, "unsupported descriptor, ABI revision or effects");
    }
};

constexpr std::array primitive_types{
    std::pair{"i8", Type::i8}, std::pair{"i16", Type::i16}, std::pair{"i32", Type::i32},
    std::pair{"i64", Type::i64}, std::pair{"u8", Type::u8}, std::pair{"u16", Type::u16},
    std::pair{"u32", Type::u32}, std::pair{"u64", Type::u64}, std::pair{"f32", Type::f32},
    std::pair{"f64", Type::f64}, std::pair{"bool", Type::boolean},
};
Type primitive(std::string_view name, bool llp64, bool result, std::size_t line) {
    for (auto [spelling, kind] : primitive_types)
        if (name == spelling && kind != Type::boolean) return kind;
    if (name == "c_int") return Type::i32;
    if (name == "c_long") return llp64 ? Type::i32 : Type::i64;
    if (name == "c_size") return Type::u64;
    if (name == "c_bool") return Type::boolean;
    if (name == "c_char") return Type::i8;
    if (name == "void" && result) return Type::none;
    reject(line, "unsupported C primitive type");
}
std::string_view void_spelling(Type type) {
    for (auto [name, kind] : primitive_types)
        if (type.kind == kind) return name;
    throw std::logic_error("C primitive spelling invariant");
}
}

std::shared_ptr<CPrimitiveAdmission> CPrimitiveAdmission::parse(std::string_view descriptor, bool conventional) {
    if (descriptor.empty() || descriptor.size() > (conventional ? c_conventional_descriptor_limit : descriptor_limit))
        reject(1, "descriptor size outside admitted profile bounds");
    auto admitted = std::shared_ptr<CPrimitiveAdmission>(new CPrimitiveAdmission);
    admitted->descriptor_ = descriptor;
    DescriptorReader reader(admitted->descriptor_);
    const auto revision = reader.take();
    if (conventional && revision == std::vector<std::string_view>{"void-c-primitive", "4", "3"}) admitted->revision_ = 4;
    else if (revision == std::vector<std::string_view>{"void-c-primitive", "3", "3"}) admitted->revision_ = 3;
    else if (revision == std::vector<std::string_view>{"void-c-primitive", "2", "2"}) admitted->revision_ = 2;
    else if (revision != std::vector<std::string_view>{"void-c-primitive", "1", "1"})
        reject(reader.line, "unsupported descriptor, ABI revision or effects");
    auto provider = reader.value("provider");
    if (!package(provider)) reject(reader.line, "invalid provider identity");
    admitted->provider_ = provider;

    auto target = reader.take();
    if (target.size() != 3 || target[0] != "target") reject(reader.line, "expected a target triple and C ABI profile");
    const bool llp64 = target[1] == "x86_64-pc-windows-msvc" && target[2] == "msvc-x64";
    if (!llp64 && (target[1] != "x86_64-unknown-linux-gnu" || target[2] != "sysv-x64"))
        reject(reader.line, "unsupported target/profile combination");
    admitted->target_ = target[1];

    const auto source = reader.take();
    if(source.size()!=2 || (source[0]!="source" && source[0]!="source-file"))
        reject(reader.line,"expected source or source-file declaration");
    admitted->physical_source_ = source[0]=="source-file";
    auto module = source[1];
    if (!module.starts_with('/') || !module.ends_with(".void") || module.size() <= 6)
        reject(reader.line,"expected a canonical VOID source identity");
    auto path=module.substr(1,module.size()-6);
    while(true) {
        const auto slash=path.find('/');
        if(!identifier(path.substr(0,slash)))reject(reader.line,"invalid declaration source path");
        if(slash==path.npos)break;
        if(!admitted->physical_source_)reject(reader.line,"expected a canonical single-file VFS identity");
        path.remove_prefix(slash+1);
    }
    admitted->module_ = module;
    admitted->thunk_identity_ = reader.value("thunk");
    if (!digest(admitted->thunk_identity_)) reject(reader.line, "invalid thunk SHA-256 identity");
    if (!digest(reader.value("fixture"))) reject(reader.line, "invalid fixture SHA-256 identity");
    const auto effects = reader.take();
    const std::initializer_list<std::string_view> nonblocking = {"effects", "direct-sync", "nonblocking", "no-callbacks"};
    const std::initializer_list<std::string_view> blocking = {"effects", "direct-sync", "blocking", "no-callbacks"};
    const std::initializer_list<std::string_view> checked = {"effects", "direct-sync", "blocking", "checked-callbacks"};
    const bool callbacks = conventional && admitted->revision_ >= 2 && std::ranges::equal(effects, checked);
    if (!callbacks && !std::ranges::equal(effects, nonblocking) && !(conventional && std::ranges::equal(effects, blocking)))
        reject(reader.line, "unsupported descriptor, ABI revision or effects");
    reader.exact({"call", "c-default"});
    if (admitted->revision_ >= 4)
        reader.exact({"aliases", "c_int:i32", llp64 ? "c_long:i32" : "c_long:i64", "c_size:u64", "c_bool:bool", "c_char:i8"});
    else reader.exact({"aliases", "c_int:i32", llp64 ? "c_long:i32" : "c_long:i64", "c_size:u64", "c_bool:bool"});
    auto number = [&](std::string_view value, unsigned limit) {
        unsigned result = 0;
        const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
        if (error != std::errc{} || end != value.data() + value.size() ||
            (value.size() > 1 && value.front() == '0') || result > limit)
            reject(reader.line, "invalid bounded canonical decimal field");
        return result;
    };
    auto parts = [](std::string_view token) {
        std::vector<std::string_view> result;
        while (true) {
            const auto colon = token.find(':');
            result.push_back(token.substr(0, colon));
            if (colon == token.npos) return result;
            token.remove_prefix(colon + 1);
        }
    };
    auto shape = [&](std::string_view token, bool field, bool result) {
        Shape type;
        auto scalar = [&](std::string_view spelling) {
            type.scalar = primitive(spelling, llp64, result, reader.line);
            if (spelling == "c_int") type.alias = Shape::Alias::c_int;
            if (spelling == "c_long") type.alias = Shape::Alias::c_long;
            if (spelling == "c_size") type.alias = Shape::Alias::c_size;
            if (spelling == "c_bool") type.alias = Shape::Alias::c_bool;
            if (spelling == "c_char") {
                if (admitted->revision_ < 4) reject(reader.line, "C char requires conventional descriptor revision 4");
                type.alias = Shape::Alias::c_char;
            }
        };
        if (admitted->revision_ >= 2) {
            const auto p = parts(token);
            if (callbacks && !field && !result && p.size() == 2 &&
                (p[0] == "scoped" || p[0] == "retained")) {
                for (unsigned n = 0; n < admitted->callbacks_.size(); ++n)
                    if (admitted->callbacks_[n].name == p[1]) {
                        type.callback = n + 1;
                        type.retained = p[0] == "retained";
                        return type;
                    }
                reject(reader.line, "callback parameter requires an earlier admitted signature");
            }
            if (admitted->revision_ >= 3 && !field && p.size() == 2) {
                if (result && p[0] == "owner") type.role = CResourceRole::acquire;
                if (!result && p[0] == "borrow") type.role = CResourceRole::borrow;
                if (!result && p[0] == "mut") type.role = CResourceRole::mutate;
                if (!result && p[0] == "close") type.role = CResourceRole::close;
                if (type.role != CResourceRole::none) {
                    for (unsigned n = 0; n < admitted->resources_.size(); ++n)
                        if (admitted->resources_[n].name == p[1]) { type.resource = n + 1; return type; }
                    reject(reader.line, "resource operation requires an earlier admitted identity");
                }
            }
            if (p.size() == 2 && p[0] == "record") {
                for (unsigned n = 0; n < admitted->records_.size(); ++n)
                    if (admitted->records_[n].name == p[1]) { type.record = n + 1; return type; }
                reject(reader.line, "record must name an earlier verified declaration");
            }
            if (field && p.size() == 4 && p[0] == "array") {
                scalar(p[1]);
                type.extent = number(p[2], 32);
                const auto stride = number(p[3], 8);
                if (!type.extent || stride != scalar_bytes(type.scalar))
                    reject(reader.line, "invalid inline array extent or stride");
                return type;
            }
            if (admitted->revision_ >= 4 && field && p.size() == 4 && p[0] == "array_record") {
                for (unsigned n = 0; n < admitted->records_.size(); ++n)
                    if (admitted->records_[n].name == p[1]) { type.record = n + 1; break; }
                if (!type.record) reject(reader.line, "array record must name an earlier verified declaration");
                type.extent = number(p[2], 32);
                if (!type.extent || number(p[3], 4096) != admitted->record(type.record).size)
                    reject(reader.line, "invalid inline record array extent or stride");
                return type;
            }
            if (!field && !result && p.size() == 4) {
                if (p[0] == "in") type.direction = 1;
                if (p[0] == "out") type.direction = 2;
                if (p[0] == "inout") type.direction = 3;
                if (type.direction) {
                    type.offset_argument = number(p[1], c_primitive_max_arguments - 1);
                    type.length_argument = number(p[2], c_primitive_max_arguments - 1);
                    type.limit = number(p[3], c_span_limit);
                    if (!type.limit || type.offset_argument == type.length_argument)
                        reject(reader.line, "invalid byte span offset/length contract");
                    return type;
                }
            }
        }
        scalar(token);
        return type;
    };
    std::set<std::string_view> names, exports, c_records;
    std::set<unsigned> used_callbacks;
    while (!reader.done()) {
        const auto fields = reader.take();
        if (callbacks && fields[0] == "callback") {
            if (fields.size() < 4 || fields.size() > 4 + c_primitive_max_arguments ||
                !admitted->bindings_.empty() || admitted->callbacks_.size() == 16 ||
                !identifier(fields[1]) || !names.insert(fields[1]).second || fields[2] != "context-first")
                reject(reader.line, "expected a fixed C callback signature with context-first");
            Callback callback{std::string(fields[1]), shape(fields[3], true, true), {}};
            if (callback.result.record || callback.result.extent)
                reject(reader.line, "callback result must be a scalar or void");
            for (auto parameter : std::span(fields).subspan(4)) {
                auto type = shape(parameter, true, false);
                if (type.record || type.extent) reject(reader.line, "callback parameters must be scalars");
                callback.parameters.push_back(type);
            }
            admitted->callbacks_.push_back(std::move(callback));
            continue;
        }
        if (admitted->revision_ >= 3 && fields[0] == "resource") {
            if (fields.size() != 10 || !admitted->bindings_.empty() || !admitted->records_.empty() ||
                admitted->resources_.size() == (admitted->revision_ >= 4 ? 64 : 16) || !identifier(fields[1]) || !identifier(fields[2], true) ||
                !names.insert(fields[1]).second || !c_records.insert(fields[2]).second ||
                fields[3] != "pointer" || fields[4] != "null" ||
                !identifier(fields[5]) || !identifier(fields[6]) || fields[5] == fields[6] ||
                fields[7] != "owner-calls" || (fields[8] != "shutdown-any" && !(conventional && fields[8] == "shutdown-owner")) ||
                fields[9] != "consume-always")
                reject(reader.line, "unsupported or contradictory native resource contract");
            admitted->resources_.push_back({std::string(fields[1]), std::string(fields[2]),
                std::string(fields[5]), std::string(fields[6]), 0, fields[8] == "shutdown-owner"});
            continue;
        }
        if (admitted->revision_ >= 2 && !fields.empty() && fields[0] == "record") {
            if (fields.size() != 6 || !admitted->bindings_.empty() || admitted->records_.size() == (admitted->revision_ >= 4 ? 64 : 16) ||
                !identifier(fields[1]) || !identifier(fields[2], true) ||
                !names.insert(fields[1]).second || !c_records.insert(fields[2]).second)
                reject(reader.line, "invalid record declaration or identity");
            const unsigned record_limit = admitted->revision_ >= 4 ? 4096 : 256;
            Record record{std::string(fields[1]), std::string(fields[2]), number(fields[3], record_limit),
                number(fields[4], 8), {}};
            const auto field_count = number(fields[5], admitted->revision_ >= 4 ? 64 : 24);
            if (!field_count) reject(reader.line, "empty C records are unsupported");
            std::set<std::string_view> members, c_members;
            unsigned end = 0, alignment = 1;
            for (unsigned index = 0; index < field_count; ++index) {
                const auto f = reader.take();
                if (f.size() != 5 || f[0] != "field" || !identifier(f[1]) || !identifier(f[2], true) ||
                    !members.insert(f[1]).second || !c_members.insert(f[2]).second)
                    reject(reader.line, "invalid or duplicate C record field");
                auto type = shape(f[3], true, false);
                const auto field_align = type.record ? admitted->record(type.record).alignment : scalar_bytes(type.scalar);
                const auto bytes = (type.record ? admitted->record(type.record).size : field_align) * std::max(type.extent, 1u);
                const auto natural = (end + field_align - 1) / field_align * field_align;
                const auto offset = number(f[4], record_limit - 1);
                if (offset != natural || bytes > record_limit - offset)
                    reject(reader.line, "C record field offset or padding disagrees with natural layout");
                alignment = std::max(alignment, field_align);
                end = offset + bytes;
                record.fields.push_back({std::string(f[1]), std::string(f[2]), type, offset});
            }
            if (record.alignment != alignment || record.size != (end + alignment - 1) / alignment * alignment)
                reject(reader.line, "C record size, alignment or tail padding disagrees with natural layout");
            admitted->records_.push_back(std::move(record));
            continue;
        }
        if (fields.size() < 4 || fields.size() > 4 + c_primitive_max_arguments || fields[0] != "bind" ||
            admitted->bindings_.size() == (conventional ? c_conventional_binding_limit : binding_limit))
            reject(reader.line, "expected a bounded primitive binding record");
        if (!identifier(fields[1]) || !identifier(fields[2], true) ||
            !names.insert(fields[1]).second || !exports.insert(fields[2]).second)
            reject(reader.line, "invalid or duplicate binding name or export");
        Binding binding;
        binding.name = fields[1];
        binding.export_name = fields[2];
        binding.abi_result = shape(fields[3], false, true);
        binding.result = binding.abi_result.scalar;
        for (auto parameter : std::span(fields).subspan(4)) {
            auto type = shape(parameter, false, false);
            binding.parameters.push_back(type.scalar);
            binding.abi_parameters.push_back(type);
        }
        unsigned spans = 0, writable = 0;
        std::set<unsigned> controls;
        bool records = binding.abi_result.record != 0;
        for (const auto& parameter : binding.abi_parameters) {
            records |= parameter.record != 0;
            if (!parameter.direction) continue;
            ++spans;
            writable += parameter.direction != 1;
            for (auto index : {parameter.offset_argument, parameter.length_argument})
                if (index >= binding.abi_parameters.size() || fields[4 + index] != "c_size" || !controls.insert(index).second)
                    reject(reader.line, "byte span offsets and lengths require exact c_size operands");
        }
        if (spans && (spans > 2 || writable > 1 ||
            (admitted->revision_ < 4 && (records || binding.result != Type::i64)) ||
            (writable && binding.result != Type::i64)))
            reject(reader.line, "unsupported span count, overlap profile, record or result contract");
        unsigned resources = binding.abi_result.resource != 0;
        for (unsigned n = 0; n < binding.abi_parameters.size(); ++n) {
            const auto& argument = binding.abi_parameters[n];
            if (!argument.resource) continue;
            ++resources;
            if ((n && admitted->revision_ < 4) || (argument.role == CResourceRole::close &&
                (binding.abi_parameters.size() != 1 || binding.result != Type::i32 ||
                 binding.name != admitted->resource(argument.resource).release)))
                reject(reader.line, "invalid native borrow or terminal release signature");
        }
        if (admitted->revision_ < 4 && (resources > 1 || (resources && (records || spans)) ||
            (binding.abi_result.resource && binding.name != admitted->resource(binding.abi_result.resource).acquire)))
            reject(reader.line, "invalid native ownership transfer or mixed ABI");
        for (unsigned n = 0; n < binding.abi_parameters.size(); ++n) {
            const auto& parameter = binding.abi_parameters[n];
            if (!parameter.callback) continue;
            if (n + 1 != binding.abi_parameters.size() || records || spans ||
                (parameter.retained ? !binding.abi_result.resource || resources != 1 : resources != 0))
                reject(reader.line, "callback must be last, with scalar transport or retained resource acquisition");
            used_callbacks.insert(parameter.callback);
        }
        admitted->bindings_.push_back(std::move(binding));
    }
    if (callbacks && (used_callbacks.empty() || used_callbacks.size() != admitted->callbacks_.size()))
        reject(reader.line, "each checked callback declaration must be used by a binding");
    if (admitted->bindings_.empty()) reject(reader.line, "expected at least one binding");
    // Revision 4 also admits value-only records/arrays without inventing an owner.
    if (admitted->revision_ == 3 && admitted->resources_.empty())
        reject(reader.line, "expected a resource contract in descriptor revision 3");
    for (unsigned n = 0; n < admitted->resources_.size(); ++n) {
        auto& resource = admitted->resources_[n];
        bool acquired = false, released = false;
        for (unsigned ordinal = 0; ordinal < admitted->bindings_.size(); ++ordinal) {
            const auto& binding = admitted->bindings_[ordinal];
            if (binding.name == resource.acquire)
                acquired = binding.abi_result.resource == n + 1 && binding.abi_result.role == CResourceRole::acquire;
            if (binding.name == resource.release && binding.abi_parameters.size() == 1 &&
                binding.abi_parameters[0].resource == n + 1 && binding.abi_parameters[0].role == CResourceRole::close) {
                resource.release_ordinal = ordinal;
                released = true;
            }
        }
        if (!acquired || !released) reject(reader.line, "missing exact native acquire or release contract");
    }
    return admitted;
}

CPrimitiveAdmission::Owner CPrimitiveAdmission::admit(std::string_view descriptor, CPrimitiveThunk* linked_thunk) {
    auto admitted = parse(descriptor);
    if (!linked_thunk) throw std::invalid_argument("C primitive provider transport is unavailable");
    if (linked_thunk(admitted->descriptor_.data(), static_cast<std::uint32_t>(admitted->descriptor_.size()),
                     c_primitive_probe, nullptr, 0, nullptr))
        throw std::invalid_argument("C primitive provider rejected the exact descriptor or fixture identity");
    admitted->thunk_ = linked_thunk;
    return admitted;
}

CPrimitiveAdmission::Owner CPrimitiveAdmission::admit_dynamic(std::string_view closure, std::filesystem::path root) {
    auto plan = DynamicClosure::parse(closure);
    auto admitted = parse(plan->descriptor(), plan->conventional());
    if (admitted->target() != plan->target() || admitted->transport() != plan->dispatch() ||
        admitted->provider() != plan->provider() || admitted->thunk_identity_ != plan->thunk_identity() ||
        !root.is_absolute() || root != root.lexically_normal())
        throw std::invalid_argument("C dynamic closure does not match the exact descriptor or host root");
    for (const auto& binding : admitted->bindings())
        if (std::none_of(plan->exports().begin(), plan->exports().end(), [&](const auto& e) {
            return (plan->conventional() || e.image == plan->images().back().name) && e.symbol == binding.export_name;
        })) throw std::invalid_argument(plan->conventional() ?
            "C dynamic closure is missing a required descriptor export: " + binding.export_name :
            "C dynamic closure is missing an owned descriptor export");
    admitted->dynamic_ = std::move(plan);
    admitted->dynamic_root_ = std::move(root);
    return admitted;
}

CPrimitiveAdmission::Owner CPrimitiveAdmission::admit_conventional(std::string_view descriptor,
    std::filesystem::path root, std::string_view library, std::string_view adapter,
    const std::vector<DynamicClosure::Payload>& payload) {
    const auto admitted = parse(descriptor, true);
    std::string plan = std::string(payload.empty() ? "void-native-conventional 1" : "void-native-managed 1") +
        "\nprovider " + admitted->provider_ +
        "\ntarget " + admitted->target_ +
        (admitted->target_ == "x86_64-pc-windows-msvc" ? " msvc-x64" : " sysv-x64") +
        "\nsystem os\nlayout executable-sibling-native\nthunk " + admitted->thunk_identity_ + ' ';
    plan += admitted->transport();
    plan += "\nlibraries ";
    plan += library;
    plan += ' ';
    plan += adapter;
    plan += '\n';
    for (const auto& file : payload)
        plan += "payload " + file.name + ' ' + file.sha256 + ' ' + std::to_string(file.size) + '\n';
    for (const auto& binding : admitted->bindings_) {
        plan += "export ";
        plan += library;
        plan += ' ' + binding.export_name + '\n';
    }
    plan += "descriptor\n";
    plan += descriptor;
    return admit_dynamic(plan, std::move(root));
}

std::string CPrimitiveAdmission::generate_adapter(std::string_view descriptor, std::string_view header) {
    const auto admitted = parse(descriptor, true);
    if (header.empty() || std::any_of(header.begin(), header.end(), [](char c) {
        return c < ' ' || c > '~' || c == '"' || c == '\\';
    })) throw std::invalid_argument("C adapter requires an include filename with forward slashes");

    using Shape = CPrimitiveAdmission::Shape;
    auto c_type = [&](const Shape& shape) -> std::string {
        if (shape.record) return admitted->record(shape.record).c_name;
        if (shape.resource)
            return (shape.role == CResourceRole::borrow ? "const " : "") +
                admitted->resource(shape.resource).c_name + "*";
        if (shape.direction) return shape.direction == 1 ? "const uint8_t*" : "uint8_t*";
        switch (shape.alias) {
            case Shape::Alias::c_int: return "int";
            case Shape::Alias::c_long: return "long";
            case Shape::Alias::c_size: return "size_t";
            case Shape::Alias::c_bool: return "_Bool";
            case Shape::Alias::c_char: return "char";
            case Shape::Alias::none: break;
        }
        switch (shape.scalar.kind) {
            case Type::none: return "void";
            case Type::f32: return "float";
            case Type::f64: return "double";
            default:
                const auto name = void_spelling(shape.scalar);
                return (name.front() == 'u' ? "uint" : "int") + std::string(name.substr(1)) + "_t";
        }
    };
    auto word_type = [](const Shape& shape) {
        return "uint" + std::to_string(8 * scalar_bytes(shape.scalar)) + "_t";
    };
    std::ostringstream out;
    out << "/* Generated by voidc native-bind. Compile as C11 with the provider header. */\n"
           "#include <stdint.h>\n#include <stddef.h>\n#include <limits.h>\n#include <string.h>\n"
           "#include \"" << header << "\"\n";
    const bool windows = admitted->target_ == "x86_64-pc-windows-msvc";
    out << (windows ? "#if !defined(_WIN32) || !defined(_M_X64)\n" : "#if !defined(__linux__) || !defined(__x86_64__)\n")
        << "#error Wrong C adapter target\n#endif\n"
        << "_Static_assert(CHAR_BIT == 8 && sizeof(void*) == 8 && sizeof(int) == 4 && sizeof(long) == "
        << (windows ? 4 : 8) << " && sizeof(size_t) == 8 && sizeof(float) == 4 && sizeof(double) == 8, \"C ABI widths\");\n";
    if (admitted->revision_ >= 4)
        out << "_Static_assert(CHAR_MIN == -128 && CHAR_MAX == 127, \"C char signedness\");\n";
    if (!admitted->callbacks_.empty())
        out << "struct vd_callback_context {\n"
               "    uint32_t (*invoke)(void*, const uint64_t*, uint32_t, uint64_t*);\n"
               "    void* owner;\n"
               "};\n";
    for (const auto& callback : admitted->callbacks_) {
        const auto result = c_type(callback.result);
        std::string parameters = "void*";
        std::string arguments = "void* opaque";
        for (unsigned i = 0; i < callback.parameters.size(); ++i) {
            const auto parameter = c_type(callback.parameters[i]);
            parameters += ", " + parameter;
            arguments += ", " + parameter + " arg" + std::to_string(i);
        }
        out << "typedef " << result << " (*vd_cb_" << callback.name << ")(" << parameters << ");\n"
            << "static " << result << " vd_enter_" << callback.name << '(' << arguments << ") {\n"
            << "    struct vd_callback_context* context = opaque;\n";
        if (!callback.parameters.empty())
            out << "    uint64_t words[" << callback.parameters.size() << "];\n";
        for (unsigned i = 0; i < callback.parameters.size(); ++i) {
            const auto& parameter = callback.parameters[i];
            if (parameter.scalar == Type::boolean) out << "    words[" << i << "] = arg" << i << ";\n";
            else out << "    " << word_type(parameter) << " bits" << i << ";\n"
                     << "    memcpy(&bits" << i << ", &arg" << i << ", sizeof bits" << i << ");\n"
                     << "    words[" << i << "] = bits" << i << ";\n";
        }
        const bool returns = callback.result.scalar != Type::none;
        if (returns) out << "    uint64_t value = 0;\n";
        out << "    " << (returns ? "if (" : "(void)") << "context->invoke(opaque, "
            << (callback.parameters.empty() ? "NULL" : "words") << ", " << callback.parameters.size()
            << ", " << (returns ? "&value)) return 0;\n" : "NULL);\n");
        if (callback.result.scalar == Type::boolean) out << "    return value != 0;\n";
        else if (returns)
            out << "    " << word_type(callback.result) << " bits = (" << word_type(callback.result) << ")value;\n"
                << "    " << result << " decoded;\n"
                << "    memcpy(&decoded, &bits, sizeof decoded);\n    return decoded;\n";
        out << "}\n";
    }
    for (const auto& record : admitted->records_) {
        out << "_Static_assert(sizeof(" << record.c_name << ") == " << record.size
            << " && _Alignof(" << record.c_name << ") == " << record.alignment
            << ", \"C layout " << record.c_name << "\");\n";
        for (const auto& field : record.fields) {
            out << "_Static_assert(offsetof(" << record.c_name << ", " << field.c_name << ") == " << field.offset
                << " && _Generic(&((" << record.c_name << "*)0)->" << field.c_name << ", " << c_type(field.shape);
            if (field.shape.extent) out << " (*)[" << field.shape.extent << ']';
            else out << '*';
            out << ": 1, default: 0), \"C field " << record.c_name << '.' << field.c_name << "\");\n";
        }
    }
    for (const auto& binding : admitted->bindings_) {
        out << "_Static_assert(_Generic(&" << binding.export_name << ", " << c_type(binding.abi_result) << " (*)(";
        if (binding.abi_parameters.empty()) out << "void";
        for (std::size_t i = 0; i < binding.abi_parameters.size(); ++i) {
            if (i) out << ", ";
            const auto& parameter = binding.abi_parameters[i];
            if (parameter.callback) out << "vd_cb_" << admitted->callbacks_[parameter.callback - 1].name << ", void*";
            else out << c_type(parameter);
        }
        out << "): 1, default: 0), \"C signature " << binding.export_name << "\");\n";
    }
    out << "static const char vd_descriptor[] =\n    \"";
    for (const auto c : descriptor) {
        if (c == '\n') out << "\\n\"\n    \"";
        else {
            if (c == '"' || c == '\\' || c == '?') out << '\\';
            out << c;
        }
    }
    out << "\";\n" << (windows ? "__declspec(dllexport)" : "__attribute__((visibility(\"default\")))")
        << "\nuint32_t " << admitted->transport() << "(const char* descriptor, uint32_t size, uint32_t ordinal,\n"
           "    const uint64_t* words, uint32_t count, uint64_t* result) {\n"
           "    if (!descriptor || size != sizeof(vd_descriptor) - 1 || memcmp(descriptor, vd_descriptor, size)) return 1;\n"
           "    if (ordinal == UINT32_MAX) return words || count || result ? 1 : 0;\n"
           "    if (count && !words) return 2;\n"
           "    switch (ordinal) {\n";
    for (std::size_t ordinal = 0; ordinal < admitted->bindings_.size(); ++ordinal) {
        const auto& binding = admitted->bindings_[ordinal];
        const auto& result = binding.abi_result;
        const bool returns = result.scalar != Type::none || result.record || result.resource;
        out << "    case " << ordinal << ": {\n        if (count != " << binding.abi_parameters.size()
            << (returns ? " || !result" : " || result") << ") return 2;\n";
        for (std::size_t i = 0; i < binding.abi_parameters.size(); ++i) {
            const auto& shape = binding.abi_parameters[i];
            const auto word = "words[" + std::to_string(i) + "]";
            const auto argument = "arg" + std::to_string(i);
            if (shape.callback) {
                out << "        if (!" << word << ") return 2;\n"
                    << "        void* " << argument << " = (void*)(uintptr_t)" << word << ";\n";
                continue;
            }
            const auto type = c_type(shape);
            if (shape.record || shape.resource) out << "        if (!" << word << ") return 2;\n";
            out << "        " << type << ' ' << argument;
            if (shape.resource || shape.direction) out << " = (" << type << ")(uintptr_t)" << word << ";\n";
            else if (shape.scalar == Type::boolean) out << " = " << word << " != 0;\n";
            else {
                out << ";\n";
                if (shape.record) out << "        memcpy(&" << argument << ", (const void*)(uintptr_t)" << word << ", sizeof " << argument << ");\n";
                else out << "        " << word_type(shape) << " bits" << i << " = (" << word_type(shape) << ")" << word
                         << ";\n        memcpy(&" << argument << ", &bits" << i << ", sizeof " << argument << ");\n";
            }
        }
        out << "        ";
        if (returns) out << c_type(result) << " value = ";
        out << binding.export_name << '(';
        for (std::size_t i = 0; i < binding.abi_parameters.size(); ++i) {
            if (i) out << ", ";
            if (auto callback = binding.abi_parameters[i].callback)
                out << "vd_enter_" << admitted->callbacks_[callback - 1].name << ", ";
            out << "arg" << i;
        }
        out << ");\n";
        if (result.record) out << "        memcpy(result, &value, sizeof value);\n";
        else if (result.resource) out << "        *result = (uintptr_t)value;\n";
        else if (result.scalar == Type::boolean) out << "        *result = value;\n";
        else if (returns) out << "        " << word_type(result)
            << " bits;\n        memcpy(&bits, &value, sizeof bits);\n        *result = bits;\n";
        out << "        return 0;\n    }\n";
    }
    out << "    default: return 2;\n    }\n}\n";
    return out.str();
}

std::string CPrimitiveAdmission::dynamic_contract() const {
    return dynamic_ ? dynamic_->bytes() + '\0' + path_utf8(dynamic_root_) : std::string{};
}

CPrimitiveAdmission::Owner CPrimitiveAdmission::at_source_module(std::string identity) const {
    if(!physical_source_ || identity.empty() || identity.front()!='/')
        throw std::invalid_argument("Physical C declarations require an admitted source identity");
    auto result=std::shared_ptr<CPrimitiveAdmission>(new CPrimitiveAdmission);
    result->descriptor_=descriptor_;result->provider_=provider_;result->target_=target_;result->module_=std::move(identity);
    result->revision_=revision_;result->physical_source_=true;result->bindings_=bindings_;
    result->callbacks_=callbacks_;result->records_=records_;result->resources_=resources_;
    result->thunk_=thunk_;result->thunk_identity_=thunk_identity_;result->dynamic_=dynamic_;result->dynamic_root_=dynamic_root_;
    return result;
}

unsigned CPrimitiveAdmission::scalar_bytes(Type type) {
    switch (type.kind) {
    case Type::i8: case Type::u8: case Type::boolean: return 1;
    case Type::i16: case Type::u16: return 2;
    case Type::i32: case Type::u32: case Type::f32: return 4;
    case Type::i64: case Type::u64: case Type::f64: return 8;
    default: throw std::logic_error("invalid C scalar size");
    }
}
bool CPrimitiveAdmission::inline_array(Type type) const {
    for (const auto& record : records_)
        for (const auto& field : record.fields)
            if (field.shape.extent && matches(field.shape, type)) return true;
    return false;
}
bool CPrimitiveAdmission::resource_type(Type type) const {
    if (type.kind != Type::managed_ref || !type.definition) return false;
    const auto& d = *type.definition;
    return d.managed && d.native_resource && d.native_provider == provider_ &&
        d.module_identity == module_ && d.identity == module_ + "::" + d.name &&
        d.base.expired() && !d.callable && !d.interface_type && d.cases.empty() &&
        d.fields.size() == 1 && d.fields[0].name == "_seal" &&
        d.fields[0].visibility == "private" && Type(d.fields[0].type) == Type::u64 &&
        std::ranges::any_of(resources_, [&](const auto& resource) { return resource.name == d.name; });
}
bool CPrimitiveAdmission::matches(const Shape& shape, Type type) const {
    if (shape.resource)
        return !type.construction && resource_type(type) && type.definition->name == resource(shape.resource).name &&
            type.nullable == (shape.role == CResourceRole::acquire) &&
            ((shape.role != CResourceRole::mutate && shape.role != CResourceRole::close) || ir::readonly_paths(type).empty());
    if (type.nullable || type.construction) return false;
    if (shape.callback) {
        const auto& signature = callbacks_.at(shape.callback - 1);
        if (!type.definition || !type.definition->callable ||
            type.definition->call_parameters.size() != signature.parameters.size() ||
            Type(*type.definition->call_result) != signature.result.scalar) return false;
        for (unsigned n = 0; n < signature.parameters.size(); ++n)
            if (Type(type.definition->call_parameters[n]) != signature.parameters[n].scalar) return false;
        return true;
    }
    if (shape.direction)
        return type.kind == Type::growable_array && type.definition && type.definition->element &&
            !type.definition->extent && ir::array_element(type) == Type::u8 &&
            (shape.direction == 1 || ir::readonly_paths(type).empty());
    if (shape.extent)
        if (type.kind == Type::fixed_array && type.definition && type.definition->element && type.definition->extent == shape.extent) {
            auto element = shape; element.extent = 0;
            return matches(element, ir::array_element(type));
        } else return false;
    if (!shape.record) return type == shape.scalar;
    const auto& expected = record(shape.record);
    if (type.kind != Type::aggregate || !type.definition) return false;
    const auto& d = *type.definition;
    if (d.identity != module_ + "::" + expected.name || d.module_identity != module_ ||
        d.name != expected.name || !d.base.expired() || d.managed || d.interface_type ||
        d.callable || d.element || d.option_type || d.result_type || !d.cases.empty() ||
        d.fields.size() != expected.fields.size()) return false;
    for (unsigned n = 0; n < d.fields.size(); ++n)
        if (d.fields[n].name != expected.fields[n].name ||
            !matches(expected.fields[n].shape, ir::field_type(type, n))) return false;
    return true;
}
bool CPrimitiveAdmission::signature(const Binding& binding, const std::vector<Type>& args, Type result) const {
    if (args.size() != binding.abi_parameters.size() || !matches(binding.abi_result, result)) return false;
    for (unsigned n = 0; n < args.size(); ++n)
        if (!matches(binding.abi_parameters[n], args[n])) return false;
    return true;
}
std::string CPrimitiveAdmission::source_type(const Shape& shape) const {
    if (shape.callback) {
        const auto& callback = callbacks_.at(shape.callback - 1);
        std::string text = "func(";
        for (unsigned n = 0; n < callback.parameters.size(); ++n) {
            if (n) text += ", ";
            text += source_type(callback.parameters[n]);
        }
        text += ")";
        if (callback.result.scalar != Type::none) text += " -> " + source_type(callback.result);
        return text;
    }
    if (shape.resource)
        return resource(shape.resource).name + (shape.role == CResourceRole::acquire ? "?" : "");
    if (shape.direction) return "[u8]";
    if (shape.extent) return "[" + (shape.record ? record(shape.record).name : std::string(void_spelling(shape.scalar))) + ", " + std::to_string(shape.extent) + "]";
    if (shape.record) return record(shape.record).name;
    return std::string(void_spelling(shape.scalar));
}
std::string CPrimitiveAdmission::source() const {
    std::string text = "# Host-admitted C primitive declarations for " + provider_ + "\n";
    if(physical_source_)text += "native \""+provider_+"\"\n\n";
    for (const auto& resource : resources_)
        text += "struct " + resource.name + ":\n    private _seal: u64 = 0\n";
    for (const auto& record : records_) {
        text += "value struct " + record.name + ":\n";
        for (const auto& field : record.fields) text += "    " + field.name + ": " + source_type(field.shape) + "\n";
    }
    for (const auto& binding : bindings_) {
        text += "func " + binding.name + "(";
        for (std::size_t index = 0; index < binding.abi_parameters.size(); ++index) {
            if (index) text += ", ";
            if (binding.abi_parameters[index].direction == 1 ||
                binding.abi_parameters[index].role == CResourceRole::borrow) text += "const ";
            text += "arg" + std::to_string(index) + ": " + source_type(binding.abi_parameters[index]);
        }
        text += ")";
        if (binding.abi_result.record || binding.abi_result.resource || binding.result != Type::none) {
            text += " -> ";
            text += source_type(binding.abi_result);
        }
        text += ":\n    pass\n";
    }
    return text;
}
std::string CPrimitiveAdmission::generate_declarations(std::string_view descriptor) {
    auto admission=parse(descriptor,true);
    if(!admission->physical_source())throw std::invalid_argument("Physical declarations require source-file in the descriptor");
    return admission->source();
}
}
