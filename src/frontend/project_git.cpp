#include "project_git.hpp"
#include "project_git_process.hpp"
#include "void/path.hpp"
#include "void/project_resolution.hpp"
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Process.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/SHA1.h>
#include <llvm/Support/SHA256.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <fstream>
#include <utility>

namespace vd::project {
namespace {
namespace fs = std::filesystem;
using Objects = std::map<std::string, std::string, std::less<>>;
using Ids = std::set<std::string, std::less<>>;
constexpr std::size_t proof_limit = 8 * 1024 * 1024, source_limit = 64 * 1024 * 1024;
constexpr std::size_t entry_limit = 8192, object_store_entry_limit = 131072;

[[noreturn]] void fail(std::string message) { throw std::runtime_error(std::move(message)); }

template<class Bytes>
std::string hex(const Bytes& bytes) {
    std::string value;
    for (unsigned char byte : bytes) {
        value += "0123456789abcdef"[byte >> 4];
        value += "0123456789abcdef"[byte & 15];
    }
    return value;
}

bool hex_id(std::string_view id, std::size_t size) {
    return id.size() == size && id.find_first_not_of("0123456789abcdef") == id.npos;
}

llvm::StringRef view(std::string_view bytes) { return {bytes.data(), bytes.size()}; }

void field(llvm::SHA256& digest, std::string_view bytes) {
    char length[8];
    for (unsigned i = 0; i < 8; ++i)
        length[i] = static_cast<char>((std::uint64_t(bytes.size()) >> (56 - i * 8)) & 255);
    digest.update(llvm::StringRef(length, 8));
    digest.update(view(bytes));
}

void verify_object(std::string_view id, std::string_view type, std::string_view bytes) {
    llvm::SHA1 hash;
    hash.update(view(type));
    hash.update(" " + std::to_string(bytes.size()));
    hash.update(llvm::StringRef("\0", 1));
    hash.update(view(bytes));
    if (hex(hash.final()) != id) fail("Git object identity mismatch for " + std::string(id) + ".");
}

std::string_view line(std::string_view& input) {
    const auto end = input.find('\n');
    if (end == input.npos || end > 128) fail("Invalid Git object record.");
    const auto value = input.substr(0, end);
    input.remove_prefix(end + 1);
    return value;
}

struct Header { std::string_view id, type; std::size_t size; };
Header header(std::string_view& input) {
    auto value = line(input);
    if (value.ends_with(" missing")) fail("Pinned Git object is unavailable: " + std::string(value));
    const auto first = value.find(' '), last = value.rfind(' ');
    if (first == value.npos || first == last || !hex_id(value.substr(0, first), 40)) fail("Invalid Git object header.");
    std::size_t size = 0;
    const auto length = value.substr(last + 1);
    auto [end, error] = std::from_chars(length.data(), length.data() + length.size(), size);
    if (error != std::errc{} || end != length.data() + length.size()) fail("Invalid Git object length.");
    return {value.substr(0, first), value.substr(first + 1, last - first - 1), size};
}

std::string_view payload(std::string_view& input, Header record) {
    if (record.size >= input.size() || input[record.size] != '\n') fail("Truncated Git object record.");
    const auto data = input.substr(0, record.size);
    verify_object(record.id, record.type, data);
    input.remove_prefix(record.size + 1);
    return data;
}

void write_bytes(const fs::path& path, std::string_view data) {
    std::ofstream stream(path, std::ios::binary);
    stream.write(data.data(), static_cast<std::streamsize>(data.size()));
    stream.close();
    if (!stream) fail("Cannot write private Git acquisition workspace.");
}

// Read only the selected object database. No source-repository config, refs,
// alternates, worktree, hooks or transports participate.
CreationRoot local_objects(const fs::path& root, const GitPin& pin) {
    auto repository = inspect_creation_root(root / utf8_path(pin.origin.substr(5))).path;
    const auto dot_git = repository / ".git";
    if (fs::exists(fs::symlink_status(dot_git))) repository = inspect_creation_root(dot_git).path;
    const auto objects = inspect_creation_root(repository / "objects");
    std::size_t visited = 0;
    for (const auto& entry : fs::recursive_directory_iterator(objects.path)) {
        const auto name = fold_project_name(path_utf8(entry.path().filename()));
        const auto status = entry.symlink_status();
        if (++visited > object_store_entry_limit) fail("Local Git object-store inventory limit exceeded.");
        if (project_path_alias(entry.path()) || !(fs::is_regular_file(status) || fs::is_directory(status)) ||
            name == "alternates" || name == "http-alternates" || name.ends_with(".promisor"))
            fail("Local Git object store contains an alias, alternate, promisor or special entry.");
    }
    return objects;
}

class GitObjects {
    fs::path work, objects;
    std::string git;
    std::vector<std::string> isolated_settings;
    const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
public:
    explicit GitObjects(const fs::path& directory) : git(VOID_GIT_EXECUTABLE) {
        if (git.empty() || !utf8_path(git).is_absolute() || !fs::is_regular_file(utf8_path(git)))
            fail("No trusted Git executable is configured. Set VOID_GIT_EXECUTABLE at compiler configuration.");
        llvm::SmallString<256> path;
        if (auto error = llvm::sys::fs::createUniqueDirectory("void-git", path)) fail(error.message());
        work = utf8_path(path.str().str());
        try {
            work = fs::canonical(work);
            objects = directory.empty() ? work / "repo.git" / "objects" : directory;
            fs::create_directories(work / "repo.git" / "refs");
            if (directory.empty()) fs::create_directory(objects);
            write_bytes(work / "repo.git" / "HEAD", "ref: refs/heads/void\n");
            write_bytes(work / "repo.git" / "config", "[core]\n\trepositoryformatversion = 0\n\tbare = true\n");
            const auto unused = path_utf8(work / "no-config");
            isolated_settings = {
                "GIT_OBJECT_DIRECTORY=" + path_utf8(objects), "GIT_CONFIG_GLOBAL=" + unused,
                "GIT_CONFIG_SYSTEM=" + unused, "GIT_CONFIG_NOSYSTEM=1", "GIT_NO_REPLACE_OBJECTS=1",
                "GIT_NO_LAZY_FETCH=1", "GIT_PROTOCOL_FROM_USER=0", "LC_ALL=C"
            };
        } catch (...) { std::error_code ignored; fs::remove_all(work, ignored); throw; }
    }
    ~GitObjects() { std::error_code ignored; fs::remove_all(work, ignored); }
    GitObjects(const GitObjects&) = delete;

