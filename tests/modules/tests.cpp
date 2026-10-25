#include "void/frontend.hpp"
#include "void/backend.hpp"
#include "void/runtime.hpp"
#include "void/tooling.hpp"
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/Error.h>
#include <fstream>
#include <iostream>
#include <set>
#include <algorithm>
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
void private_linkage(const fs::path& output) {
    Fixture f{output / "private linkage"};
    const std::string helper="private func helper(x: i32) -> i32:\n    return x + 2\n";
    f.file("lib.void", "private func snapshot(values: [i32, 3], x: i32) -> [i32, 3]:\n    values[1] = x\n    return values\npublic func imported(x: i32) -> i32:\n    var original: [i32, 3] = [10, 20, 30]\n    var changed = snapshot(original, x)\n    return changed[0] + changed[1] + original[1]\ninternal func project(x: i32) -> i32:\n    return x + 3\n");
    const auto main=std::string("use \"/lib.void\" as library\n")+helper+
        "func entry(x: i32) -> i32:\n    return library.imported(helper(x)) + library.project(x)\n";
    f.file("main.void",main);
    f.run({{0,35},{7,49},{-1,33}},5);
    auto original=f.compile();
    vd::Jit jit;
    for(bool optimized:{false,true}) {
        auto runtime=std::make_shared<vd::Runtime>(1);
        // The selected host entry need not be the module's default entry, and
        // explicit selection of a private function must remain lookup-visible.
        auto selected=jit.compile(original.module,"/main.void::helper",optimized,{},runtime);
        require(selected->invoke(7)==9,"private selected JIT entry is not resolvable");
        auto imported=jit.compile(original.module,"/lib.void::imported",optimized,{},runtime);
        require(imported->invoke(7)==37,"imported JIT entry is not resolvable");
        auto old=jit.compile(original.module,original.module.entry_symbol,optimized,{},runtime);
        vd::ReloadSlot slot(old);
        auto updated=main;updated.replace(updated.find("x + 2"),5,"x + 5");
        f.file("main.void",updated);auto replacement=f.compile();
        require(slot.replace(jit.compile(replacement.module,replacement.module.entry_symbol,optimized,{},runtime)),"private helper replacement rejected");
        require(slot.invoke(7)==52 && old->invoke(7)==49,"private helper generation identity changed");
        f.file("main.void",main);
        {vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);
            require(runtime->collect()==vd::Runtime::Collection::completed && !runtime->snapshot().objects,"private array calls retained temporary roots");}
        const auto mode=optimized ? "optimized" : "normal";
        vd::emit_object(original.module,f.root/(std::string(mode)+".o"),optimized,true);
        vd::emit_runtime_object(original.module,f.root/(std::string(mode)+"-library.o"),optimized);
        auto object=llvm::object::ObjectFile::createObjectFile((f.root/(std::string(mode)+"-library.o")).string());
        if(!object)throw std::runtime_error(llvm::toString(object.takeError()));
        for(auto [identity,external]:{std::pair{"/main.void::helper",false},{"/lib.void::snapshot",false},
                {"/main.void::entry",true},{"/lib.void::imported",true},{"/lib.void::project",true}}) {
            std::string native="__vd_";const char* hex="0123456789abcdef";
            for(unsigned char c:std::string(identity)) {native+=hex[c>>4];native+=hex[c&15];}
            bool found=false;
            for(const auto& symbol:object->getBinary()->symbols()) {
                auto name=symbol.getName();if(!name)throw std::runtime_error(llvm::toString(name.takeError()));
                // Object platforms may prepend their normal global prefix.
                if(!name->ends_with(native))continue;
                auto flags=symbol.getFlags();if(!flags)throw std::runtime_error(llvm::toString(flags.takeError()));
                require(bool(*flags & llvm::object::BasicSymbolRef::SF_Global)==external,"wrong native function linkage");
                found=true;
            }
            require(found || (!external && optimized),"required native symbol missing");
        }
    }
    require(jit.live_generations()==0,"private linkage generation leaked");
}
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
    f.rejects("use \"math\"\n", "must name a .void");
    f.file("main.void", "use \"a.void\"\n" + entry_start + "update(x)\n");
    f.run({{3,3}});
    f.rejects("use \"C:/a.void\"\n", "OS filesystem paths");
    f.rejects("use \"/a.void/\"\n", "must name a .void");
    f.rejects("use \"/.\"\n", "must name a .void");
    f.rejects("use \"/a.VOID\"\n", "must name a .void");
    for (const auto& path : {"/con.void", "/bad?.void", "/bad:/x.void", "/bad./x.void", "/bad /x.void", "/caf\xc3\xa9.void"})
        f.rejects("use \"" + std::string(path) + "\"\n", "Non-portable module path");
    f.rejects("use /a.void\n", "quoted project path");
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
void constants(const fs::path& output) {
    Fixture f{output / "constant project"};
    f.file("game/scoring.void", "func calculate(value: i32) -> i32:\n    const score = bonus + value\n    return score * 2\n");
    const std::string main = "use \"/game/scoring.void\" as scoring\nconst bonus = base + 5\nconst base = 20\n" + entry_start + "scoring.calculate(x)\n";
    f.file("main.void", main); f.run({{0,50},{1,52},{10,70},{-1,48}});
    vd::Jit jit;
    auto old = jit.compile(f.compile().module, "entry"); vd::ReloadSlot slot(old);
    auto changed_root = main; changed_root.replace(changed_root.find("base = 20"), 9, "base = 30");
    f.file("main.void", changed_root);
    auto updated_root = jit.compile(f.compile().module, "entry");
    require(!slot.replace(updated_root) && slot.invoke(0) == 50 && updated_root->invoke(0) == 70, "Project constant change escaped restart contract");
    f.file("main.void", main);
    f.file("game/scoring.void", "func calculate(value: i32) -> i32:\n    const score = bonus + value + 1\n    return score * 2\n");
    require(!slot.replace(jit.compile(f.compile().module, "entry")) && slot.invoke(0) == 50, "Imported local const change escaped restart contract");
    f.file("a.void", "private const hidden = 3\ninternal const amount = other + hidden\nconst other = 4\n");
    f.file("main.void", "use \"/a.void\" as math\n" + entry_start + "math.amount + x\n"); f.run({{0,7},{2,9}});
    f.rejects("use \"/a.void\" as math\n" + entry_start + "math.hidden\n", "private imported constant", "main.void", "3:17");
    f.file("main.void", "use \"/a.void\"\nuse \"./a.void\"\nconst derived = amount * 2\n" + entry_start + "derived + amount\n"); f.run({{0,21}});
    f.file("b.void", "const amount = 10\n");
    f.rejects("use \"/a.void\"\nuse \"/b.void\"\n" + entry_start + "amount\n", "Ambiguous imported constant", "main.void", "4:12");
    f.file("b.void", "func amount() -> i32:\n    return 1\n");
    f.rejects("use \"/a.void\"\nuse \"/b.void\"\n" + entry_start + "amount\n", "Ambiguous imported declaration", "main.void", "4:12");
    f.file("b.void", "use \"/a.void\"\n");
    f.rejects("use \"/b.void\"\n" + entry_start + "amount\n", "Unknown identifier", "main.void", "3:12");
    f.rejects("use \"/a.void\" as math\nconst math = 1\n", "conflicts with a constant declaration", "main.void", "1:18");
    f.file("a.void", "const base = 4\n");
    f.rejects("use \"/a.void\"\nconst base = 3\n", "shadows project-root constant", "a.void", "1:7");
    f.file("a.void", "func base() -> i32:\n    return 4\n");
    f.rejects("use \"/a.void\"\nconst base = 3\n", "shadows project-root constant", "a.void", "1:6");
    f.file("a.void", "func get() -> i32:\n    return hidden\n");
    f.rejects("use \"/a.void\"\nprivate const hidden = 3\n", "Unknown identifier", "a.void", "2:12");
    f.file("a.void", "use \"/main.void\" as main\nconst other = main.base + 1\n");
    f.rejects("use \"/a.void\" as math\nconst base = math.other + 1\n", "Circular constant dependency", "a.void", "2:7");
    f.file("a.void", "use \"/b.void\" as second\nconst amount = second.base + 2\nfunc get(x: i32) -> i32:\n    return amount + x\n");
    f.file("b.void", "use \"/a.void\" as first\nconst base = 7\nfunc other(x: i32) -> i32:\n    return first.get(x)\n");
    f.file("main.void", "use \"/b.void\" as second\n" + entry_start + "second.other(x)\n"); f.run({{0,9},{2,11}});
    std::cout << "Project constants, forwards, visibility, cycles, imports and restart checks passed\n";
}

