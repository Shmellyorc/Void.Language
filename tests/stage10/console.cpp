#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/tooling.hpp"
#include "void/console.hpp"
#include "void/console_ops.hpp"
#include "../managed/probe.hpp"
#include <deque>
#include <cstring>
#include <cctype>
#include <future>
#include <fstream>
#include <iostream>
#include <semaphore>
using namespace vd::console;
using namespace std::chrono_literals;
namespace {
unsigned checks=0;
std::filesystem::path project;
auto project_analysis(std::string source){vd::SourceOverrides files;files.emplace("/main.void",vd::SourceFile(project.string(),std::move(source)));return vd::analyze_project(project,files);}
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);++checks;}
struct Fake:Adapter {
    std::string output,error,input;std::size_t at=0,calls=0;Error fault{};std::size_t partial=0;bool once=false;
    bool terminal_enabled=false,keyboard_enabled=false;unsigned mode=7;Error restoration;bool restoration_once=false;
    std::function<void()> on_write;
    std::function<void(const Request&)> on_read,on_write_request;
    std::array<Endpoint,3> inspect() const override{return {{{Stream::input,Kind::pipe},{Stream::output,Kind::pipe},{Stream::error,Kind::pipe}}};}
    std::chrono::steady_clock::time_point now() const noexcept override{return std::chrono::steady_clock::now();}
    std::uint64_t capabilities(Stream stream) override{return keyboard_enabled && stream==Stream::input?13:terminal_enabled && stream==Stream::output?2034:Adapter::capabilities(stream);}
    Error save_mode(InputMode& saved) override {if(!keyboard_enabled)return {ErrorKind::unsupported};saved.native[0]=mode;return {};}
    Error apply_mode(const InputMode&,ModeOptions options) override {mode=(options.echo?1:0)|(options.line_input?2:0)|(options.control_c_as_input?0:4);return {};}
    Error restore_mode(const InputMode& saved) override {if(restoration){auto error=restoration;if(restoration_once)restoration={};return error;}mode=saved.native[0];return {};}
    TerminalResult present(Stream,Presentation operation,std::int64_t,std::int64_t,std::string_view text,const Request&) override {TerminalResult r;if(!terminal_enabled){r.transfer.error={ErrorKind::unsupported};return r;}r.first=80;r.second=24;if(operation!=Presentation::size)r.sequence.assign(text.begin(),text.end());return r;}
    Transfer read(std::span<std::uint8_t> out,const Request& request) override{if(on_read)on_read(request);auto n=std::min(out.size(),input.size()-at);std::copy_n(input.begin()+at,n,out.begin());at+=n;return {n,!n,{}};}
    Transfer write(Stream stream,std::span<const std::uint8_t> bytes,const Request& request) override{
        if(vd::Runtime::current().is_managed())throw std::logic_error("Managed console write");
        if(on_write_request)on_write_request(request);if(on_write)on_write();++calls;auto n=fault?std::min(partial,bytes.size()):bytes.size();
        (stream==Stream::output?output:error).append(reinterpret_cast<const char*>(bytes.data()),n);auto error=fault;if(once)fault={};return {n,false,error};
    }
};
auto analyze(std::string source){auto a=project_analysis(std::move(source));if(!a.succeeded())throw vd::CompilationError(a.diagnostics);return a.program;}
void empty(vd::Runtime& runtime){vd::Runtime::Attachment a(runtime);vd::Runtime::Managed m(runtime);check(runtime.collect()==vd::Runtime::Collection::completed && runtime.snapshot().objects==0,"console roots retained after call");}
void reject(const std::string& body,const std::string& needle){auto a=project_analysis(body);if(a.succeeded())throw std::runtime_error("invalid console source accepted:\n"+body);++checks;bool found=false;for(auto& d:a.diagnostics){auto text=d.description,expected=needle;for(auto& c:text)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));for(auto& c:expected)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));found|=text.find(expected)!=std::string::npos;}if(!found)throw vd::CompilationError(a.diagnostics);++checks;}
void invalid_ir(vd::ir::Module module,const std::function<bool(vd::ir::Instruction&)>& edit){bool changed=false;for(auto& f:module.functions)for(auto& b:f.blocks)for(auto& i:b.instructions)if(!changed && edit(i))changed=true;check(changed,"IR test did not find target");bool rejected=false;try{vd::ir::validate(module);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"forged console IR accepted");}
void negatives(){
    const std::string head="use console\nfunc entry(n: i32) -> i32:\n    ",tail="\n    return n\n";
    for(auto [body,needle]:std::vector<std::pair<std::string,std::string>>{
        {"print(values = 1)","Unknown parameter"},{"print(1, nope = true)","Unknown parameter"},{"print(sep = 1)","Type mismatch"},{"print(flush = 1)","Type mismatch"},{"print(file = console.stdin)","console.output"},{"print(file = 1)","console.output"},{"print(end = 1)","Type mismatch"},{"print(end = \"x\", end = \"y\")","more than once"},{"print(end = \"x\", 1)","Positional arguments"},
        {"var endpoint = console.output(_id = 1)","compiler-owned"},{"var endpoint = console.input()","compiler-owned"},{"var endpoint: console.output","compiler-owned"},{"console.stdout._id = 2","const"},{"var leaked = console.stdout._id","not accessible"},
        {"_console_write(console.stdout, \"bad\")","Unknown"},{"var count = print(1)","requires a value"},
    })reject(head+body+tail,needle);
    reject("use console\nfunc entry(n: i32) -> i32:\n    var failed = console.stdin.read_text(-1)\n    if failed is Err(var reason):\n        reason.partial[0] = 9\n    return n\n","const stored field");
    reject("func task(n: i32) -> i32:\n    print(n)\n    return n\nasync func entry(n: i32) -> i32:\n    var job = spawn task(n)\n    return await job\n","worker");
    reject("use console\nfunc task(port: console.output) -> i32:\n    return 0\nasync func entry(n: i32) -> i32:\n    var job = spawn task(console.stdout)\n    return await job\n","spawn transfers");
    reject("value struct frozen:\n    const n: i32\nfunc entry(n: i32) -> i32:\n    var values = [frozen(n = 1)]\n    values[0].n = 2\n    return n\n","const stored field");
    // A const path cannot excuse lost authority on another mutable field.
    reject("struct cell:\n    n: i32\nvalue struct pair:\n    const safe: cell\n    mutable: cell\nfunc unsafe(const source: pair) -> pair:\n    return source\nfunc entry(n: i32) -> i32:\n    var original = pair(safe = cell(n = n), mutable = cell(n = n))\n    var escaped = unsafe(original)\n    escaped.mutable.n = 4\n    return n\n","readonly");
    auto cell=std::make_shared<vd::ir::ValueType>();cell->identity=cell->name="test-cell";cell->managed=true;
    auto pair=std::make_shared<vd::ir::ValueType>();pair->identity=pair->name="test-pair";pair->fields={{"safe",vd::ir::Type(cell,vd::ir::Type::managed_ref),"test-pair",false,false,"public",true},{"mutable",vd::ir::Type(cell,vd::ir::Type::managed_ref),"test-pair"}};
    vd::ir::Type type(pair);auto readonly=type;readonly.readonly_references.insert({0});check(vd::ir::capability_assignable(readonly,type),"intrinsic const-field authority not recognized");readonly.readonly_references.insert({1});check(!vd::ir::capability_assignable(readonly,type),"unrelated mutable path lost readonly authority");
    auto shadow=analyze("func print(n: i32) -> i32:\n    return n + 1\nfunc entry(n: i32) -> i32:\n    return print(n)\n");vd::Jit jit;check(jit.compile(*shadow)->invoke(3)==4,"ordinary print declaration precedence changed");
}
void execution(){
    auto program=analyze("use console\nfunc entry(n: i32) -> i32:\n    print()\n    print(\"value\", n, true, sep = \"|\", end = \"!\")\n    print(\"ERR\", file = console.stderr, end = \"\")\n    return n\n");
    auto module=program->lower();
    invalid_ir(module,[](auto& i){if(i.op!=vd::ir::Op::console_endpoint)return false;i.immediate=99;return true;});
    invalid_ir(module,[](auto& i){if(i.op!=vd::ir::Op::console_endpoint)return false;i.op=vd::ir::Op::aggregate;i.immediate=0;return true;});
    invalid_ir(module,[](auto& i){if(i.op!=vd::ir::Op::console_print)return false;i.b=i.a;return true;});
    invalid_ir(module,[](auto& i){if(i.op!=vd::ir::Op::console_native)return false;i.immediate=99;return true;});
    auto uninitialized=module;bool changed=false;
    for(auto& f:uninitialized.functions)for(auto& block:f.blocks)for(auto& i:block.instructions)if(!changed && i.op==vd::ir::Op::console_endpoint){auto slot=f.locals.size();f.locals.push_back(i.type);i.op=vd::ir::Op::load;i.a=static_cast<unsigned>(slot);i.immediate=0;changed=true;}
    bool refused=false;try{vd::ir::validate(uninitialized);}catch(const std::invalid_argument&){refused=true;}check(changed && refused,"uninitialized endpoint local forged a borrowed token");
    for(bool opt:{false,true}){
        auto fake=std::make_shared<Fake>();auto runtime=std::make_shared<vd::Runtime>(1,Streams::isolated(fake));vd::Jit jit;auto code=jit.compile(module,program->entry_symbol(),opt,{},runtime);
        managed_test::Probe p;p.inspect_roots=true;p.collect_before=true;p.frames.reserve(1024);
        {managed_test::Observe observe(p);check(code->invoke(7)==7,"print changed entry result");}
        check(fake->output=="\nvalue|7|true!" && fake->error=="ERR" && fake->calls==3,"source print records differ");check(p.roots_valid && p.commits_valid && p.frames.empty() && p.roots_checked,"print roots/frame validation");empty(*runtime);
        bool success=false;unsigned failures=0;
        for(int budget=0;budget<=4096 && !success;budget=budget<64?budget+1:budget*2){
            fake->output.clear();fake->error.clear();managed_test::Probe probe;probe.inspect_roots=true;probe.frames.reserve(1024);
            {managed_test::Observe observe(probe);try{managed_test::AllocationFailure fault(budget);success=code->invoke(7)==7;}catch(const std::exception&){++failures;}}
            check(fake->output.empty() || fake->output=="\n" || fake->output=="\nvalue|7|true!","allocation failure emitted a partial pre-write record");
            check(probe.roots_valid && probe.commits_valid && probe.frames.empty(),"allocation failure corrupted console roots/frames");empty(*runtime);
        }
        check(success && failures,"console allocation sweep never reached failure/success");
    }
}
void failures(){
    auto recovered=analyze(R"(use console
func entry(n: i32) -> i32:
    var sent = console.stderr.write("ab🎮tail")
    match sent:
        Err(var reason):
            if reason.operation != "write" or reason.stream != "stderr" or reason.message != "io_failure":
                return -1
            if reason.os_code != 87 or reason.bytes_transferred != 2 or reason.cleanup_code != 19 or reason.partial_utf16_units != 1:
                return -2
            if reason.partial.count != 0:
                return -3
            return n
        _:
            return -4
)");
    auto print=analyze("use console\nfunc cleanup():\n    print(\"clean\")\nfunc entry(n: i32) -> i32:\n    defer cleanup()\n    print(\"ab🎮tail\", file = console.stderr)\n    return n\n");
    auto hook=analyze("value struct rejected:\n    n: i32\nfunc (self rejected) format(spec: string) -> result<string, string>:\n    return Err(\"original-hook-failure\")\nfunc (self rejected) string() -> string:\n    return \"{self:reject}\"\nfunc entry(n: i32) -> i32:\n    print(\"NO PREFIX\", rejected(n = n))\n    return n\n");
    for(bool opt:{false,true})for(auto program:{recovered,print,hook}){
        auto fake=std::make_shared<Fake>();fake->partial=2;fake->fault={ErrorKind::io_failure,87,19,1};fake->once=program==print;auto runtime=std::make_shared<vd::Runtime>(1,Streams::isolated(fake));vd::Jit jit;auto code=jit.compile(program->lower(),program->entry_symbol(),opt,{},runtime);std::string diagnostic;
        try{check(code->invoke(7)==7,"recoverable console error fields/progress changed");}catch(const std::runtime_error& e){diagnostic=e.what();}
        if(program==hook)check(diagnostic.find("original-hook-failure")!=std::string::npos && fake->calls==0 && fake->output.empty() && fake->error.empty(),"hook failed after record output or lost reason");
        else if(program==print)check(diagnostic.find("console print stderr")!=std::string::npos && diagnostic.find("completed=2")!=std::string::npos && diagnostic.find("native=87")!=std::string::npos && diagnostic.find("partial_utf16_units=1")!=std::string::npos && diagnostic.find("cleanup=19")!=std::string::npos && diagnostic.find("main.void")!=std::string::npos && fake->error=="ab" && fake->output=="clean\n",("checked print failure/provenance lost: "+diagnostic+"; bytes="+fake->error).c_str());
        else check(diagnostic.empty() && fake->error=="ab","recoverable write failed");empty(*runtime);
    }
}
void error_kinds(){
    auto source=std::string("use console\nfunc entry(n: i32) -> i32:\n    var sent = console.stdout.write(\"x\")\n    if sent is Err(var reason):\n");
    unsigned kind=0;for(const auto& name:{"invalid_argument","invalid_encoding","unsupported","would_block","timed_out","closed","broken_pipe","interrupted","io_failure"}){++kind;source+="        if reason.kind is io_error_kind."+std::string(name)+":\n            return "+std::to_string(kind)+"\n";}
    source+="    return 0\n";auto program=analyze(source);
    for(bool opt:{false,true}){auto fake=std::make_shared<Fake>();auto runtime=std::make_shared<vd::Runtime>(1,Streams::isolated(fake));vd::Jit jit;auto code=jit.compile(program->lower(),program->entry_symbol(),opt,{},runtime);
        for(unsigned n=1;n<=9;++n){fake->fault={static_cast<ErrorKind>(n),123,456,1};check(code->invoke(0)==static_cast<int>(n),"public io_error_kind mapping differs");empty(*runtime);}
    }
}
void null_order(){
    auto program=analyze("struct trace:\n    count: i32\nfunc touch(state: trace) -> i32:\n    state.count++\n    return 3\nfunc entry(n: i32) -> i32:\n    var state = trace(count = 0)\n    print(null, touch(state))\n    return n\n");
    for(bool opt:{false,true}){auto fake=std::make_shared<Fake>();auto runtime=std::make_shared<vd::Runtime>(1,Streams::isolated(fake));vd::Jit jit;auto code=jit.compile(program->lower(),program->entry_symbol(),opt,{},runtime);bool success=false;unsigned observed=0;
        for(int budget=0;budget<=4096 && !success;budget=budget<128?budget+1:budget*2){managed_test::Probe p;p.capture=true;p.capture_into_existing=true;p.capture_at=1;p.frames.reserve(1024);std::string diagnostic;fake->output.clear();
            {vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);p.retained=std::make_unique<vd::Runtime::Handle>(*runtime,nullptr);}
            {managed_test::Observe observe(p);try{managed_test::AllocationFailure fault(budget,true);success=code->invoke(7)==7;}catch(const std::exception& e){diagnostic=e.what();}}
            if(p.retained){vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);int count=0;if(p.retained->get())std::memcpy(&count,p.retained->get(),sizeof(count));
                if(diagnostic.find("String operation failed")!=std::string::npos || diagnostic.find("Display operation failed")!=std::string::npos || diagnostic.find("Interpolation failed")!=std::string::npos){++observed;check(count==1,"null display allocated before the written RHS expression");}p.retained.reset();}
            check(p.frames.empty(),"null order failure retained a frame");empty(*runtime);
        }check(success && observed,"null ordering sweep missed renderer failures");check(fake->output=="null 3\n","null literal display changed");
    }
}
void lifecycle(){
    auto prompt=analyze(R"(use console
func entry(n: i32) -> i32:
    print("Name é🎮: ", end = "", flush = true)
    if console.stdin.read_line() is Ok(var response):
        if response is text_read.text(var name):
            print("Hello,", name)
    return n
)");
    auto shutdown=analyze(R"(use console
func cleanup():
    if console.stdout.flush() is Err(var error):
        if error.kind is io_error_kind.closed:
            return
    print("unexpected shutdown result", file = console.stderr)
func entry(n: i32) -> i32:
    defer cleanup()
    var kept = "root é🎮"
    print(kept)
    return n
)");
    for(bool opt:{false,true}){
        {auto fake=std::make_shared<Fake>();fake->input="Ada\n";unsigned reads=0;fake->on_read=[&](const Request&){check(fake->output=="Name é🎮: ","prompt did not complete before native input");++reads;};auto runtime=std::make_shared<vd::Runtime>(1,Streams::isolated(fake));vd::Jit jit;auto code=jit.compile(prompt->lower(),prompt->entry_symbol(),opt,{},runtime);
            check(code->invoke(17)==17 && reads && fake->output=="Name é🎮: Hello, Ada\n","prompt/read/output changed");empty(*runtime);}
        for(bool fail:{false,true}){
            auto fake=std::make_shared<Fake>();auto streams=Streams::isolated(fake);auto runtime=std::make_shared<vd::Runtime>(1,streams);auto context=runtime->console_context();vd::Jit jit;auto code=jit.compile(shutdown->lower(),shutdown->entry_symbol(),opt,{},runtime);
            std::binary_semaphore entered(0),cancelled(0),release(0);std::atomic<bool> closed=false;
            fake->on_write_request=[&](const Request& request){std::stop_callback wake(request.cancellation,[&]{cancelled.release();});entered.release();release.acquire();if(fail){fake->partial=2;fake->fault={ErrorKind::io_failure,87,19,1};}};
            auto run=std::async(std::launch::async,[&]{try{return std::pair{code->invoke(17),std::string{}};}catch(const std::runtime_error& e){return std::pair{0,std::string(e.what())};}});
            entered.acquire();
            {vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);check(runtime->collect(5s)==vd::Runtime::Collection::completed,"shutdown native wait blocked precise-root GC");}
            auto close=std::async(std::launch::async,[&]{runtime->close();closed=true;});cancelled.acquire();check(!closed && context->snapshot().admitted==1,"shutdown returned before entered output completed");release.release();close.get();auto [status,diagnostic]=run.get();
            check(fail?(diagnostic.find("console print stdout: io_failure")!=std::string::npos && diagnostic.find("completed=2")!=std::string::npos && diagnostic.find("cleanup=19")!=std::string::npos):(status==17 && diagnostic.empty()),("shutdown primary result changed: "+diagnostic).c_str());
            check(diagnostic.find("Additional failure")==std::string::npos && diagnostic.find("Invalid console")==std::string::npos,("shutdown cleanup did not observe recoverable closed: "+diagnostic).c_str());
            check(context->snapshot().admitted==0 && fake->calls==1 && fake->error.empty(),"closed cleanup wrote native bytes");
            check(runtime->snapshot().threads==0,"source call remained attached after shutdown");
            bool refused=false;try{runtime->console_context();}catch(const std::logic_error&){refused=true;}check(refused,"closed Runtime admitted new host I/O");
            check(fail?fake->output=="ro":fake->output=="root é🎮\n","entered output completion/progress changed");
        }
    }
}

