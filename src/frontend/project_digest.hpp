#pragma once
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/SHA256.h>
#include <cstdint>
#include <string>
#include <string_view>

namespace vd::project {
class Digest {
    llvm::SHA256 hash;
public:
    void bytes(std::string_view value) { hash.update(llvm::StringRef(value.data(), value.size())); }
    void field(std::string_view value) {
        const auto length = static_cast<std::uint64_t>(value.size());
        char encoded[8];
        for (unsigned i = 0; i < 8; ++i) encoded[i] = static_cast<char>((length >> (56 - i * 8)) & 255);
        bytes({encoded, sizeof encoded});
        bytes(value);
    }
    std::string finish() {
        constexpr std::string_view hex = "0123456789abcdef";
        std::string text;
        for (auto byte : hash.final()) {
            text += hex[byte >> 4];
            text += hex[byte & 15];
        }
        return text;
    }
};
}