    void fetch(const GitPin& pin) const {
        auto settings = isolated_settings;
        const auto directory = path_utf8(work);
        const std::array<std::string, 10> transport_settings{
            "GIT_ALLOW_PROTOCOL=https", "GIT_TERMINAL_PROMPT=0", "GIT_ALLOC_LIMIT=134217728",
            "HOME=" + directory, "USERPROFILE=" + directory, "XDG_CONFIG_HOME=" + directory,
            "NETRC=" + path_utf8(work / "no-netrc"), "TMPDIR=" + directory,
            "TEMP=" + directory, "TMP=" + directory
        };
        settings.insert(settings.end(), transport_settings.begin(), transport_settings.end());
        // A caller may provide a local CA bundle. Git still verifies the server
        // certificate and hostname. Other Git, curl and TLS settings are scrubbed.
        if (const auto ca = llvm::sys::Process::GetEnv("GIT_SSL_CAINFO")) {
            const auto bundle = utf8_path(*ca);
            if (!bundle.is_absolute() || !fs::is_regular_file(bundle))
                fail("GIT_SSL_CAINFO must name an absolute path to a local CA bundle.");
            settings.push_back("GIT_SSL_CAINFO=" + *ca);
        }
        // Only the named remote may connect directly. Standalone HTTP
        // helpers inherit a proxy URL that Git rejects before connecting.
        const std::vector<std::string> args{
            git, "--git-dir=" + path_utf8(work / "repo.git"),
            "-c", "http.proxy=://", "-c", "remote.void-source.proxy=",
            "-c", "remote.void-source.url=" + pin.origin,
            "-c", "credential.helper=", "-c", "credential.interactive=false",
            "-c", "http.sslVerify=true", "-c", "http.schannelUseSSLCAInfo=true",
            "-c", "http.followRedirects=false", "-c", "http.emptyAuth=false",
            "-c", "http.lowSpeedLimit=1", "-c", "http.lowSpeedTime=10",
            "-c", "fetch.fsckObjects=true", "-c", "fetch.unpackLimit=1",
            "fetch", "--quiet", "--depth=1", "--no-tags", "--no-recurse-submodules",
            "--no-write-fetch-head", "--no-auto-gc", "--no-write-commit-graph", "--", "void-source", pin.revision
        };
        const int status = run_git_fetch(work, args, settings, std::min(deadline,
            std::chrono::steady_clock::now() + std::chrono::seconds(30)));
        if (status == 0) return;
        std::string diagnostic(2048, '\0');
        std::ifstream errors(work / "fetch-error", std::ios::binary);
        errors.read(diagnostic.data(), static_cast<std::streamsize>(diagnostic.size()));
        diagnostic.resize(static_cast<std::size_t>(errors.gcount()));
        if (diagnostic.find("credential url cannot be parsed: ://") != diagnostic.npos)
            fail("HTTPS Git refused a secondary HTTP download. Fetches may only use the declared remote.");
        if (diagnostic.find("unadvertised object") != diagnostic.npos ||
            diagnostic.find("not our ref") != diagnostic.npos ||
            diagnostic.find("couldn't find remote ref") != diagnostic.npos)
            fail("HTTPS Git remote did not supply the pinned commit. The commit may be unavailable or the "
                 "server may refuse exact object-ID requests for unadvertised commits.");
        for (char& c : diagnostic) if (c < ' ' || c > '~') c = ' ';
        while (!diagnostic.empty() && diagnostic.back() == ' ') diagnostic.pop_back();
        fail("HTTPS Git fetch failed (exit " + std::to_string(status) + ")." +
             (diagnostic.empty() ? std::string{} : " " + diagnostic));
    }

