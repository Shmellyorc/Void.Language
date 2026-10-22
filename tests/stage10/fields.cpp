#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/tooling.hpp"
#include <iostream>
#include <stdexcept>
namespace {
unsigned checks=0;
std::shared_ptr<const vd::ValidatedAnalysis> analyze(const std::string& text) {
    auto result=vd::analyze_source({"/stage10_fields.void",text});
    if(!result.succeeded())throw vd::CompilationError(result.diagnostics);
    ++checks;return result.program;
}
void reject(const std::string& text,const std::string& message) {
    auto a=vd::analyze_source({"/stage10_fields.void",text});
    if(a.succeeded())throw std::runtime_error("invalid field program accepted: "+text);
    for(const auto& d:a.diagnostics)if(d.description.find(message)!=std::string::npos){++checks;return;}
    throw vd::CompilationError(a.diagnostics);
}
void run(const std::string& text,int want) {
    auto a=analyze(text);vd::Jit jit;
    for(bool optimized:{false,true}){if(jit.compile(*a,optimized)->invoke(4)!=want)throw std::runtime_error("field result");++checks;}
}
const std::string entry="func entry(input: i32) -> i32:\n";
const std::string owner="struct box:\n    private secret: i32 = 3\n    const id: i32 = 0\nfunc (self box) read() -> i32:\n    return self.secret + self.id\n";
const std::string initialized="struct box:\n    private const id: i32 = 0\nfunc (self box) init(id: i32):\n    self.id = id\n    self.id = id + 1\nfunc (self box) read() -> i32:\n    return self.id\n";
void invalid_ir(vd::ir::Module module) {
    try{vd::ir::validate(module);}catch(const std::invalid_argument&){++checks;return;}
    throw std::runtime_error("malformed field IR accepted");
}
}
int main() {
    try {
        run(owner+entry+"    var b = box(id = input)\n    return b.read()\n",7);
        run(initialized+entry+"    var b = box(id = input)\n    return b.read()\n",5);
        run("value struct point:\n    private const x: i32 = 0\nfunc (self point) init(x: i32):\n    self.x = x\nfunc (self point) read() -> i32:\n    return self.x\n"+entry+"    var p = point(x = input)\n    var copy = p\n    return copy.read()\n",4);
        run(owner+"func (self box) other(other: box) -> i32:\n    return other.secret\n"+entry+"    var b = box()\n    return b.other(b)\n",3);
        reject(owner+entry+"    var b = box()\n    return b.secret\n","not accessible");
        reject(owner+entry+"    var b = box(secret = 8)\n    return 0\n","not accessible");
        reject(owner+entry+"    var b = box()\n    b.id = 9\n    return 0\n","const stored field");
        reject(owner+"func helper(b: box):\n    b.secret = 1\n"+entry+"    return input\n","not accessible");
        reject(initialized+"func (self box) helper():\n    self.id = 9\n"+entry+"    return 0\n","const stored field");
        reject("struct box:\n    const id: i32\nfunc (self box) init():\n    var alias = self\n    alias.id = 1\n"+entry+"    return 0\n","const stored field");
        reject("struct box:\n    private const const id: i32\n"+entry+"    return 0\n","modifiers");
        reject("struct box:\n    const event ready: func()\n"+entry+"    return 0\n","event");
        const std::string refs="struct box:\n    const items: [i32]\n";
        reject(refs+entry+"    var b = box()\n    b.items.push(1)\n    return 0\n","readonly");
        reject(refs+entry+"    var b = box()\n    b.items[0] = 1\n    return 0\n","const stored field");
        run(refs+entry+"    var items = [1]\n    var b = box(items = items)\n    items.push(2)\n    return b.items.count as i32\n",2);
        run("struct box:\n    private id: i32 = 9\nfunc (self box) read() -> i32:\n    var f = func() -> i32:\n        return self.id\n    return f()\n"+entry+"    return box().read()\n",9);
        run("value struct item<t>:\n    private const stored: t\nfunc (self item<t>) read() -> t:\n    return self.stored\n"+entry+"    return item<i32>().read()\n",0);
        run("struct base:\n    private const id: i32 = 0\nfunc (self base) init(n: i32):\n    self.id = n\nfunc (self base) read() -> i32:\n    return self.id\nstruct child extends base:\n    extra: i32\nfunc (self child) init(n: i32):\n    super.init(n)\n"+entry+"    return child(n = input).read()\n",4);
        reject("struct base:\n    private id: i32\nstruct child extends base:\n    extra: i32\nfunc (self child) read() -> i32:\n    return self.id\n"+entry+"    return input\n","not accessible");
        reject("value struct component:\n    n: i32\nfunc (self component) bump():\n    self.n++\nstruct box:\n    const using component\n"+entry+"    var b = box()\n    b.bump()\n    return 0\n","readonly");
        reject("value struct inner:\n    const id: i32\nvalue struct outer:\n    child: inner\n"+entry+"    var original = outer()\n    var copy = original\n    copy.child.id = 2\n    return 0\n","const stored field");
        reject(owner+entry+"    var b = box()\n    if b is box(secret = var x):\n        return x\n    return 0\n","accessible");
        const std::string interface_component="interface api:\n    func bump()\n    func read() -> i32\nvalue struct component:\n    n: i32 = 7\nfunc (self component) bump():\n    self.n++\nfunc (self component) read() -> i32:\n    return self.n\nstruct box:\n    const using component\n";
        run(interface_component+entry+"    var object = box()\n    var view: api = object\n    return view.read()\n",7);
        reject(interface_component+entry+"    var object = box()\n    var view: api = object\n    view.bump()\n    return 0\n","readonly");
        reject("struct child:\n    id: i32\nstruct box:\n    private required: child\n"+entry+"    var b = box()\n    return 0\n","inaccessible required field");
        run("struct box:\n    const id: i32\nfunc (self box) init(n: i32):\n    self.id += n\n    self.id++\n"+entry+"    return box(n = input).id\n",5);
        auto before=analyze(owner+entry+"    return box().read()\n");
        auto after=analyze(std::string("struct box:\n    public secret: i32 = 3\n    const id: i32 = 0\nfunc (self box) read() -> i32:\n    return self.secret + self.id\n")+entry+"    return box().read()\n");
        if(before->compatibility_contract()==after->compatibility_contract())throw std::runtime_error("privacy omitted from compatibility");++checks;
        auto module=before->lower();vd::ir::validate(module);
        for(auto& f:module.functions)if(f.method_name=="read")f.lexical_owner.clear();
        invalid_ir(std::move(module));
        auto const_program=analyze("struct box:\n    const id: i32\n    mutable: i32\nfunc (self box) update():\n    self.mutable = 1\n"+entry+"    var b = box()\n    b.update()\n    return b.id\n");
        auto const_ir=const_program->lower();
        for(auto& f:const_ir.functions)if(f.method_name=="update")for(auto& b:f.blocks)for(auto& i:b.instructions)if(i.op==vd::ir::Op::gc_write){i.immediate=0;i.initialization=true;}
        invalid_ir(std::move(const_ir));
        auto forged=const_program->lower();
        for(auto& f:forged.functions)if(f.method_name=="update")f.initializer=true;
        invalid_ir(std::move(forged));
        auto metadata=before->declarations();bool found=false;for(const auto& d:metadata)if(d.kind==vd::AnalyzedDeclaration::Kind::managed_field && d.visibility=="private")found=true;if(!found)throw std::runtime_error("private metadata");++checks;
        std::cout<<checks<<" field policy checks passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
