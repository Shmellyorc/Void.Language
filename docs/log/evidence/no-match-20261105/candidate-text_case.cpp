#include "void/runtime_abi.hpp"
#include "void/utf8.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace {

#include "unicode_case_tables.inc"

struct Scalar {
    std::uint32_t value;
    std::size_t offset;
    unsigned width;
};

template<std::size_t N>
bool member(std::uint32_t value, const Range (&ranges)[N]) {
    const auto* found = std::upper_bound(std::begin(ranges), std::end(ranges), value,
        [](std::uint32_t scalar, const Range& range) { return scalar < range.first; });
    return found != std::begin(ranges) && value < std::prev(found)->limit;
}

template<std::size_t N>
std::string_view mapped(std::uint32_t value, const Mapping (&mappings)[N]) {
    const auto* found = std::lower_bound(std::begin(mappings), std::end(mappings), value,
        [](const Mapping& mapping, std::uint32_t scalar) { return mapping.scalar < scalar; });
    if (found != std::end(mappings) && found->scalar == value) {
        return found->bytes;
    }
    return {};
}

std::string transform(std::string_view text, bool upper) {
    std::vector<Scalar> scalars;
    for (std::size_t offset = 0; offset < text.size();) {
        const auto width = vd::utf8_scalar_bytes(text, offset);
        if (!width) {
            throw std::invalid_argument("Invalid UTF-8 in Unicode casing input");
        }
        const auto lead = static_cast<unsigned char>(text[offset]);
        std::uint32_t value = width == 1 ? lead : lead & (width == 2 ? 31 : width == 3 ? 15 : 7);
        for (unsigned index = 1; index < width; ++index) {
            value = (value << 6) | (static_cast<unsigned char>(text[offset + index]) & 63);
        }
        scalars.push_back({value, offset, width});
        offset += width;
    }

    // Two linear context passes avoid rescanning long case-ignorable sequences.
    // Case_Ignorable takes precedence when the two properties overlap (U+0345).
    std::vector<bool> following(scalars.size());
    bool cased = false;
    for (std::size_t index = scalars.size(); index > 0;) {
        --index;
        following[index] = cased;
        if (!member(scalars[index].value, ignorable_ranges)) {
            cased = member(scalars[index].value, cased_ranges);
        }
    }

    std::string output;
    output.reserve(text.size());
    bool preceding = false;
    for (std::size_t index = 0; index < scalars.size(); ++index) {
        const auto& scalar = scalars[index];
        auto replacement = upper ? mapped(scalar.value, upper_mappings) :
                                   mapped(scalar.value, lower_mappings);
        if (!upper && scalar.value == 0x3A3 && preceding && !following[index]) {
            replacement = "\317\202"; // U+03C2 GREEK SMALL LETTER FINAL SIGMA.
        }
        output.append(replacement.empty() ? text.substr(scalar.offset, scalar.width) : replacement);
        if (!member(scalar.value, ignorable_ranges)) {
            preceding = member(scalar.value, cased_ranges);
        }
    }
    return output;
}

} // namespace

extern "C" void vd_text_case(void** output, void* input, bool upper,
                              vd::RuntimeFailure* failure) noexcept {
    try {
        auto& runtime = vd::Runtime::current();
        // Own input and output before string construction can poll or collect.
        const std::string source(runtime.string_bytes(input));
        const auto result = transform(source, upper);
        runtime.string_into(output, result);
    } catch (const std::bad_alloc&) {
        failure->message = "Unicode casing allocation failed";
    } catch (const std::exception&) {
        failure->message = "Unicode casing failed";
    }
}

