#include "void/url.hpp"
#include "void/url_abi.hpp"
#include "void/url_ops.hpp"
#include <cstring>
#include <stdexcept>

namespace vd::url {
namespace {
constexpr std::array<unsigned, 8> reference_fields = {2, 3, 4, 6, 7, 8, 11, 12};
std::array<std::size_t, 8> root_offsets(const ReplyLayout& layout) {
    std::array<std::size_t, 8> offsets{};
    for (unsigned n = 0; n < offsets.size(); ++n) offsets[n] = layout.fields[reference_fields[n]];
    return offsets;
}
class Reply {
public:
    const ReplyLayout& layout;
    std::vector<std::byte> storage;
    std::array<std::size_t, 8> references{};
    Runtime::Slot slot;
    Runtime::Frame roots;
    Reply(Runtime& runtime, const ReplyLayout& geometry)
        : layout(geometry), storage(layout.size), references(root_offsets(layout)),
          slot{storage.data(), storage.size(), references.data(), references.size()}, roots(runtime, &slot, 1) {}
    void** reference(unsigned field) { return reinterpret_cast<void**>(storage.data() + layout.fields[field]); }
    template<class T> void scalar(unsigned field, T value) {
        std::memcpy(storage.data() + layout.fields[field], &value, sizeof(value));
    }
    void build(Runtime& runtime, const Result& result, const QueryResult& query) {
        scalar(0, static_cast<std::int64_t>(result.error)); scalar(1, result.offset);
        const std::array<std::string_view, 6> text = {result.address.text, result.address.scheme,
            result.address.host, result.address.path, result.address.query, result.address.fragment};
        for (unsigned n = 0; n < text.size(); ++n) runtime.string_into(reference(reference_fields[n]), text[n]);
        scalar(5, result.address.port); scalar(9, result.address.has_query); scalar(10, result.address.has_fragment);
        runtime.bytes_into(reference(11), result.bytes);
        auto array = runtime.native_array_layout(*layout.item, 0);
        runtime.array_new(reference(12), array, true);
        std::vector<std::byte> item(layout.item->bytes);
        std::array<std::size_t, 2> offsets = {layout.item->fields[0].offset, layout.item->fields[1].offset};
        Runtime::Slot item_slot{item.data(), item.size(), offsets.data(), offsets.size()};
        Runtime::Frame item_roots(runtime, &item_slot, 1);
        for (const auto& input : query.items) {
            runtime.string_into(reinterpret_cast<void**>(item.data() + offsets[0]), input.name);
            runtime.string_into(reinterpret_cast<void**>(item.data() + offsets[1]), input.value);
            std::memcpy(item.data() + layout.item->fields[2].offset, &input.has_value, sizeof(input.has_value));
            runtime.array_push(runtime.root_word(reference(12)), array->schema(), item.data());
        }
    }
};
std::vector<QueryItem> snapshot(Runtime& runtime, const ReplyLayout& layout, void* input, bool& rejected) {
    Runtime::Slot input_slot{&input, sizeof(input), nullptr, 0};
    constexpr std::size_t zero = 0;
    input_slot.offsets = &zero;
    input_slot.count = 1;
    Runtime::Frame input_root(runtime, &input_slot, 1);
    auto array = runtime.native_array_layout(*layout.item, 0);
    const auto count = runtime.array_count(runtime.root_word(&input), array, true);
    // A rejected oversize list never needs an unbounded native snapshot.
    if (count > max_url_bytes + 1) { rejected = true; return {}; }
    std::vector<QueryItem> result;
    result.reserve(count);
    std::vector<std::byte> item(layout.item->bytes);
    std::size_t copied_bytes = 0;
    for (std::uint64_t n = 0; n < count; ++n) {
        runtime.array_read(item.data(), runtime.root_word(&input), array, true, n);
        void* name = nullptr; void* value = nullptr; bool has_value = false;
        std::memcpy(&name, item.data() + layout.item->fields[0].offset, sizeof(name));
        std::memcpy(&value, item.data() + layout.item->fields[1].offset, sizeof(value));
        std::memcpy(&has_value, item.data() + layout.item->fields[2].offset, sizeof(has_value));
        auto name_bytes = runtime.string_bytes(name), value_bytes = runtime.string_bytes(value);
        if (name_bytes.size() > max_url_bytes - copied_bytes) { rejected = true; return {}; }
        copied_bytes += name_bytes.size();
        if (has_value) {
            if (value_bytes.size() > max_url_bytes - copied_bytes) { rejected = true; return {}; }
            copied_bytes += value_bytes.size();
        }
        result.push_back({std::string(name_bytes), has_value ? std::string(value_bytes) : std::string{}, has_value});
    }
    return result;
}
}
}
extern "C" void vd_url_call(void* output, const vd::url::ReplyLayout* layout, unsigned operation,
    void* const* references, const std::int64_t* numbers, vd::RuntimeFailure* failure) noexcept {
    using namespace vd;
    using namespace vd::url;
    try {
        if (!output || !layout || operation >= source_operations.size() || !references || !numbers ||
            !layout->item || layout->item->count != 3 || layout->item->fields[0].kind != 12 ||
            layout->item->fields[1].kind != 12 || layout->item->fields[2].kind != 10)
            throw std::logic_error("Invalid URL native geometry");
        auto& runtime = Runtime::current();
        std::array<std::string, 2> text;
        std::vector<QueryItem> items;
        bool rejected = false;
        unsigned reference = 0, string = 0;
        for (char argument : source_operations[operation].arguments) {
            if (argument == 't') {
                const auto value = runtime.string_bytes(references[reference++]);
                const auto op = static_cast<Operation>(operation);
                const bool bounded = op == Operation::parse || op == Operation::resolve ||
                    op == Operation::parse_query || op == Operation::with_query;
                if (bounded && value.size() > max_url_bytes) rejected = true;
                else text[string] = value;
                ++string;
            }
            else if (argument == 'q') items = snapshot(runtime, *layout, references[reference++], rejected);
        }
        Result result;
        QueryResult query;
        {
            Runtime::Native safe(runtime);
            if (rejected) result.error = ErrorKind::limit_exceeded;
            else switch (static_cast<Operation>(operation)) {
            case Operation::parse: result = parse(text[0], numbers[0] != 0); break;
            case Operation::resolve: result = resolve(text[0], text[1]); break;
            case Operation::encode: result.address.text = encode_component(text[0], numbers[0] != 0); break;
            case Operation::decode:
                result = decode(text[0], numbers[0] != 0, numbers[1] != 0);
                if (numbers[1] && result.error == ErrorKind::success) result.address.text = result.bytes;
                break;
            case Operation::parse_query: query = parse_query(text[0], numbers[0] != 0, numbers[1]); break;
            case Operation::encode_query: query = encode_query(items, numbers[0] != 0); break;
            case Operation::with_query: result = with_query(text[0], items); break;
            }
        }
        if (!rejected && (static_cast<Operation>(operation) == Operation::parse_query ||
            static_cast<Operation>(operation) == Operation::encode_query)) {
            result.error = query.error; result.offset = query.offset; result.address.text = query.text;
        }
        Reply reply(runtime, *layout);
        reply.build(runtime, result, query);
        std::memcpy(output, reply.storage.data(), layout->size);
    } catch (const std::bad_alloc&) { failure->message = "URL boundary allocation failed"; }
    catch (const std::length_error&) { failure->message = "URL boundary extent is unrepresentable"; }
    catch (...) { failure->message = "URL boundary invariant failed"; }
}
