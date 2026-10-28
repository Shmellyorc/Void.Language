#include "void/os.hpp"
#include "void/os_abi.hpp"
#include <cstring>
#include <stdexcept>

namespace {
struct Reply {
    std::int64_t kind = 0, code = 0, number = 0;
    bool flag = false;
    void* text = nullptr;
    void* domain = nullptr;
};
constexpr std::array<std::size_t, 6> fields = {
    offsetof(Reply, kind), offsetof(Reply, code), offsetof(Reply, number),
    offsetof(Reply, flag), offsetof(Reply, text), offsetof(Reply, domain)
};
}
extern "C" void vd_os_call(void* output, const vd::os::ReplyLayout* layout,
                            unsigned operation, void* input, vd::RuntimeFailure* failure) noexcept {
    try {
        using namespace vd;
        if (!failure || !output || operation >= os::operations.size()) throw std::logic_error("Invalid OS ABI");
        auto selected = static_cast<os::Operation>(operation);
        const bool scalar = selected == os::Operation::process_id;
        if (scalar ? layout != nullptr : !layout || layout->size != sizeof(Reply) || layout->fields != fields) {
            throw std::logic_error("Invalid OS reply geometry");
        }
        if (os::operations[operation].text_argument != (input != nullptr)) throw std::logic_error("Invalid OS input geometry");
        auto& runtime = Runtime::current();
        std::string copied;
        if (input) copied = runtime.string_bytes(input);
        os::Result result;
        {
            Runtime::Native safe(runtime);
            result = os::query(selected, copied);
        }
        if (scalar) {
            if (result.kind != os::ErrorKind::success) throw std::logic_error("Invalid OS PID result");
            std::memcpy(output, &result.number, sizeof(result.number));
            return;
        }
        Reply reply;
        constexpr std::array<std::size_t, 2> references = {fields[4], fields[5]};
        Runtime::Slot slot{&reply, sizeof(reply), references.data(), references.size()};
        Runtime::Frame roots(runtime, &slot, 1);
        reply.kind = static_cast<std::int64_t>(result.kind);
        reply.code = result.os_code;
        reply.number = result.number;
        reply.flag = result.flag;
        runtime.string_into(&reply.text, result.text);
        runtime.string_into(&reply.domain, result.native_domain);
        std::memcpy(output, &reply, sizeof(reply));
    } catch (const std::bad_alloc&) {
        if (failure) failure->message = "OS boundary allocation failed";
    } catch (const std::length_error&) {
        if (failure) failure->message = "OS boundary extent is unrepresentable";
    } catch (...) {
        if (failure) failure->message = "OS boundary invariant failed";
    }
}