void cleanup_allocations(){
    auto program=analyze("func clean():\n    print(\"clean;\", end = \"\")\nfunc entry(n: i32) -> i32:\n    defer clean()\n    print(\"body;\", end = \"\")\n    return n\n");
    for(bool opt:{false,true}){
        auto fake=std::make_shared<Fake>();auto runtime=std::make_shared<vd::Runtime>(1,Streams::isolated(fake));vd::Jit jit;auto code=jit.compile(program->lower(),program->entry_symbol(),opt,{},runtime);
        for(bool once:{false,true}){bool success=false;unsigned failures=0,cleanup_only=0;
            for(int budget=0;budget<=4096 && !success;budget=budget<96?budget+1:budget*2){fake->output.clear();managed_test::Probe p;p.inspect_roots=true;p.frames.reserve(1024);
                {managed_test::Observe observe(p);try{managed_test::AllocationFailure fault(budget,once);success=code->invoke(17)==17;}catch(const std::exception&){++failures;}}
                check(fake->output.empty() || fake->output=="clean;" || fake->output=="body;" || fake->output=="body;clean;","allocation failure split/reordered completed cleanup records");
                if(fake->output=="body;")++cleanup_only;
                check(p.roots_valid && p.commits_valid && p.frames.empty(),"failed cleanup retained/corrupted precise frames");empty(*runtime);
            }check(success && failures && cleanup_only,"allocation sweep missed cleanup failure or recovery");
        }
    }
    // First console use may itself be reached after Runtime close. No native
    // session may be initialized, but the admitted source must report closed.
    auto first=analyze("struct gate:\n    n: i32\nfunc entry(n: i32) -> i32:\n    var state = gate(n = n)\n    print(\"first I/O\", state.n)\n    return n\n");
    for(bool opt:{false,true}){auto runtime=std::make_shared<vd::Runtime>(1);vd::Jit jit;auto code=jit.compile(first->lower(),first->entry_symbol(),opt,{},runtime);managed_test::Probe p;bool closed=false;p.before_construct=[&](auto n,const void*,auto){if(n==1){vd::Runtime::Native safe(*runtime);runtime->close();closed=true;}};std::string diagnostic;
        {managed_test::Observe observe(p);try{code->invoke(17);}catch(const std::runtime_error& e){diagnostic=e.what();}}
        check(closed && diagnostic.find("console print stdout: closed")!=std::string::npos,("first console admission during close misreported: "+diagnostic).c_str());
        vd::NominalDescriptor spec{"new-after-close","","plain",{8,{}}};bool rejected=false;try{runtime->admit_types({&spec,1});}catch(const std::logic_error&){rejected=true;}check(rejected,"new host metadata admitted after Runtime close");
        check(runtime->snapshot().threads==0,"first I/O shutdown retained source attachment");
    }
}

