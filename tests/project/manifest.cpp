#include "void/project.hpp"
#include "void/frontend.hpp"
#include "void/path.hpp"
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <cstdlib>
#ifdef __linux__
#include <cerrno>
#include <cstddef>
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#endif
namespace {
using namespace vd::project;
using Code = vd::DiagnosticCode;
std::size_t checks = 0;
void check(bool condition, const std::string& message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
const std::string application = R"(schema = 1
[project]
id = "org.example.orbit.sources"
name = "Orbit"
version = "1.2.3-beta.1+build.7"
kind = "application"
[application]
id = "org.example.orbit"
)";
const std::string library = R"(schema = 1
[project]
id = "org.example.utilities.sources"
name = "Utilities"
version = "1.0.0"
kind = "library"
[package]
id = "org.example.utilities"
name = "utilities"
)";
std::string replace(std::string text, const std::string& before, const std::string& after) {
    auto at = text.find(before);
    if (at == std::string::npos) throw std::runtime_error("bad test replacement: " + before);
    text.replace(at, before.size(), after); return text;
}
std::string quote(std::string_view input) {
    std::string out(1, char(34));
    for (unsigned char c : input) {
        if (c == 92 || c == 34) { out += char(92); out += char(c); }
        else if (c == 10) { out += char(92); out += 'n'; }
        else if (c == 13) { out += char(92); out += 'r'; }
        else if (c == 9) { out += char(92); out += 't'; }
        else out += char(c);
    }
    return out + char(34);
}
ParseResult parse(std::string text) {
    return parse_manifest(vd::SourceFile("/unreadable/void.project.toml", std::move(text)));
}
std::shared_ptr<const Manifest> good(const std::string& text) {
    auto result = parse(text);
    if (!result.succeeded()) throw vd::CompilationError(result.diagnostics);
    check(result.diagnostics.empty(), "success must have no diagnostics");
    return result.manifest;
}
ParseResult bad(const std::string& text, Code code) {
    auto first = parse(text), second = parse(text);
    check(!first.succeeded() && !second.succeeded(), "invalid manifest supplied metadata: " + text);
    check(!first.diagnostics.empty(), "missing diagnostic");
    bool found = false;
    check(first.diagnostics.size() == second.diagnostics.size(), "nondeterministic error count");
    std::size_t previous = 0;
    for (std::size_t i = 0; i < first.diagnostics.size(); ++i) {
        const auto& d = first.diagnostics[i];
        found |= d.code == code;
        check(d.primary.source && d.primary.source->text() == text, "diagnostic snapshot ownership");
        check(d.primary.begin <= d.primary.end && d.primary.end <= text.size(), "diagnostic range bounds");
        check(d.primary.begin >= previous, "diagnostic source order"); previous = d.primary.begin;
        check(!d.hint.empty(), "missing actionable hint");
        check(vd::render_diagnostic(d) == vd::render_diagnostic(second.diagnostics[i]), "nondeterministic rendered diagnostic");
    }
    check(found, "missing expected diagnostic category");
    return first;
}
void versions() {
    for (auto s : {"1.0.0", "0.1.0", "2.5.13", "1.2.3-beta.1", "1.2.3+build.7", "1.2.3-rc.2+build.9",
        "1.2.3-0", "1.2.3+000", "1.2.3-a-b.C", "999999999999999999999999999999.0.0"}) {
        auto parsed = parse_version(s); check(bool(parsed) && parsed->text() == s, "valid SemVer rejected");
    }
    for (auto s : {"", "1", "1.2", "1.2.3.4", "01.2.3", "1.02.3", "1.2.03", "1.0.0-01", "1.0.0-a.01",
        "1.2.3-", "1.2.3+", "1.2.3-a..b", "1.2.3+build..7", "1.2.3+a+b", "v1.2.3", "+1.2.3", "-1.2.3",
        " 1.2.3", "1.2.3 ", "1.2.3-α", "1.2.3+build_7", "1.2.3\n", "1.2.3-+x"})
        check(!parse_version(s), "invalid SemVer accepted");
    std::vector<std::string> order = {"1.0.0-alpha", "1.0.0-alpha.1", "1.0.0-alpha.beta", "1.0.0-beta", "1.0.0-beta.2",
        "1.0.0-beta.11", "1.0.0-rc.1", "1.0.0", "1.0.1", "1.1.0", "2.0.0", "10.0.0", "999999999999999999999999.0.0"};
    for (std::size_t i = 0; i < order.size(); ++i) for (std::size_t j = 0; j < order.size(); ++j) {
        auto a = parse_version(order[i]), b = parse_version(order[j]);
        check(a->compare_precedence(*b) == (i <=> j), "SemVer precedence mismatch");
    }
    check(!parse_version(std::string("1.2.3") + char(0)), "NUL version accepted");
    auto a = parse_version("1.0.0+a"), b = parse_version("1.0.0+b");
    check(a->compare_precedence(*b) == 0 && !(*a == *b), "build identity versus precedence");
    check(parse_version(std::string(251, '9') + ".0.0").has_value(), "255-byte version boundary");
    check(!parse_version(std::string(252, '9') + ".0.0"), "256-byte version limit");
    auto small = parse_version("1.0.0-999999999999999999999999"), big = parse_version("1.0.0-1000000000000000000000000");
    check(small->compare_precedence(*big) < 0, "prerelease numeric overflow");
}
void identities() {
    for (auto s : {"a", "0", "org.example.orbit", "a_b-c.7", "com0", "com10", "console", "nul-safe"}) check(valid_identity(s), "valid identity rejected");
    check(!valid_identity(std::string("a") + char(0) + "b"), "NUL identity accepted");
    check(valid_identity(std::string(255, 'a')) && !valid_identity(std::string(256, 'a')), "identity extent");
    for (auto s : {"", "Bad.ID", ".a", "a.", "a..b", "a/b", "a\\b", "a:b", "a b", "a~b", "_a", "a-", "é", "a\n", "con", "con.txt", "prn", "aux", "nul", "com1", "com9.x", "lpt1", "lpt9.x"}) {
        check(!valid_identity(s), "invalid identity accepted");
        bad(replace(application, "\"org.example.orbit\"", quote(s)), Code::project_identity);
    }
    // Equal text in different roles is expressly permitted, not a collision registry.
    auto same = good(replace(application, "org.example.orbit.sources", "org.example.orbit"));
    check(same->project().id == same->application()->id, "cross-role equality contract");
    for (auto s : {"main", "utilities", "_helpers", "math2"}) good(replace(library, "name = \"utilities\"", "name = \"" + std::string(s) + "\""));
    for (auto s : {"Upper", "1utilities", "func", "i32", "some-library", ""}) bad(replace(library, "name = \"utilities\"", "name = \"" + std::string(s) + "\""), Code::project_identity);
}
void schema() {
    auto app = good(application), lib = good(library);
    check(app->kind() == Kind::application && app->application() && !app->package(), "app shape");
    check(lib->kind() == Kind::library && lib->package() && !lib->application(), "library shape");
    check(!app->entry() && !lib->entry(), "mandatory or invented entry");
    check(app->application()->name == "Orbit" && app->application()->version == app->project().version, "application defaults");
    check(lib->package()->version == lib->project().version, "package version default");
    check(app->sources().roots == std::vector<std::string>{"."} && app->sources().exclude.size() == 5, "source defaults");
    check(app->debug().optimization == Optimization::none && app->debug().debug_info == DebugInfo::full &&
        app->release().optimization == Optimization::O2 && app->release().debug_info == DebugInfo::line, "profile defaults");
    auto settings = good(application + R"(name = "Orbit application"
version = "9.8.7+local"
[sources]
roots = ["src", "tools"]
exclude = []
[build.debug]
optimization = "O2"
debug_info = "none"
[build.release]
optimization = "none"
debug_info = "full"
[dependencies]
[native]
)");
    check(settings->application()->name == "Orbit application" && settings->application()->version.text() == "9.8.7+local", "optional app overrides");
    check(settings->project().version.text() == "1.2.3-beta.1+build.7", "role version conflation");
    check(settings->sources().roots.size() == 2 && settings->sources().exclude.empty(), "source settings");
    check(settings->debug().optimization == Optimization::O2 && settings->release().debug_info == DebugInfo::full, "profile settings");
    good(replace(application, "kind = \"application\"", "kind = \"application\"\nentry = \"game.void\""));
    for (auto s : {"schema = true", "schema = \"1\"", "schema = 1.0", "schema = 0", "schema = 2"}) bad(replace(application, "schema = 1", s), Code::project_schema);
    bad(replace(application, "schema = 1\n", ""), Code::project_schema);
    for (auto field : {"id", "name", "version", "kind"}) {
        auto text = application; auto start = text.find(std::string(field) + " = "); auto end = text.find('\n', start); text.erase(start, end - start + 1);
        bad(text, Code::project_field);
    }
    bad(replace(application, "[project]", "[wrong_project]"), Code::project_field);
    bad(replace(application, "[application]", "[wrong_application]"), Code::project_field);
    bad(application + "[package]\nid = 'org.example.lib'\nname = 'lib'\n", Code::project_field);
    bad(library + "[application]\nid = 'org.example.app'\n", Code::project_field);
    bad(replace(library, "kind = \"library\"", "kind = \"library\"\nentry = 'lib.void'"), Code::project_field);
    for (auto s : {"7", "true", "[]", "{}", "1979-05-27", "1.0"}) bad(replace(application, "name = \"Orbit\"", "name = " + std::string(s)), Code::project_field);
    bad(application + "[build.fast]\noptimization = 'O3'\n", Code::project_field);
    bad(application + "[build.debug]\noptimization = 'O3'\n", Code::project_field);
    bad(application + "[build.release]\ndebug_info = true\n", Code::project_field);
    bad(application + "[sources]\nroots = 'src'\n", Code::project_field);
    bad(application + "[sources]\nroots = []\n", Code::limit);
    bad(application + "[sources]\nexclude = [1]\n", Code::project_field);
    bad("build = []\n" + application, Code::project_field);
    bad("sources = true\n" + application, Code::project_field);
    bad("native = 2\n" + application, Code::project_field);
    bad(application + "[native.raylib]\nlink = 'dynamic'\n", Code::project_reserved);
    bad(application + "[dependencies]\nurl = 'https://invalid.example/never-fetch'\n", Code::project_reserved);
    const std::string dependency = R"([dependencies.geometry]
id = "org.example.geometry"
path = "../geometry"
version = ">=1.4.0, <2.0.0"
)";
    auto dependent = good(application + dependency);
    const auto& edge = dependent->dependencies().at("geometry");
    check(edge.id == "org.example.geometry" && edge.path == "../geometry", "local dependency declaration");
    check(edge.version.text() == ">=1.4.0,<2.0.0" &&
        edge.version.accepts(*parse_version("1.4.0+revision")) &&
        !edge.version.accepts(*parse_version("1.4.0-rc.1")) &&
        !edge.version.accepts(*parse_version("2.0.0")), "bounded stable requirement");
    check(good(library + "entry = 'src/api.void'\n")->package_entry() == "src/api.void", "package entry");
    check(!good(library)->package_entry(), "standalone library keeps optional package entry");
    bad(library + "entry = 'build/api.void'\n", Code::project_path);
    bad(application + "entry = 'src/api.void'\n", Code::project_field);
    bad(application + replace(dependency, "../geometry", "../.git/geometry"), Code::project_path);
    bad(application + replace(dependency, ">=1.4.0, <2.0.0", ">=1.4.0+metadata"), Code::project_version);
    bad(application + replace(dependency, ">=1.4.0, <2.0.0", ">1.4.99,<1.4.100"), Code::project_version);
    bad(application + dependency + "git = 'https://invalid.example/never-fetch'\n", Code::project_field);
    for (auto s : {"1.2", "01.2.3", "1.0.0-01", "1.0.0+"}) bad(replace(application, "1.2.3-beta.1+build.7", s), Code::project_version);
    for (auto s : {"", " ", "\\n", "\\u0085", "　"}) bad(replace(application, "name = \"Orbit\"", "name = \"" + std::string(s) + "\""), Code::project_identity);
    good(replace(application, "Orbit\"", std::string(128, 'a') + "\""));
    bad(replace(application, "Orbit\"", std::string(129, 'a') + "\""), Code::project_identity);
    good(replace(application, "Orbit\"", "🎮 Orbit\""));
    for (auto s : {"/abs.void", "../app.void", "a/../app.void", "./app.void", "a//app.void", "a/", "CON.txt/file.void", "con.void", "a?.void", "a\\\\b.void", "C:/app.void", "app.VOID", ""})
        bad(replace(application, "kind = \"application\"", "kind = \"application\"\nentry = \"" + std::string(s) + "\""), Code::project_path);
    for (auto root : {"libs", "src/build/inner", ".git", "foo/.void"})
        bad(application + "[sources]\nroots = ['" + std::string(root) + "']\n", Code::project_path);
    bad(application + "[sources]\nroots = ['.', 'src']\n", Code::project_path);
    bad(application + "[sources]\nroots = ['src', 'SRC']\n", Code::project_duplicate);
    bad(application + "[sources]\nexclude = ['a', 'a']\n", Code::project_duplicate);
    good(replace(application, "kind = \"application\"", "kind = \"application\"\nentry = \"a.CON/file.void\""));
    good(application + "[sources]\nroots = ['src', 'src2']\nexclude = ['absent', 'src/a']\n");
}
void managed_native_metadata() {
    const std::string requirement = R"([native-requirements.graphics]
id = "org.example.raylib"
version = "=6.0.0+local"
abi = 3
)";
    const auto manifest = good(application + requirement);
    const auto& selected = manifest->native_requirement();
    check(selected && selected->alias == "graphics" && selected->id == "org.example.raylib" &&
        selected->version.text() == "6.0.0+local" && selected->abi == 3, "managed native requirement identity");
    check(manifest->native().empty() && manifest->dependencies().empty() &&
        selected->version != manifest->project().version, "managed provider has its own identity");
    check(!good(application)->native_requirement(), "dependency-free manifest acquired a native requirement");
    const std::string manual = R"([native.linux]
