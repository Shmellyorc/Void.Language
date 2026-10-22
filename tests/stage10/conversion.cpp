#include "void/backend.hpp"
#include "void/frontend.hpp"
#include <iostream>
#include <stdexcept>
namespace {
unsigned checks=0;
const std::string entry="func entry(input: i32) -> i32:\n";
std::shared_ptr<const vd::ValidatedAnalysis> analyze(const std::string& text) {
    auto result=vd::analyze_source({"/stage10_conversion.void",text});
    if(!result.succeeded())throw vd::CompilationError(result.diagnostics);
    ++checks;return result.program;
}
void run(const std::string& text,int want=1) {
    auto a=analyze(text);vd::Jit jit;
    for(bool optimized:{false,true}){if(jit.compile(*a,optimized)->invoke(4)!=want)throw std::runtime_error("conversion result: "+text);++checks;}
}
void reject(const std::string& text) {
    if(vd::analyze_source({"/stage10_conversion.void",text}).succeeded())throw std::runtime_error("invalid conversion accepted");++checks;
}
void invalid(vd::ir::Module m) {
    try{vd::ir::validate(m);}catch(const std::invalid_argument&){++checks;return;}throw std::runtime_error("invalid display IR accepted");
}
}
int main() {
    try {
        run(entry+"    if string(input) == \"4\":\n        return 1\n    return 0\n");
        for(const auto& type:{"i8","i16","i32","i64","u8","u16","u32","u64","f32","f64"})
            run(entry+"    if string(42 as "+type+") == \"42\":\n        return 1\n    return 0\n");
        run(entry+"    if string(true) == \"true\" and string(false) == \"false\" and string(\"a\\0é\") == \"a\\0é\":\n        return 1\n    return 0\n");
        run(entry+"    var spec = \"000\"\n    if format(input, spec) is Ok(var text):\n        if text == \"004\":\n            return 1\n    return 0\n");
        run(entry+"    if format(spec = \"0.00\", value = 2.125d) is Ok(var text):\n        if text == \"2.12\":\n            return 1\n    return 0\n");
        run(entry+"    if format(-1 as i8, \"X\") is Ok(var text):\n        if text == \"FF\":\n            return 1\n    return 0\n");
        run(entry+"    if format(\"é\", \">3\") is Ok(var text):\n        if text == \"  é\":\n            return 1\n    return 0\n");
        run(entry+"    if format(true, \"^6\") is Ok(var text):\n        if text == \" true \":\n            return 1\n    return 0\n");
        run(entry+"    if format(input, \"???\") is Err(var message):\n        if message == \"format at byte 0: Expected integer digits in groups of three\":\n            return 1\n    return 0\n");
        run(entry+"    if format(2.5d, \"X\") is Err(var message):\n        return 1\n    return 0\n");
        run(entry+"    if format(true, \"000\") is Err(var message):\n        return 1\n    return 0\n");
        run("const text = string(2.125d)\nconst formatted = format(2.125d, \"0.00\")\n"+entry+"    if formatted is Ok(var value):\n        if value == \"2.12\" and text == \"2.125\":\n            return 1\n    return 0\n");
        run("const formatted = format(2.5d, \"X\")\n"+entry+"    if formatted is Err(var message):\n        return 1\n    return 0\n");
        run("func value(log: [i32]) -> i32:\n    log.push(1)\n    return 4\nfunc spec(log: [i32]) -> string:\n    log.push(2)\n    return \"000\"\n"+entry+"    var log: [i32] = []\n    var rendered = format(spec = spec(log), value = value(log))\n    return log[0] * 10 + log[1]\n",21);
        run("func format(value: i32) -> i32:\n    return value + 1\n"+entry+"    return format(input)\n",5);
        reject(entry+"    return string()\n");
        reject(entry+"    var text = format(input)\n    return 0\n");
        reject(entry+"    var text = format(input, 3)\n    return 0\n");
        reject(entry+"    var text = string(value = input, value = 3)\n    return 0\n");
        auto a=analyze(entry+"    return string(input).length as i32\n");
        auto m=a->lower();vd::ir::validate(m);
        for(auto& f:m.functions)for(auto& b:f.blocks)for(auto& i:b.instructions)if(i.op==vd::ir::Op::scalar_display)i.initialization=true;
        invalid(m);
        std::cout<<checks<<" source conversion checks passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
