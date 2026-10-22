#include "void/backend.hpp"
#include "void/frontend.hpp"
#include <iostream>
#include <stdexcept>
namespace {
unsigned checks=0;
const std::string entry="func entry(input: i32) -> i32:\n";
auto analyze(const std::string& text){auto a=vd::analyze_source({"/hooks.void",text});if(!a.succeeded())throw vd::CompilationError(a.diagnostics);++checks;return a.program;}
void run(const std::string& text,int expected=1){auto a=analyze(text);vd::Jit jit;for(bool optimized:{false,true}){auto code=jit.compile(*a,optimized);if(code->invoke(4)!=expected)throw std::runtime_error("wrong display result: "+text);++checks;}}
void reject(const std::string& text){if(vd::analyze_source({"/hooks.void",text}).succeeded())throw std::runtime_error("invalid hook accepted: "+text);++checks;}
const std::string point="value struct point:\n    private const x: i32 = 0\nfunc (p point) init(x: i32):\n    p.x = x\nfunc (p point) string() -> string:\n    return \"({p.x:000})\"\nfunc (p point) format(spec: string) -> result<string, string>:\n    if spec == \"short\":\n        return Ok(\"{p.x}\")\n    return Err(\"unknown point format\")\n";
}
int main(){try{
    run(point+entry+"    var p = point(x = input)\n    if string(p) == \"(004)\":\n        if format(p, \"short\") is Ok(var text):\n            if text == \"4\":\n                return 1\n    return 0\n");
    run(point+entry+"    var p = point(x = input)\n    if \"{p:short}/{p}\" == \"4/(004)\":\n        return 1\n    return 0\n");
    run(point+entry+"    var p = point(x = input)\n    if format(spec = \"\", value = p) is Ok(var text):\n        if text == \"(004)\":\n            return 1\n    return 0\n");
    run(point+entry+"    var p = point(x = input)\n    if format(p, \"bad\") is Err(var text):\n        if text == \"unknown point format\":\n            return 1\n    return 0\n");
    run("struct opaque:\n    private secret: i32 = 5\n"+entry+"    var p = opaque()\n    if string(p) == \"</hooks.void::opaque>\":\n        return 1\n    return 0\n");
    run("struct node:\n    x: i32\nfunc (self node) string() -> string:\n    return \"{self.x}\"\n"+entry+"    var n: node? = null\n    if string(n) == \"null\":\n        n = node(x = input)\n        if string(n) == \"4\":\n            return 1\n    return 0\n");
    run("value struct box<t>:\n    value: t\nfunc (self box<t>) string() -> string:\n    return \"box({self.value})\"\n"+entry+"    if string(box<i32>(value = input)) == \"box(4)\":\n        return 1\n    return 0\n");
    run("value struct component:\n    value: i32\nfunc (self component) string() -> string:\n    return \"{self.value}\"\nstruct wrapper:\n    const using component\n"+entry+"    if string(wrapper(component = component(value = input))) == \"4\":\n        return 1\n    return 0\n");
    run(point+entry+"    var p = point(x = input)\n    if \"{p:>6|short}/{p:^7}\" == \"     4/ (004) \":\n        return 1\n    return 0\n");
    run("value struct component:\n    value: i32\nfunc (self component) string() -> string:\n    return \"{self.value}\"\nstruct wrapper:\n    private const using component\nfunc (self wrapper) render() -> string:\n    return string(self)\n"+entry+"    if wrapper().render() == \"0\":\n        return 1\n    return 0\n");
    run(entry+"    if string(null) == \"null\":\n        return 1\n    return 0\n");
    reject("struct node:\n    value: i32\n"+entry+"    var text = \"{node():short}\"\n    return 0\n");
    run(point+"func format(value: i32) -> i32:\n    return value\n"+entry+"    if \"{point(x = input):short}\" == \"4\":\n        return 1\n    return 0\n");
    auto rejection=analyze(point+entry+"    return \"{point(x = input):bad}\".length as i32\n");
    for(bool optimized:{false,true}){vd::Jit jit;auto code=jit.compile(*rejection,optimized);bool failed=false;try{code->invoke(4);}catch(const std::runtime_error& error){failed=std::string(error.what()).find("unknown point format")!=std::string::npos;}if(!failed)throw std::runtime_error("hook rejection reason lost");++checks;}
    reject("struct node:\n    value: i32\nfunc (self node) string() -> string:\n    self.value++\n    return \"bad\"\n"+entry+"    return 0\n");
    reject("struct node:\n    values: [i32]\nfunc (self node) string() -> string:\n    self.values.push(1)\n    return \"bad\"\n"+entry+"    return 0\n");
    reject("value struct node:\n    value: i32\nfunc (self node) string() -> i32:\n    return self.value\n"+entry+"    return 0\n");
    auto recursive=analyze("struct node:\n    value: i32\nfunc (self node) string() -> string:\n    if self.value == 0:\n        return \"done\"\n    return string(self)\n"+entry+"    return string(node(value = input)).length as i32\n");
    for(bool optimized:{false,true}){vd::Jit jit;auto code=jit.compile(*recursive,optimized);bool failed=false;try{code->invoke(4);}catch(const std::runtime_error& error){failed=std::string(error.what()).find("Recursive display")!=std::string::npos;}if(!failed || code->invoke(0)!=4)throw std::runtime_error("managed recursion guard/cleanup");checks+=2;}
    auto value_recursion=analyze("value struct node:\n    value: i32\nfunc (self node) string() -> string:\n    if self.value == 0:\n        return \"done\"\n    return string(self)\n"+entry+"    return string(node(value = input)).length as i32\n");
    for(bool optimized:{false,true}){vd::Jit jit;auto code=jit.compile(*value_recursion,optimized);bool failed=false;try{code->invoke(4);}catch(const std::runtime_error& error){failed=std::string(error.what()).find("nesting limit")!=std::string::npos;}if(!failed || code->invoke(0)!=4)throw std::runtime_error("value recursion guard/cleanup");checks+=2;}
    auto worker=analyze("value struct node:\n    value: i32\nfunc (self node) format(spec: string) -> result<string, string>:\n    return Err(spec)\nfunc cleanup():\n    var ignored = \"{node():cleanup}\"\nfunc work(input: i32) -> i32:\n    defer cleanup()\n    return \"{node():primary}\".length as i32\nasync func entry(input: i32) -> i32:\n    var pending = spawn work(input)\n    return await pending\n");
    for(bool optimized:{false,true}){vd::Jit jit;auto code=jit.compile_async(*worker,optimized);bool failed=false;try{code->invoke(4);}catch(const std::runtime_error& error){auto message=std::string(error.what());auto primary=message.find("Format rejected: primary");auto cleanup=message.find("Format rejected: cleanup");failed=primary!=std::string::npos && cleanup!=std::string::npos && primary<cleanup;}if(!failed)throw std::runtime_error("worker display error retention/cleanup");++checks;}
    for(const auto& expression:{"string(self)","self.string()","\"{self}\""})
        reject(std::string("struct node:\n    value: i32\nfunc (self node) string() -> string:\n    return \"ready\"\nfunc (self node) init():\n    var text = ")+expression+"\n"+entry+"    return 0\n");
    run("struct base:\n    value: i32\nvirtual func (self base) string() -> string:\n    return \"base\"\nstruct child extends base:\n    extra: i32\noverride func (self child) string() -> string:\n    return \"child\"\n"+entry+"    var parent: base = child()\n    if string(parent) == \"child\":\n        return 1\n    return 0\n");
    run("interface text:\n    func string() -> string\nstruct node:\n    value: i32\nfunc (self node) string() -> string:\n    return \"node\"\n"+entry+"    var object = node()\n    var view: text = object\n    if view.string() == \"node\" and string(view) == \"</hooks.void::text>\":\n        return 1\n    return 0\n");
    run(point+entry+"    var p = point(x = input)\n    var display: func() -> string = p.string\n    if display() == \"(004)\":\n        return 1\n    return 0\n");
    reject("value struct first:\n    value: i32\nfunc (self first) string() -> string:\n    return \"first\"\nvalue struct second:\n    value: i32\nfunc (self second) string() -> string:\n    return \"second\"\nstruct both:\n    using first\n    using second\n"+entry+"    var shown = string(both())\n    return 0\n");
    auto malformed=analyze(point+entry+"    return string(point(x = input)).length as i32\n")->lower();
    for(auto& function:malformed.functions)if(function.method_name=="string")function.receiver_mutates=true;
    try{vd::ir::validate(malformed);throw std::runtime_error("malformed mutating hook IR accepted");}catch(const std::invalid_argument&){++checks;}
    run("value struct first:\n    value: i32\nfunc (self first) string() -> string:\n    return \"first\"\nvalue struct second:\n    value: i32\nfunc (self second) string() -> string:\n    return \"second\"\n"+entry+"    if string(first()) == \"first\" and string(second()) == \"second\" and \"{first()}/{second()}\" == \"first/second\":\n        return 1\n    return 0\n");
    run("struct node:\n    value: i32\nfunc (self node) string() -> string:\n    return \"node\"\n"+entry+"    var object = node()\n    var optional: node? = null\n    if string(object) == \"node\" and string(optional) == \"null\" and \"{object}/{optional}\" == \"node/null\":\n        return 1\n    return 0\n");
    std::cout<<checks<<" nominal/custom display checks passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
