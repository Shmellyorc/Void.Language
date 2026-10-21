#include "void/frontend.hpp"
#include "void/backend.hpp"
#include "void/numeric.hpp"
#include <fstream>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
void require(bool condition, const std::string& message) { if (!condition) throw std::runtime_error(message); }
void reject(const std::string& text, vd::DiagnosticCode expected) {
    try { vd::compile_source({"numeric-invalid.void",text}); }
    catch (const vd::CompilationError& failure) {
        for (const auto& d : failure.diagnostics()) if (d.code == expected) {
            require(d.primary.source->text() == text && d.primary.end > d.primary.begin,"Lost numeric snapshot/span"); return;
        }
        throw std::runtime_error("Wrong numeric diagnostic: " + std::string(failure.what()));
    }
    throw std::runtime_error("Invalid numeric program accepted: " + text);
}
}
void numeric_tests(const std::filesystem::path& root) {
    using C = vd::DiagnosticCode;
    vd::Jit jit;
    unsigned executions = 0;
    for (const auto& item : std::filesystem::directory_iterator(root / "tests/numeric/programs")) {
        if (item.path().extension() != ".void") continue;
        auto module = vd::compile_project(item.path()).module;
        bool fails = item.path().stem().string().starts_with("fail_");
        for (bool optimized : {false,true}) {
            auto code = jit.compile(module,"entry",optimized);
            if (fails) {
                try { code->invoke(0); }
                catch (const std::runtime_error& failure) {
                    auto message = std::string(failure.what());
                    require(message.find("runtime error:") != std::string::npos && message.find("Called from") != std::string::npos && message.find("^") != std::string::npos,"Missing conversion error source/caller: " + message);
                    ++executions; continue;
                }
                throw std::runtime_error("Invalid runtime numeric conversion/division succeeded: " + item.path().string());
            }
            for (auto input : {0,1,10}) {
                auto answer = code->invoke(input);
                auto expected = item.path().stem() == "dynamic_widths" ? (input == 0 ? 47 : input == 1 ? -7 : 48) : 1;
                require(answer == expected,item.path().string() + " returned failure marker " + std::to_string(answer)); ++executions;
            }
        }
    }
    const std::vector<std::pair<std::string,C>> invalid = {
        {"var x: u64 = 18446744073709551616",C::integer_range},
        {"var x: i64 = 9223372036854775808",C::integer_range},
        {"var x: i8 = -129",C::integer_range},
        {"var x: u8 = -1",C::integer_range},
        {"var x: f32 = 16777217",C::numeric_representability},
        {"var x: f64 = 9007199254740993",C::numeric_representability},
        {"var x: f32 = 0.1d",C::type_mismatch},
        {"var x = 1\n    var y: i64 = x",C::type_mismatch},
        {"var x: i32 = 1\n    var y: u32 = 2\n    var z = x + y",C::type_mismatch},
        {"var x: f32 = 1\n    var y: f64 = 2\n    var z = x < y",C::type_mismatch},
        {"var x = 0.5 % 1.0",C::numeric_operator},
        {"var x = true as i32",C::numeric_conversion},
        {"var x = 1 as bool",C::numeric_conversion},
        {"var x = 300 as i8",C::numeric_conversion},
        {"var x = 1.0q",C::numeric_literal},
        {"var x = 1.0 as? i32",C::character}
    };
    for (const auto& [body,code] : invalid) reject("func f():\n    " + body + "\n",code);
    reject("const bad: i32 = (0.0 / 0.0) as i32\n",C::numeric_conversion);
    reject("const bad = 1 + ((0.0 / 0.0) as i32)\n",C::numeric_conversion);
    reject("const bad: u64 = (-1 as i64) as u64\n",C::numeric_conversion);
    auto bad_source = "func f():\n    var a: u8 = -1\n    var b = true as f32\n    var c = 0.5 % 1.0\n";
    try { vd::compile_source({"multiple-numeric.void",bad_source}); throw std::runtime_error("Invalid numeric source returned IR"); }
    catch (const vd::CompilationError& failure) {
        require(failure.diagnostics().size() == 3,"Numeric recovery cascaded or lost independent errors");
        require(failure.diagnostics()[0].primary.begin < failure.diagnostics()[1].primary.begin && failure.diagnostics()[1].primary.begin < failure.diagnostics()[2].primary.begin,"Numeric errors are unordered");
    }
    // Exact types are required even when native storage widths happen to match.
    for (const auto& source : {
        "func entry(value: u32) -> i32:\n    return value as i32\n",
        "func entry(value: i32) -> u32:\n    return value as u32\n",
        "func entry(value: f32) -> i32:\n    return value as i32\n"}) {
        auto module = vd::compile_source({"numeric-abi.void",source}); bool rejected = false;
        try { jit.compile(module,"entry"); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected,"Host accepted same-width incompatible numeric signature");
    }
    const std::string old_source = "const amount: f64 = 42.0\nfunc entry(value: i32) -> i32:\n    return (amount + (value as f64)) as i32\n";
    auto old = jit.compile(vd::compile_source({"numeric-reload.void",old_source}),"entry");
    vd::ReloadSlot slot(old);
    auto changed = old_source; changed.replace(changed.find("42.0"),4,"43.0");
    require(!slot.replace(jit.compile(vd::compile_source({"numeric-reload.void",changed}),"entry")),"Numeric const edit bypassed restart token");
    reject("func entry(value: i32) -> i32:\n    return value as bool\n",C::numeric_conversion);
    require(slot.invoke(1) == 43 && old->invoke(0) == 42,"Invalid numeric edit damaged retained generation");
    bool recovered_calls = false;
    try { vd::compile_source({"numeric-calls.void","func f():\n    var x = missing_a(unknown_a) + missing_b(unknown_b)\n"}); }
    catch (const vd::CompilationError& failure) { recovered_calls = failure.diagnostics().size() == 4; }
    require(recovered_calls,"Numeric context probing hid independent operand errors");
    std::string casts = "func f():\n    var x = 1";
    for (unsigned i=0; i<1024; ++i) casts += " as i32";
    reject(casts + "\n    var later = missing\n",C::nesting);
    // Sprint 2 canonical prelude: exact bit policies and private IR admission.
    using T=vd::ir::Type;using O=vd::ir::Op;
    auto canonical=vd::compile_source({"canonical-numeric.void","func entry(value: i32) -> i32:\n    return i32.abs(value) + value.abs()\n"});
    require(std::count_if(canonical.functions.begin(),canonical.functions.end(),[](const auto& f){return f.name==vd::numeric::symbol(T::i32,"abs");})==1,"type and receiver forms duplicated the canonical implementation");
    require(vd::numeric::absolute(0xffc01234u,T::f32)==0x7fc01234u,"f32 abs retained a NaN sign");
    require(vd::numeric::absolute(0xfff8000000001234ull,T::f64)==0x7ff8000000001234ull,"f64 abs retained a NaN sign");
    require(vd::numeric::limit(T::f32,false)==0xff7fffffu && vd::numeric::limit(T::f64,true)==0x7fefffffffffffffull,"finite floating limits");
    for(bool optimized:{false,true}) {
        vd::ir::Module raw;raw.identity="numeric-bits";vd::ir::Function function;function.name="numeric-bits::entry";function.module_identity="numeric-bits";function.source_name="entry";
        vd::ir::Instruction nan{O::constant};nan.type=T::f32;nan.immediate=0xffc01234u;
        vd::ir::Instruction abs{O::call};abs.callee=vd::numeric::symbol(T::f32,"abs");abs.arguments={0};
        vd::ir::Instruction bits{O::bitcast,1};bits.type=T::i32;
        function.blocks={{{nan,abs,bits},{vd::ir::Terminator::Kind::return_value,2}}};raw.functions.push_back(function);
        vd::numeric::append_prelude(raw);vd::ir::validate(raw);
        require(jit.compile(raw,"entry",optimized)->invoke(0)==0x7fc01234,"native NaN abs sign/payload disagrees with constant kernel");
        auto invalid=raw;invalid.functions[0].blocks[0].instructions[2].type=T::i64;
        bool rejected=false;try{vd::ir::validate(invalid);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"IR accepted a width-changing bitcast");
        const std::string before="func amount(value: i32, extra: i32 = 2) -> i32:\n    return value.abs() + extra\nfunc entry(value: i32) -> i32:\n    return amount(value = value)\n";
        auto initial=vd::compile_source({"sprint2-reload.void",before});auto live=jit.compile(initial,"entry",optimized);vd::ReloadSlot named_slot(live);
        auto body=before;body.replace(body.find("+ extra"),7,"+ extra + 1");
        require(named_slot.replace(jit.compile(vd::compile_source({"sprint2-reload.void",body}),"entry",optimized)),"compatible numeric body replacement failed");
        auto defaults=before;defaults.replace(defaults.find("= 2"),3,"= 3");
        require(!named_slot.replace(jit.compile(vd::compile_source({"sprint2-reload.void",defaults}),"entry",optimized)),"default change bypassed reload contract");
        auto names=before;for(std::size_t at=0;(at=names.find("extra",at))!=std::string::npos;at+=5)names.replace(at,5,"bonus");
        require(!named_slot.replace(jit.compile(vd::compile_source({"sprint2-reload.void",names}),"entry",optimized)),"parameter name change bypassed reload contract");
        require(named_slot.invoke(-4)==7 && live->invoke(-4)==6,"numeric/default reload changed an old lease");
    }
    std::cout << executions << " real-file numeric constant/JIT/O2 checks; structured numeric recovery passed\n";
}