    Objects read(const Ids& ids, std::string_view type, std::size_t limit) const {
        if (ids.empty()) return {};
        std::string request;
        for (const auto& id : ids) request += id + "\n";
        write_bytes(work / "input", request);
        const auto input = path_utf8(work / "input"), output = path_utf8(work / "output");
        const auto git_dir = "--git-dir=" + path_utf8(work / "repo.git");
        std::vector<llvm::StringRef> env(isolated_settings.begin(), isolated_settings.end());
        env.emplace_back("GIT_ALLOW_PROTOCOL=");
        const std::array<std::optional<llvm::StringRef>, 3> redirects{input, output, llvm::StringRef("")};
        auto execute = [&](llvm::StringRef option) {
            write_bytes(work / "output", "");
            const std::array<llvm::StringRef, 4> args{git, git_dir, "cat-file", option};
            std::string error;
            const auto remaining = std::chrono::ceil<std::chrono::seconds>(deadline - std::chrono::steady_clock::now()).count();
            if (remaining <= 0) fail("Git snapshot acquisition time limit exceeded.");
            const auto timeout = static_cast<unsigned>(std::min<std::int64_t>(remaining, 30));
            if (llvm::sys::ExecuteAndWait(git, args, env, redirects, timeout, 0, &error) != 0)
                fail("Git object read failed" + (error.empty() ? std::string(".") : ": " + error));
        };
        execute("--batch-check");
        const auto metadata = read_project_input(work / "output", ids.size() * 128);
        auto headers = std::string_view(metadata);
        std::size_t bytes = 0;
        for (const auto& id : ids) {
            const auto record = header(headers);
            if (record.id != id || record.type != type) fail("Pinned Git object is not a " + std::string(type) + ".");
            if (record.size > limit - bytes) fail("Git snapshot object byte limit exceeded.");
            bytes += record.size;
        }
        if (!headers.empty()) fail("Unexpected Git object metadata.");
        execute("--batch");
        const auto data = read_project_input(work / "output", bytes + ids.size() * 129);
        auto records = std::string_view(data);
        Objects result;
        for (const auto& id : ids) {
            const auto record = header(records);
            if (record.id != id || record.type != type || record.size > bytes) fail("Git object changed during acquisition.");
            bytes -= record.size;
            result.emplace(id, payload(records, record));
        }
        if (!records.empty() || bytes) fail("Unexpected Git object bytes.");
        return result;
    }
};

void add_proof(std::string& proof, std::string_view id, std::string_view type, std::string_view data) {
    proof += std::string(id) + " " + std::string(type) + " " + std::to_string(data.size()) + "\n";
    proof.append(data);
    proof += '\n';
    if (proof.size() > proof_limit) fail("Git commit/tree proof byte limit exceeded.");
}

struct Inventory {
    std::map<std::string, std::string, std::less<>> blobs;
    Ids directories{""};
};

template<class ReadTrees>
Inventory inventory(std::string_view commit, ReadTrees read_trees) {
    if (!commit.starts_with("tree ") || commit.size() <= 45 || commit[45] != '\n' || !hex_id(commit.substr(5, 40), 40))
        fail("Invalid pinned Git commit tree.");
    Inventory result;
    std::map<std::string, std::string> pending{{"", std::string(commit.substr(5, 40))}};
    std::size_t depth = 0;
    while (!pending.empty()) {
        if (++depth > 64) fail("Git repository tree depth limit exceeded.");
        Ids ids;
        for (const auto& [path, id] : pending) ids.insert(id);
        const auto trees = read_trees(ids);
        auto current = std::exchange(pending, {});
        for (const auto& [parent, id] : current) {
            auto bytes = std::string_view(trees.at(id));
            Ids names;
            while (!bytes.empty()) {
                const auto space = bytes.find(' '), end = bytes.find('\0');
                if (space == bytes.npos || end == bytes.npos || end <= space || bytes.size() - end < 21)
                    fail("Invalid raw Git tree entry.");
                const auto mode = bytes.substr(0, space), name = bytes.substr(space + 1, end - space - 1);
                if (mode != "40000" && mode != "100644" && mode != "100755")
                    fail("Git snapshot contains an unsupported link, submodule or special entry.");
                const auto folded = fold_project_name(name);
                if (!portable_path_component(name) || name == "." || name == ".." || folded == ".git" ||
                    name.find('/') != name.npos || !names.insert(folded).second)
                    fail("Invalid Git source path or case collision in pinned tree.");
                const auto path = parent.empty() ? std::string(name) : parent + "/" + std::string(name);
                if (path.size() > 4096 || result.directories.size() + result.blobs.size() >= entry_limit)
                    fail("Git repository tree entry or path limit exceeded.");
                auto object = hex(bytes.substr(end + 1, 20));
                bytes.remove_prefix(end + 21);
                if (mode == "40000") {
                    result.directories.insert(path);
                    pending.emplace(path, std::move(object));
                } else result.blobs.emplace(path, std::move(object));
            }
        }
    }
    return result;
}

void finish(GitSnapshot& snapshot, const GitPin& pin) {
    llvm::SHA256 digest;
    field(digest, "void-git-snapshot-v1");
    field(digest, pin.origin);
    field(digest, pin.revision);
    field(digest, snapshot.proof);
    for (const auto& [path, data] : snapshot.files) {
        if (data.size() > source_limit - snapshot.bytes) fail("Git snapshot source byte limit exceeded.");
        snapshot.bytes += data.size();
        field(digest, path);
        field(digest, data);
    }
    snapshot.digest = hex(digest.final());
}

bool present(const fs::path& path) {
    std::error_code error;
    const auto status = fs::symlink_status(path, error);
    if (error == std::errc::no_such_file_or_directory) return false;
    if (error) throw fs::filesystem_error("Cannot inspect Git snapshot", path, error);
    return fs::exists(status);
}

void inventory_matches(const GitSnapshot& snapshot) {
    std::size_t seen = 1;
    const auto expected = snapshot.files.size() + snapshot.directories.size();
    for (const auto& item : fs::recursive_directory_iterator(snapshot.tree)) {
        const auto name = path_utf8(item.path().lexically_relative(snapshot.tree));
        const auto status = item.symlink_status();
        if (++seen > expected || project_path_alias(item.path()) ||
            (fs::is_directory(status) ? !snapshot.directories.contains(name) :
             !fs::is_regular_file(status) || !snapshot.files.contains(name)))
            fail("Git snapshot contains unexpected files, directories or aliases.");
    }
    if (seen != expected) fail("Git snapshot inventory is incomplete.");
}
}

std::string git_store_key(const GitPin& pin) {
    llvm::SHA256 digest;
    field(digest, "void-git-pin-v1");
    field(digest, pin.origin);
    field(digest, pin.revision);
    return hex(digest.final());
}

fs::path git_snapshot_path(const fs::path& root, const GitPin& pin) {
    return root / ".void" / git_store_name / git_store_key(pin);
}

GitSnapshot acquire_git_snapshot(const fs::path& root, const GitPin& pin) {
    if (!valid_git_pin(pin)) fail("Invalid Git acquisition pin.");
    const auto objects = pin.origin.starts_with("file:") ? std::optional(local_objects(root, pin)) : std::nullopt;
    GitObjects git(objects ? objects->path : fs::path{});
    if (!objects) git.fetch(pin);
    GitSnapshot result;
    auto commit = git.read({pin.revision}, "commit", manifest_byte_limit);
    add_proof(result.proof, pin.revision, "commit", commit.at(pin.revision));
    Objects seen_trees;
    const auto entries = inventory(commit.at(pin.revision), [&](const Ids& ids) {
        Ids missing;
        for (const auto& id : ids) if (!seen_trees.contains(id)) missing.insert(id);
        auto found = git.read(missing, "tree", proof_limit - result.proof.size());
        for (const auto& [id, data] : found) add_proof(result.proof, id, "tree", data);
        seen_trees.merge(found);
        Objects trees;
        for (const auto& id : ids) trees.emplace(id, seen_trees.at(id));
        return trees;
    });
    Ids ids;
    for (const auto& [path, id] : entries.blobs) ids.insert(id);
    const auto blobs = git.read(ids, "blob", source_limit);
    std::size_t bytes = 0;
    for (const auto& [path, id] : entries.blobs) {
        const auto& data = blobs.at(id);
        if (data.size() > source_limit - bytes) fail("Git snapshot source byte limit exceeded.");
        bytes += data.size();
        result.files.emplace(path, data);
    }
    if (objects && inspect_creation_root(objects->path).identity != objects->identity)
        fail("Local Git object directory changed during acquisition.");
    result.directories = entries.directories;
    finish(result, pin);
    result.tree = git_snapshot_path(root, pin) / result.digest / "tree";
    return result;
}

std::optional<GitSnapshot> read_git_snapshot(const fs::path& directory, const GitPin& pin) {
    // Check existing ancestors even when the pin itself is missing.
    for (const auto& ancestor : {directory.parent_path().parent_path(), directory.parent_path(), directory}) {
        if (!present(ancestor)) return {};
        (void)inspect_creation_root(ancestor);
    }
    auto leaf = fs::directory_iterator(directory);
    if (leaf == fs::directory_iterator{} || !hex_id(path_utf8(leaf->path().filename()), 64))
        fail("Git snapshot content index is incomplete or invalid.");
    const auto content = inspect_creation_root(leaf->path()).path;
    if (++leaf != fs::directory_iterator{}) fail("Git snapshot contains an unexpected content index.");
    Ids content_names;
    for (const auto& item : fs::directory_iterator(content)) {
        content_names.insert(path_utf8(item.path().filename()));
        if (content_names.size() > 2) break;
    }
    if (content_names != Ids{"proof", "tree"}) fail("Git snapshot contains unexpected content metadata.");
    GitSnapshot result;
    result.tree = inspect_creation_root(content / "tree").path;
    result.proof = read_project_input(content / "proof", proof_limit);
    auto records = std::string_view(result.proof);
    const auto commit = header(records);
    if (commit.id != pin.revision || commit.type != "commit" || commit.size > manifest_byte_limit)
        fail("Git snapshot commit proof does not match the pinned revision.");
    const auto commit_data = payload(records, commit);
    Objects trees;
    while (!records.empty()) {
        const auto record = header(records);
        if (record.type != "tree" || trees.size() >= entry_limit || trees.contains(record.id))
            fail("Invalid Git snapshot tree proof.");
        trees.emplace(record.id, payload(records, record));
    }
    Ids used;
    std::string canonical_proof;
    add_proof(canonical_proof, commit.id, "commit", commit_data);
    const auto entries = inventory(commit_data, [&](const Ids& ids) {
        Objects found;
        for (const auto& id : ids) {
            if (!trees.contains(id)) fail("Git snapshot tree proof is incomplete.");
            if (used.insert(id).second) add_proof(canonical_proof, id, "tree", trees.at(id));
            found.emplace(id, trees.at(id));
        }
        return found;
    });
    if (canonical_proof != result.proof) fail("Git snapshot tree proof is not canonical.");
    std::size_t bytes = 0;
    for (const auto& [path, id] : entries.blobs) {
        auto data = read_project_input(result.tree / utf8_path(path), source_limit - bytes);
        verify_object(id, "blob", data);
        bytes += data.size();
        result.files.emplace(path, std::move(data));
    }
    result.directories = entries.directories;
    inventory_matches(result);
    finish(result, pin);
    if (result.digest != path_utf8(content.filename())) fail("Git snapshot content digest mismatch.");
    return result;
}

void publish_git_snapshot(const CreationRoot& root, const GitPin& pin, const GitSnapshot& snapshot) {
    const auto prefix = snapshot.digest + "/";
    std::map<std::string, std::string_view> files{{prefix, {}}, {prefix + "proof", snapshot.proof}};
    for (const auto& [path, bytes] : snapshot.files) files.emplace(prefix + "tree/" + path, bytes);
    // Empty Git trees carry no usable source. Materialize them as empty
    // directories so the verified inventory is still exact.
    for (const auto& path : snapshot.directories)
        files.emplace(prefix + "tree/" + (path.empty() ? "" : path + "/"), std::string_view{});
    publish_git_snapshot(root, git_store_key(pin), files, [&](const fs::path& directory) {
        auto observed = read_git_snapshot(directory, pin);
        if (!observed || observed->digest != snapshot.digest) fail("Git snapshot changed during publication.");
    });
}
}
