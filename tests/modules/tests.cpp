#include "void/frontend.hpp"
#include "void/backend.hpp"
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
namespace {
namespace fs = std::filesystem;
void require(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
void write(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary); stream << text;
    require(bool(stream), "Cannot write module fixture");
}
struct Fixture {
    fs::path root;
    void file(const std::string& path, const std::string& text) { write(root / path, text); }
    vd::SourceProgram compile() const { return vd::compile_project(root / "main.void"); }
    void run(const std::vector<std::pair<int,int>>& cases, std::size_t count = 0) const {
        auto program = compile(); vd::require_source_entry(program.entry, program.module);
        require(program.module.source_path == program.entry.path(), "VFS identity replaced physical debug source");
        if (count) require(program.module.functions.size() == count, "Repeated imports compiled declarations twice");
        std::set<std::string> symbols;
        for (const auto& f : program.module.functions) {
            require(f.name == f.module_identity + "::" + f.source_name, "Lost function ownership");
            require(symbols.insert(f.name).second, "Colliding module symbols");
            require(!f.source.path.empty(), "Missing originating source");
        }
        vd::Jit jit;
        for (bool optimize : {false, true}) {
            auto code = jit.compile(program.module, program.module.entry_symbol, optimize);
            require(code->compatibility().function_id == "/main.void::entry", "Unstable source identity");
            for (auto [value, answer] : cases) require(code->invoke(value) == answer, "Wrong multi-file result");
        }
    }
    void rejects(const std::string& entry, const std::string& reason,
                 const std::string& file = "main.void", const std::string& position = "1:5") {
        this->file("main.void", entry);
        try { compile(); }
        catch (const std::invalid_argument& e) {
            const std::string message = e.what();
            require(message.find(reason) != std::string::npos, "Wrong module diagnostic: " + message);
            require(message.find(file + ":" + position + ": error:") != std::string::npos, "Wrong module location: " + message);
            require(message.find('^') != std::string::npos && message.find("LLVM") == std::string::npos, "Unhelpful module diagnostic: " + message);
            return;
        }
        throw std::runtime_error("Invalid module graph accepted: " + reason);
    }
};
const std::string entry_start = "func entry(x: i32) -> i32:\n    return ";
void positives(const fs::path& output) {
    Fixture f{output / "positive project"};
    f.file("lib/core.void", "private func hidden(x: i32) -> i32:\n    return x + 2\npublic func update(x: i32) -> i32:\n    return hidden(x)\n");
    f.file("other/core.void", "internal func update(x: i32) -> i32:\n    return x * 3\n");
    f.file("main.void", "use \"/lib/core.void\" as first\nuse \"./other/core.void\" as second\n" + entry_start + "first.update(x) + second.update(x)\n");
    f.run({{0,2},{1,6},{10,42},{-1,-2}}, 4);
    f.file("main.void", "use \"/lib/core.void\"\nuse \"./lib/./core.void\"\nuse \"/lib/../lib//core.void\" as core\n" + entry_start + "update(x) + core.update(x)\n");
    f.run({{0,4},{3,10}}, 3);
    // Matching filename stems do not make distinct simple functions ambiguous.
    f.file("one/core.void", "func first(x: i32) -> i32:\n    return x + 1\n");
    f.file("two/core.void", "func second(x: i32) -> i32:\n    return x + 2\n");
    f.file("main.void", "use \"/one/core.void\"\nuse \"/two/core.void\"\n" + entry_start + "first(x) + second(x)\n");
    f.run({{0,3},{5,13}}, 3);
    // Same module through multiple aliases remains one declaration owner.
    f.file("main.void", "use \"/lib/core.void\" as one\nuse \"/lib/core.void\" as one\nuse \"/lib/core.void\" as two\n" + entry_start + "one.update(x) + two.update(x)\n");
    f.run({{5,14}}, 3);
    // Relative imports are resolved by the importer, not the entry or cwd.
    f.file("deep/nested/helper.void", "use \"../../lib/core.void\"\nfunc nested(x: i32) -> i32:\n    return update(x)\n");
    f.file("main.void", "use \"/deep/nested/helper.void\"\n" + entry_start + "nested(x)\n");
    f.run({{0,2},{9,11}}, 4);
    // Declaration cycles and mutual recursion, with no runtime initialization.
    f.file("cycle/even.void", "use \"./odd.void\"\nfunc even(x: i32) -> bool:\n    if x == 0:\n        return true\n    return odd(x - 1)\n");
    f.file("cycle/odd.void", "use \"/cycle/even.void\"\nfunc odd(x: i32) -> bool:\n    if x == 0:\n        return false\n    return even(x - 1)\n");
    f.file("main.void", "use \"/cycle/even.void\" as parity\nfunc entry(x: i32) -> i32:\n    if parity.even(x):\n        return 1\n    return 0\n");
    f.run({{0,1},{1,0},{10,1},{11,0}}, 3);
    f.file("main.void", "use \"./main.void\"\nprivate func helper(x: i32) -> i32:\n    return x + 1\n" + entry_start + "main.helper(x)\n");
    f.run({{0,1},{2,3}}, 2);
    // Local declarations have precedence; imported collisions require qualification.
    f.file("main.void", "use \"/lib/core.void\"\nfunc update(x: i32) -> i32:\n    return x + 20\n" + entry_start + "update(x) + core.update(x)\n");
    f.run({{0,22},{2,26}}, 4);
    f.file("main.void", "use \"/lib/core.void\" as calc\nfunc entry(x: i32) -> i32:\n    var total: i32\n    for x > 0:\n        total += calc.update(x)\n        x--\n    return total\n");
    f.run({{0,0},{3,12}}, 3);
    f.file("folder with spaces/helper file.void", "func spaced(x: i32) -> i32:\n    return x + 6\n");
    f.file("main.void", "use \"./folder with spaces/helper file.void\" as helper\n" + entry_start + "helper.spaced(x)\n");
    f.run({{0,6}}, 2);
    // Cross-file argument evaluation and bool/no-result signatures.
    f.file("lib/typed.void", "func noop():\n    return\nfunc choose(a: bool, b: i32, c: bool) -> i32:\n    if a and c:\n        return b\n    return -b\nfunc risky(x: i32) -> bool:\n    return 10 / x > 0\n");
    f.file("main.void", "use \"/lib/typed.void\" as typed\nfunc entry(x: i32) -> i32:\n    typed.noop()\n    return typed.choose(x == 0 or typed.risky(x), x + 4, not x < 0 and true)\n");
    f.run({{0,4},{2,6},{-1,-3},{20,-24}}, 4);
    // An imported 'entry' must not replace the configured entry-file adapter.
    f.file("lib/entry.void", "func entry(x: i32) -> i32:\n    return 999\n");
    f.file("main.void", "use \"/lib/entry.void\" as library\n" + entry_start + "x + 5\n");
    f.run({{0,5},{10,15}}, 2);
    // Relocation/alias spelling do not change project-scoped function identity.
    auto before = f.compile(); vd::Jit jit;
    auto first = jit.compile(before.module, before.module.entry_symbol);
    vd::ReloadSlot slot(first);
    Fixture moved{output / "relocated project"};
    moved.file("lib/entry.void", "func entry(x: i32) -> i32:\n    return 999\n");
    moved.file("main.void", "use \"./lib/entry.void\" as changed_alias\n" + entry_start + "x + 8\n");
    auto after = moved.compile();
    auto second = jit.compile(after.module, after.module.entry_symbol);
    require(slot.replace(second) && slot.invoke(2) == 10 && first->invoke(2) == 7, "Module graph lease/replacement identity regression");
}
void negatives(const fs::path& output) {
    Fixture f{output / "negative project"};
    f.file("a.void", "private func hidden(x: i32) -> i32:\n    return x\nfunc update(x: i32) -> i32:\n    return hidden(x)\n");
    f.file("b.void", "func update(x: i32) -> i32:\n    return x * 2\n");
    f.rejects("use \"/missing.void\"\n", "Cannot find imported file");
    f.rejects("use \"./missing.void\"\n", "Cannot find imported file");
    f.rejects("use \"\"\n", "Invalid import path");
    f.rejects("use \"//a.void\"\n", "Invalid root-relative");
    f.rejects("use \"../outside.void\"\n", "escapes the project root");
    f.rejects("use \"/../outside.void\"\n", "escapes the project root");
    f.rejects("use \"./folder/../../a.void\"\n", "escapes the project root");
    f.rejects("use \"./a\\b.void\"\n", "Invalid import path");
    f.rejects("use \"math\"\n", "Package imports");
    f.rejects("use \"a.void\"\n", "Package imports");
    f.rejects("use \"C:/a.void\"\n", "OS filesystem paths");
    f.rejects("use \"/a.void/\"\n", "must name a .void");
    f.rejects("use \"/.\"\n", "must name a .void");
    f.rejects("use \"/a.VOID\"\n", "must name a .void");
    for (const auto& path : {"/con.void", "/bad?.void", "/bad:/x.void", "/bad./x.void", "/bad /x.void", "/caf\xc3\xa9.void"})
        f.rejects("use \"" + std::string(path) + "\"\n", "Non-portable module path");
    f.rejects("use /a.void\n", "quoted import path");
    f.rejects("use \"/a.void\n", "Unterminated import path");
    f.rejects("use \"/a.void\" as if\n", "lowercase identifier", "main.void", "1:18");
    f.rejects("use \"/a.void\" as same\nuse \"/b.void\" as same\n", "Conflicting import alias", "main.void", "2:18");
    f.rejects("use \"/a.void\" as entry\n" + entry_start + "x\n", "conflicts with a function", "main.void", "1:18");
    f.rejects("use \"/a.void\"\nuse \"/b.void\"\n" + entry_start + "update(x)\n", "Ambiguous imported function", "main.void", "4:12");
    f.rejects("use \"/a.void\" as one\n" + entry_start + "one.hidden(x)\n", "private", "main.void", "3:16");
    f.rejects("use \"/a.void\"\n" + entry_start + "hidden(x)\n", "private", "main.void", "3:12");
    f.rejects("use \"/a.void\" as one\n" + entry_start + "one.absent(x)\n", "Unknown imported function", "main.void", "3:16");
    f.rejects(entry_start + "absent.update(x)\n", "Unknown import qualifier", "main.void", "2:12");
    f.rejects("use \"/a.void\"\n" + entry_start + "update()\n", "Incorrect argument count", "main.void", "3:12");
    f.rejects("use \"/a.void\" as one\n" + entry_start + "one.update(true)\n", "Type mismatch", "main.void", "3:23");
    f.rejects("use \"/a.void\" as one\n" + entry_start + "update(x)\n", "Unknown function", "main.void", "3:12");
    f.rejects("use \"/a.void\" as one\nfunc entry(x: i32) -> i32:\n    var one = 1\n    return x\n", "shadowing", "main.void", "3:9");
    f.rejects("func f():\n    return\nuse \"/a.void\"\n", "Imports must precede", "main.void", "3:5");
    f.file("dup.void", "func same():\n    return\nfunc same():\n    return\n");
    f.rejects("use \"/dup.void\"\n", "Duplicate function", "dup.void", "3:6");
    f.file("middle.void", "use \"/a.void\"\nfunc wrapper(x: i32) -> i32:\n    return update(x)\n");
    f.rejects("use \"/middle.void\"\n" + entry_start + "update(x)\n", "Unknown function", "main.void", "3:12");
    f.file("broken.void", "func broken(x: i32) -> i32:\n    return unknown\n");
    f.rejects("use \"/broken.void\"\n", "Unknown identifier", "broken.void", "2:12");
    f.file("nested/bad.void", "use \"../../outside.void\"\n");
    f.rejects("use \"/nested/bad.void\"\n", "escapes the project root", "bad.void", "1:5");
    f.file("nested/missing.void", "use \"./absent.void\"\n");
    f.rejects("use \"/nested/missing.void\"\n", "Cannot find imported file", "missing.void", "1:5");
    f.file("syntax.void", "func f():\n    while true:\n        return\n");
    f.rejects("use \"/syntax.void\"\n", "for condition:", "syntax.void", "2:5");
    f.file("utf8.void", "# \xc0\xaf\n");
    f.rejects("use \"/utf8.void\"\n", "Invalid UTF-8", "utf8.void", "1:3");
    fs::create_directories(f.root / "directory.void");
    f.rejects("use \"/directory.void\"\n", "not a regular");
    f.file("Mixed.void", "func mixed():\n    return\n");
    f.rejects("use \"/mixed.void\"\n", "case mismatch");
    f.file("same/core.void", "func one():\n    return\n");
    f.file("other/core.void", "func two():\n    return\n");
    f.rejects("use \"/same/core.void\"\nuse \"/other/core.void\"\n" + entry_start + "core.one()\n", "Ambiguous import qualifier", "main.void", "4:12");
    f.rejects("use \"/same/core.void\"\nuse \"/other/core.void\"\nuse \"/same/core.void\" as core\n", "Conflicting import alias", "main.void", "3:26");
    // Test real filesystem facilities, reporting capability differences explicitly.
    f.file("collision.void", "func lower():\n    return\n");
    f.file("COLLISION.void", "func upper():\n    return\n");
    std::error_code ec;
    if (!fs::equivalent(f.root / "collision.void", f.root / "COLLISION.void", ec))
        f.rejects("use \"/collision.void\"\n", "case collision");
    else std::cout << "Case-only siblings cannot be created on this filesystem; exact-case rejection still tested\n";
    fs::remove(f.root / "symbolic.void", ec); ec.clear();
    fs::create_symlink(f.root / "a.void", f.root / "symbolic.void", ec);
    if (!ec) f.rejects("use \"/symbolic.void\"\n", "Symbolic links");
    else std::cout << "Host cannot create symlinks: " << ec.message() << '\n';
    fs::remove(f.root / "linked.void", ec); ec.clear();
    fs::create_hard_link(f.root / "a.void", f.root / "linked.void", ec);
    if (!ec) f.rejects("use \"/a.void\"\nuse \"/linked.void\"\n", "Hard-link aliases", "main.void", "2:5");
    else std::cout << "Host cannot create hard links: " << ec.message() << '\n';
    // An imported entry is not an entry-file declaration.
    f.file("only_entry.void", entry_start + "x\n");
    f.file("main.void", "use \"/only_entry.void\"\n");
    auto library = f.compile();
    try { vd::require_source_entry(library.entry, library.module); }
    catch (const std::invalid_argument& e) {
        require(std::string(e.what()).find("No entry function") != std::string::npos, "Wrong entry ownership diagnostic");
        return;
    }
    throw std::runtime_error("Imported entry used as CLI entry");
}
}
void module_tests(const std::filesystem::path& output) {
    positives(output); negatives(output);
    std::cout << "Multi-file ownership, cycles, visibility, paths, diagnostics and native JIT checks passed\n";
}
