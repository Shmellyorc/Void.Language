#include "proof.hpp"
#include "void/backend.hpp"
#include "void/frontend.hpp"
#include <algorithm>
#include <future>
#include <iostream>
using managed_test::require;
namespace {
std::shared_ptr<const vd::ValidatedAnalysis> analyze(const std::string& text) {
    auto result=vd::analyze_source({"/strings.void",text});if(!result.succeeded()) throw vd::CompilationError(result.diagnostics);return result.program;
}
void reject(const std::string& text) {
    auto result=vd::analyze_source({"/strings.void",text});require(!result.succeeded() && !result.diagnostics.empty(),"invalid string program accepted");
}
void run(const std::string& text,int expected) {
    auto program=analyze(text);vd::Jit jit;for(bool opt:{false,true}) require(jit.compile(*program,opt)->invoke(10)==expected,"string program result");
}
void language() {
    const std::string entry="func entry(v: i32) -> i32:\n";
    for(const auto& expression:{"\"score\" + 100","\"a\" < \"b\"","\"a\" * 2","\"a\" == 1","null","\"a\" as i32"})
        reject(entry+"    var text: string = "+expression+"\n    return v\n");
    for(const auto& literal:{"\"\\q\"","\"\\xFF\"","\"\\u{d800}\"","\"\\u{110000}\"","\"\\u{}\"","\"\\u{1234567}\"","\"\\u1234\"","\"\\u{zz}\"","\"{name}\"","\"}\"","\"\"\"multiline\"\"\"","\"unfinished","\"line\nnext\""})
        reject(entry+"    var text = "+literal+"\n    return v\n");
    for(char control:{'\0','\t','\x7f'}) reject(entry+"    var text = \"a"+control+"b\"\n    return v\n");
    reject(entry+"    var text: string?\n    return v\n");
    reject(entry+"    var text = \"a\"\n    text -= \"b\"\n    return v\n");
    reject(entry+"    const text = \"a\"\n    text += \"b\"\n    return v\n");
    reject("struct box:\n    text: string\n"+entry+"    const b = box()\n    b.text = \"changed\"\n    return v\n");
    reject("struct node:\n    n: i32\nvalue struct mixed:\n    text: string\n    child: node\n"+entry+"    const b = mixed(child = node())\n    var copy = b\n    copy.child.n++\n    return v\n");
    reject("func string():\n    pass\n");
    reject("func f(text: string = \"x\"):\n    pass\n");
    run("value struct box:\n    text: string\nfunc edit(b: box) -> box:\n    b.text += \"!\"\n    return b\n"+entry+"    const b = box(text = \"hello\")\n    var c = edit(b)\n    if c.text == \"hello!\" and b.text == \"hello\":\n        return 1\n    return 0\n",1);
    run("struct state:\n    text: string\nstruct owner:\n    child: state\n    n: i32\nfunc rhs(s: owner) -> string:\n    s.n++\n    s.child = state(text = \"changed\")\n    return \"!\"\n"+entry+"    var old = state(text = \"original\")\n    var s = owner(child = old)\n    s.child.text += rhs(s)\n    if old.text == \"original!\" and s.child.text == \"changed\" and s.n == 1:\n        return v\n    return -100\n",10);
    run("func recurse(n: i32, text: string) -> string:\n    if n == 0:\n        return text\n    return recurse(n - 1, text + \"a\")\n"+entry+"    if recurse(10, \"\") == \"aaaaaaaaaa\":\n        return v\n    return -100\n",10);
    run("struct base:\n    text: string = \"base\"\nvirtual func (b base) read() -> string:\n    return b.text\nstruct derived extends base:\n    suffix: string = \"child\"\noverride func (b derived) read() -> string:\n    return b.text + b.suffix\n"+entry+"    var b: base = derived()\n    if b.read() == \"basechild\":\n        return v\n    return -100\n",10);
    auto bad=vd::analyze_source({"/recover.void",entry+"    var text = \"bad\\q\"\n    missing()\n    return v\n"});require(!bad.succeeded() && bad.diagnostics.size()==2,"literal recovery lost independent error");
    bad=vd::analyze_source({"/recover.void",entry+"    var text = \"unfinished\n    missing()\n    return v\n"});require(!bad.succeeded() && bad.diagnostics.size()==2 && bad.diagnostics[1].primary.source->location(bad.diagnostics[1].primary.begin).line==3,"unterminated string consumed next physical line");
}
void independent() {
    auto escapes=analyze("const text = \"\\\"\\\\\\n\\r\\t\\0\"\n")->lower();
    require(std::find(escapes.strings.begin(),escapes.strings.end(),std::string("\"\\\n\r\t\0",6))!=escapes.strings.end(),"escape byte template differs from specified content");
    auto module=analyze("func entry(v: i32) -> i32:\n    var text = \"hello\" + \" world\"\n    if text == \"hello world\":\n        return v\n    return 0\n")->lower();
    auto rejected=[](vd::ir::Module m) {bool bad=false;try{vd::ir::validate(m);}catch(const std::invalid_argument&){bad=true;}require(bad,"forged string IR accepted");};
    auto bad=module;bad.coordinated=false;rejected(std::move(bad));
    bad=module;bad.strings[0]="\xc0\xaf";rejected(std::move(bad));
    bad=module;bad.strings.push_back(bad.strings.front());rejected(std::move(bad));
    for(auto& block:module.functions.front().blocks) for(auto& i:block.instructions) {
        if(i.op==vd::ir::Op::string_literal) {auto old=i.immediate;i.immediate=999999;rejected(module);i.immediate=old;}
        if(i.op==vd::ir::Op::string_concat) {auto old=i.op;i.op=vd::ir::Op::add;rejected(module);i.op=old;}
        if(i.op==vd::ir::Op::string_equal) {auto old=i.op;i.op=vd::ir::Op::equal;rejected(module);i.op=old;}
    }
    bad=module;bad.strings={std::string(vd::ir::string_constant_limit+1,'a')};rejected(std::move(bad));
    bad=module;bad.strings.clear();for(char c='a';c<='e';++c) bad.strings.emplace_back(vd::ir::string_constant_limit,c);rejected(std::move(bad));
    reject("const huge = \""+std::string(vd::ir::string_constant_limit+1,'x')+"\"\n");
    std::string doubling="const a0 = \"a\"\n";for(unsigned n=1;n<=25;++n) doubling+="const a"+std::to_string(n)+" = a"+std::to_string(n-1)+" + a"+std::to_string(n-1)+"\n";reject(doubling);
}
void source(const std::filesystem::path& root,bool opt) {
    vd::Jit jit;
    for(auto [file,expected]:{std::pair{"main",11},std::pair{"initialization",12},std::pair{"unicode",13}}) {
        auto p=vd::compile_project(root/"examples/strings"/(std::string(file)+".void"));require(jit.compile(p.module,p.module.entry_symbol,opt)->invoke(10)==expected,"real string fixture result");
    }
    auto p=vd::compile_project(root/"examples/strings/stress.void");auto runtime=std::make_shared<vd::Runtime>(1);
    auto code=jit.compile(p.module,p.module.entry_symbol,opt,{},runtime);
    managed_test::Probe roots;roots.inspect_roots=true;string_test::Calls calls;
    {managed_test::Observe observe(roots);string_test::Observe strings(calls);require(code->invoke(10)==310,"string GC stress result");}
    require(roots.roots_valid && roots.frames.empty() && calls.nested_roots>0 && calls.count>1000,"string actual nested roots not observed");
    unsigned rejected=0;
    for(unsigned call:{1u,2u,3u,8u}) for(int fail=0;fail<4;++fail) {
        managed_test::Probe cleanup;cleanup.inspect_roots=true;string_test::Calls failure;failure.fail_at=call;failure.fail_after=fail;
        {managed_test::Observe observe(cleanup);string_test::Observe strings(failure);try {code->invoke(10);}catch(const std::runtime_error&) {++rejected;}}
        require(failure.protected_output && cleanup.roots_valid && cleanup.frames.empty(),"string failure publication/frame cleanup");
        vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);runtime->collect();require(runtime->snapshot().objects==0,"string failure leaked objects");
    }
    require(rejected>0,"string allocation failures did not execute");
    require(code->invoke(10)==310,"string failure recovery");
    managed_test::Probe retained;retained.capture=true;retained.capture_at=1;
    {managed_test::Observe observe(retained);require(code->invoke(10)==310,"old string holder setup");}
    code.reset();require(jit.live_generations()==0,"strings retained allocating JIT generation");
    {vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);runtime->collect();require(runtime->contains(retained.retained->get()),"old string-bearing holder collected");}
    auto next=jit.compile(p.module,p.module.entry_symbol,opt,{},runtime);require(next->invoke(10)==310,"new string generation failed");retained.retained.reset();
    {vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);runtime->collect();require(runtime->snapshot().objects==0,"string holder leaked");}
    auto initialization=vd::compile_project(root/"examples/strings/initialization.void");
    auto initialized=jit.compile(initialization.module,initialization.module.entry_symbol,opt,{},runtime);
    rejected=0;
    for(unsigned call=1;call<=8;++call) {
        managed_test::Probe cleanup;cleanup.inspect_roots=true;string_test::Calls fault;fault.fail_at=call;
        {managed_test::Observe observe(cleanup);string_test::Observe strings(fault);try {initialized->invoke(10);}catch(const std::runtime_error& e) {++rejected;require(std::string(e.what()).find("initialization.void:")!=std::string::npos,"string init failure lost source trace");}}
        require(fault.protected_output && cleanup.roots_valid && cleanup.frames.empty(),"string default/init failure cleanup");
        vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);runtime->collect();require(runtime->snapshot().objects==0,"string initialization failure leak");
    }
    require(rejected>0 && initialized->invoke(10)==12,"string default/init failure recovery");
}
void reload(bool opt) {
    const std::string text="func entry(v: i32) -> i32:\n    var text = \"before\"\n    if text == \"before\":\n        return v\n    return 0\n";
    auto before=analyze(text);auto after=text;auto n=after.find("before");after.replace(n,6,"after");vd::Jit jit;vd::ReloadSlot slot(jit.compile(*before,opt));
    require(jit.replace(slot,*analyze(after),opt) && slot.invoke(10)==0,"body string literal reload rejected");
    auto constant=analyze("const text = \"a\"\nfunc entry(v: i32) -> i32:\n    return v\n");vd::ReloadSlot frozen(jit.compile(*constant,opt));
    require(!jit.replace(frozen,*analyze("const text = \"b\"\nfunc entry(v: i32) -> i32:\n    return v\n"),opt),"string const bypassed restart contract");
}
void active_reload(bool opt) {
    const std::string source="func entry(v: i32) -> i32:\n    var text = \"before\"\n    if text == \"before\":\n        return v\n    return 0\n";
    auto after=source;after.replace(after.find("before"),6,"after");vd::Jit jit;vd::ReloadSlot slot(jit.compile(*analyze(source),opt));auto runtime=slot.acquire()->runtime_owner();
    managed_test::Probe roots;roots.inspect_roots=true;roots.gate_poll=2;
    auto worker=std::async(std::launch::async,[&]{managed_test::Observe observe(roots);return slot.invoke(10);});
    struct Release {managed_test::Probe& p;bool done=false;void now(){if(!done){done=true;p.release.release();}}~Release(){now();}} release{roots};
    require(roots.reached.try_acquire_for(std::chrono::seconds(5)),"old string generation safepoint absent");
    require(jit.replace(slot,*analyze(after),opt) && jit.live_generations()==2 && slot.invoke(10)==0,"active string generation publication failed");
    auto collector=std::async(std::launch::async,[&]{vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);return runtime->collect(std::chrono::seconds(5));});
    bool requested=runtime->wait_for_request(std::chrono::seconds(5));release.now();
    require(collector.get()==vd::Runtime::Collection::completed && requested && worker.get()==10 && roots.roots_valid && roots.frames.empty() && jit.live_generations()==1,"active string generation/GC lease failure");
}
}
int main(int argc,char** argv) {
    try {
        if(argc==5 && std::string(argv[1])=="--emit-library") {auto p=vd::compile_project(argv[2]);vd::emit_runtime_object(p.module,argv[3],std::string(argv[4])=="optimized");return 0;}
        if(argc==5 && std::string(argv[1])=="--emit-reload") {auto text=vd::read_source(argv[2]).text();auto at=text.find("return 7");require(at!=std::string::npos,"reload fixture marker missing");text.replace(at,8,"return 8");auto p=vd::analyze_source({"/stress.void",text});if(!p.succeeded()) throw vd::CompilationError(p.diagnostics);vd::emit_runtime_object(p.program->lower(),argv[3],std::string(argv[4])=="optimized","__reloaded");return 0;}
        require(argc==2,"string project root required");language();independent();string_test::runtime_checks();for(bool opt:{false,true}) {source(argv[1],opt);reload(opt);active_reload(opt);}std::cout<<"UTF-8 string source, IR, roots, failures and reload passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
