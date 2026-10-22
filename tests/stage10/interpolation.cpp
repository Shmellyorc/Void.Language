#include "void/backend.hpp"
#include "void/frontend.hpp"
#include <iostream>
#include <stdexcept>
namespace {
unsigned checks=0;
const std::string entry="func entry(input: i32) -> i32:\n";
void run(const std::string& text,int want=1) {
    auto a=vd::analyze_source({"/stage10_interpolation.void",text});
    if(!a.succeeded())throw vd::CompilationError(a.diagnostics);++checks;vd::Jit jit;
    for(bool optimized:{false,true}){if(jit.compile(*a.program,optimized)->invoke(4)!=want)throw std::runtime_error("interpolation result: "+text);++checks;}
}
void reject(const std::string& text,const std::string& message) {
    auto a=vd::analyze_source({"/stage10_interpolation.void",text});
    if(a.succeeded())throw std::runtime_error("invalid interpolation accepted: "+text);
    for(const auto& d:a.diagnostics)if(d.description.find(message)!=std::string::npos){++checks;return;}throw vd::CompilationError(a.diagnostics);
}
void equal(const std::string& value,const std::string& expected) {
    run(entry+"    var text = "+value+"\n    if text == "+expected+":\n        return 1\n    return 0\n");
}
}
int main() {
    try {
        equal("\"Score: {input:000}; {true}; {2.125d:0.00}\"","\"Score: 004; true; 2.12\"");
        equal("\"{{{input}}}\"","\"{{4}}\"");
        equal("\"{input + 2:>4|000}\"","\" 006\"");
        equal("\"{[1, 2, 3][1]}\"","\"2\"");
        equal("\"{({1: 2})[1]}\"","\"2\"");
        equal("\"{\"nested {input}\"}\"","\"nested 4\"");
        equal("\"{input:'{'000'}'}\"","\"{{004}}\"");
        equal("\"{\"é\":>3}\"","\"  é\"");
        equal("\"\"\"a\nb\r\nc\r{input}\n\"\"\"","\"a\\nb\\r\\nc\\r4\\n\"");
        equal("\"{(\n        input +\n        2\n    ):000}\"","\"006\"");
        equal("\"{(\n        input # the delimiter } is in this comment\n        + 2\n    )}\"","\"6\"");
        equal("\"{(if input == 4:\n        7\n    else:\n        8\n    ):000}\"","\"007\"");
        equal("\"{(match input:\n        4: 7\n        _: 8\n    ):000}\"","\"007\"");
        equal("\"{(func() -> i32:\n        return input + 1\n    )()}\"","\"5\"");
        run("const text = \"A{2.125d:0.00}/{false}\"\n"+entry+"    if text == \"A2.12/false\":\n        return 1\n    return 0\n");
        run("func next(log: [i32], n: i32) -> i32:\n    log.push(n)\n    return n\n"+entry+"    var log: [i32] = []\n    var text = \"{next(log, 1)}-{next(log, 2)}-{next(log, 3)}\"\n    if text == \"1-2-3\":\n        return log[0] * 100 + log[1] * 10 + log[2]\n    return 0\n",123);
        run(entry+"    var cursor = 0\n    var text = \"\"\n    for cursor < 1000:\n        text = \"{cursor:00000}/{input}\"\n        cursor++\n    if text == \"00999/4\":\n        return 1\n    return 0\n");
        reject(entry+"    var text = \"{input:0.#0}\"\n    return 0\n","Required fraction");
        reject(entry+"    var text = \"{2.5d:X}\"\n    return 0\n","require an integer");
        reject(entry+"    var text = \"{true:000}\"\n    return 0\n","only alignment");
        reject(entry+"    var text = \"{}\"\n    return 0\n","requires an expression");
        reject(entry+"    var text = \"{input:{width}}\"\n    return 0\n","nested format fields");
        reject(entry+"    var text = \"{(input]}\"\n    return 0\n","Unbalanced");
        std::cout<<checks<<" interpolation checks passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
