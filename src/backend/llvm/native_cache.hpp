#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
namespace vd::llvm_backend {
std::string digest(std::string_view);
void cache_field(std::string&, std::string_view);
struct NativeArtifact { bool coordinated; std::string object; };
// An owned directory handle pins the private cache during all operations.
class NativeCache {
    int directory_ = -1;
    std::string key_;
public:
    NativeCache(const std::filesystem::path&, std::string_view identity) noexcept;
    ~NativeCache();
    NativeCache(const NativeCache&)=delete;
    NativeCache& operator=(const NativeCache&)=delete;
    bool available() const {return directory_>=0;}
    std::optional<NativeArtifact> read() const noexcept;
    bool write(bool coordinated, std::string_view object) const noexcept;
};
}