void terminal(){
    auto program=analyze("use console\nfunc entry(n: i32) -> i32:\n    var title = \"é🎮\"\n    var sent = console.stdout.set_title(title)\n    if sent is Err(var error):\n        return -1\n    if title.byte_length != 6:\n        return -2\n    match console.stdout.size():\n        Ok(var size):\n            if size.columns != 80 or size.rows != 24:\n                return -3\n        _:\n            return -4\n    return n\n");
    reject("use console\nfunc entry(n: i32) -> i32:\n    var caps = console.stdout.capabilities()\n    caps.color = true\n    return n\n","const");
    reject("use console\nfunc entry(n: i32) -> i32:\n    var set = console.stdout.set_color(1, 2)\n    return n\n","Type mismatch");
    reject("use console\nfunc task() -> i32:\n    var caps = console.stdout.capabilities()\n    return 0\nasync func entry(n: i32) -> i32:\n    var job = spawn task()\n    return await job\n","worker");
    for(bool opt:{false,true}){
        auto fake=std::make_shared<Fake>();fake->terminal_enabled=true;auto runtime=std::make_shared<vd::Runtime>(1,Streams::isolated(fake));vd::Jit jit;auto code=jit.compile(program->lower(),program->entry_symbol(),opt,{},runtime);
        std::binary_semaphore entered(0),release(0);fake->on_write=[&]{entered.release();release.acquire();};
        auto call=std::async(std::launch::async,[&]{return code->invoke(17);});entered.acquire();{vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);check(runtime->collect(5s)==vd::Runtime::Collection::completed,"pending presentation blocked GC");}release.release();check(call.get()==17 && fake->output=="é🎮","presentation bytes/roots changed during GC");fake->on_write={};empty(*runtime);
        bool success=false;unsigned failures=0;
        for(int budget=0;budget<=4096 && !success;budget=budget<96?budget+1:budget*2){fake->output.clear();managed_test::Probe p;p.inspect_roots=true;p.frames.reserve(1024);{managed_test::Observe observe(p);try{managed_test::AllocationFailure fault(budget,true);success=code->invoke(17)==17;}catch(const std::exception&){++failures;}}check(fake->output.empty() || fake->output=="é🎮","presentation allocation fault emitted partial record");check(p.roots_valid && p.commits_valid && p.frames.empty(),"presentation fault retained roots/frame");empty(*runtime);}check(success && failures,"presentation allocation sweep missed failure/recovery");
    }
}