descriptor = "native.descriptor"
library = "libprovider.so"
adapter = "libadapter.so"
[native.windows]
descriptor = "windows.descriptor"
library = "provider.dll"
adapter = "adapter.dll"
)";
    const auto manual_manifest = good(application + manual);
    check(!manual_manifest->native_requirement() && manual_manifest->native().size() == 2 &&
        manual_manifest->native().at("linux").library == "libprovider.so" &&
        manual_manifest->native().at("windows").adapter == "adapter.dll", "manual native target metadata");
    bad(library + requirement, Code::project_reserved);
    bad(application + manual + requirement, Code::project_reserved);
    bad(application + requirement + replace(requirement, ".graphics]", ".other]"), Code::project_reserved);
    bad(application + replace(requirement, ".graphics]", ".func]"), Code::project_identity);
    bad(application + replace(requirement, "org.example.raylib", "../provider"), Code::project_field);
    for (const auto* version : {"6.0.0", ">=6.0.0", "=06.0.0", "=6.0.0, <7.0.0"})
        bad(application + replace(requirement, "=6.0.0+local", version), Code::project_field);
    for (const auto* abi : {"0", "4", "-1", "3.0", "'3'", "true"})
        bad(application + replace(requirement, "abi = 3", "abi = " + std::string(abi)), Code::project_field);
    bad(application + requirement + "command = 'never-execute'\n", Code::project_field);

    const std::string hash(64, 'a');
    const auto identity = [&](std::uint64_t size) { return quote(std::to_string(size) + ":" + hash); };
    const std::string text = R"(format = 1
