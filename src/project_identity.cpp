#include "void/project.hpp"
#include "void/utf8.hpp"
#include <algorithm>

namespace vd::project {
namespace {
bool digit(char c) { return c >= '0' && c <= '9'; }
bool numeric(std::string_view s) { return !s.empty() && std::all_of(s.begin(), s.end(), digit); }
std::strong_ordering number_order(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return a.size() <=> b.size();
    return a <=> b;
}
bool component(std::string_view s) {
    return numeric(s) && (s.size() == 1 || s.front() != '0');
}
bool identifiers(std::string_view s, std::vector<std::string>& out, bool prerelease) {
    if (s.empty()) return false;
    for (std::size_t begin = 0; begin <= s.size();) {
        auto end = s.find('.', begin);
        if (end == std::string_view::npos) end = s.size();
        auto piece = s.substr(begin, end - begin);
        if (piece.empty() || !std::all_of(piece.begin(), piece.end(), [](char c) {
            return digit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-';
        }) || (prerelease && numeric(piece) && !component(piece))) return false;
        out.emplace_back(piece);
        if (end == s.size()) break;
        begin = end + 1;
    }
    return true;
}
}
bool valid_name(std::string_view s) {
    bool non_space = false;
    std::size_t scalars = 0;
    for (std::size_t i = 0; i < s.size();) {
        auto n = utf8_scalar_bytes(s, i);
        if (!n || ++scalars > 128) return false;
        unsigned cp = static_cast<unsigned char>(s[i]) & (n == 1 ? 127 : n == 2 ? 31 : n == 3 ? 15 : 7);
        for (unsigned k = 1; k < n; ++k) cp = (cp << 6) | (static_cast<unsigned char>(s[i+k]) & 63);
        if (cp < 32 || (cp >= 127 && cp <= 159)) return false;
        // Unicode White_Space, without locale-dependent classification.
        bool space = cp == 32 || cp == 0xa0 || cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200a) ||
            cp == 0x2028 || cp == 0x2029 || cp == 0x202f || cp == 0x205f || cp == 0x3000;
        non_space |= !space;
        i += n;
    }
    return scalars && non_space;
}
std::optional<SemanticVersion> parse_version(std::string_view text) {
    if (text.empty() || text.size() > 255) return {};
    SemanticVersion version;
    version.text_ = text;
    auto build = text.find('+');
    if (build != std::string_view::npos) {
        if (!identifiers(text.substr(build + 1), version.build_, false)) return {};
        text = text.substr(0, build);
    }
    auto prerelease = text.find('-');
    if (prerelease != std::string_view::npos) {
        if (!identifiers(text.substr(prerelease + 1), version.prerelease_, true)) return {};
        text = text.substr(0, prerelease);
    }
    std::size_t begin = 0;
    for (std::size_t i = 0; i < 3; ++i) {
        auto end = text.find('.', begin);
        if (i < 2 && end == std::string_view::npos) return {};
        if (i == 2 && end != std::string_view::npos) return {};
        if (end == std::string_view::npos) end = text.size();
        auto value = text.substr(begin, end - begin);
        if (!component(value)) return {};
        version.core_[i] = value;
        begin = end + 1;
    }
    return version;
}
std::strong_ordering SemanticVersion::compare_precedence(const SemanticVersion& other) const noexcept {
    for (std::size_t i = 0; i < 3; ++i) {
        auto order = number_order(core_[i], other.core_[i]);
        if (order != 0) return order;
    }
    if (prerelease_.empty() != other.prerelease_.empty())
        return prerelease_.empty() ? std::strong_ordering::greater : std::strong_ordering::less;
    for (std::size_t i = 0; i < std::min(prerelease_.size(), other.prerelease_.size()); ++i) {
        std::string_view a = prerelease_[i], b = other.prerelease_[i];
        bool na = numeric(a), nb = numeric(b);
        if (na != nb) return na ? std::strong_ordering::less : std::strong_ordering::greater;
        auto order = na ? number_order(a, b) : a <=> b;
        if (order != 0) return order;
    }
    return prerelease_.size() <=> other.prerelease_.size();
}
bool valid_identity(std::string_view s) noexcept {
    auto alnum = [](char c) { return digit(c) || (c >= 'a' && c <= 'z'); };
    if (s.empty() || s.size() > 255 || !alnum(s.front()) || !alnum(s.back())) return false;
    char previous = 0;
    for (auto c : s) {
        if ((!alnum(c) && c != '.' && c != '_' && c != '-') || (c == '.' && previous == '.')) return false;
        previous = c;
    }
    auto stem = s.substr(0, s.find('.'));
    return !(stem == "con" || stem == "prn" || stem == "aux" || stem == "nul" ||
        (stem.size() == 4 && (stem.starts_with("com") || stem.starts_with("lpt")) && stem[3] >= '1' && stem[3] <= '9'));
}

}
