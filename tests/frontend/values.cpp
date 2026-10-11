#include "void/frontend.hpp"
#include "void/backend.hpp"
#include <atomic>
#include <bit>
#include <limits>
#include <iostream>
#include <thread>
#include <stdexcept>
#include <vector>
namespace {
void require(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
void reject(const std::string& source, const std::string& reason) {
    try { vd::compile_source({"values-invalid.void", source}); }
    catch (const std::invalid_argument& e) {
        std::string message = e.what();
        require(message.find(reason) != std::string::npos && message.find('^') != std::string::npos,
                "Wrong const diagnostic: " + message);
        return;
    }
    throw std::runtime_error("Invalid const accepted: " + source);
}
}
void value_tests(const std::filesystem::path& root) {
    for (const auto& op : {"= 3", "+= 1", "-= 1", "*= 2", "/= 1", "%= 2", "++", "--"}) {
        reject("func f(x: i32):\n    const item = x\n    item " + std::string(op) + "\n", "readonly");
        reject("const item = 2\nfunc f():\n    item " + std::string(op) + "\n", "readonly");
    }
    reject("const item: i32\n", "requires an initializer");
    reject("func f():\n    const item\n", "requires an initializer");
    reject("const item = true + 1\n", "Type mismatch");
    reject("const item: bool = 1\n", "Type mismatch");
    reject("const item = missing\n", "Unknown identifier");
    reject("const item = later()\nfunc later() -> i32:\n    return 2\n", "cannot call a function");
    reject("const item = false and later()\nfunc later() -> bool:\n    return true\n", "cannot call a function");
    reject("const a = b + 1\nconst b = a - 1\n", "Circular constant dependency");
    reject("const a = a\n", "Circular constant dependency");
    reject("const a = 1 / 0\n", "by zero");
    reject("const a = 1 % (2 - 2)\n", "by zero");
    reject("const a = 2147483648\n", "outside the i32 range");
    reject("const a = -2147483649\n", "outside the i32 range");
    reject("const a = 1\nconst a = 2\n", "Duplicate declaration");
    reject("const a = 1\nfunc a():\n    return\n", "Duplicate declaration");
    reject("const a = 1\nfunc f(a: i32):\n    return\n", "shadowing");
    reject("const a = 1\nfunc f():\n    var a = 2\n", "shadowing");
    reject("func f(x: i32):\n    const a = x\n    if true:\n        const a = 2\n", "shadowing");
    reject("func f():\n    const a = true\n    var b = a + 1\n", "Type mismatch");
    reject("func f():\n    const a = a\n", "Unknown identifier");
    reject("const a = 1\nfunc f():\n    a()\n", "not callable");
    reject("func f():\n    const a = 1\n    a()\n", "not callable");
    {
        vd::Jit engine;
        const std::vector<std::int32_t> boundaries{-2147483647-1, -2147483647, -7, -1, 0, 1, 3, 2147483647};
        unsigned comparisons = 0;
        for (const auto& op : {"+", "-", "*", "/", "%"}) for (auto a : boundaries) for (auto b : boundaries) {
            std::string operation = op;
            if (!b && (operation == "/" || operation == "%")) continue;
            std::int64_t expected = operation == "+" ? std::int64_t(a) + b : operation == "-" ? std::int64_t(a) - b :
                operation == "*" ? std::int64_t(a) * b : operation == "/" ? std::int64_t(a) / b : std::int64_t(a) % b;
            auto answer = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(expected));
            auto source = "const folded = (" + std::to_string(a) + ") " + operation + " (" + std::to_string(b) + ")\n"
                "func entry(x: i32) -> i32:\n    if x == " + std::to_string(a) + ":\n        return x " + operation + " (" + std::to_string(b) + ")\n    return folded\n";
            auto module = vd::compile_source({"numeric-matrix.void", source});
            for (bool optimized : {false, true}) {
                auto code = engine.compile(module, "entry", optimized);
                auto other = a == std::numeric_limits<std::int32_t>::max() ? 0 : a + 1;
                require(code->invoke(a) == answer && code->invoke(other) == answer, "Numeric matrix disagreement: " + source);
                comparisons += 2;
            }
        }
        std::cout << comparisons << " constant/normal/O2 numeric matrix comparisons passed\n";
    }
    {
        auto anonymous = vd::compile_source({"", "func entry(x: i32) -> i32:\n    return 1 / x\n"});
        vd::Jit host; auto lease = host.compile(anonymous, "entry");
        bool caught = false;
        try { lease->invoke(0); } catch (const std::runtime_error&) { caught = true; }
        require(caught, "Anonymous source lost its checked ABI");
        auto unary = host.compile(vd::compile_source({"unary.void", "func entry(x: i32) -> i32:\n    return -(-2147483648) + x\n"}), "entry");
        require(unary->invoke(0) == -2147483647-1, "Negation of a boundary literal failed");
    }
    const std::string source = "const high = low + 1\nconst low = 2147483647\nconst flag = not high > low\n"
        "func entry(x: i32) -> i32:\n    const y: i32 = x + 10\n    const active: bool = flag\n    var copy = y\n    copy += 3\n    if active:\n        return copy * 2\n    return 99\n";
    vd::Jit jit;
    for (bool optimized : {false, true}) {
        auto code = jit.compile(vd::compile_source({"values.void", source}), "entry", optimized);
        require(code->invoke(0) == 26 && code->invoke(-10) == 6, "Runtime readonly binding or compile-time dependency failed");
        auto numeric = vd::compile_project(root / "examples/numeric.void");
        auto number = jit.compile(numeric.module, "entry", optimized);
        for (auto [input, expected] : std::vector<std::pair<int,int>>{{1,-21},{0,-1},{-1,-19},{-2147483647-1,-2147483647-1}})
            require(number->invoke(input) == expected, "Numeric compile-time/runtime disagreement");
        for (const auto& op : {"/", "%"}) {
            auto module = vd::compile_source({"failure.void", "func fails(x: i32) -> i32:\n    return 100 " + std::string(op) + " x\nfunc entry(x: i32) -> i32:\n    return fails(x)\n"});
            auto failure = jit.compile(module, "entry", optimized);
            for (int repeat = 0; repeat < 3; ++repeat) {
                bool caught = false;
                try { failure->invoke(0); }
                catch (const std::runtime_error& e) {
                    std::string message = e.what();
                    require(message.find("failure.void:2:16: runtime error:") != std::string::npos &&
                        message.find("by zero") != std::string::npos && message.find("Called from failure.void::entry") != std::string::npos &&
                        message.find("failure.void:4:12: source call") != std::string::npos && message.find('^') != std::string::npos,
                        "Wrong runtime diagnostic: " + message);
                    caught = true;
                }
                require(caught, "Missing dynamic zero diagnostic");
                require(failure->invoke(2) == (std::string(op) == "/" ? 50 : 0), "Error leaked into next invocation");
            }
            std::atomic<bool> good{true};
            std::vector<std::thread> callers;
            try {
                for (int i = 0; i < 4; ++i) callers.emplace_back([&] {
                    for (int n = 0; n < 30; ++n) {
                        try { failure->invoke(0); good = false; }
                        catch (const std::runtime_error& e) { if (std::string(e.what()).find("by zero") == std::string::npos) good = false; }
                        if (failure->invoke(2) != (std::string(op) == "/" ? 50 : 0)) good = false;
                    }
                });
            } catch (...) { for (auto& t : callers) t.join(); throw; }
            for (auto& t : callers) t.join();
            require(good, "Concurrent runtime boundaries interfered");
        }
        auto recursive = jit.compile(vd::compile_source({"recursive-error.void",
            "func recurse(x: i32) -> i32:\n    if x > 0:\n        return recurse(x - 1)\n    return 1 / x\nfunc entry(x: i32) -> i32:\n    return recurse(x)\n"}), "entry", optimized);
        bool limited = false;
        try { recursive->invoke(40); }
        catch (const std::runtime_error& e) { limited = std::string(e.what()).find("limited to 32 callers") != std::string::npos; }
        require(limited, "Bounded recursive trace did not report truncation");
        auto no_result = jit.compile(vd::compile_source({"no-result.void",
            "func fails(x: i32):\n    var y = 1 / x\nfunc entry(x: i32) -> i32:\n    fails(x)\n    return 99\n"}), "entry", optimized);
        bool propagated = false;
        try { no_result->invoke(0); } catch (const std::runtime_error&) { propagated = true; }
        require(propagated && no_result->invoke(1) == 99, "No-result call did not propagate runtime failure");
    }
    auto old = jit.compile(vd::compile_source({"reload-const.void", source}), "entry");
    vd::ReloadSlot slot(old);
    auto changed = source; changed.replace(changed.find("x + 10"), 6, "x + 20");
    auto replacement = jit.compile(vd::compile_source({"reload-const.void", changed}), "entry");
    require(!slot.replace(replacement) && old->invoke(0) == 26 && replacement->invoke(0) == 46, "Const edit installed without restart");
    require(slot.replace(jit.compile(vd::compile_source({"reload-const.void", source}), "entry")), "Unchanged constant contract rejected");
    std::cout << "Readonly/compile-time constants, numeric boundaries, concurrent failures, bounded traces and restart checks passed\n";
}