[native]
id = "org.example.raylib"
version = "6.0.0+local"
abi = 3
target = "x86_64-unknown-linux-gnu"
revision = "vendor-build-2"
descriptor = "native.descriptor"
library = "libprovider.so"
adapter = "libadapter.so"
)" + std::string("descriptor-identity = ") + identity(17) + "\n[native.runtime]\n" +
        "\"libprovider.so\" = " + identity(101) + "\n\"libadapter.so\" = " + identity(102) +
        "\n\"libvendor.so.1\" = " + identity(103) + "\n[native.link]\n\"libvendor.a\" = " + identity(31) +
        "\n[native.notices]\n\"LICENSE.txt\" = " + identity(19) + "\n";
    const auto catalog = parse_native_catalog(std::string(text));
    check(catalog.id == selected->id && catalog.version == selected->version && catalog.abi == selected->abi &&
        catalog.target == "x86_64-unknown-linux-gnu" && catalog.revision == "vendor-build-2", "catalog target identity");
    check(catalog.descriptor.name == "native.descriptor" && catalog.descriptor.size == 17 &&
        catalog.descriptor.sha256 == hash && catalog.library == "libprovider.so" &&
        catalog.adapter == "libadapter.so", "owned descriptor and provider metadata");
    check(catalog.runtime.size() == 3 && catalog.link.size() == 1 && catalog.notices.size() == 1 &&
        catalog.runtime.back().name == "libvendor.so.1" && catalog.link.front().name == "libvendor.a" &&
        catalog.notices.front().name == "LICENSE.txt", "catalog artifact roles");
    auto windows = replace(text, "x86_64-unknown-linux-gnu", "x86_64-pc-windows-msvc");
    windows = replace(replace(windows, "libprovider.so", "provider.dll"), "libprovider.so", "provider.dll");
    windows = replace(replace(windows, "libadapter.so", "adapter.dll"), "libadapter.so", "adapter.dll");
    windows = replace(replace(windows, "libvendor.so.1", "vendor.dll"), "libvendor.a", "vendor.lib");
    const auto windows_catalog = parse_native_catalog(windows);
    check(windows_catalog.target == "x86_64-pc-windows-msvc" && windows_catalog.library == "provider.dll" &&
        windows_catalog.link.front().name == "vendor.lib", "Windows target-specific artifact names");

    const auto reject = [&](const std::string& input) {
        bool rejected = false;
        try { (void)parse_native_catalog(input); }
        catch (const std::runtime_error&) { rejected = true; }
        check(rejected, "invalid managed catalog accepted");
    };
    reject(replace(text, "format = 1", "format = 2"));
    reject(replace(text, "x86_64-unknown-linux-gnu", "aarch64-apple-darwin"));
    reject(replace(text, "6.0.0+local", "=6.0.0"));
    reject(replace(text, "abi = 3", "abi = 9"));
    reject(replace(text, "native.descriptor", "../native.descriptor"));
    reject(replace(text, "\"libprovider.so\" = ", "\"another.so\" = "));
    reject(replace(text, "\"LICENSE.txt\" = ", "\"LIBVENDOR.SO.1\" = "));
    reject(replace(text, "\"LICENSE.txt\" = " + identity(19), ""));
    reject(replace(text, identity(17), identity(16385)));
    reject(replace(text, identity(19), quote("019:" + hash)));
    reject(replace(text, identity(19), quote("19:" + std::string(64, 'A'))));
    reject(replace(text, identity(19), quote("18446744073709551616:" + hash)));
    reject(replace(text, identity(19), identity(native_catalog_payload_limit)));
    check(parse_native_catalog(text + std::string(native_catalog_byte_limit - text.size(), ' ')).id == catalog.id,
        "catalog byte limit includes exact boundary");
    reject(text + std::string(native_catalog_byte_limit + 1 - text.size(), ' '));
    auto many = text;
    constexpr std::size_t files = 6;
    for (std::size_t i = files; i < native_catalog_file_limit; ++i)
        many += "\"notice-" + std::to_string(i) + ".txt\" = " + identity(1) + "\n";
    check(parse_native_catalog(many).notices.size() == 1 + native_catalog_file_limit - files,
        "catalog file limit includes exact boundary");
    reject(many + "\"overflow.txt\" = " + identity(1) + "\n");
}
void syntax_and_spans() {
    bad(application + "id = 'org.example.another'\n", Code::project_duplicate);
    bad(replace(application, "id = \"org.example.orbit.sources\"", "id = 'org.example.orbit.sources'\nid = 'org.example.other.sources'"), Code::project_duplicate);
    auto duplicate = bad("schema = 1\nschema = 1\n", Code::project_duplicate);
    check(duplicate.diagnostics.front().related.size() == 1, "duplicate original location absent");
    check(duplicate.diagnostics.front().related[0].span.begin == 0, "wrong first key range");
    check(duplicate.diagnostics.front().primary.source->location(duplicate.diagnostics.front().primary.begin).line == 2, "wrong duplicate line");
    auto tables = bad(application + "[application]\nid = 'another'\n", Code::project_duplicate);
    check(!tables.diagnostics.front().related.empty(), "table duplicate origin absent");
    bad("a.b = 1\na.b = 2\n", Code::project_duplicate);
    bad("a = 1\n[a.b]\n", Code::project_duplicate);
    bad("a = { b = 1, b = 2 }\n", Code::project_duplicate);
    // Regression: v3.4.0 table-header parsing previously assumed valid key input.
    for (auto text : {"[=roject]", "[=]", "[.]", "[[=]]", "a = { = 1 }", "a = { . = 1 }", "a = { b. = 1 }", "a..b = 1"})
        bad(text, Code::project_syntax);
    for (auto text : {"schema = [", "schema = \"unterminated", "schema = 1 unexpected", "[project", "schema = 9999999999999999999999999999999", "schema = 1\r"}) bad(text, Code::project_syntax);
    auto unicode = replace(application, "name = \"Orbit\"", "name = \"🎮é\"");
    unicode = replace(unicode, "version = \"1.2.3-beta.1+build.7\"", "version = \"bad\"");
    auto errors = bad(unicode, Code::project_version);
    check(errors.diagnostics.front().primary.begin == unicode.find("\"bad\""), "UTF-8 byte range mapping");
    std::string inline_bad = "schema = 1\nproject = { id = 'org.example.x', name = '🎮é', version = 'bad', kind = 'application' }\napplication = { id = 'org.example.a' }\n";
    auto inline_errors = bad(inline_bad, Code::project_version);
    check(inline_errors.diagnostics.front().primary.begin == inline_bad.find("'bad'"), "scalar columns after multibyte characters");
    std::string crlf; for (auto c : application) crlf += c == '\n' ? "\r\n" : std::string(1, c);
    auto bom = good("\xef\xbb\xbf" + crlf);
    check(bom->fields().at("schema").key.begin == 3, "BOM key source range");
    check(bom->fields().at("project.version").value.begin == bom->source().text().find("\"1.2.3-beta"), "CRLF source offsets");
    auto quoted = good(replace(application, "[project]", "[\"project\"]"));
    check(quoted->project().name == "Orbit", "quoted key contract");
    auto multiline = good(replace(application, "name = \"Orbit\"", "name = \"\"\"\nOrbit\"\"\""));
    check(multiline->project().name == "Orbit", "valid multiline TOML strings");
    auto held = parse(application); auto retained = held.manifest; held = {};
    check(retained->source().text() == application && retained->fields().at("project.id").value.source->text() == application, "owned config lifetime");
    try { (void)parse(std::string("schema = 1\n# ") + char(0xff)); check(false, "invalid UTF-8 accepted"); }
    catch (const vd::CompilationError& e) { check(e.diagnostics()[0].code == Code::utf8, "UTF-8 shared diagnostic"); }
}
void closeout_malformed_keys() {
    // Header/key grammar probes are independent of VOID's schema. Some mutations
    // are legal TOML; they still must be deterministic and never crash/assume.
    const std::vector<std::string> malformed = {
        "[=roject]", "[[=roject]]", "[.project]", "[[.project]]", "[project..name]",
        "[[project..name]]", "[project.]", "[[project.]]", "[project.=]", "[[project.=]]",
        "[project] extra", "[[project]", "[ [project]]", "[[[project]]]", "[",
        "[[", "[project", "[project.", "[[project.", "[\"unterminated]",
        "[['unterminated]]", "[\"bad\\q\"]", "a. = 1", "a..b = 1",
        "a = { = 1 }", "a = { [ = 1 }", "a = { a..b = 1 }", "a = { a. = 1 }",
        "[🎮]", "[\xef\xbb\xbfproject]"
    };
    for (const auto& text : malformed) {
        bad(text, Code::project_syntax);
        bad("\xef\xbb\xbf# 🎮\r\n" + text, Code::project_syntax);
    }
    for (unsigned value = 0; value < 128; ++value) {
        for (const auto& framing : std::vector<std::pair<std::string, std::string>>{
                {"[", "project]"}, {"[[", "project]]"}, {"[project.", "]"},
                {"a = { ", "key = 1 }"}}) {
            auto input = framing.first + char(value) + framing.second;
            auto a = parse(input), b = parse(input);
            check(!a.succeeded() && !b.succeeded(), "incomplete schema from key probe");
            check(a.diagnostics.size() == b.diagnostics.size(), "key-probe diagnostic count");
            for (std::size_t i = 0; i < a.diagnostics.size(); ++i) {
                const auto& d = a.diagnostics[i];
                check(d.primary.begin <= d.primary.end && d.primary.end <= input.size(), "key-probe range");
                check(vd::render_diagnostic(d) == vd::render_diagnostic(b.diagnostics[i]), "key-probe determinism");
            }
        }
    }
}
void closeout_ranges_and_ownership() {
    auto exact = [](const vd::Diagnostic& d, std::size_t begin, std::size_t end) {
        check(d.primary.begin == begin && d.primary.end == end, "exact primary half-open range");
    };
    auto header = bad("[=roject]", Code::project_syntax);
    exact(header.diagnostics.at(0), 1, 1);
    auto prefixed = bad("\xef\xbb\xbf# 🎮\r\n[=roject]", Code::project_syntax);
    exact(prefixed.diagnostics.at(0), 12, 12);
    auto position = prefixed.diagnostics[0].primary.source->location(12);
    check(position.line == 2 && position.column == 2, "BOM/CRLF/astral malformed-header position");
    auto duplicate = bad("schema = 1\nschema = 1\n", Code::project_duplicate);
    exact(duplicate.diagnostics[0], 11, 11);
    auto related = duplicate.diagnostics[0].related.at(0).span;
    check(related.begin == 0 && related.end == 6, "exact first duplicate key range");
    auto bom_duplicate = bad("\xef\xbb\xbfschema = 1\r\nschema = 1\r\n", Code::project_duplicate);
    exact(bom_duplicate.diagnostics[0], 15, 15);
    check(bom_duplicate.diagnostics[0].related.at(0).span.begin == 3 &&
        bom_duplicate.diagnostics[0].related.at(0).span.end == 9, "BOM first-key range");
    auto unknown = bad(replace(application, "schema = 1\n", "schema=1\nbogus=7\n"), Code::project_field);
    check(unknown.diagnostics.size() == 1, "isolated unknown field");
    exact(unknown.diagnostics[0], 9, 14);
    const std::string inline_input = "schema=1\nproject={id='org.a',name='🎮',version='bad',kind='application'}\napplication={id='org.app'}\n";
    auto version = bad(inline_input, Code::project_version);
    auto start = inline_input.find("'bad'");
    exact(version.diagnostics.at(0), start, start + 5);
    auto missing = bad("schema=1\n[project]\n", Code::project_field);
    check(missing.diagnostics.size() == 4, "required project fields");
    for (const auto& d : missing.diagnostics) exact(d, 19, 19);
    auto quoted = good(replace(application, "version = \"1.2.3-beta.1+build.7\"", "\"ver\\u0073ion\" = \"1.2.3-beta.1+build.7\""));
    const auto& field = quoted->fields().at("project.version");
    check(quoted->source().text().substr(field.key.begin, field.key.end-field.key.begin) == "\"ver\\u0073ion\"", "escaped quoted key exact raw span");
    check(quoted->source().text().substr(field.value.begin, field.value.end-field.value.begin) == "\"1.2.3-beta.1+build.7\"", "quoted value exact raw span");
    auto multiline = good(replace(application, "name = \"Orbit\"", "name = \"\"\"\nOrbit\"\"\""));
    const auto& name_span = multiline->fields().at("project.name").value;
    check(name_span.source->text().substr(name_span.begin, name_span.end-name_span.begin) == "\"\"\"\nOrbit\"\"\"", "multiline exact half-open span");

    std::string original;
    std::shared_ptr<const Manifest> owned;
    {
        std::string bytes = application + "name = '" + std::string(80, 'n') + "'\nversion = '123456789012345678901234567890.2.3-rc.100+build.9'\n[sources]\nroots = ['" + std::string(70, 's') + "']\n";
        original = bytes;
        vd::SourceFile source("ephemeral/void.project.toml", bytes);
        auto parsed = parse_manifest(source);
        check(parsed.succeeded(), "owned manifest fixture");
        owned = parsed.manifest;
        bytes.assign(bytes.size(), '!');
        source = vd::SourceFile("replacement", "changed");
        parsed = {};
    }
    std::vector<std::string> churn(200, std::string(original.size(), '?'));
    check(owned->project().id == "org.example.orbit.sources" && owned->application()->name == std::string(80, 'n'), "owned identities after input destruction");
    check(owned->application()->version.core()[0] == "123456789012345678901234567890" &&
        owned->application()->version.prerelease() == std::vector<std::string>{"rc", "100"} &&
        owned->application()->version.build() == std::vector<std::string>{"build", "9"}, "owned version parts after TOML tree destruction");
    check(owned->sources().roots == std::vector<std::string>{std::string(70, 's')} && owned->source().text() == original, "owned roots and source after input replacement");
    auto copied_field = owned->fields().at("application.name");
    std::weak_ptr<const vd::SourceFile> weak = copied_field.value.source;
    owned.reset();
    check(!weak.expired() && copied_field.value.source->text() == original, "field independently retains snapshot after manifest destruction");
    vd::Diagnostic copied_error{Code::project_field};
    {
        auto temporary = parse(replace(application, "version = \"1.2.3-beta.1+build.7\"", "version = 'bad'"));
        check(!temporary.succeeded(), "owned diagnostic fixture");
        copied_error = temporary.diagnostics.at(0);
    }
    check(copied_error.code == Code::project_version && !vd::render_diagnostic(copied_error).empty(), "diagnostic survives result and source destruction");
    std::string semver_input = "123456789012345678901234567890.2.3-rc.100+build.9";
    auto semver = parse_version(semver_input);
    semver_input.assign(semver_input.size(), '!');
    check(semver && semver->text() == "123456789012345678901234567890.2.3-rc.100+build.9" && semver->build()[1] == "9", "standalone SemVer owns its input");
}
void closeout_bounds_and_precedence() {
    const auto prefix = application + "# 🎮\n";
    good(prefix + std::string(manifest_byte_limit-prefix.size(), ' '));
    auto oversized = bad(prefix + std::string(manifest_byte_limit+1-prefix.size(), ' '), Code::limit);
    check(oversized.diagnostics.size() == 1 && oversized.diagnostics[0].primary.begin == manifest_byte_limit+1 &&
        oversized.diagnostics[0].primary.end == manifest_byte_limit+1, "oversize EOF diagnostic and no TOML work");
    for (std::size_t count : {32u, 33u, 256u, 257u}) {
        bool roots = count < 100;
        std::string paths = application + "[sources]\n" + (roots ? "roots" : "exclude") + " = [";
        for (std::size_t i = 0; i < count; ++i) paths += (i ? "," : "") + std::string("'source") + std::to_string(i) + "'";
        paths += "]\n";
        if (count == 32 || count == 256) good(paths); else bad(paths, Code::limit);
    }
    for (std::size_t count : {128u, 129u}) {
        auto bytes = application;
        for (std::size_t i = 0; i < count; ++i) bytes += "unknown" + std::to_string(i) + "=0\n";
        auto parsed = bad(bytes, count == 128 ? Code::project_field : Code::limit);
        check(parsed.diagnostics.size() == count, "diagnostic cap edge");
        check((parsed.diagnostics.back().code == Code::limit) == (count > 128), "overflow notice only beyond cap");
    }
    check(parse_version("0.0.0-" + std::string(249, 'a')).has_value() &&
        !parse_version("0.0.0-" + std::string(250, 'a')), "prerelease byte-cap edge");
    check(parse_version("0.0.0+" + std::string(249, '0')).has_value() &&
        !parse_version("0.0.0+" + std::string(250, '0')), "build byte-cap edge, leading zero allowed");
    std::string emoji;
    for (unsigned i = 0; i < 128; ++i) emoji += "🎮";
    good(replace(application, "name = \"Orbit\"", "name = " + quote(emoji)));
    bad(replace(application, "name = \"Orbit\"", "name = " + quote(emoji + "🎮")), Code::project_identity);
    const std::vector<std::string> ordered = {"1.0.0-0", "1.0.0-1", "1.0.0-9999999999999999999999999",
        "1.0.0--", "1.0.0-A", "1.0.0-Z", "1.0.0-a", "1.0.0-a.0", "1.0.0-a.1", "1.0.0-a.A",
        "1.0.0-a.a", "1.0.0-alpha", "1.0.0-alpha.1", "1.0.0-alpha.beta", "1.0.0-beta", "1.0.0-beta.2",
        "1.0.0-beta.11", "1.0.0-rc.1", "1.0.0", "2.0.0", "999999999999999999999999999999999999.0.0"};
    std::vector<SemanticVersion> parsed;
    for (const auto& value : ordered) { auto v = parse_version(value); check(v.has_value(), "precedence fixture valid"); parsed.push_back(*v); }
    for (std::size_t i = 0; i < parsed.size(); ++i) for (std::size_t j = 0; j < parsed.size(); ++j) {
        check(parsed[i].compare_precedence(parsed[j]) == (i <=> j), "independent precedence order");
        check((parsed[i].compare_precedence(parsed[j]) < 0) == (parsed[j].compare_precedence(parsed[i]) > 0), "precedence antisymmetry");
    }
    for (std::size_t i = 0; i+2 < parsed.size(); ++i)
        check(parsed[i].compare_precedence(parsed[i+1]) < 0 && parsed[i+1].compare_precedence(parsed[i+2]) < 0 &&
            parsed[i].compare_precedence(parsed[i+2]) < 0, "precedence transitivity");
    auto plain = parse_version("1.0.0-rc.2"), one = parse_version("1.0.0-rc.2+001"), two = parse_version("1.0.0-rc.2+ABC");
    check(plain->compare_precedence(*one) == 0 && one->compare_precedence(*two) == 0 &&
        !(*plain == *one) && !(*one == *two), "build metadata equivalence versus exact identity");
}
void mutations() {
    // Deterministic edits to independent TOML text exercise poison/range handling.
    // Success is allowed; a failed result must never contain validated metadata.
    for (std::size_t i = 0; i < application.size(); ++i) {
        for (char replacement : {'=', '[', '\n'}) {
            auto changed = application; changed[i] = replacement;
            auto result = parse(changed);
            check(result.succeeded() == result.diagnostics.empty(), "mutation success/error gate");
            for (const auto& diagnostic : result.diagnostics) {
                check(diagnostic.primary.begin <= diagnostic.primary.end && diagnostic.primary.end <= changed.size(), "mutation source bounds");
                check(!vd::render_diagnostic(diagnostic).empty(), "mutation diagnostic render");
            }
        }
    }
}
void limits_and_authority() {
    good(application + std::string(manifest_byte_limit - application.size(), ' '));
    bad(application + std::string(manifest_byte_limit + 1 - application.size(), ' '), Code::limit);
    for (auto key : {std::string("unknown = ") + std::string(100, '[') + "0" + std::string(100, ']'),
        std::string("[a.b.c.d.e.f.g.h.i]\n"), std::string("a.b.c.d.e.f.g.h.i = 0\n")}) bad(key, Code::limit);
    std::string many = application + "[sources]\nroots = [";
    for (int i = 0; i < 33; ++i) many += (i ? "," : "") + std::string("'src") + std::to_string(i) + "'";
    bad(many + "]\n", Code::limit);
    std::string errors = application;
    for (int i = 0; i < 180; ++i) errors += "unknown" + std::to_string(i) + " = 0\n";
    auto capped = bad(errors, Code::limit);
    check(capped.diagnostics.size() == manifest_diagnostic_limit + 1, "bounded diagnostic output");
    auto literal = good(replace(application, "name = \"Orbit\"", "name = '$(touch NEVER_EXECUTE); ${HOME}'") + "[sources]\nroots = ['${HOME}', '~', 'https']\n");
    check(literal->project().name == "$(touch NEVER_EXECUTE); ${HOME}" && literal->sources().roots[0] == "${HOME}", "configuration interpolation/execution");
    bad(application + "[hooks]\nrun = 'touch NEVER_EXECUTE'\n", Code::project_field);
    // The input path is never opened. Even a name that looks like installed trust
    // metadata only supplies owned bytes, not native-provider or filesystem access.
    auto memory = parse_manifest(vd::SourceFile("/forbidden/void-native.txt", application));
    check(memory.succeeded(), "parser attempted host-path admission/IO");
}
#ifdef __linux__
void deny_authority() {
    // A violation kills this test. Parsing must not open files, mutate namespaces,
    // start processes/threads, connect to a network or invoke a native loader.
    std::vector<int> denied = {SYS_openat, SYS_execve, SYS_socket, SYS_socketpair, SYS_connect,
        SYS_sendto, SYS_sendmsg, SYS_clone, SYS_unlinkat, SYS_renameat, SYS_mkdirat,
        SYS_linkat, SYS_symlinkat, SYS_mknodat, SYS_fchmodat, SYS_fchownat, SYS_truncate, SYS_ftruncate};
#ifdef SYS_execveat
    denied.push_back(SYS_execveat);
#endif
#ifdef SYS_fork
    denied.push_back(SYS_fork);
#endif
#ifdef SYS_vfork
    denied.push_back(SYS_vfork);
#endif
#ifdef SYS_creat
    denied.push_back(SYS_creat);
#endif
#ifdef SYS_unlink
    denied.push_back(SYS_unlink);
#endif
#ifdef SYS_rename
    denied.push_back(SYS_rename);
#endif
#ifdef SYS_mkdir
    denied.push_back(SYS_mkdir);
#endif
#ifdef SYS_rmdir
    denied.push_back(SYS_rmdir);
#endif
#ifdef SYS_link
    denied.push_back(SYS_link);
#endif
#ifdef SYS_symlink
    denied.push_back(SYS_symlink);
#endif
#ifdef SYS_openat2
    denied.push_back(SYS_openat2);
#endif
#ifdef SYS_renameat2
    denied.push_back(SYS_renameat2);
#endif
#ifdef SYS_open
    denied.push_back(SYS_open);
#endif
#ifdef SYS_clone3
    denied.push_back(SYS_clone3);
#endif
    std::vector<sock_filter> filter;
    filter.push_back(BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(seccomp_data, nr)));
    for (auto syscall : denied) {
        filter.push_back(BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, static_cast<unsigned>(syscall), 0, 1));
        filter.push_back(BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS));
    }
    filter.push_back(BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW));
    sock_fprog program{static_cast<unsigned short>(filter.size()), filter.data()};
    check(prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == 0 && prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &program) == 0, "install negative authority filter");
}
#endif
}
int main(int argc, char** argv) {
    const auto mode = argc == 2 ? std::string_view(argv[1]) : std::string_view{};
    bool control = mode.starts_with("--deny-");
    bool sandbox = mode == "--sandbox" || control;
    try {
#ifdef __linux__
        if (sandbox) deny_authority();
        if (control) {
            if (mode == "--deny-file") (void)syscall(SYS_openat, AT_FDCWD, "/MUST_NOT_OPEN", O_RDONLY);
            else if (mode == "--deny-process") (void)syscall(SYS_execve, "/MUST_NOT_EXECUTE", nullptr, nullptr);
            else if (mode == "--deny-network") (void)syscall(SYS_socket, AF_INET, SOCK_STREAM, 0);
            else if (mode == "--deny-mutation") (void)syscall(SYS_mkdirat, AT_FDCWD, "/MUST_NOT_CREATE", 0700);
            std::_Exit(99); // Every negative control must instead die with SIGSYS.
        }
#else
        check(!sandbox, "syscall sandbox is Linux-only");
#endif
        versions(); identities(); schema(); managed_native_metadata(); syntax_and_spans(); limits_and_authority(); mutations(); closeout_malformed_keys(); closeout_ranges_and_ownership(); closeout_bounds_and_precedence();
        std::cout << "project_manifest: " << checks << " checks passed" << (sandbox ? " under syscall denial" : "") << '\n';
        if (sandbox) {
            std::cout.flush();
            // LeakSanitizer itself reads procfs/spawns a helper at exit. The normal
            // (unfiltered) test independently retains fatal ASan/UBSan/leak checks.
            std::_Exit(0);
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "project_manifest: " << e.what() << '\n';
        if (sandbox) { std::cerr.flush(); std::_Exit(1); }
        return 1;
    }
}
