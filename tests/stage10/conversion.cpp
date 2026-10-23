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
    if(vd::analyze_source({"/stage10_conversion.void",text}).succeeded())throw std::runtime_error("invalid conversion accepted: "+text);++checks;
}
void invalid(vd::ir::Module m) {
    try{vd::ir::validate(m);}catch(const std::invalid_argument&){++checks;return;}throw std::runtime_error("invalid display IR accepted");
}
void sums(){
    const std::string declaration="enum shade:\n    plain\n    pair(n: i32, text: string)\n";
    run(declaration+entry+R"VOID(    var value = shade.pair(input, "é\0🎮")
    if string(shade.plain) == "/stage10_conversion.void::shade.plain" and string(value) == "/stage10_conversion.void::shade.pair(4, é\0🎮)":
        return 1
    return 0
)VOID");
    run("struct opaque:\n    private secret: i32 = 8\n"+entry+R"VOID(    var successful: result<opaque?, string> = Ok(null)
    var failure: result<opaque?, string> = Err("é\0🎮")
    if string(successful) == "Ok(null)" and string(failure) == "Err(é\0🎮)":
        return 1
    return 0
)VOID");
    run(entry+R"VOID(    var inner: result<i32, string> = Err("reason")
    var outer: result<result<i32, string>, string> = Ok(inner)
    var fixed: [result<i32, string>, 2] = [Ok(input), inner]
    var dictionary = {"k": fixed}
    if string(outer) == "Ok(Err(reason))" and string(dictionary) == "{{k: [Ok(4), Err(reason)]}}":
        return 1
    return 0
)VOID");
    run("value struct item<t>:\n    private const value: t\nfunc (self item<t>) init(n: t):\n    self.value = n\nfunc (self item<t>) string() -> string:\n    return \"<{self.value:000}>\"\nfunc render<t>(const value: result<item<t>, string>) -> string:\n    return string(value)\n"+entry+R"VOID(    var value: result<item<i32>, string> = Ok(item<i32>(n = input))
    if render<i32>(value) == "Ok(<004>)":
        return 1
    return 0
)VOID");
    run("struct unused:\n    n: i32\nfunc (self unused) string() -> string:\n    var bad = 1 / self.n\n    return string(bad)\n"+entry+R"VOID(    var good: result<i32, unused> = Ok(input)
    var bad: result<unused, string> = Err("active")
    if string(good) == "Ok(4)" and string(bad) == "Err(active)":
        return 1
    return 0
)VOID");
    run(entry+R"VOID(    var good: result<string, i32> = Ok("é🎮")
    var pattern = "000"
    if format(good, pattern) is Err(var reason):
        if format(good, "^10") is Ok(var text):
            if text == "  Ok(é🎮)  " and "{good:>8}" == "  Ok(é🎮)":
                return 1
    return 0
)VOID");
    run("enum branch:\n    leaf(n: i32)\n    nested(items: [branch])\n"+entry+R"VOID(    var values: [branch] = []
    values.push(branch.nested(values))
    if string(values) == "[/stage10_conversion.void::branch.nested(<cycle>)]":
        return 1
    return 0
)VOID");
    run("enum branch:\n    empty\n    nested(items: [branch])\n"+entry+R"VOID(    var shared = [branch.empty]
    var values = [branch.nested(shared), branch.nested(shared)]
    if string(values) == "[/stage10_conversion.void::branch.nested([/stage10_conversion.void::branch.empty]), /stage10_conversion.void::branch.nested([/stage10_conversion.void::branch.empty])]":
        return 1
    return 0
)VOID");
    run(declaration+entry+R"VOID(    var value = shade.plain
    if format(value, "<45") is Ok(var text):
        if text == "/stage10_conversion.void::shade.plain        ":
            return 1
    return 0
)VOID");
    reject(declaration+entry+"    var text = format(shade.plain, \"000\")\n    return 0\n");
    reject(entry+"    var value: result<i32, string> = Ok(input)\n    var text = \"{value:yyyy}\"\n    return 0\n");
    reject("struct item:\n    values: [i32]\nfunc (self item) string() -> string:\n    self.values.push(1)\n    return \"invalid\"\n"+entry+"    var value: result<item, string> = Ok(item(values = [input]))\n    return string(value).length as i32\n");
    reject("enum branch:\n    empty\n    nested(values: [branch])\nfunc mutate(const value: branch):\n    if value is branch.nested(var children):\n        children.push(branch.empty)\n"+entry+"    return input\n");
    {auto worker=analyze("enum shade:\n    plain\nfunc work(n: i32) -> i32:\n    var value: result<i32, shade> = Ok(n)\n    return string(value).length as i32\nasync func entry(input: i32) -> i32:\n    return await spawn work(input)\n");vd::Jit jit;for(bool opt:{false,true}){if(jit.compile_async(*worker,opt)->invoke(4)!=5)throw std::runtime_error("worker result display");++checks;}}
    reject("enum branch:\n    empty\n    nested(values: [branch])\nfunc use_mutable(values: [branch]):\n    values.push(branch.empty)\nfunc escape(const value: branch):\n    if value is branch.nested(var children):\n        use_mutable(children)\n"+entry+"    return input\n");
    reject("func escape(const value: result<[i32], string>) -> [i32]:\n    if value is Ok(var values):\n        return values\n    return []\n"+entry+"    var source: result<[i32], string> = Ok([input])\n    var values = escape(source)\n    values.push(2)\n    return input\n");
    run(entry+R"VOID(    var original: [i32, 2] = [1, 2]
    var value: result<[i32, 2], string> = Ok(original)
    var copied = value
    original[0] = 9
    if string(value) == "Ok([1, 2])" and string(copied) == "Ok([1, 2])" and string(original) == "[9, 2]":
        return 1
    return 0
)VOID");
    auto a=analyze(declaration+entry+"    return string(shade.pair(input, \"text\")).length as i32\n");
    for(unsigned variant=0;variant<3;++variant){auto m=a->lower();bool changed=false;for(auto& f:m.functions)for(auto& b:f.blocks)for(auto& i:b.instructions){if(variant==0 && i.op==vd::ir::Op::sum){i.immediate=i.type.definition->cases.size();changed=true;}else if(variant>0 && i.op==vd::ir::Op::sum_get){i.immediate=variant==1?0:999;changed=true;}}if(!changed)throw std::runtime_error("missing sum IR target");invalid(std::move(m));}
    {auto m=a->lower();bool changed=false;for(auto& f:m.functions)for(auto& b:f.blocks)for(auto& i:b.instructions)if(i.op==vd::ir::Op::sum_get){i.op=vd::ir::Op::extract;changed=true;}if(!changed)throw std::runtime_error("missing checked payload target");invalid(std::move(m));}
    {auto m=a->lower();auto d=std::make_shared<vd::ir::ValueType>();d->identity="/stage10_conversion.void::forged_result";d->name="forged_result";d->module_identity="/stage10_conversion.void";d->result_type=true;d->fields={{"tag",vd::ir::Type::i32},{"ok",vd::ir::Type::i32},{"error",vd::ir::Type::string}};d->cases={{"Ok",1,1},{"Err",2,0}};m.nominal_types.push_back(vd::ir::Type(d));invalid(std::move(m));}
}