void keyboard(){
    reject("use console\nfunc entry(n: i32) -> i32:\n    var session = terminal_session(_token = 1)\n    return n\n","nonforgeable");
    reject("use console\nfunc entry(n: i32) -> i32:\n    match console.stdin.begin_mode():\n        Ok(var session):\n            session._token = 1\n        _:\n            pass\n    return n\n","accessible");
    reject("use console\nasync func consume(session: terminal_session) -> i32:\n    return 0\nfunc entry(n: i32) -> i32:\n    return n\n","boundary");
    const std::string source="use console\nfunc entry(n: i32) -> i32:\n    match console.stdin.begin_mode(echo = false, line_input = false):\n        Ok(var session):\n            with session:\n                match console.stdin.read_key():\n                    Ok(var pressed):\n                        if pressed.character == \"é\":\n                            return n\n                    _:\n                        pass\n        _:\n            pass\n    return -1\n";
    auto program=analyze(source);
    auto module=program->lower();invalid_ir(module,[](auto& i){if(i.op!=vd::ir::Op::console_native || i.immediate!=21)return false;i.immediate=20;return true;});
    bool changed=false;for(auto& f:module.functions)if(f.module_identity==vd::console::source_module)for(auto& b:f.blocks)for(auto& i:b.instructions)if(i.op==vd::ir::Op::gc_construct && vd::console::session_type(i.type)){f.module_identity="/forged.void";changed=true;break;}check(changed,"session manufacture test target missing");bool refused=false;try{vd::ir::validate(module);}catch(const std::invalid_argument&){refused=true;}check(refused,"forged session construction accepted");
    for(bool opt:{false,true}){
        auto fake=std::make_shared<Fake>();fake->keyboard_enabled=true;fake->input="é";auto runtime=std::make_shared<vd::Runtime>(1,Streams::isolated(fake));vd::Jit jit;auto code=jit.compile(program->lower(),program->entry_symbol(),opt,{},runtime);
        check(code->invoke(17)==17 && fake->mode==7,"source key/scoped cleanup failed");empty(*runtime);
        bool success=false;unsigned failures=0;
        for(int budget=0;budget<=4096 && !success;budget=budget<96?budget+1:budget*2){fake->at=0;managed_test::Probe p;p.inspect_roots=true;p.frames.reserve(1024);{managed_test::Observe observe(p);try{managed_test::AllocationFailure fault(budget,true);success=code->invoke(17)==17;}catch(const std::exception&){++failures;}}check(fake->mode==7,"source allocation failure left a live terminal mode");check(p.roots_valid && p.commits_valid && p.frames.empty(),"key allocation fault retained roots/frame");empty(*runtime);}check(success && failures,"key allocation sweep missed failure/recovery");
        fake->at=0;std::binary_semaphore entered(0),release(0);fake->on_read=[&](const Request&){entered.release();release.acquire();};auto call=std::async(std::launch::async,[&]{return code->invoke(17);});entered.acquire();{vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);check(runtime->collect(5s)==vd::Runtime::Collection::completed,"blocked key input prevented GC");}release.release();check(call.get()==17 && fake->mode==7,"key GC/capture lost mode");fake->on_read={};empty(*runtime);
        fake->at=0;fake->on_read=[&](const Request&){fake->restoration={ErrorKind::io_failure,99};fake->restoration_once=true;throw std::bad_alloc();};std::string unwound;try{code->invoke(17);}catch(const std::exception& error){unwound=error.what();}fake->on_read={};check(unwound.find("Console allocation failed")!=std::string::npos && unwound.find("native=99")!=std::string::npos,"exceptional native restoration failure vanished from source diagnostic");check(unwound.find("native=99")<unwound.find("would_block") && fake->mode==7,"exceptional native cleanup ordering/restoration changed");empty(*runtime);
        auto unclosed=analyze("use console\nfunc entry(n: i32) -> i32:\n    var mode = console.stdin.begin_mode(echo = false, line_input = false)\n    return n\n");auto unclosed_code=jit.compile(unclosed->lower(),unclosed->entry_symbol(),opt,{},runtime);check(unclosed_code->invoke(18)==18 && fake->mode==7,"entry completion left unscoped native mode");
        auto failed=analyze("use console\nfunc entry(n: i32) -> i32:\n    match console.stdin.begin_mode(echo = false, line_input = false):\n        Ok(var session):\n            with session:\n                var zero = n - n\n                return n / zero\n        _:\n            return -1\n");auto failed_code=jit.compile(failed->lower(),failed->entry_symbol(),opt,{},runtime);fake->restoration={ErrorKind::io_failure,89};std::string error;try{failed_code->invoke(2);}catch(const std::exception& e){error=e.what();}check(error.find("zero")!=std::string::npos && error.find("restore_mode")!=std::string::npos && error.find("89")!=std::string::npos,"mode cleanup replaced primary source failure");fake->restoration={};runtime->console_context()->restore_modes();check(fake->mode==7,"failed native restore was not retained for retry");empty(*runtime);
    }
}


