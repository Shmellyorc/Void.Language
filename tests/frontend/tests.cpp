#include "void/frontend.hpp"
#include "void/backend.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
void require(bool condition, const std::string& message) { if (!condition) throw std::runtime_error(message); }
void run(const std::string& source, const std::vector<std::pair<int, int>>& cases) {
    auto module = vd::compile_source({"/independent.void", source});
    vd::Jit jit;
    for (bool optimized : {false, true}) {
        auto code = jit.compile(module, "entry", optimized);
        for (auto [input, expected] : cases)
            require(code->invoke(input) == expected, "native source result mismatch for input " + std::to_string(input) + "\n" + source);
    }
}
void rejects(const std::string& text, const std::string& reason, const std::string& location = {}) {
    try { vd::compile_source({"/invalid.void", text}); }
    catch (const std::invalid_argument& e) {
        std::string message = e.what();
        require(message.find(reason) != std::string::npos, "wrong diagnostic, expected " + reason + "\n" + message);
        require(message.find('^') != std::string::npos, "missing source caret");
        if (!location.empty()) require(message.starts_with("/invalid.void:" + location + ": error:"), "wrong diagnostic location\n" + message);
        return;
    }
    throw std::runtime_error("invalid source accepted\n" + text);
}
void diagnostics() {
    rejects("func f() -> i32:\n    pass\n", "Missing return path", "1:6");
    rejects("func f(x: bool) -> i32:\n    if x:\n        return 1\n    else:\n        pass\n", "Missing return path");
    rejects("func f() -> i32:\n    for false:\n        pass\n", "Missing return path");
    rejects("func f():\n    # Still no statement.\n", "indented block");
    rejects("func f():\n    if true:\n        # Missing conditional body.\n    pass\n", "indented block");
    rejects("func f():\n    for false:\n        # Missing loop body.\n    pass\n", "indented block");
    for (const auto& suffix : {" 1", "()", " = 1", ":"})
        rejects(std::string("func f():\n    pass") + suffix + "\n", "end of statement");
    rejects("func f() -> i32:\n    return pass\n", "Expected an expression", "2:12");
    rejects("func f():\n    var x = pass\n", "Expected an expression", "2:13");
    rejects("func f():\n    var pass = 1\n", "Expected a lowercase identifier");
    require(vd::compile_source({"/top-pass.void", "pass\n"}).application, "top-level pass must select an application body");
    rejects("struct child:\nstruct owner:\n    child: child\nfunc (o owner) init():\n    pass\n", "initialized");
    auto recovered = vd::analyze_source({"/pass-recovery.void", "func f():\n    pass 1\n    pass\nfunc g() -> i32:\n    return missing\n"});
    require(!recovered.succeeded() && recovered.diagnostics.size() == 2, "pass syntax recovery diagnostic count");
    require(recovered.diagnostics[0].primary.source->location(recovered.diagnostics[0].primary.begin).line == 2 &&
        recovered.diagnostics[1].description.find("Unknown identifier") != std::string::npos &&
        recovered.diagnostics[1].primary.source->location(recovered.diagnostics[1].primary.begin).line == 5,
        "pass syntax recovery lost later function or location");
    rejects("interface idle:\n    func wait():\n        pass\n", "end of statement");
    rejects("interface idle:\n    pass\n", "func");
    rejects("func entry(x: i32) -> i32:\n    return x\n  return 2\n", "Indentation", "3:3");
    rejects("func entry(x: i32) -> i32:\n\treturn x\n", "Tabs", "2:1");
    rejects("func entry(x: i32) -> i32:\nreturn x\n", "indented block", "2:1");
    rejects("func entry(x: i32) -> i32:\n    return @\n", "Unexpected character", "2:12");
    rejects("func entry(x: i32) -> i32:\n    return missing\n", "Unknown identifier", "2:12");
    rejects("func entry(x: i32) -> i32:\n    var x = 1\n    return x\n", "Duplicate declaration", "2:9");
    rejects("func entry(x: i32) -> i32:\n    if true:\n        var x = 1\n    return x\n", "shadowing", "3:13");
    rejects("func same():\n    return\nfunc same():\n    return\n", "Duplicate function", "3:6");
    rejects("func f(x: i32, x: bool):\n    return\n", "Duplicate declaration", "1:16");
    rejects("func f(x: i32) -> i32:\n    return x\nfunc entry(x: i32) -> i32:\n    return f()\n", "Incorrect argument count", "4:12");
    rejects("func f(x: i32) -> i32:\n    return x\nfunc entry(x: i32) -> i32:\n    return f(true)\n", "Type mismatch", "4:14");
    rejects("func f() -> bool:\n    return 1\n", "expected bool", "2:5");
    rejects("func f() -> i32:\n    return\n", "no result", "2:5");
    rejects("func f():\n    return 1\n", "Type mismatch", "2:5");
    rejects("func f(x: i32) -> i32:\n    if x > 0:\n        return x\n", "Missing return path", "1:6");
    rejects("func f(x: i32) -> i32:\n    for x > 0:\n        return x\n", "Missing return path");
    rejects("func f(x: i32) -> i32:\n    for:\n        break\n", "Missing return path");
    rejects("func f():\n    break\n", "inside a for", "2:5");
    rejects("func f():\n    continue\n", "inside a for", "2:5");
    rejects("func f():\n    while true:\n        return\n", "Replace 'while condition:' with 'for condition:'", "2:5");
    rejects("func f():\n    for item in 3:\n        return\n", "Iteration requires an array, dictionary or integer range", "2:5");
    rejects("func f():\n    if 1:\n        return\n", "expected bool");
    rejects("func f():\n    var x = true + false\n", "expected i32");
    rejects("func f():\n    var x = 1 and true\n", "expected bool");
    rejects("func f():\n    var x = true == 1\n", "Type mismatch");
    rejects("func f():\n    var x: bool = 1\n", "Type mismatch");
    rejects("func f():\n    var x = x\n", "Unknown identifier");
    rejects("func f():\n    missing = 1\n", "Unknown identifier");
    rejects("func f():\n    var x\n", "requires a value type");
    rejects("func f():\n    var x = f()\n", "requires a value type");
    rejects("func f():\n    return f()\n", "cannot return an expression");
    rejects("func f():\n    unknown()\n", "Unknown function");
    rejects("func f():\n    42\n", "standalone expressions");
    rejects("func f():\n    var x = 2147483648\n", "outside the i32 range");
    rejects("func f():\n    var x = -2147483649\n", "outside the i32 range");
    rejects("func f():\n    var x = 999999999999999999999999999\n", "too large");
    rejects("func f():\n    var x = 1 / 0\n", "by zero");
    rejects("func f():\n    var x = 1\n    x %= 0\n", "by zero");
    rejects("func f():\n    var x = 1.5q\n", "Unsupported numeric literal");
    // Phase 22 replaces the obsolete string-type deferral with real execution
    // in strings_source; unknown types must still be rejected here.
    rejects("func f(x: unsupported_type):\n    return\n", "Unsupported type");
    rejects("func f():\n    if true:\n        var local = 1\n    var value = local\n", "Unknown identifier");
    rejects("func f():\n    var x = 0\n    var y = x++\n", "end of statement");
    rejects("func f():\n    var x = 0\n    ++x\n", "Expected an expression");
    rejects("func f():\n    var flag = true\n    flag += false\n", "expected i32");
    rejects("func f():\n    return\n        return\n", "Expected an expression");
    rejects("func f():\n    var x = not 1\n", "expected bool");
    rejects("func f():\n    var x = 1 < 2 < 3\n", "expected i32");
    rejects("func f():\n    var x = (1 + )\n", "Expected an expression");
    rejects("func f():\n    var x = 1;\n", "Unexpected character");
    for (const auto& invalid : std::vector<std::string>{"\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe2\x82", "\x80"})
        rejects("# " + invalid + "\n", "Invalid UTF-8", "1:3");
    vd::SourceFile utf8("/utf8.void", "# \xc3\xa9\xf0\x9f\x98\x80!\n");
    require(utf8.location(8).column == 5, "UTF-8 scalar column");
    auto library = vd::compile_source({"/library.void", "func f() -> bool:\n    return true\n"});
    vd::Jit jit;
    bool rejected = false;
    try { jit.compile(library, "f"); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "unsafe host ABI accepted");
    std::cout << "50+ source diagnostic cases and UTF-8/ABI checks passed\n";
}
void programs(const std::filesystem::path& root) {
    run("interface idle:\n    func wait()\nstruct worker:\nfunc (w worker) init():\n    pass\nfunc (w worker) wait():\n    pass\nfunc entry(x: i32) -> i32:\n    var w = worker()\n    var i: idle = w\n    i.wait()\n    pass\n    return x\n", {{0,0},{9,9}});
    vd::Jit jit;
    for (auto file : {"architecture.void", "loops.void", "functions.void", "pass.void"}) {
        auto source = vd::read_source(root / "examples" / file); auto module = vd::compile_source(source);
        vd::require_source_entry(source, module);
        for (bool optimized : {false, true}) {
            auto code = jit.compile(module, "entry", optimized);
            if (std::string(file) == "architecture.void")
                for (auto [input, expected] : std::vector<std::pair<int,int>>{{-1,7},{0,42},{1,43},{10,52}}) require(code->invoke(input) == expected, "real architecture source");
            else if (std::string(file) == "loops.void")
                for (auto [input, expected] : std::vector<std::pair<int,int>>{{-1,21},{0,21},{1,22},{10,25}}) require(code->invoke(input) == expected, "loop file");
            else if (std::string(file) == "pass.void")
                for (int input : {-1,0,1,10}) require(code->invoke(input) == input + 8, "pass source fallthrough");
            else
                for (auto [input, expected] : std::vector<std::pair<int,int>>{{-1,9},{0,1},{1,1},{5,120},{10,3628800},{11,0}}) require(code->invoke(input) == expected, "recursive function file");
        }
    }
    run("func entry(x: i32) -> i32:\n    var value = x\n    value += 4\n    value -= 2\n    value *= 6\n    value /= 3\n    value %= 5\n    value++\n    value--\n    return value\n", {{0,4},{2,3},{-5,-1}});
    run("func entry(x: i32) -> i32:\n    var flag: bool\n    var zero: i32\n    if flag == false and zero == 0:\n        flag = true\n    if flag:\n        return x\n    return 99\n", {{0,0},{8,8}});
    run("func risky(x: i32) -> bool:\n    return 10 / x > 0\nfunc entry(x: i32) -> i32:\n    if (x == 0 or risky(x)) and not x < 0:\n        return 1\n    return 0\n", {{0,1},{2,1},{-1,0},{20,0}});
    run("func entry(x: i32) -> i32:\n    if false and 1 / x > 0:\n        return 9\n    return 2\n", {{0,2},{-2,2}});
    run("func pick(a: bool, b: i32, c: bool) -> i32:\n    if a and c:\n        return b\n    return -b\nfunc entry(x: i32) -> i32:\n    return pick(x == 0 or x > 10, x + 7, not x < 0 and true) + 5\n", {{0,12},{1,-3},{11,23},{-1,-1}});
    run("func entry(x: i32) -> i32:\n    var total = 0\n    var outer = 0\n    for outer < 3:\n        outer++\n        var inner: i32\n        for:\n            inner++\n            if inner == 2:\n                continue\n            if inner > 3:\n                break\n            total += outer * inner\n    return total + x\n", {{0,24},{4,28}});
    run("func entry(x: i32) -> i32:\n    for true:\n        if x > 0:\n            return x\n        else:\n            return 7\n", {{-1,7},{3,3}});
    run("func empty():\n    var x: i32\nfunc zero() -> i32:\n    empty()\n    return 3\nfunc entry(x: i32) -> i32:\n    if x == 0:\n        return zero()\n    else if x < 0:\n        if x < -10:\n            return 1\n        return 2\n    else:\n        return 4\n", {{0,3},{-1,2},{-11,1},{1,4}});
    run("func entry(x: i32) -> i32:\n    return (-2147483648 / -1) + (-7 % 3) + (2147483647 + 1)\n", {{0,-1}});
    run("func entry(x: i32) -> i32:\n    var flag = (x <= 3 and x >= 1) or (x != 9 and x == 10)\n    if flag:\n        return 1\n    return 0\n", {{0,0},{1,1},{3,1},{9,0},{10,1}});
    run("\xef\xbb\xbf# UTF-8: \xc3\xa9\xf0\x9f\x98\x80\r\nfunc entry(x: i32) -> i32:\r\n  # comment\r\n  return +x + 5", {{0,5},{8,13}});
    run("func entry(x: i32) -> i32:\r    return x + 2\r", {{0,2}});
    run("func even(x: i32) -> bool:\n    if x == 0:\n        return true\n    return odd(x - 1)\nfunc odd(x: i32) -> bool:\n    if x == 0:\n        return false\n    return even(x - 1)\nfunc entry(x: i32) -> i32:\n    if even(x):\n        return 1\n    return 0\n", {{0,1},{5,0},{10,1}});
    std::cout << "Real .void files and independent programs executed through normal/O2 JIT\n";
}
}
void initialization_tests(const std::filesystem::path& root);
void method_tests(const std::filesystem::path& root);
void struct_tests(const std::filesystem::path& root);
void value_tests(const std::filesystem::path& root);
#include "diagnostics.hpp"
void frontend_tests(const std::filesystem::path& root) {
    auto group = [](const char* name, auto run) {
        std::cerr << "Frontend begin: " << name << std::endl;
        run();
        std::cerr << "Frontend passed: " << name << std::endl;
    };
    group("diagnostics", [] { diagnostics(); });
    group("programs", [&] { programs(root); });
    group("numeric values", [&] { value_tests(root); });
    group("diagnostic recovery/resource limits", [&] { diagnostic_acceptance::run(root); });
    group("value structs", [&] { struct_tests(root); });
    group("receiver methods", [&] { method_tests(root); });
    group("initialization", [&] { initialization_tests(root); });
}