namespace {
// Status values are consumed by the canonical text wrapper. Runtime failures
// travel separately through RuntimeFailure and never publish a partial result.
enum TranslationStatus { translated, invalid_range, invalid_argument, limit_exceeded };
using Rules = std::map<std::int32_t, std::string>;

struct RuleSnapshot {
    vd::Runtime& runtime;
    Rules entries;
    ~RuleSnapshot() {
        // A large rule set must also be disposable during unwinding without
        // retaining the Managed world. These entries own only native bytes.
        vd::Runtime::Native native(runtime);
        entries.clear();
    }
};

// Both passes execute NativeSafe, accessing only owned bytes. The callback
// receives contiguous unmapped spans or complete replacements.
template<class Emit>
bool translation_spans(std::string_view source, const Rules& rules, bool& changed, Emit emit) {
    std::size_t start = 0;
    for (std::size_t at = 0; at < source.size();) {
        const unsigned width = vd::utf8_scalar_bytes(source, at);
        if (!width) throw std::logic_error("invalid UTF-8 in managed string");
        const auto lead = static_cast<unsigned char>(source[at]);
        std::int32_t scalar = width == 1 ? lead : lead & (width == 2 ? 31 : width == 3 ? 15 : 7);
        for (unsigned i = 1; i < width; ++i)
            scalar = (scalar << 6) | (static_cast<unsigned char>(source[at + i]) & 63);
        const auto found = rules.find(scalar);
        if (found != rules.end()) {
            if (!emit(source.substr(start, at - start)) || !emit(found->second)) return false;
            changed = true;
            start = at + width;
        }
        at += width;
    }
    return emit(source.substr(start));
}
}