void generation_lifetime(){
    const std::string source=R"(use console
func cleanup():
    print("cleanup", end = "")
func entry(n: i32) -> i32:
    match console.stdin.begin_mode(echo = false, line_input = false):
        Ok(var session):
            with session:
                defer cleanup()
                var kept = "é🎮"
                print("old", kept, n, end = "")
                if kept.byte_length == 6:
                    return n + 1
        _:
            pass
    return -1
)";
    auto before=analyze(source);auto text=source;text.replace(text.find("return n + 1"),12,"return n + 2");auto after=analyze(text);
    check(before->compatibility_contract()==after->compatibility_contract(),"console body edit unexpectedly incompatible");
    for(bool opt:{false,true}){
        auto fake=std::make_shared<Fake>();fake->keyboard_enabled=true;auto runtime=std::make_shared<vd::Runtime>(1,Streams::isolated(fake));auto context=runtime->console_context();vd::Jit jit;
        auto old=jit.compile(before->lower(),before->entry_symbol(),opt,{},runtime);std::weak_ptr<const vd::NativeCode> retired=old;vd::ReloadSlot slot(old);old.reset();
        std::binary_semaphore entered(0),release(0);std::weak_ptr<void> native_lease;std::atomic<unsigned> calls=0;
        fake->on_write=[&]{if(calls++==0){native_lease=vd::NativeCode::active_lease();entered.release();release.acquire();}};
        auto call=std::async(std::launch::async,[&]{return slot.invoke(7);});entered.acquire();
        check(!retired.expired() && !native_lease.expired() && fake->mode==4,"entered source generation/mode not retained");
        check(jit.replace(slot,*after,opt),"compatible console generation rejected");
        check(!retired.expired() && !native_lease.expired(),"replacement retired an entered console generation");
        {vd::Runtime::Attachment attachment(*runtime);vd::Runtime::Managed managed(*runtime);check(runtime->collect(5s)==vd::Runtime::Collection::completed,"retained generation/native console wait blocked GC");}
        release.release();check(call.get()==8 && fake->mode==7 && fake->output=="old é🎮 7cleanup","old generation/cleanup bytes changed during replacement");
        check(retired.expired() && native_lease.expired(),"completed console generation lease retained");fake->on_write={};fake->output.clear();
        check(slot.invoke(7)==9 && fake->mode==7 && fake->output=="old é🎮 7cleanup","new generation used stale code or lost cleanup");empty(*runtime);
        // The same source/code then meets closing + entered partial native failure.
        fake->output.clear();fake->partial=2;fake->fault={ErrorKind::io_failure,87,19};fake->restoration={ErrorKind::io_failure,89};
        std::binary_semaphore blocked(0),cancelled(0),finish(0);std::atomic<bool> drained=false;
        fake->on_write_request=[&](const Request& request){std::stop_callback stop(request.cancellation,[&]{cancelled.release();});blocked.release();finish.acquire();};
        auto failed=std::async(std::launch::async,[&]{try{slot.invoke(7);return std::string{};}catch(const std::exception& e){return std::string(e.what());}});blocked.acquire();
        {vd::Runtime::Attachment attachment(*runtime);vd::Runtime::Managed managed(*runtime);check(runtime->collect(5s)==vd::Runtime::Collection::completed,"closing source generation blocked GC");}
        auto close=std::async(std::launch::async,[&]{auto error=runtime->close();drained=true;return error;});cancelled.acquire();
        check(!drained && context->snapshot().admitted==1,"close released admitted generation/native buffers");finish.release();auto error=close.get();auto diagnostic=failed.get();
        check(error.os_code==89 && diagnostic.find("native=87")!=std::string::npos && diagnostic.find("cleanup=19")!=std::string::npos && diagnostic.find("completed=2")!=std::string::npos,"shutdown/source primary and native restoration errors were conflated");
        check(diagnostic.find("Additional failure")!=std::string::npos && diagnostic.find("closed")!=std::string::npos && diagnostic.find("console print stdout")<diagnostic.find("Additional failure"),"reached cleanup replaced original partial-write failure");
        check(fake->output=="ol" && context->snapshot().admitted==0 && runtime->snapshot().threads==0,"shutdown emitted cleanup output or retained source threads");
        fake->restoration={};check(!runtime->close() && fake->mode==7,"matching Runtime close ABI lost failed restoration retry");fake->on_write_request={};
    }
}

