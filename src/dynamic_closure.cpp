#include "void/c_dynamic.hpp"
#include "void/project.hpp"
#include "void/path.hpp"

#include <algorithm>
#include <charconv>
#include <set>
#include <span>
#include <stdexcept>

namespace vd {
namespace {
[[noreturn]] void bad(std::string_view message) {
    throw std::invalid_argument("C dynamic closure: " + std::string(message));
}
bool digest(std::string_view token) {
    return token.size() == 64 && std::all_of(token.begin(), token.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}
bool component(std::string_view token) {
    return token.size() <= 96 && token.starts_with("lib") && token.ends_with(".so") &&
        project::valid_identity(token);
}
bool symbol(std::string_view token) {
    auto letter = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; };
    return !token.empty() && token.size() <= 128 && letter(token.front()) &&
        std::all_of(token.begin(), token.end(), [&](char c) { return letter(c) || (c >= '0' && c <= '9'); });
}
bool library_name(std::string_view name, bool windows) {
    return !name.empty() && name.size() <= 128 && portable_path_component(name) &&
        name.front() != '.' && std::all_of(name.begin(), name.end(), [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
        }) && (windows ? name.ends_with(".dll") : name.find(".so") != name.npos);
}
std::vector<std::string_view> words(std::string_view line) {
    std::vector<std::string_view> result;
    while (!line.empty()) {
        auto end = line.find(' ');
        auto token = line.substr(0, end);
        if (token.empty() || result.size() == 11) bad("noncanonical row");
        result.push_back(token);
        if (end == line.npos) return result;
        line.remove_prefix(end + 1);
    }
    bad("empty row or trailing space");
}
}

DynamicClosure::Owner DynamicClosure::parse(std::string_view bytes) {
    if (bytes.empty() || bytes.size() > 32768 || bytes.back() != '\n' ||
        std::any_of(bytes.begin(), bytes.end(), [](char c) { return c != '\n' && (c < ' ' || c > '~'); }))
        bad("invalid size or encoding");
    auto plan = std::shared_ptr<DynamicClosure>(new DynamicClosure);
    plan->bytes_ = bytes;
    auto next = [&] {
        auto end = bytes.find('\n');
        if (end == bytes.npos) bad("incomplete header");
        auto line = bytes.substr(0, end);
        bytes.remove_prefix(end + 1);
        return line;
    };
    const auto header = next();
    const bool managed = header == "void-native-managed 1";
    const bool conventional = managed || header == "void-native-conventional 1";
    if (!conventional && header != "void-native-closure 1") bad("unsupported revision");
    auto provider = words(next());
    if (provider.size() != 2 || provider[0] != "provider" || !project::valid_identity(provider[1]))
        bad("invalid provider");
    plan->provider_ = provider[1];
    const auto target = next();
    const bool windows = conventional && target == "target x86_64-pc-windows-msvc msvc-x64";
    if (!windows && target != "target x86_64-unknown-linux-gnu sysv-x64")
        bad("unsupported target ABI");
    plan->target_ = windows ? "x86_64-pc-windows-msvc" : "x86_64-unknown-linux-gnu";
    if (next() != (conventional ? "system os" : "system none") ||
        next() != "layout executable-sibling-native") bad("unsupported target, system ABI or relocation policy");
    auto thunk = words(next());
    if (thunk.size() != 3 || thunk[0] != "thunk" || !digest(thunk[1]) ||
        (thunk[2] != transport && (!conventional ||
            (thunk[2] != "vd_c_primitive_dispatch_v1" && thunk[2] != "vd_c_primitive_dispatch_v2"))))
        bad("invalid target thunk identity");
    plan->thunk_ = thunk[1];
    plan->dispatch_ = thunk[2];
    if (conventional) {
        const auto row = words(next());
        if (row.size() != 3 || row[0] != "libraries" || !library_name(row[1], windows) || !library_name(row[2], windows))
            bad("expected provider and adapter filenames");
        plan->library_ = row[1];
        plan->adapter_ = row[2];
    }
    std::set<std::string_view> images, exports;
    std::set<std::string> payload_names;
    std::uint64_t payload_bytes = 0;
    for (auto line = next(); line != "descriptor"; line = next()) {
        auto row = words(line);
        if (!conventional && row[0] == "image") {
            if (row.size() < 3 || !plan->exports_.empty() || plan->images_.size() == 8 ||
                !component(row[1]) || images.contains(row[1]) || !digest(row[2]))
                bad("invalid or duplicate image");
            Image image{std::string(row[1]), std::string(row[2]), {}};
            std::set<std::string_view> needed;
            for (auto item : std::span(row).subspan(3)) {
                if (!images.contains(item) || !needed.insert(item).second) bad("invalid dependency order or duplicate edge");
                image.needed.emplace_back(item);
            }
            images.insert(row[1]);
            plan->images_.push_back(std::move(image));
        } else if (managed && row[0] == "payload") {
            std::uint64_t size = 0;
            if (row.size() != 4 || plan->payload_.size() == payload_count_limit || !plan->exports_.empty() ||
                row[1].size() > 128 || !portable_path_component(row[1]) ||
                row[1].front() == '.' || row[1].find('/') != row[1].npos || !digest(row[2]))
                bad("invalid managed payload record");
            auto name = std::string(row[1]);
            for (char& c : name) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
            const auto [end, error] = std::from_chars(row[3].data(), row[3].data() + row[3].size(), size);
            if (error != std::errc{} || end != row[3].data() + row[3].size() || !size ||
                size > payload_limit - payload_bytes || !payload_names.insert(name).second)
                bad("invalid managed payload size or duplicate name");
            payload_bytes += size;
            plan->payload_.push_back({std::string(row[1]), std::string(row[2]), size});
        } else if (row[0] == "export") {
            if (row.size() != 3 || plan->exports_.size() == 128 ||
                (conventional ? row[1] != plan->library_ : !images.contains(row[1])) ||
                !symbol(row[2]) || !exports.insert(row[2]).second)
                bad("invalid or conflicting required export");
            plan->exports_.push_back({std::string(row[1]), std::string(row[2])});
        } else bad("unknown row");
    }
    if (conventional) {
        if (plan->exports_.empty()) bad("missing provider exports");
        if (managed)
            for (const auto* filename : {&plan->library_, &plan->adapter_})
                if (std::none_of(plan->payload_.begin(), plan->payload_.end(),
                                 [&](const auto& file) { return file.name == *filename; }))
                    bad("managed payload omits provider or adapter");
    } else {
        if (plan->images_.empty())
            bad("missing closure or exact revision 3 descriptor");
        const auto& main = plan->images_.back().name;
        if (std::none_of(plan->exports_.begin(), plan->exports_.end(), [&](const auto& e) {
            return e.image == main && e.symbol == transport;
        })) bad("missing main-image transport");
        std::set<std::string_view> reachable{main};
        for (auto it = plan->images_.rbegin(); it != plan->images_.rend(); ++it)
            if (reachable.contains(it->name)) reachable.insert(it->needed.begin(), it->needed.end());
        if (reachable.size() != images.size()) bad("unreachable image");
    }
    const auto descriptor = bytes;
    const auto revision = plan->dispatch_.back();
    const auto expected = std::string("void-c-primitive ") + revision + " " + revision;
    if (bytes.size() > 16384 || next() != expected || next() != "provider " + plan->provider_ || next() != target)
        bad("exact descriptor provider or target mismatch");
    if (!next().starts_with("source ") || next() != "thunk " + plan->thunk_)
        bad("exact descriptor thunk mismatch");
    plan->descriptor_ = descriptor;
    return plan;
}
} // namespace vd