void local_libraries(const fs::path& output) {
    Fixture f{output/"local-libraries"};fs::remove_all(f.root);
    f.file("main.void","use lib/missing\n");require(!vd::analyze_project(f.root/"main.void").program,"missing libs directory admitted");
    const std::string main="use lib/raylib as rl\nuse lib/raylib/graphics as gfx\nuse \"/libs/raylib/raylib.void\" as again\nuse lib/raylib\nconst root_bonus = 3\nfunc entry(x: i32) -> i32:\n    return rl.read(x) + gfx.read(x) + again.read(x) + raylib.read(x)\n";
    f.file("main.void",main);
    f.file("libs/raylib/raylib.void","use lib/raylib/graphics as gfx\nfunc read(x: i32) -> i32:\n    return gfx.read(x) + root_bonus\nfunc main() -> i32:\n    return 99\nprivate func secret(x: i32) -> i32:\n    return x\n");
    f.file("libs/raylib/graphics.void","use lib/shared as shared\nuse \"/libs/shared/shared.void\" as same\nfunc read(x: i32) -> i32:\n    return shared.add(x)\n");
    const std::string shared="use lib/raylib as cycle\ninternal func add(x: i32) -> i32:\n    return x + 2\n";
    f.file("libs/shared/shared.void",shared);
    f.run({{0,17},{3,29}});
    auto before=vd::analyze_project(f.root/"main.void");require(bool(before.program),"local library analysis");
    require(before.program->modules().size()==4,"quoted/local/diamond/cyclic imports duplicated module identity");
    vd::ToolingIndex index(before.program);
    auto def=index.definition_at("/main.void",main.find("rl.read")+4);
    require(def && def->module=="/libs/raylib/raylib.void","local definition lost canonical project identity");
    auto changed=shared;changed.replace(changed.find("x + 2"),5,"x + 4");f.file("libs/shared/shared.void",changed);
    auto after=vd::analyze_project(f.root/"main.void");require(bool(after.program),"edited local library analysis");
    auto plan=vd::plan_invalidation(*before.program,{},after.program.get());
    require(plan.directly_changed_modules==std::vector<std::string>{"/libs/shared/shared.void"} && !plan.graph_changed,"local edit lost dependency identity");
    require(std::find(plan.dependent_modules.begin(),plan.dependent_modules.end(),"/main.void")!=plan.dependent_modules.end(),"local edit did not invalidate importing entry");
    require(index.declaration(def->identity)->source.source->text().find("root_bonus")!=std::string::npos,"local edit corrupted retained snapshot");
    {vd::Jit jit;vd::ReloadSlot slot(jit.compile(*before.program));auto lease=slot.acquire();
        require(jit.replace(slot,*after.program) && slot.invoke(3)==37 && lease->invoke(3)==29,"local library edit broke compatible reload/old lease");}
    f.run({{3,37}});
    auto reject=[&](const std::string& code){auto a=vd::analyze_project(f.root/"main.void",{{"/main.void",vd::SourceFile("main.void",code+"\nconst root_bonus = 3\n")}});require(!a.program && !a.diagnostics.empty(),"invalid local import admitted: "+code);};
    for(const auto& path:{"lib/missing","lib/empty","lib/raylib/missing","lib/RAYLIB","lib/raylib/../shared","lib//raylib","lib/raylib.void","lib/con"})reject("use "+std::string(path)+"\n");
    fs::create_directories(f.root/"libs/empty");reject("use lib/empty\n");
    f.file("libs/direct.void","func read(x: i32) -> i32:\n    return x\n");reject("use lib/direct\n");
    auto collision=vd::analyze_project(f.root/"main.void",{{"/main.void",vd::SourceFile("main.void",main)},{"/libs/raylib/Graphics.void",vd::SourceFile("case.void","")}});
    require(!collision.program,"local library overlay case collision admitted");
    reject("use lib/raylib as rl\nfunc entry(x: i32) -> i32:\n    return rl.secret(x)\n");
    f.file("libs/empty/empty.void","var shared = 1\n");
    auto globals=vd::analyze_project(f.root/"main.void",{{"/main.void",vd::SourceFile("main.void","use lib/empty as library\nfunc entry(x: i32) -> i32:\n    library.shared += x\n    return library.shared\n")}});
    require(bool(globals.program),"local library runtime globals rejected");
    for(bool optimized:{false,true}){vd::Jit jit;auto code=jit.compile(*globals.program,optimized);require(code->invoke(2)==3 && code->invoke(3)==6,"local library global storage was not shared");}
    f.file("libs/empty/empty.void","var shared = 1\npass\n");reject("use lib/empty\n");
    f.file("libs/raylib/redirect.void","use \"../../../outside.void\"\n");reject("use lib/raylib/redirect\n");
    // Application selection belongs only to the selected root, never library main.
    f.file("main.void","use lib/raylib as rl\nconst root_bonus = 3\nfunc main() -> i32:\n    return rl.read(1)\n");
    auto app=vd::analyze_project(f.root/"main.void");require(app.program && app.program->application(),"selected local-library application lost its entry");
    for(bool opt:{false,true}){vd::Jit jit;require(jit.compile(*app.program,opt)->invoke(0)==8,"imported main was selected/executed");}
    // Overlay-only libraries retain their project identities and unavailable guards.
    vd::SourceOverrides overlays{{"/main.void",vd::SourceFile("main.void","use lib/virtual as v\nfunc entry(x: i32) -> i32:\n    return v.read(x)\n")},{"/libs/virtual/virtual.void",vd::SourceFile("unsaved.void","func read(x: i32) -> i32:\n    return x\n")}};
    auto virtual_program=vd::analyze_project(f.root/"main.void",overlays);require(virtual_program.program && virtual_program.program->modules().size()==2,"virtual local library failed");
    overlays["/libs/virtual/virtual.void"]=std::nullopt;require(!vd::analyze_project(f.root/"main.void",overlays).program,"local tombstone ignored");
    std::error_code ec;fs::create_hard_link(f.root/"libs/shared/shared.void",f.root/"libs/shared/linked.void",ec);
    if(!ec)reject("use lib/shared\nuse lib/shared/linked\n");
    fs::create_directory_symlink(fs::absolute(f.root/"libs/raylib"),f.root/"libs/alias",ec);
    if(!ec)reject("use lib/alias/raylib\n");
    write(f.root.parent_path()/"external.void","func read() -> i32:\n    return 7\n");
    fs::create_directory_symlink(fs::absolute(f.root.parent_path()),f.root/"libs/escape",ec);
    if(!ec)reject("use lib/escape/external\n");
    std::cout<<"Project-local library identities, cycles, ownership, overlays, native JIT and invalidation passed\n";
}
}
void module_tests(const std::filesystem::path& output) {
    private_linkage(output);
    positives(output); negatives(output); constants(output); local_libraries(output);
    std::cout << "Multi-file ownership, cycles, visibility, paths, diagnostics and native JIT checks passed\n";
}