void collection_display(){
    for(bool sums:{false,true}){
    const auto expected=sums?std::string("Ok(/main.void::packet.pair(<007>, <002>))"):std::string("[[<007>], [<002>]]");
    const auto output=sums?"items: ["+expected+"]!":"items: "+expected+"!";
    auto program=analyze(std::string(sums?"struct item:\n":"value struct item:\n")+R"(    value: i32
func (self item) string() -> string:
    return "<{self.value:000}>"
)"+(sums?R"(enum packet:
    empty
    pair(left: item, right: item)
func entry(n: i32) -> i32:
    var value: result<packet, string> = Ok(packet.pair(item(value = n), item(value = 2)))
    print("items:", [value], end = "!")
    return string(value).length as i32
)":R"(func entry(n: i32) -> i32:
    var values = [[item(value = n)], [item(value = 2)]]
    print("items:", values, end = "!")
    return string(values).length as i32
)"));
    for(bool opt:{false,true}){
        auto fake=std::make_shared<Fake>();auto runtime=std::make_shared<vd::Runtime>(1,Streams::isolated(fake));vd::Jit jit;auto code=jit.compile(program->lower(),program->entry_symbol(),opt,{},runtime);
        managed_test::Probe p;p.inspect_roots=true;p.collect_before=true;p.frames.reserve(4096);{managed_test::Observe observe(p);check(code->invoke(7)==static_cast<int>(expected.size()),"collection display result changed");}
        check(fake->output==output && p.roots_valid && p.commits_valid && p.frames.empty(),"collection builder/typed stacks lost bytes or precise roots");empty(*runtime);
        bool success=false;unsigned faults=0;for(int budget=0;budget<=4096 && !success;budget=budget<64?budget+1:budget*2){fake->output.clear();managed_test::Probe probe;probe.inspect_roots=true;probe.frames.reserve(4096);{managed_test::Observe observe(probe);try{managed_test::AllocationFailure fault(budget,true);success=code->invoke(7)==static_cast<int>(expected.size());}catch(const std::exception&){++faults;}}
            check(fake->output.empty() || fake->output==output,"failed collection conversion emitted an outer record prefix");check(probe.roots_valid && probe.commits_valid && probe.frames.empty(),"collection allocation failure damaged roots");empty(*runtime);
        }check(success && faults,"collection allocation failure sweep missed success/recovery");
    }
    }
    {auto program=analyze("enum choice:\n    left(n: i32)\n    right(n: i32)\nfunc entry(n: i32) -> i32:\n    print(\"NO PREFIX\", choice.left(n))\n    return n\n");auto m=program->lower();bool changed=false;
        for(auto& f:m.functions)for(auto& b:f.blocks)for(auto& i:b.instructions)if(i.op==vd::ir::Op::sum_get && i.immediate==1){i.immediate=2;changed=true;}
        check(changed,"missing inactive payload probe");vd::ir::validate(m);
        for(bool opt:{false,true}){auto fake=std::make_shared<Fake>();auto runtime=std::make_shared<vd::Runtime>(1,Streams::isolated(fake));vd::Jit jit;auto code=jit.compile(m,program->entry_symbol(),opt,{},runtime);managed_test::Probe probe;probe.inspect_roots=true;probe.frames.reserve(4096);std::string message;
            {managed_test::Observe observe(probe);try{code->invoke(7);}catch(const std::exception& failure){message=failure.what();}}
            check(message.find("Enum/result payload does not match its active variant")!=std::string::npos && fake->output.empty(),"unchecked inactive payload or partial outer record");check(probe.roots_valid && probe.frames.empty(),"failed sum payload damaged roots");empty(*runtime);
        }
    }
    for(bool dictionary:{false,true})for(bool through_sum:{false,true}){
        auto source=std::string(R"(struct item:
    action: func() -> string
func (self item) string() -> string:
    return self.action()
)");
        if(through_sum)source+="enum parcel:\n    payload(value: item)\n";
        source+="func entry(n: i32) -> i32:\n";
        const auto element=std::string(through_sum?"parcel":"item");
        source+=dictionary?"    var values: {string: "+element+"} = {}\n":"    var values: ["+element+"] = []\n";
        source+=R"(    var callback = func() -> string:
        values.clear()
        return "modified"
)";
        const auto value=through_sum?"parcel.payload(item(action = callback))":"item(action = callback)";
        source+=dictionary?std::string("    values[\"key\"] = ")+value+"\n":std::string("    values.push(")+value+")\n";
        source+=R"(    print("NO OUTER PREFIX", values)
    return n
)";
        auto invalidated=analyze(source);for(bool opt:{false,true}){auto fake=std::make_shared<Fake>();auto runtime=std::make_shared<vd::Runtime>(1,Streams::isolated(fake));vd::Jit jit;auto code=jit.compile(invalidated->lower(),invalidated->entry_symbol(),opt,{},runtime);std::string diagnostic;try{code->invoke(3);}catch(const std::exception& error){diagnostic=error.what();}check(diagnostic.find("structurally modified")!=std::string::npos && fake->output.empty(),"collection structural invalidation lost failure or published partial output");empty(*runtime);}
    }
}
void four_systems(){
    const auto fixtures=std::filesystem::path(__FILE__).parent_path();
    for(const auto& name:{"operator_console_model.void","operator_systems.void"})std::filesystem::copy_file(fixtures/name,project.parent_path()/name,std::filesystem::copy_options::overwrite_existing);
    auto before=analyze("use \"operator_console_model.void\" as model\nfunc entry(n: i32) -> i32:\n    return model.run(n)\n");
    const auto model_path=project.parent_path()/"operator_console_model.void";const auto text=vd::read_source(model_path).text();
    auto overlay=[&](std::string text){vd::SourceOverrides files;files.emplace("/operator_console_model.void",vd::SourceFile(model_path.string(),std::move(text)));files.emplace("/main.void",vd::SourceFile(project.string(),"use \"operator_console_model.void\" as model\nfunc entry(n: i32) -> i32:\n    return model.run(n)\n"));return vd::analyze_project(project,files);};
    auto change=[](std::string text,const std::string& from,const std::string& to){auto at=text.find(from);if(at==std::string::npos)throw std::runtime_error("four-system replacement missing");text.replace(at,from.size(),to);return text;};
    auto after=overlay(change(text,"return \"q:{self.stored:000}\"","return \"r:{self.stored:000}\""));if(!after.succeeded())throw vd::CompilationError(after.diagnostics);
    check(before->compatibility_contract()==after.program->compatibility_contract(),"four-system body changed contract");
    std::vector<std::shared_ptr<const vd::ValidatedAnalysis>> incompatible;
    for(auto source:{change(text,"private const stored: t","public const stored: t"),change(text,"    marker: i32","    marker: i32\n    stamp: i32"),change(text,"return \"q:{self.stored:000}\"","return \"q:{self.stored:0000}\""),change(text,"    operator %(const left:","    operator +(const left: number<t>) -> number<t>:\n        return left\n    operator %(const left:")}){
        auto a=overlay(source);if(!a.succeeded())throw vd::CompilationError(a.diagnostics);auto plan=vd::plan_invalidation(*before,{"/operator_console_model.void"},a.program.get());check(plan.source_compatibility==vd::SourceCompatibility::restart_required && !plan.dependent_modules.empty(),"combined layout/visibility/operator/spec contract did not invalidate");incompatible.push_back(a.program);
    }
    {auto library=vd::read_source(project.parent_path()/"operator_systems.void").text();library=change(library,"return \"q:{self.stored:000}\"","var sample = now_utc()\n    return \"q:{self.stored:000}\"");vd::SourceOverrides files;files.emplace("/operator_systems.void",vd::SourceFile((project.parent_path()/"operator_systems.void").string(),library));files.emplace("/main.void",vd::SourceFile(project.string(),"use \"operator_console_model.void\" as model\nfunc entry(n: i32) -> i32:\n    return model.run(n)\n"));auto a=vd::analyze_project(project,files);if(!a.succeeded())throw vd::CompilationError(a.diagnostics);check(before->compatibility_contract()!=a.program->compatibility_contract(),"combined custom-hook clock effect lost compatibility");incompatible.push_back(a.program);}
    const std::string packet="Ok(/operator_console_model.void::packet.data(v:q:002/9))";
    const std::string output="["+packet+"]|{item: "+packet+"}\n["+packet+", "+packet+"] [/operator_console_model.void::branch.nested(<cycle>)] Ok(2024-02-29)\naccount:010/010 q:008\n002|Ok(2024-02-29)|00:00:01:05.500\nOk(2026-11-01 01:30:00 -05:00 America/New_York)\n";
    const std::string cleanup="done [10, 20, 30, 12, 10, 31, 40, 10, 10, 1, 2, 3, 50, 50, 50, 50, 51];";
    for(bool opt:{false,true}){
        auto fake=std::make_shared<Fake>();fake->keyboard_enabled=true;auto runtime=std::make_shared<vd::Runtime>(1,Streams::isolated(fake));vd::Jit jit;auto code=jit.compile(before->lower(),before->entry_symbol(),opt,{},runtime);
        managed_test::Probe p;p.inspect_roots=true;p.collect_before=true;p.frames.reserve(4096);
        {managed_test::Observe observe(p);check(code->invoke(0)==0,"four-system result");}
        check(fake->output==output && fake->error==cleanup,"four-system snapshot/hook/print order or exact bytes");check(p.roots_valid && p.commits_valid && p.frames.empty() && p.roots_checked>0 && p.constructions>10,"combined GC roots/commit/frame probe missing");empty(*runtime);
        fake->output.clear();fake->error.clear();fake->input=std::string("Ada🎮\r\né\0",12);fake->at=0;
        fake->on_read=[&](const Request&){check(fake->output==output+"Name é🎮: ","integrated prompt before input");};
        check(code->invoke(4)==4,"integrated mixed input result");fake->on_read={};
        check(fake->output==output+"Name é🎮: Ada🎮 Ok(/@stdlib/console.void::bytes_read.data([10])) Ok(/@stdlib/console.void::text_read.text(é)) Ok(/@stdlib/console.void::bytes_read.data([0])) Ok(/@stdlib/console.void::text_read.eof)\n" && fake->error==cleanup,"integrated Unicode/CRLF/raw/NUL/EOF ownership");empty(*runtime);
        for(auto [mode,needle]:{std::pair{-1,"Division by zero"},std::pair{-2,"integrated--1"},std::pair{-3,"structurally modified"},std::pair{-4,"Recursive display hook"}}){
            fake->output.clear();fake->error.clear();std::string message;managed_test::Probe failed;failed.inspect_roots=true;failed.collect_before=true;failed.frames.reserve(4096);
            {managed_test::Observe observe(failed);try{code->invoke(mode);}catch(const std::exception& error){message=error.what();}}
            check(message.find(needle)!=std::string::npos && fake->output.empty(),("combined failure or partial outer print: "+message).c_str());check(message.find("operator_console_model.void")!=std::string::npos && failed.roots_valid && failed.frames.empty(),"combined failure source/roots lost");
            if(mode==-1)check(fake->error=="uncommitted 4;done [10, 20, 30, 10];" && message.find("current.amount += rhs()")!=std::string::npos,"failed compound committed or cleanup order lost");
            if(mode==-2)check(message.find("integrated--1")<message.find("Additional failure during defer") && message.find("Additional failure during defer")<message.find("integrated--2") && fake->error=="done [50, 51, 51];","combined hook primary/cleanup order");empty(*runtime);
        }
        fake->output.clear();fake->error.clear();fake->partial=7;fake->fault={ErrorKind::io_failure,87,19};fake->once=true;
        std::string partial;try{code->invoke(0);}catch(const std::exception& error){partial=error.what();}
        check(fake->output==output.substr(0,7) && partial.find("completed=7")!=std::string::npos && partial.find("native=87")!=std::string::npos && partial.find("cleanup=19")!=std::string::npos && fake->error.starts_with("done ["),"combined native partial progress/cleanup");empty(*runtime);
        bool success=false;unsigned faults=0;
        for(int budget=0;budget<=32768 && !success;budget=budget<32?budget+1:budget*2){fake->output.clear();fake->error.clear();managed_test::Probe failed;failed.inspect_roots=true;failed.frames.reserve(4096);{managed_test::Observe observe(failed);try{managed_test::AllocationFailure fault(budget,true);success=code->invoke(0)==0;}catch(const std::exception&){++faults;}}
            check(output.starts_with(fake->output) && (fake->output.empty() || fake->output.back()=='\n'),"allocation failure published partial composite record");check(failed.roots_valid && failed.commits_valid && failed.frames.empty(),"combined allocation fault roots/frames");empty(*runtime);
        }check(success && faults>0,"combined allocation sweep missed recovery");
        fake->output.clear();fake->error.clear();fake->input="é";fake->at=0;
        vd::ReloadSlot slot(code);std::weak_ptr<const vd::NativeCode> retired=code;code.reset();std::weak_ptr<void> lease;
        std::binary_semaphore entered(0),release(0);bool once=false;
        fake->on_write=[&]{if(fake->mode==4 && !once){once=true;lease=vd::NativeCode::active_lease();entered.release();release.acquire();}};
        auto call=std::async(std::launch::async,[&]{return slot.invoke(8);});entered.acquire();
        check(!retired.expired() && !lease.expired(),"four-system native wait lost generation");check(jit.replace(slot,*after.program,opt),"combined compatible replacement failed");
        {vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);check(runtime->collect(5s)==vd::Runtime::Collection::completed,"four-system NativeSafe wait blocked GC");}
        release.release();check(call.get()==8 && fake->mode==7 && fake->output==output+"READY\n[Ok(é)]\n" && fake->error==cleanup,"old generation/mode/composite payload changed across replacement");check(retired.expired() && lease.expired(),"old generation retained after call");fake->on_write={};empty(*runtime);
        for(auto candidate:incompatible)check(!jit.replace(slot,*candidate,opt),"combined incompatible reload published");
        fake->output.clear();fake->error.clear();auto next=output;std::size_t at=0;while((at=next.find("v:q:",at))!=std::string::npos){next.replace(at,4,"v:r:");at+=4;}check(slot.invoke(0)==0 && fake->output==next && fake->error==cleanup,"new generation used stale hook");empty(*runtime);
        fake->output.clear();fake->error.clear();fake->input="é";fake->at=0;fake->partial=7;fake->once=true;
        std::binary_semaphore blocked(0),cancelled(0),finish(0);std::atomic<bool> drained=false;
        fake->on_write_request=[&](const Request& request){if(fake->mode!=4)return;fake->fault={ErrorKind::io_failure,87,19};std::stop_callback stop(request.cancellation,[&]{cancelled.release();});blocked.release();finish.acquire();};
        auto failing=std::async(std::launch::async,[&]{try{slot.invoke(8);return std::string{};}catch(const std::exception& error){return std::string(error.what());}});blocked.acquire();
        auto context=runtime->console_context();auto close=std::async(std::launch::async,[&]{auto result=runtime->close();drained=true;return result;});cancelled.acquire();bool held=!drained && context->snapshot().admitted==1;finish.release();close.get();auto diagnostic=failing.get();check(held,"closing combined native operation lost admission/lifetime");
        check(diagnostic.find("native=87")!=std::string::npos && diagnostic.find("completed=6")!=std::string::npos && fake->mode==7,"combined shutdown lost primary progress/restoration");
    }
}

