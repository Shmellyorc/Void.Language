#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace vd {
// Format v1 owns six UTF-8 fields in project ID/name/version, application
// ID/name/version order. Encoding is independent of native layout and TOML.
class ApplicationMetadata final {
public:
    static constexpr std::uint32_t revision = 1;
    static constexpr std::size_t byte_limit = 4 + 6 * 4 + 4 * 255 + 2 * 512;
    using Fields = std::array<std::string_view, 6>;
    using Owner = std::shared_ptr<const ApplicationMetadata>;

    static Owner make(const Fields&);
    static Owner decode(std::string_view);
    const std::string& bytes() const { return bytes_; }
    std::string_view application_id() const;
    bool operator==(const ApplicationMetadata&) const = default;
private:
    ApplicationMetadata(std::string bytes, std::uint32_t app_offset, std::uint32_t app_length);
    std::string bytes_;
    std::uint32_t app_offset_, app_length_;
};
}
