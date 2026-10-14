#include "void/frontend.hpp"
#include "void/backend.hpp"
#include "void/tooling.hpp"
#include <iostream>
#include <stdexcept>
namespace {
void check(bool value, const std::string& message) { if (!value) throw std::runtime_error("Receiver methods: " + message); }
std::shared_ptr<const vd::ValidatedAnalysis> analyzed(const std::string& text) {
    auto a = vd::analyze_source({"/methods.void",text});
    if (!a.succeeded()) throw vd::CompilationError(a.diagnostics);
    return a.program;
}
void invalid(const std::string& text, vd::DiagnosticCode code, const std::string& token) {
    auto a = vd::analyze_source({"/methods.void",text});
    check(!a.succeeded(),"invalid source accepted");
    auto offset = text.rfind(token); check(offset != std::string::npos,"bad diagnostic test anchor");
    for (const auto& d : a.diagnostics) if (d.code == code && d.primary.begin == offset) {
        auto location = d.primary.source->location(offset);
        check(location.line > 0 && location.column > 0 && d.primary.source->text() == text,"diagnostic snapshot/location");
        return;
    }
    throw vd::CompilationError(a.diagnostics);
}
void run(const std::string& text, int expected) {
    vd::Jit jit; auto a = analyzed(text);
    for (bool optimized : {false,true}) check(jit.compile(*a,optimized)->invoke(0)==expected,"source value/evaluation semantics");
}
const std::string point = "value struct point:\n    x: i32 = 4\n    y: i32\n";
const std::string methods = "func (p point) change(amount: i32) -> i32:\n    p.x += amount\n    return p.x\nfunc (p point) inspect() -> i32:\n    return p.x\n";
}
void method_tests(const std::filesystem::path& root) {
    vd::Jit jit;
    for (auto program : {"main","order","recursive","failure","numeric"}) {
        auto a = vd::analyze_project(root/"examples/receiver_methods"/(std::string(program)+".void"));
        check(a.succeeded(),std::string("real example ")+program);
        for (bool optimized : {false,true}) {
            auto native = jit.compile(*a.program,optimized);
            for (int input : {0,1,2,10}) {
                if (std::string(program)=="failure" && input==0) {
                    bool failed=false;
                    try { native->invoke(input); } catch (const std::runtime_error& e) {
                        auto message = std::string(e.what());
                        check(message.find("failure.void:6:")!=std::string::npos && message.find("::method::divide")!=std::string::npos && message.find("::method::forward")!=std::string::npos,"method failure source/trace"); failed=true;
                    }
                    check(failed,"method failure missing"); continue;
                }
                int expected = std::string(program)=="main" ? (input+7)*(input+7)+12 : std::string(program)=="order" ? 154*input+324 : std::string(program)=="recursive" ? (input+1)/2 : std::string(program)=="numeric" ? input*2+16 : 11/input;
                check(native->invoke(input)==expected,std::string("real example result ")+program);
            }
        }
    }
    using Code = vd::DiagnosticCode;
    invalid(point+methods+"func entry(value: i32) -> i32:\n    const p = point()\n    return p.change(1)\n",Code::readonly,"change(1)");
    invalid(point+methods+"func entry(value: i32) -> i32:\n    return point().change(1)\n",Code::receiver,"change(1)");
    invalid(point+methods+"func entry(value: i32) -> i32:\n    var p = point()\n    return p.missing()\n",Code::unknown_method,"missing()");
    invalid(point+methods+"func entry(value: i32) -> i32:\n    var p = point()\n    return p.change()\n",Code::argument_count,"change()");
    invalid(point+methods+"func entry(value: i32) -> i32:\n    var p = point()\n    return p.change(true)\n",Code::type_mismatch,"true");
    invalid(point+methods+"func entry(value: i32) -> i32:\n    return value.inspect()\n",Code::receiver,"inspect()");
    invalid("func (p i32) inspect() -> i32:\n    return p\n",Code::receiver,"p i32");
    invalid(point+"func (p point) x():\n    return\n",Code::duplicate,"x():");
    invalid(point+methods+"func (p point) inspect() -> i32:\n    return 1\n",Code::duplicate,"inspect()");
    run(point+"func (p point) init():\n    p.x = 17\nfunc entry(value: i32) -> i32:\n    return point().x\n",17);
    invalid(point+methods+"func entry(value: i32) -> i32:\n    var p = point()\n    p.init()\n    return value\n",Code::initializer_deferred,"init()");
    invalid(point+methods+"const answer = point().inspect()\n",Code::constant_call,"inspect()");
    invalid(point+methods+"func (p point) bad() -> i32:\n    if p.x > 0:\n        return p.x\n",Code::missing_return,"bad()");
    invalid(point+methods+"func (p point) bad() -> bool:\n    return p.x\n",Code::type_mismatch,"return p.x");
    invalid(point+"func (p point) bad(p: i32):\n    return\n",Code::duplicate,"p: i32");
    invalid(point+methods+"const fixed = point()\nfunc entry(value: i32) -> i32:\n    return fixed.change(1)\n",Code::readonly,"change(1)");
    const std::string box = "value struct box:\n    child: point\n";
    invalid(point+box+methods+"func entry(value: i32) -> i32:\n    const b = box()\n    return b.child.change(1)\n",Code::readonly,"change(1)");
    auto independent_errors = vd::analyze_source({"/methods.void",point+methods+"func f():\n    var p = point()\n    p.unknown(missing)\n"});
    bool method_error=false, argument_error=false;
    for (const auto& d : independent_errors.diagnostics) { method_error |= d.code==Code::unknown_method; argument_error |= d.code==Code::unknown_identifier; }
    check(method_error && argument_error,"unresolved method hid independent argument diagnostics");
    // Whole receiver replacement and copied ordinary parameters are distinct.
    run(point+methods+"func (p point) replace():\n    p = point(x = 20)\nfunc copied(p: point):\n    p.change(100)\nfunc (p point) read_copy() -> i32:\n    copied(p)\n    return p.x\nfunc entry(value: i32) -> i32:\n    const fixed = point()\n    var p = fixed\n    p.replace()\n    return p.x + fixed.read_copy()\n",24);
    // A mutation reached only through a method cycle still poisons readonly calls.
    const std::string cycle = point+"func (p point) first(n: i32):\n    if n > 0:\n        p.second(n - 1)\nfunc (p point) second(n: i32):\n    p.x++\n    p.first(n)\n";
    invalid(cycle+"func entry(value: i32) -> i32:\n    const p = point()\n    p.first(1)\n    return value\n",Code::readonly,"first(1)");
    // Mutating arguments execute before the addressed receiver is read.
    run(point+methods+"func (p point) pair(a: i32, b: i32) -> i32:\n    return p.x * 100 + a * 10 + b\nfunc entry(value: i32) -> i32:\n    var p = point()\n    return p.pair(p.change(1), p.change(1))\n",656);
    // An aggregate copied as an earlier ordinary argument is a snapshot.
    run(point+methods+"func (p point) take(copy: point, changed: i32) -> i32:\n    return copy.x * 100 + p.x * 10 + changed\nfunc entry(value: i32) -> i32:\n    var p = point()\n    return p.take(p, p.change(1))\n",455);
    // Receiver and argument CFGs retain exactly one owned temporary each.
    run(point+methods+"func make(value: i32) -> point:\n    return point(x = 10 / value)\nfunc (p point) flag(ok: bool) -> i32:\n    if ok:\n        return p.x\n    return 0\nfunc entry(value: i32) -> i32:\n    return make(2).flag(true or 10 / value > 0) + point().inspect()\n",9);
    // Nested receiver pointer survives replacement of its enclosing aggregate by
    // an argument-side call; the subsequent mutation must update current storage.
    run(point+box+methods+"func (b box) reset() -> i32:\n    b = box(child = point(x = 10))\n    return 2\nfunc entry(value: i32) -> i32:\n    var b = box()\n    b.child.change(b.reset())\n    return b.child.x\n",12);
    auto source = point+methods+"func entry(value: i32) -> i32:\n    var p = point(x = value)\n    return p.inspect()\n";
    auto before = analyzed(source); vd::ToolingIndex tools(before);
    auto key = "/methods.void::point::method::inspect";
    auto record = tools.declaration(key);
    check(record && record->kind == vd::AnalyzedDeclaration::Kind::receiver_method && !record->receiver_mutates && record->owner=="/methods.void::point" && record->receiver.kind==vd::ir::Type::aggregate,"owned receiver metadata");
    check(tools.definition_at("/methods.void",source.rfind("inspect()"))->identity==key,"method call navigation");
    check(tools.references_to(key).size()==1,"method references");
    check(tools.declaration("/methods.void::point::method::change")->receiver_mutates,"inferred write effect");
    for (bool optimized : {false,true}) {
        auto old = jit.compile(*before,optimized); vd::ReloadSlot slot(old);
        auto edit = source; edit.replace(edit.find("return p.x\n",edit.find("inspect()")),10,"return p.x + 1");
        auto after = analyzed(edit);
        check(jit.replace(slot,*after,optimized) && slot.invoke(10)==11 && old->invoke(10)==10,"method body reload/old lease");
        auto mutate = source; mutate.replace(mutate.find("return p.x\n",mutate.find("inspect()")),10,"p.x++\n    return p.x");
        auto changed = analyzed(mutate);
        check(changed->compatibility_contract()!=before->compatibility_contract() && !jit.replace(slot,*changed,optimized) && slot.invoke(10)==11,"changed mutability published");
        auto plan = vd::plan_invalidation(*before,{"/methods.void"},changed.get());
        check(plan.source_compatibility==vd::SourceCompatibility::restart_required && !plan.changed_declarations.empty(),"tooling missed receiver effect");
    }
    const auto entry = root/"examples/receiver_methods/main.void";
    vd::SourceOverrides overlays;
    auto put = [&](const std::string& id,const std::string& text) { overlays.insert_or_assign(id,vd::SourceFile(id,text)); };
    put("/one.void",point+methods+"private func (p point) hidden() -> i32:\n    return p.x\n");
    put("/two.void",point+"func (p point) inspect() -> i32:\n    return p.x + 10\n");
    put("/main.void","use \"./one.void\" as one\nuse \"./two.void\" as two\nfunc entry(value: i32) -> i32:\n    const a = one.point()\n    const b = two.point()\n    return a.inspect() + b.inspect()\n");
    auto imported = vd::analyze_project(entry,overlays);
    check(imported.succeeded(),"distinct imported method owners");
    for (bool optimized : {false,true}) check(jit.compile(*imported.program,optimized)->invoke(0)==18,"nominal method binding collapsed");
    put("/main.void","use \"./one.void\" as one\nfunc entry(value: i32) -> i32:\n    var p = one.point()\n    return p.hidden()\n");
    auto hidden = vd::analyze_project(entry,overlays);
    check(!hidden.succeeded() && hidden.diagnostics.front().code==Code::visibility && hidden.diagnostics.front().primary.begin==hidden.diagnostics.front().primary.source->text().find("hidden()"),"private imported method exposed/location");
    put("/main.void","use \"./one.void\" as one\nfunc (p one.point) added():\n    return\nfunc entry(value: i32) -> i32:\n    return value\n");
    auto extension = vd::analyze_project(entry,overlays);
    check(!extension.succeeded() && extension.diagnostics.front().code==Code::receiver,"foreign extension method accepted");
    put("/bridge.void","use \"./one.void\" as one\nfunc make() -> one.point:\n    return one.point()\n");
    put("/main.void","use \"./bridge.void\" as bridge\nfunc entry(value: i32) -> i32:\n    return bridge.make().inspect()\n");
    auto indirect = vd::analyze_project(entry,overlays);
    check(!indirect.succeeded() && indirect.diagnostics.front().code==Code::visibility,"indirect import exposed method");
    // Malformed receiver declarations must recover to later independent errors.
    auto recovery = vd::analyze_source({"/methods.void",point+"func (p: point) bad():\n    return\nfunc later():\n    missing()\n"});
    bool syntax=false, unknown=false;
    for (const auto& d : recovery.diagnostics) { syntax |= d.code==Code::unsupported_type; unknown |= d.code==Code::unknown_function; }
    check(!recovery.succeeded() && syntax && unknown,"receiver parser recovery");
    // Signature-first analysis and iterative effects handle a large method SCC.
    std::string graph = point;
    for (int n=0;n<180;++n) graph += "func (p point) m_"+std::to_string(n)+"(left: i32) -> i32:\n    if left > 0:\n        return p.m_"+std::to_string((n+1)%180)+"(left - 1)\n    return p.x\n";
    graph += "func entry(value: i32) -> i32:\n    const p = point()\n    return p.m_0(2)\n";
    run(graph,4);
    std::cerr << "Frontend resource case: bounded receiver-chain semantics" << std::endl;
    std::string deep = point + "func (p point) same() -> point:\n    return p\nfunc entry(value: i32) -> i32:\n    return point()";
    for (int n=0;n<400;++n) deep += ".same()";
    deep += ".inspect() + 1\nfunc later():\n    missing()\n";
    // Include the scalar reader so only the depth limit and independent missing
    // function are errors; contextual type hints must also stay bounded.
    deep = methods + deep;
    auto limited = vd::analyze_source({"/deep-methods.void",deep});
    bool nesting=false, independent=false;
    for (const auto& d : limited.diagnostics) { nesting |= d.code==Code::nesting; independent |= d.code==Code::unknown_function; }
    check(!limited.succeeded() && nesting && independent,"deep method chain did not recover at resource limit");
    auto broken = before->lower(); bool tested=false;
    for (auto& f : broken.functions) for (auto& b : f.blocks) for (auto& i : b.instructions) if (!tested && i.receiver) {
        i.receiver->fields.push_back(999); bool rejected=false;
        try { vd::ir::validate(broken); } catch(const std::invalid_argument&) { rejected=true; }
        check(rejected,"IR accepted invalid receiver path"); tested=true;
    }
    check(tested,"missing receiver IR operand");
    auto readonly_ir = before->lower(); bool readonly_checked=false;
    for (auto& f : readonly_ir.functions) if (f.receiver && !f.receiver_mutates) {
        auto& b = f.blocks.front(); auto value = static_cast<unsigned>(b.instructions.size());
        b.instructions.push_back({vd::ir::Op::argument,0});
        b.instructions.push_back({vd::ir::Op::store,0,value});
        bool rejected=false; try { vd::ir::validate(readonly_ir); } catch(const std::invalid_argument&) { rejected=true; }
        check(rejected,"IR permitted a non-mutating method to write receiver storage"); readonly_checked=true; break;
    }
    check(readonly_checked,"missing readonly receiver IR proof");
    std::cout << "Receiver methods: actual storage, copies, imports, source order, effects, cycles, diagnostics, tooling and reload passed\n";
}
