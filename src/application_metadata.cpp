#include "void/application_metadata.hpp"
#include "void/project.hpp"
#include <stdexcept>
#include <utility>

namespace vd {
namespace {
[[noreturn]] void invalid() { throw std::invalid_argument("Invalid application metadata."); }
void append_word(std::string& bytes, std::uint32_t word) {
    for (unsigned shift = 0; shift < 32; shift += 8) bytes.push_back(static_cast<char>((word >> shift) & 255));
}
std::uint32_t take_word(std::string_view& bytes) {
    if (bytes.size() < 4) invalid();
    std::uint32_t word = 0;
    for (unsigned i = 0; i < 4; ++i) word |= std::uint32_t(static_cast<unsigned char>(bytes[i])) << (i * 8);
    bytes.remove_prefix(4);
    return word;
}
}
ApplicationMetadata::ApplicationMetadata(std::string bytes, std::uint32_t offset, std::uint32_t length)
    : bytes_(std::move(bytes)), app_offset_(offset), app_length_(length) {}

ApplicationMetadata::Owner ApplicationMetadata::make(const Fields& fields) {
    for (unsigned i : {0u, 3u}) {
        if (!project::valid_identity(fields[i]) || !project::valid_name(fields[i+1]) ||
            !project::parse_version(fields[i+2])) invalid();
    }
    std::string bytes;
    append_word(bytes, revision);
    std::uint32_t offset = 0;
    for (unsigned i = 0; i < fields.size(); ++i) {
        append_word(bytes, static_cast<std::uint32_t>(fields[i].size()));
        if (i == 3) offset = static_cast<std::uint32_t>(bytes.size());
        bytes.append(fields[i]);
    }
    return Owner(new ApplicationMetadata(std::move(bytes), offset, static_cast<std::uint32_t>(fields[3].size())));
}
ApplicationMetadata::Owner ApplicationMetadata::decode(std::string_view bytes) {
    if (bytes.size() > byte_limit || take_word(bytes) != revision) invalid();
    Fields fields;
    for (auto& field : fields) {
        const auto size = take_word(bytes);
        if (size > bytes.size()) invalid();
        field = bytes.substr(0, size);
        bytes.remove_prefix(size);
    }
    if (!bytes.empty()) invalid();
    return make(fields);
}
std::string_view ApplicationMetadata::application_id() const {
    return std::string_view(bytes_).substr(app_offset_, app_length_);
}
}
