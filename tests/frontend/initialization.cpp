#include "void/frontend.hpp"
#include "void/backend.hpp"
#include "void/tooling.hpp"
#include <iostream>
#include <stdexcept>
namespace {
void check(bool condition,const std::string& message) { if (!condition) throw std::runtime_error("Initialization: " + message); }
const std::string type="value struct point:\n    x: i32 = 4\n    y: i32\n";
const std::string init="func (p point) init(x: i32 = 3, y: i32 = 10):\n    p.x += x\n    p.y = y\n";
std::shared_ptr<const vd::ValidatedAnalysis> analyze(const std::string& text) {
    auto result=vd::analyze_source({"/initialization.void",text});
    if (!result.succeeded()) throw vd::CompilationError(result.diagnostics);
    return result.program;
}
void invalid(const std::string& text,vd::DiagnosticCode code,const std::string& token) {
    auto result=vd::analyze_source({"/initialization.void",text});
    check(!result.succeeded(),"invalid source accepted");
    auto offset=text.rfind(token); check(offset!=std::string::npos,"test anchor");
    for (const auto& d : result.diagnostics) if (d.code==code && d.primary.begin==offset && d.primary.source->text()==text) return;
    throw vd::CompilationError(result.diagnostics);
}
void run(const std::string& text,int expected) {
    auto analysis=analyze(text); vd::Jit jit;
    for (bool optimized : {false,true}) check(jit.compile(*analysis,optimized)->invoke(0)==expected,"value/order/copy result");
}
// Retire each native generation/context while retaining one JIT. LLVM layout
// caches must not confuse recycled Type addresses with earlier aggregate shapes.
void layout_retirement() {
    vd::Jit jit;
    for (int generation=0; generation<64; ++generation) {
        const int fields=1+(generation*7)%17;
        std::string source="value struct cell:\n";
        for (int field=0; field<fields; ++field) source+="    f"+std::to_string(field)+": i32 = "+std::to_string(field)+"\n";
        source+="func (c cell) init(value: i32):\n    c.f"+std::to_string(fields-1)+" += value\nfunc entry(value: i32) -> i32:\n    return cell(value).f"+std::to_string(fields-1)+"\n";
        auto analysis=analyze(source);
        std::cerr << "Initialization layout generation " << generation << " fields=" << fields << std::endl;
        for (bool optimized : {false,true}) check(jit.compile(*analysis,optimized)->invoke(10)==fields+9,"layout generation result");
    }
    for (int generation=0; generation<16; ++generation) {
        auto wide=generation%2 ? "i64" : "i8";
        std::cerr << "Initialization nested layout generation " << generation << " padding=" << wide << std::endl;
        auto analysis=analyze(std::string("value struct cell:\n    flag: bool = true\n    padding: ")+wide+" = 3\n    value: i32\n"
            "func (c cell) init(value: i32 = 0):\n    c.value = value\n"
            "value struct wrapper:\n    tag: bool\n    child: cell\n    tail: f64 = 1.5\n"
            "func (w wrapper) init(value: i32):\n    w.child = cell(value)\n"
            "func copied(w: wrapper) -> wrapper:\n    w.child.value++\n    return w\n"
            "func entry(value: i32) -> i32:\n    var original = wrapper(value)\n    var copy = copied(original)\n    return original.child.value + copy.child.value\n");
        for (bool optimized : {false,true}) check(jit.compile(*analysis,optimized)->invoke(10)==21,"nested layout retirement/copy/return");
    }
    std::cout << "80 aggregate layout generations retired through normal/O2 JIT\n";
}
}
void initialization_tests(const std::filesystem::path& root) {
    vd::Jit jit;
    for (auto name : {"main","order","required","numeric","failure","copies"}) {
        auto result=vd::analyze_project(root/"examples/initialization"/(std::string(name)+".void"));
        if (!result.succeeded()) throw vd::CompilationError(result.diagnostics);
        for (bool optimized : {false,true}) {
            std::cerr << "Initialization project " << name << " optimized=" << optimized << std::endl;
            auto code=jit.compile(*result.program,optimized);
            for (int input : {1,2,10}) {
                int expected=std::string(name)=="main" ? 146+2*input : std::string(name)=="order" ? 14603+211*input : std::string(name)=="required" ? 2*input+2 : std::string(name)=="numeric" ? input-4 : std::string(name)=="copies" ? 3*input+213 : 100/input;
                check(code->invoke(input)==expected,std::string("real source ")+name);
            }
            if (std::string(name)=="failure") {
                bool failed=false; try { code->invoke(0); } catch (const std::runtime_error& e) {
                    auto message=std::string(e.what());
                    check(message.find("failure.void:5:")!=std::string::npos && message.find("::method::init")!=std::string::npos,"initializer failure trace"); failed=true;
                }
                check(failed,"failure was published as success");
            }
        }
    }
    std::cerr << "Initialization begin: layout retirement" << std::endl;
    layout_retirement();
    using C=vd::DiagnosticCode;
    auto entry=[](const std::string& expression) { return "func entry(value: i32) -> i32:\n    return "+expression+"\n"; };
    run(type+init+entry("point(y = 2, x = 5).x + point(1).y"),19);
    run(type+init+"func entry(value: i32) -> i32:\n    var a: point\n    const b = a\n    a.x = 100\n    return b.x + b.y\n",17);
    run(type+"func (p point) init():\n    return\n"+entry("point().x"),4);
    invalid(type+init+entry("point(1, x = 2).x"),C::duplicate,"x = 2");
    invalid(type+init+entry("point(x = 1, 2).x"),C::argument_count,"2");
    invalid(type+init+entry("point(z = 1).x"),C::argument_count,"z = 1");
    invalid(type+init+entry("point(true).x"),C::type_mismatch,"true");
    invalid(type+init+entry("point(1, 2, 3).x"),C::argument_count,"point(1");
    invalid(type+"func (p point) init(x: i32):\n    p.x = x\n"+entry("point().x"),C::argument_count,"point()");
    invalid(type+"func (p point) init(x: i32):\n    p.x = x\nfunc entry(value: i32) -> i32:\n    var p: point\n    return value\n",C::argument_count,"p: point");
    invalid(type+"func (p point) init() -> i32:\n    return 1\n",C::type_mismatch,"init()");
    invalid(type+"func (p point) init():\n    return 1\n",C::type_mismatch,"return 1");
    invalid(type+init+"func (p point) init():\n    return\n",C::duplicate,"init()");
    invalid(type+init+"func entry(value: i32) -> i32:\n    var p = point()\n    p.init()\n    return value\n",C::initializer_deferred,"init()");
    invalid(type+init+entry("point.init()"),C::initializer_deferred,"init()");
    invalid(type+"func (p point) deinit():\n    return\n",C::initializer_deferred,"deinit()");
    invalid(type+"func (p point) init(x: i32 = true):\n    return\n",C::type_mismatch,"x: i32");
    invalid(type+"func (p point) init(x: i32 = 1, y: i32):\n    return\n",C::argument_count,"y: i32");
    invalid(type+"func (p point) init(x: i32 = p.x):\n    return\n",C::unknown_identifier,"p.x");
    invalid(type+"func f() -> i32:\n    return 1\nfunc (p point) init(x: i32 = f()):\n    return\n",C::constant_call,"f()");
    invalid(type+init+"const fixed = point()\n",C::constant_call,"point()");
    invalid(type+init+"value struct holder:\n    child: point = point()\n",C::constant_call,"point()");
    invalid(type+init+"value struct holder:\n    child: point\nconst fixed = holder()\n",C::constant_call,"holder()");
    invalid(type+"func (p point) init(x: i32):\n    p.x = x\nvalue struct holder:\n    child: point\nfunc (h holder) init():\n    return\n",C::argument_count,"child: point");
    run(type+init+"value struct holder:\n    child: point\nfunc (h holder) init(copy: point):\n    copy.x = 100\n    h.child.y = copy.y\n"+entry("holder(point(9, 8)).child.x + point(2).x"),13);
    run(type+init+"func init(value: i32) -> i32:\n    return value + 1\n"+entry("init(point().x)"),8);
    invalid(type+init+"value struct holder:\n    child: point\nfunc (h holder) init(copy: point = point()):\n    return\n",C::constant_call,"point()");
    invalid(type+init+"func entry(value: i32) -> i32:\n    const p = point()\n    p.x++\n    return value\n",C::readonly,"p.x++");
    auto recovered=vd::analyze_source({"/initialization.void",type+"func (p point) init(x: i32 = true):\n    return\nfunc later():\n    missing()\n"});
    bool mismatch=false, unknown=false;
    for (const auto& d : recovered.diagnostics) { mismatch |= d.code==C::type_mismatch; unknown |= d.code==C::unknown_function; }
    check(mismatch && unknown,"initializer default hid independent later diagnostics");
    const auto required_child=type+"func (p point) init(x: i32):\n    p.x = x\nvalue struct holder:\n    child: point\n";
    invalid(required_child+entry("holder().child.x"),C::argument_count,"holder()");
    invalid(required_child+"func entry(value: i32) -> i32:\n    var h: holder\n    return value\n",C::argument_count,"h: holder");
    auto default_policy=analyze(type+init+entry("point().x"));
    auto changed_default=init; changed_default.replace(changed_default.find("= 3"),3,"= 5");
    check(default_policy->compatibility_contract()!=analyze(type+changed_default+entry("point().x"))->compatibility_contract(),"initializer default policy absent from contract");
    check(default_policy->compatibility_contract()!=analyze(type+"private "+init+entry("point().x"))->compatibility_contract(),"initializer access absent from contract");
    // Pure explicit aggregate defaults remain copies, not runtime construction.
    run("value struct seed:\n    x: i32 = 8\nconst fixed = seed(x = 9)\nvalue struct holder:\n    child: seed = fixed\nfunc (h holder) init():\n    h.child.x++\n"+entry("holder().child.x"),10);
    // Methods reached by the initializer are frozen transitively, including cycles.
    auto source=type+"func help(value: i32) -> i32:\n    if value > 0:\n        return other(value - 1)\n    return 8\nfunc other(value: i32) -> i32:\n    return help(value)\nfunc (p point) init():\n    p.x = help(1)\nfunc unrelated() -> i32:\n    return 1\n"+entry("point().x + unrelated()");
    auto before=analyze(source); vd::ToolingIndex tools(before);
    auto identity="/initialization.void::point::method::init";
    check(tools.declaration(identity)->kind==vd::AnalyzedDeclaration::Kind::initializer,"lifecycle metadata");
    check(tools.definition_at("/initialization.void",source.rfind("point().x"))->identity==identity,"constructor navigation");
    auto parameters=analyze(type+init+entry("point().x")); vd::ToolingIndex parameter_tools(parameters);
    auto record=parameter_tools.declaration(identity);
    check(record->initializer_parameters.size()==2 && record->initializer_parameters[0].name=="x" && record->initializer_parameters[0].default_source.has_value(),"parameter/default source metadata");
    for (bool optimized : {false,true}) {
        auto old=jit.compile(*before,optimized); vd::ReloadSlot slot(old);
        auto body=source; body.replace(body.find("return 8"),8,"return 9"); auto changed=analyze(body);
        check(!jit.replace(slot,*changed,optimized) && slot.invoke(0)==9,"initializer helper changed state without restart");
        auto plan=vd::plan_invalidation(*before,{"/initialization.void"},changed.get());
        check(plan.source_compatibility==vd::SourceCompatibility::restart_required && !plan.changed_declarations.empty(),"tooling restart metadata");
        auto direct=source; direct.replace(direct.find("p.x = help(1)"),13,"p.x = help(2)");
        check(!jit.replace(slot,*analyze(direct),optimized),"initializer body reloaded");
        check(jit.replace(slot,*analyze(source+"# unchanged policy\n"),optimized),"initializer trivia forced restart");
        auto free_body=source; free_body.replace(free_body.find("return 1\n",free_body.find("func unrelated")),8,"return 2");
        check(jit.replace(slot,*analyze(free_body),optimized) && slot.invoke(0)==10 && old->invoke(0)==9,"unrelated body reload/old lease");
    }
    auto ir=before->lower(); bool checked=false;
    for (auto& f : ir.functions) for (auto& b : f.blocks) for (auto& i : b.instructions) if (!checked && i.initialization) {
        i.initialization=false; bool rejected=false; try { vd::ir::validate(ir); } catch (const std::invalid_argument&) { rejected=true; }
        check(rejected,"IR accepted ordinary lifecycle invocation"); checked=true;
    }
    check(checked,"missing initializer IR proof");
    // Owner visibility applies to construction, but copies and public wrappers work.
    vd::SourceOverrides overlays;
    auto put=[&](const std::string& id,const std::string& text) { overlays.insert_or_assign(id,vd::SourceFile(id,text)); };
    put("/types.void",type+"private "+init+"value struct wrapper:\n    child: point\nfunc make() -> point:\n    return point()\n");
    put("/main.void","use \"./types.void\" as types\n"+entry("types.point().x"));
    auto project=root/"examples/initialization/main.void";
    auto hidden=vd::analyze_project(project,overlays); check(!hidden.succeeded() && hidden.diagnostics.front().code==C::visibility,"private construction exposed");
    put("/main.void","use \"./types.void\" as types\n"+entry("types.make().x + types.wrapper().child.x"));
    auto factory=vd::analyze_project(project,overlays); if (!factory.succeeded()) throw vd::CompilationError(factory.diagnostics);
    check(jit.compile(*factory.program)->invoke(0)==14,"private factory/nested owner access");
    put("/main.void","use \"./types.void\" as types\nvalue struct foreign:\n    child: types.point\n"+entry("foreign().child.x"));
    auto inaccessible=vd::analyze_project(project,overlays); check(!inaccessible.succeeded() && inaccessible.diagnostics.front().code==C::visibility,"foreign field bypassed private init");
    std::cout << "Initialization: parameters/defaults, recursive values, order, copies, lifecycle diagnostics, visibility, tooling, failure and reload passed\n";
}