void collections(){
    for(const auto& type:{"i8","i16","i32","i64","u8","u16","u32","u64","f32","f64"})run(entry+"    var values: ["+type+"] = [42 as "+type+"]\n    if string(values) == \"[42]\":\n        return 1\n    return 0\n");
    run(entry+R"(    var empty: [i32] = []
    var zero: [i32, 0] = []
    var dictionary: {string: i32} = {}
    if string(empty) == "[]" and string(zero) == "[]" and string(dictionary) == "{{}}":
        return 1
    return 0
)");
    run(entry+R"(    var values = ["é\0🎮", "raw, [text]"]
    if string(values) == "[é\0🎮, raw, [text]]" and string([true, false]) == "[true, false]":
        return 1
    return 0
)");
    run(entry+R"(    var fixed: [i32, 3] = [1, 2, 3]
    var copy = fixed
    copy[0] = 9
    var shared = [1, 2]
    var alias = shared
    alias[0] = 4
    if string(fixed) == "[1, 2, 3]" and string(copy) == "[9, 2, 3]" and string([shared, shared]) == "[[4, 2], [4, 2]]":
        return 1
    return 0
)");
    run(entry+R"(    var values = {"z": [3, 2], "a": [1]}
    if string(values) != "{{z: [3, 2], a: [1]}}":
        return 0
    values.remove("z")
    values["z"] = [9]
    if string(values) == "{{a: [1], z: [9]}}":
        return 1
    return 0
)");
    run(entry+R"(    var values = {3: [true, false]}
    if string(values) == "{{3: [true, false]}}":
        return 1
    return 0
)");
    run(entry+R"(    var values = ["é", "🎮"]
    var spec = "000"
    if format(values, spec) is Err(var reason):
        if format(values, "^12") is Ok(var aligned):
            if aligned == "   [é, 🎮]   " and "{values:>8}" == "  [é, 🎮]":
                return 1
    return 0
)");
    run("struct opaque:\n    private secret: i32 = 5\n"+entry+"    var values: [opaque?] = [null, opaque()]\n    if string(values) == \"[null, </stage10_conversion.void::opaque>]\":\n        return 1\n    return 0\n");
    run("struct node:\n    children: [node]\nfunc (self node) string() -> string:\n    return string(self.children)\n"+entry+"    var values: [node] = []\n    values.push(node(children = values))\n    if string(values) == \"[<cycle>]\" and string(values) == \"[<cycle>]\":\n        return 1\n    return 0\n");
    run("value struct box<t>:\n    private const value: t\nfunc (self box<t>) init(value: t):\n    self.value = value\nfunc (self box<t>) string() -> string:\n    return \"({self.value:000})\"\nfunc render(const values: [box<i32>]) -> string:\n    return string(values)\n"+entry+"    if render([box<i32>(value = input)]) == \"[(004)]\":\n        return 1\n    return 0\n");
    {auto worker=analyze("func work(n: i32) -> i32:\n    return string([[n], [n]]).length as i32\nasync func entry(input: i32) -> i32:\n    return await spawn work(input)\n");vd::Jit jit;for(bool opt:{false,true}){if(jit.compile_async(*worker,opt)->invoke(4)!=10)throw std::runtime_error("worker collection display");++checks;}}
    reject("struct item:\n    n: i32\nfunc (self item) string() -> string:\n    print(self.n)\n    return \"item\"\nfunc work(n: i32) -> i32:\n    return string([item(n = n)]).length as i32\nasync func entry(input: i32) -> i32:\n    return await spawn work(input)\n");
    run("value struct key:\n    n: i32\nfunc (self key) string() -> string:\n    return \"K{self.n}\"\n"+entry+"    var values = {key(n = 3): [true, false]}\n    if string(values) == \"{{K3: [true, false]}}\":\n        return 1\n    return 0\n");
    run("func format(n: i32) -> i32:\n    return n\n"+entry+"    if \"{[input]:>5}\" == \"  [4]\":\n        return 1\n    return 0\n");
    {std::string deep=entry+"    var level0 = [1]\n";for(unsigned n=1;n<72;++n)deep+="    var level"+std::to_string(n)+" = [level"+std::to_string(n-1)+"]\n";deep+="    if string(level71) == \""+std::string(72,'[')+"1"+std::string(72,']')+"\":\n        return 1\n    return 0\n";run(deep);}
    reject(entry+"    var value = format([1], \"000\")\n    return 0\n");
    reject(entry+"    var value = \"{[1]:yyyy}\"\n    return 0\n");
    run("enum color:\n    red\n"+entry+"    if string([color.red]) == \"[/stage10_conversion.void::color.red]\":\n        return 1\n    return 0\n");
    auto a=analyze(entry+"    return string([input]).length as i32\n");
    for(auto op:{vd::ir::Op::collection_enter,vd::ir::Op::collection_append,vd::ir::Op::collection_finish}){
        auto m=a->lower();bool changed=false;for(auto& f:m.functions)for(auto& b:f.blocks)for(auto& i:b.instructions)if(i.op==op){i.type=vd::ir::Type::i64;changed=true;}
        if(!changed)throw std::runtime_error("collection malformed IR target missing");invalid(std::move(m));
    }
    {auto m=a->lower();bool changed=false;for(auto& f:m.functions)if(f.name.find("::$display$collections$")!=std::string::npos){auto slot=f.locals.size();f.locals.push_back(f.parameters.at(0));f.blocks.at(0).instructions.push_back({vd::ir::Op::load,static_cast<unsigned>(slot)});changed=true;break;}if(!changed)throw std::runtime_error("missing collection initialization proof");invalid(std::move(m));}
}

}
int main(int argc,char** argv) {
    try {
        sums();
        if(argc==2 && std::string(argv[1])=="--sum-display"){std::cout<<checks<<" sum display checks passed\n";return 0;}
        collections();
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