void convenience() {
    auto program = analyze(R"VOID(use console
func entry(n: i32) -> i32:
    var value = readline("Prompt: ")
    return value.length as i32
)VOID");
    for (bool optimize : {false, true}) {
        for (int failure = 0; failure < 3; ++failure) {
            auto fake = std::make_shared<Fake>();
            fake->input = "é🎮\r\nTAIL";
            auto runtime = std::make_shared<vd::Runtime>(1, Streams::isolated(fake));
            bool read = false;
            fake->on_read = [&](const Request&) {
                check(fake->output == "Prompt: ", "readline consumed input before its complete prompt");
                read = true;
            };
            if (failure == 1) {
                fake->fault = {ErrorKind::io_failure, 87};
                fake->partial = 2;
                fake->once = true;
            }
            if (failure == 2) {
                fake->on_write = [&] { runtime->console_context()->begin_close(); };
            }
            vd::Jit jit;
            auto code = jit.compile(program->lower(), program->entry_symbol(), optimize, {}, runtime);
            check(code->invoke(0) == (failure ? 0 : 2), "readline convenience result differs");
            check(read == (failure == 0), "failed prompt write/flush consumed input");
            if (failure) {
                check(fake->at == 0, "failed prompt changed native input position");
            }
            if (failure == 1) {
                fake->output.clear();
                check(code->invoke(0) == 2 && read, "recoverable prompt failure prevented retry");
            }
            empty(*runtime);
        }
    }
    auto shadow = analyze(R"VOID(use console as port
func read(count: i64 = 1) -> string:
    return "local"
func readline(prompt: string = "") -> string:
    return prompt
func entry(n: i32) -> i32:
    if read() == "local" and readline("shadow") == "shadow":
        return n
    return -1
)VOID");
    vd::Jit jit;
    check(jit.compile(*shadow)->invoke(7) == 7, "ordinary convenience name binding changed shadowing");
}

