#include "void/frontend.hpp"
#include "void/backend.hpp"
#include "void/tooling.hpp"
#include <iostream>
#include <stdexcept>
namespace {
void check(bool value, const std::string& message) { if (!value) throw std::runtime_error("Value structs: " + message); }
std::shared_ptr<const vd::ValidatedAnalysis> analyze(const std::string& source) {
    auto a = vd::analyze_source({"/structs.void",source});
    if (!a.succeeded()) throw vd::CompilationError(a.diagnostics);
    return a.program;
}
void invalid(const std::string& source, const std::string& message, unsigned line, unsigned column) {
    auto a = vd::analyze_source({"/invalid.void",source});
    check(!a.succeeded(),"invalid source accepted");
    bool found = false;
    for (const auto& d : a.diagnostics) if (d.description.find(message) != std::string::npos) {
        auto location = d.primary.source->location(d.primary.begin);
        check(location.line == line && location.column == column,"wrong diagnostic location: " + d.description);
        found = true;
    }
    check(found,"missing diagnostic: " + message);
}
}
void struct_tests(const std::filesystem::path& root) {
    vd::Jit jit;
    for (auto name : {"main", "numeric", "order"}) {
        auto a = vd::analyze_project(root/"examples"/"value_structs"/(std::string(name)+".void"));
        check(a.succeeded(),std::string("example analysis ")+name);
        if (std::string(name)=="main") {
            auto source = vd::read_source(root/"examples"/"value_structs"/"main.void");
            vd::ToolingIndex tools(a.program);
            auto definition = tools.definition_at("/main.void",source.text().find("geometry.shape")+9);
            check(definition && definition->identity=="/shapes.void::shape","qualified annotation navigation");
        }
        for (bool optimized : {false,true}) {
            auto native = jit.compile(*a.program,optimized);
            if (std::string(name)=="main") for (auto [input,answer] : {std::pair{-1,28},{0,36},{10,46}}) check(native->invoke(input)==answer,"nested copies/parameters/returns");
            else if (std::string(name)=="numeric") for (int input : {-10,0,123}) check(native->invoke(input)==input+5,"mixed numeric layout/default parity");
            else {
                check(native->invoke(2)==5,"named runtime construction");
                for (int input : {0,1}) {
                    bool failed = false;
                    try { native->invoke(input); }
                    catch (const std::exception& failure) {
                        auto text = std::string(failure.what());
                        check(text.find(input==0 ? "zero" : "conversion")!=std::string::npos,"named fields reordered: " + text); failed = true;
                    }
                    check(failed,"runtime failure missing");
                }
            }
        }
    }
    const std::string point = "value struct point:\n    x: i32 = 4\n    flag: bool\n";
    invalid(point+"func f():\n    var p = point(missing = 1)\n","Unknown constructor field",5,19);
    invalid(point+"func f():\n    var p = point(x = 1, x = 2)\n","Duplicate initializer",5,26);
    invalid(point+"func f():\n    var p = point(1)\n","requires named fields",5,19);
    invalid(point+"func f():\n    var p = point(x = true)\n","Type mismatch",5,23);
    invalid(point+"func f():\n    const p = point()\n    p.x = 8\n","readonly",6,5);
    invalid(point+"func f():\n    var p = point()\n    p.nope = 8\n","Unknown field",6,7);
    invalid(point+"func f():\n    var p = point()\n    var v = p.nope\n","Unknown field",6,15);
    invalid("value struct bad:\n    x: string\n","Unsupported type",2,8);
    invalid("value struct bad:\n    x: i32\n    x: bool\n","Duplicate field",3,5);
    invalid("value struct bad:\n    child: bad\n","Recursive value layout",1,14);
    invalid("value struct first:\n    child: second\nvalue struct second:\n    child: first\n","Recursive value layout",1,14);
    invalid("func runtime() -> i32:\n    return 1\nvalue struct bad:\n    x: i32 = runtime()\n","cannot call a function",4,14);
    invalid("value struct bad:\n    x: i32 = self.x\n","Unknown identifier",2,14);
    invalid(point+"func f():\n    var p = point()\n    var q = p == p\n","equality is not implemented",6,15);
    invalid(point+"func f() -> point:\n    return 1\n","Type mismatch",5,5);
    invalid(point+"func f(p: point) -> point:\n    if p.flag:\n        return p\n","Missing return path",4,6);
    const std::string nested = point+"value struct box:\n    p: point\nfunc entry(value: i32) -> i32:\n    const original = box()\n    var copy = original\n    copy.p.x += value\n    return copy.p.x\n";
    auto before = analyze(nested);
    vd::ToolingIndex index(before);
    auto read = nested.rfind(".x")+1;
    check(index.definition_at("/structs.void",read)->identity=="/structs.void::point::field::x","field read navigation");
    auto write = nested.find(".x +=")+1;
    check(index.definition_at("/structs.void",write)->identity=="/structs.void::point::field::x","field write navigation");
    auto type = nested.find("p: point")+3;
    check(index.definition_at("/structs.void",type)->identity=="/structs.void::point","type navigation");
    auto constructor = nested.find("box()",nested.find("func"));
    check(index.definition_at("/structs.void",constructor)->identity=="/structs.void::box","constructor navigation");
    auto native = jit.compile(*before); vd::ReloadSlot slot(native);
    auto body = nested; body.replace(body.rfind("return copy.p.x"),15,"return copy.p.x + 1");
    check(jit.replace(slot,*analyze(body)) && slot.invoke(2)==7,"compatible aggregate function-body reload");
    for (auto change : {std::pair{std::string("i32 = 4"),std::string("i32 = 5")},std::pair{std::string("flag: bool"),std::string("flag: i32")}}) {
        auto edited = body; edited.replace(edited.find(change.first),change.first.size(),change.second);
        auto after = analyze(edited);
        auto plan = vd::plan_invalidation(*before,{"/structs.void"},after.get());
        check(plan.source_compatibility == vd::SourceCompatibility::restart_required,"aggregate layout change invalidation missed");
        for (const auto& key : plan.dependent_declarations) check(index.declaration(key).has_value(),"non-declaration dependency escaped tooling plan");
        check(!jit.replace(slot,*after) && slot.invoke(2)==7,"aggregate contract change published");
    }
    invalid(nested+"func mutate():\n    const original_copy = box()\n    original_copy.p.x++\n","readonly",13,5);
    invalid(point+"value struct point:\n    other: bool\n","Duplicate declaration",4,14);
    invalid(point+"const point = 1\n","Duplicate declaration",4,7);
    invalid("value struct bad:\n    x: i32 = 1 / 0\n","zero",2,16);
    invalid("value struct bad:\n    x: i32 = true\n","Type mismatch",2,5);
    invalid("value struct bad:\n    x: i32 = later\nconst later = bad().x\n","Circular",1,14);
    auto nominal = point+"value struct other:\n    x: i32 = 4\n    flag: bool\nfunc f():\n    var p: point = other()\n";
    invalid(nominal,"Type mismatch",8,20);
    // Distinct module identities remain nominal even for identical layouts.
    const auto entry = root/"examples"/"value_structs"/"main.void";
    vd::SourceOverrides overlays;
    auto put = [&](const std::string& id,const std::string& text) { overlays.insert_or_assign(id,vd::SourceFile(id,text)); };
    put("/one.void",point+"private value struct hidden:\n    x: bool\n");
    put("/two.void",point);
    put("/main.void","use \"./one.void\" as one\nuse \"./two.void\" as two\nfunc entry(value: i32) -> i32:\n    var p: one.point = two.point()\n    return value\n");
    auto mismatch = vd::analyze_project(entry,overlays);
    check(!mismatch.succeeded() && mismatch.diagnostics.front().code == vd::DiagnosticCode::type_mismatch,"cross-module nominal types collapsed");
    put("/main.void","use \"./one.void\" as one\nfunc entry(value: i32) -> i32:\n    var p: one.hidden\n    return value\n");
    check(!vd::analyze_project(entry,overlays).succeeded(),"private imported value type exposed");
    put("/main.void","use \"./one.void\"\nuse \"./two.void\"\nfunc entry(value: i32) -> i32:\n    var p = point()\n    return value\n");
    auto ambiguous = vd::analyze_project(entry,overlays);
    check(!ambiguous.succeeded() && ambiguous.diagnostics.front().code == vd::DiagnosticCode::ambiguous,"ambiguous types selected arbitrarily");
    put("/one.void","const wide: u64 = 18446744073709551614\n");
    put("/main.void","use \"./one.void\" as values\nvalue struct wide_value:\n    x: u64 = values.wide + 1\nfunc entry(value: i32) -> i32:\n    var p = wide_value()\n    if p.x == values.wide + 1:\n        return value\n    return -1\n");
    auto contextual = vd::analyze_project(entry,overlays);
    check(contextual.succeeded(),"qualified constants lost contextual numeric typing");
    for (bool optimized : {false,true}) check(jit.compile(*contextual.program,optimized)->invoke(8)==8,"qualified numeric default/runtime parity");
    // Struct declaration recovery must retain independent later source errors.
    auto recovered = vd::analyze_source({"/recover.void","value struct damaged:\n    field i32\nfunc later() -> i32:\n    return missing\n"});
    check(!recovered.succeeded() && recovered.diagnostics.size()==2,"struct parser recovery lost later declaration");
    std::string excessive = "value struct t_0:\n    x: i32\n";
    for (unsigned n=1;n<18;++n) excessive += "value struct t_"+std::to_string(n)+":\n    a: t_"+std::to_string(n-1)+"\n    b: t_"+std::to_string(n-1)+"\n";
    check(!vd::analyze_source({"/large.void",excessive}).succeeded(),"unbounded expanded layout accepted");
    auto temporaries = analyze(point+"func make() -> point:\n    return point(x = 7, flag = true)\nfunc entry(value: i32) -> i32:\n    var p = point(flag = value == 0 or 1 / value > 0, x = make().x)\n    return (p).x + point().x\n");
    for (bool optimized : {false,true}) check(jit.compile(*temporaries,optimized)->invoke(0)==11,"temporary fields/short-circuit constructor operand");
    bool host_rejected = false;
    try { jit.compile(temporaries->lower(),"make"); } catch (const std::invalid_argument&) { host_rejected = true; }
    check(host_rejected,"unapproved external aggregate ABI accepted");
    auto lowered = before->lower();
    // Deliberately malformed aggregate operations are rejected before LLVM.
    bool checked = false;
    for (auto& f : lowered.functions) for (auto& b : f.blocks) for (auto& i : b.instructions) if (!checked && i.op==vd::ir::Op::extract) {
        i.immediate = 999; bool rejected=false; try { vd::ir::validate(lowered); } catch(const std::invalid_argument&) { rejected=true; }
        check(rejected,"IR accepted invalid field index"); checked=true;
    }
    check(checked,"aggregate IR missing");
    std::cout << "Value struct source: nested/mixed layouts, copies, normal/O2 JIT, diagnostics, tooling and reload passed\n";
}
