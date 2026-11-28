#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace vd::diagnostics {
inline constexpr std::size_t map_byte_limit = 16 * 1024 * 1024;
inline constexpr std::size_t site_limit = 65536;
// Public categories are deliberately independent of source text and ABI names.
enum class Category : std::uint32_t { runtime = 1, arithmetic, bounds, null_value, call, global };
struct Site {
    Category category = Category::runtime;
    std::string function, path, excerpt, description;
    std::uint32_t line = 0, column = 0;
};
// A draft cannot be serialized/resolved until bound to the exact final image.
// Contains private plaintext. Persistence/owner-only placement is the caller's
// responsibility; never put this object or its serialization in an app bundle.
class PrivateMap {
    std::string build_, image_;
    std::vector<Site> sites_;
    std::size_t bytes_ = 0;
public:
    // Digest of normalized semantic inputs supplied by a future admitted plan,
    // including target/toolchain/policy and source content, excluding host paths.
    explicit PrivateMap(std::string_view semantic_inputs_sha256);
    const auto& build_id() const { return build_; }
    const auto& image_sha256() const { return image_; }
    const auto& sites() const { return sites_; }
    std::string add(Site);
    void bind_image(std::string_view final_image_bytes);
    std::string encode() const;
    static PrivateMap decode(std::string_view);
    // Exact public token, not arbitrary stderr. Throws for a missing map,
    // malformed/unknown token, wrong build, wrong category or wrong image.
    static Site resolve(std::string_view map, std::string_view final_image_bytes,
                        std::string_view token);
};
Category category(std::string_view description);
}

namespace vd {
namespace ir { struct Module; }
// Production emission uses the shared AOT lowerer and returns a draft for
// association with the audited, stripped executable.
using PublicationResources = std::map<std::string, std::string>;
diagnostics::PrivateMap emit_publication_object(const ir::Module&, const std::filesystem::path&,
    bool optimize, std::string_view semantic_inputs_sha256, bool size = false,
    bool debug = true, const PublicationResources* resources = nullptr, bool thin_lto = false);
}