void blocked(){
    auto program=analyze("func entry(n: i32) -> i32:\n    var text = \"é🎮\"\n    print(text, n)\n    if text.byte_length != 6:\n        return -1\n    return n\n");auto fake=std::make_shared<Fake>();auto runtime=std::make_shared<vd::Runtime>(1,Streams::isolated(fake));vd::Jit jit;auto code=jit.compile(program->lower(),program->entry_symbol(),false,{},runtime);
    std::binary_semaphore entered(0),release(0);fake->on_write=[&]{entered.release();release.acquire();};auto call=std::async(std::launch::async,[&]{return code->invoke(9);});entered.acquire();
    {vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);check(runtime->collect(5s)==vd::Runtime::Collection::completed,"source blocking console prevented GC");}
    release.release();check(call.get()==9 && fake->output=="é🎮 9\n","source console native bytes/lifetime corrupted");empty(*runtime);
}
}
int main(int argc,char** argv){try{project=std::filesystem::temp_directory_path()/("VOID-console27-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))/"main.void";std::filesystem::create_directories(project.parent_path());std::ofstream(project)<<"func entry(n: i32) -> i32:\n    return n\n";if(argc==2 && std::string(argv[1])=="--four-system"){four_systems();std::filesystem::remove_all(project.parent_path());std::cout<<checks<<" four-system console checks passed\n";return 0;}negatives();execution();failures();error_kinds();null_order();convenience();blocked();lifecycle();cleanup_allocations();terminal();keyboard();generation_lifetime();collection_display();four_systems();std::filesystem::remove_all(project.parent_path());std::cout<<"source console checks: "<<checks<<'\n';return 0;}catch(const std::exception& e){if(!project.empty())std::filesystem::remove_all(project.parent_path());std::cerr<<e.what()<<'\n';return 1;}}
