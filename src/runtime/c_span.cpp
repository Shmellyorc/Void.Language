#include "void/c_primitive.hpp"
#include "void/runtime.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <stdexcept>

extern "C" std::uint32_t vd::vd_c_span_dispatch_v2(CPrimitiveThunk* thunk, const char* descriptor,
    std::uint32_t descriptor_size, std::uint32_t ordinal, const std::uint64_t* words,
    std::uint32_t count, std::uint64_t* result, const CSpanRequest* spans, std::uint32_t span_count) noexcept {
    if (!thunk || !descriptor || count > c_primitive_max_arguments || (count && !words) ||
        span_count > 2 || (span_count && !spans)) return 2;
    try {
        auto& runtime = Runtime::current();
        std::array<std::uint64_t, c_primitive_max_arguments> arguments{};
        if (count) std::copy_n(words, count, arguments.begin());
        std::array<std::vector<std::uint64_t>, 2> storage;
        std::array<std::span<std::uint8_t>, 2> buffers;
        std::array<std::uint64_t, 2> versions{}, offsets{}, lengths{};
        std::array<bool, c_primitive_max_arguments> occupied{};
        unsigned writable = span_count;
        for (unsigned n = 0; n < span_count; ++n) {
            const auto& span = spans[n];
            if (!span.root || !span.direction || span.direction > 3) return 2;
            for (auto index : {span.argument, span.offset_argument, span.length_argument}) {
                if (index >= count || occupied[index]) return 2;
                occupied[index] = true;
            }
            offsets[n] = words[span.offset_argument];
            lengths[n] = words[span.length_argument];
            if (!span.limit || span.limit > c_span_limit || lengths[n] > span.limit ||
                offsets[n] > UINT64_MAX - lengths[n]) return 3;
            if (span.direction != 1) {
                if (writable != span_count || !result) return 2;
                writable = n;
            }
            // Capture only into independent zero-initialized native words.
            auto root = runtime.root_word(span.root);
            storage[n].resize(static_cast<std::size_t>((lengths[n] + 7) / 8));
            buffers[n] = {reinterpret_cast<std::uint8_t*>(storage[n].data()), static_cast<std::size_t>(lengths[n])};
            versions[n] = runtime.c_span_bytes(root, offsets[n], buffers[n], span.direction != 2);
            arguments[span.argument] = lengths[n] ? reinterpret_cast<std::uintptr_t>(buffers[n].data()) : 0;
        }
        if (span_count == 2) {
            const auto& a = spans[0];
            const auto& b = spans[1];
            if (*a.root == *b.root && lengths[0] && lengths[1] &&
                offsets[0] < offsets[1] + lengths[1] && offsets[1] < offsets[0] + lengths[0])
                return 4;
        }
        std::uint32_t status;
        {
            Runtime::Native native(runtime);
            status = thunk(descriptor, descriptor_size, ordinal, count ? arguments.data() : nullptr, count, result);
        }
        if (status || writable == span_count) return status;
        const auto written = std::bit_cast<std::int64_t>(*result);
        if (written < 0) return 0;
        const auto& span = spans[writable];
        if (static_cast<std::uint64_t>(written) > lengths[writable]) return 5;
        runtime.c_span_bytes(runtime.root_word(span.root), offsets[writable],
            buffers[writable].first(static_cast<std::size_t>(written)), false, versions[writable]);
        return 0;
    } catch (const std::out_of_range&) {
        return 3;
    } catch (const std::exception&) {
        return 6;
    }
}
