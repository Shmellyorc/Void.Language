#pragma once

#include <filesystem>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace vd {

// Bounded conventional providers may expose complete C libraries. The verified
// closure profile retains its smaller limits and all ABI checks remain shared.
inline constexpr std::size_t c_conventional_descriptor_limit = 128 * 1024;
inline constexpr std::size_t c_conventional_binding_limit = 1024;

// Pure offline data. Only the embedding host or a linked AOT plan may
// authorize its use. Parsing neither opens artifacts nor runs a probe.
class DynamicClosure final {
public:
    struct Image {
        std::string name, sha256;
        std::vector<std::string> needed;
    };
    struct Export {
        std::string image, symbol;
    };
    struct Payload {
        std::string name, sha256;
        std::uint64_t size;
    };
    static constexpr std::uint64_t payload_limit = 128 * 1024 * 1024;
    static constexpr std::size_t payload_count_limit = 24;
    using Owner = std::shared_ptr<const DynamicClosure>;
    static Owner parse(std::string_view);
    const std::string& bytes() const { return bytes_; }
    const std::string& provider() const { return provider_; }
    const std::string& thunk_identity() const { return thunk_; }
    const std::string& descriptor() const { return descriptor_; }
    const std::vector<Image>& images() const { return images_; }
    const std::vector<Export>& exports() const { return exports_; }
    bool conventional() const { return !library_.empty(); }
    const auto& payload() const { return payload_; }
    const std::string& library() const { return library_; }
    const std::string& adapter() const { return adapter_; }
    const std::string& dispatch() const { return dispatch_; }
    const std::string& target() const { return target_; }
    static constexpr std::string_view transport = "vd_c_primitive_dispatch_v3";
private:
    DynamicClosure() = default;
    std::string bytes_, provider_, thunk_, descriptor_;
    std::string library_, adapter_, dispatch_, target_;
    std::vector<Image> images_;
    std::vector<Export> exports_;
    std::vector<Payload> payload_;
};

class DynamicBinding;
std::shared_ptr<const DynamicBinding> load_dynamic(DynamicClosure::Owner, const std::filesystem::path&);
bool matches_dynamic(const DynamicBinding&, const DynamicClosure&, const std::filesystem::path&);
std::filesystem::path installed_dynamic_root();

} // namespace vd