int vd::Runtime::translate_scalars(void** output, RegisteredRootWord input, RegisteredRootWord keys,
                                   RegisteredRootWord replacements, const NativeArrayElement& strings,
                                   std::int64_t max_bytes) {
    // Validate all root authority before considering even an empty result.
    {
        std::lock_guard lock(mutex_);
        require_buffer_output(output);
        heap_.validate_output_address(output);
        (void)heap_.string_bytes(reference_source(input));
        (void)reference_source(keys);
        (void)reference_source(replacements);
    }
    if (!strings.key || strings.bytes != sizeof(void*) || strings.alignment != alignof(void*) ||
        strings.count != 1 || !strings.fields || strings.fields[0].kind != 12 ||
        strings.fields[0].offset || strings.fields[0].extent || strings.fields[0].nullable)
        throw std::invalid_argument("invalid translation replacement element");
    auto key_schema = fixed_scalar_schema(ScalarEncoding::i32);
    auto string_schema = native_array_layout(strings, 0)->schema();

    constexpr std::size_t byte_quota = 16384;
    std::size_t bytes_left = byte_quota;
    auto snapshot_string = [&](RegisteredRootWord root) {
        std::string copy;
        for (;;) {
            if (!bytes_left) { poll(); bytes_left = byte_quota; }
            // This borrow ends before any poll. Revalidate the registered slot
            // and borrow again for the next chunk.
            std::lock_guard lock(mutex_);
            auto bytes = heap_.string_bytes(reference_source(root));
            if (copy.empty()) copy.reserve(bytes.size());
            auto part = bytes.substr(copy.size(), bytes_left);
            copy.append(part);
            bytes_left -= part.size();
            if (copy.size() == bytes.size()) return copy;
        }
    };
    RuleSnapshot snapshot{*this, {}};
    auto& rules = snapshot.entries;
    std::optional<std::size_t> unchanged_bytes;
    {
        void *key_backing = nullptr, *string_backing = nullptr, *replacement = nullptr;
        constexpr std::size_t zero = 0;
        Slot slots[] = {{&key_backing,sizeof(void*),&zero,1}, {&string_backing,sizeof(void*),&zero,1},
                        {&replacement,sizeof(void*),&zero,1}};
        Frame roots(*this, slots, 3);
        auto lease = [&](RegisteredRootWord source, const ElementSchemaRef& schema, void*& backing,
                         BufferLayoutRef& layout, std::optional<BufferReadLease>& held) {
            {
                std::lock_guard lock(mutex_);
                const auto& header = list_header(source, schema);
                backing = header.backing;
                if (backing) layout = heap_.buffer_state(backing).layout;
            }
            if (backing) held.emplace(*this, root_word(&backing), layout);
            return held ? held->count() : std::uint64_t{0};
        };
        BufferLayoutRef key_layout, string_layout;
        std::optional<BufferReadLease> key_lease, string_lease;
        const auto key_count = lease(keys, key_schema, key_backing, key_layout, key_lease);
        const auto string_count = lease(replacements, string_schema, string_backing, string_layout, string_lease);
        if (key_count != string_count) return invalid_argument;
        // Bounded, invocation-local proof: the leased backings cannot be
        // modified while we validate their current contents. No heap borrow
        // survives this lock, and inconclusive probes use these same leases.
        auto short_ascii_no_match = [&]() -> std::optional<std::size_t> {
            if (key_count > 256) return {};
            std::lock_guard lock(mutex_);
            const auto source = heap_.string_bytes(reference_source(input));
            if (source.size() > 128) return {};
            std::uint64_t present[2] = {};
            for (unsigned char byte : source) {
                if (byte >= 128) return {};
                present[byte / 64] |= std::uint64_t{1} << (byte % 64);
            }
            auto remaining = byte_quota - source.size();
            std::int32_t previous = -1;
            for (std::uint64_t i = 0; i < key_count; ++i) {
                std::int32_t key;
                std::memcpy(&key, static_cast<const char*>(key_backing) + key_layout->element_offset(i), sizeof(key));
                void* value = nullptr;
                std::memcpy(&value, static_cast<const char*>(string_backing) + string_layout->element_offset(i), sizeof(value));
                // Preserve reference-before-key validation. Stop at the first
                // inconclusive entry so later rules cannot mask its failure.
                const auto bytes = heap_.string_bytes(value);
                if (key <= previous || key > 0x10ffff || (key >= 0xd800 && key <= 0xdfff) ||
                    bytes.size() > remaining ||
                    (key < 128 && (present[key / 64] & (std::uint64_t{1} << (key % 64))))) return {};
                remaining -= bytes.size();
                previous = key;
            }
            return source.size();
        };
        unchanged_bytes = short_ascii_no_match();
        auto replacement_root = root_word(&replacement);
        for (std::uint64_t i = 0; !unchanged_bytes && i < key_count; ++i) {
            if (i && i % 256 == 0) poll();
            std::int32_t key;
            {
                // Each backing is complete, rooted and leased, with its exact
                // checked layout and frozen live prefix. Recompute addresses.
                std::lock_guard lock(mutex_);
                std::memcpy(&key, static_cast<const char*>(key_backing) + key_layout->element_offset(i), sizeof(key));
                void* value = nullptr;
                std::memcpy(&value, static_cast<const char*>(string_backing) + string_layout->element_offset(i), sizeof(value));
                (void)heap_.string_bytes(value);
                replacement = value;
            }
            if (key < 0 || key > 0x10ffff || (key >= 0xd800 && key <= 0xdfff)) return invalid_argument;
            if (!rules.emplace(key, snapshot_string(replacement_root)).second) return invalid_argument;
        }
    }
    if (max_bytes < 0) return invalid_range;
    auto publish_alias = [&] {
        std::lock_guard lock(mutex_);
        require_buffer_output(output);
        heap_.validate_output_address(output);
        *output = reference_source(input);
    };
    if (unchanged_bytes) {
        if (*unchanged_bytes > static_cast<std::uint64_t>(max_bytes)) return limit_exceeded;
        publish_alias();
        return translated;
    }
    auto source = snapshot_string(input);
    std::string result;
    bool changed = false;
    {
        Native native(*this);
        std::size_t size = 0;
        auto measure = [&](std::string_view part) {
            if (part.size() > static_cast<std::uint64_t>(max_bytes) - size ||
                part.size() > std::numeric_limits<std::size_t>::max() - size) return false;
            size += part.size();
            return true;
        };
        if (!translation_spans(source, rules, changed, measure)) return limit_exceeded;
        if (changed) {
            (void)string_payload_size(size, 0);
            result.reserve(size);
            translation_spans(source, rules, changed, [&](std::string_view part) {
                result.append(part);
                return true;
            });
        }
    }
    if (changed) {
        string_into(output, result);
    } else {
        publish_alias();
    }
    return translated;
}

extern "C" int vd_text_translate(void** out, void** value, void** keys, void** replacements,
                                 const vd::NativeArrayElement* strings, std::int64_t max_bytes,
                                 vd::RuntimeFailure* failure) noexcept {
    try {
        auto& runtime = vd::Runtime::current();
        if (!strings) throw std::invalid_argument("missing replacement schema");
        return runtime.translate_scalars(out, runtime.root_word(value), runtime.root_word(keys),
                                         runtime.root_word(replacements), *strings, max_bytes);
    } catch (const std::bad_alloc&) {
        failure->message = "Scalar translation allocation failed";
    } catch (const std::exception&) {
        failure->message = "Scalar translation failed";
    }
    return 0;
}
