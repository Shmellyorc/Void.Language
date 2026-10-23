#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/tooling.hpp"
#include <chrono>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#endif
#include "void/native_runtime.hpp"
#include "../managed/probe.hpp"
#include "../native/probe.hpp"
#include <atomic>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
namespace {
unsigned checks=0;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);++checks;}
auto analyze(const std::filesystem::path& file){
    auto a=file.stem().string().starts_with("operator_native")?vd::analyze_project(file,vd::native_fixture_project()):vd::analyze_project(file);
    if(!a.succeeded())throw vd::CompilationError(a.diagnostics);return a.program;
}
void empty(vd::Runtime& runtime){vd::Runtime::Attachment a(runtime);vd::Runtime::Managed m(runtime);check(runtime.collect()==vd::Runtime::Collection::completed && runtime.snapshot().objects==0,"operator roots retained after invocation");}
void roots(const managed_test::Probe& p){check(p.roots_valid && p.commits_valid && p.frames.empty() && p.roots_checked>0,"operator precise roots/frame or construction commit failure");}
std::atomic<unsigned> collections{0};
std::thread::id owner;
void collect_worker(){if(std::this_thread::get_id()==owner)return;if(vd::Runtime::current().collect()!=vd::Runtime::Collection::completed)std::terminate();++collections;}
std::vector<int> events;
void native_event(std::int32_t value){events.push_back(value);}
struct Polls {explicit Polls(void(*poll)()){managed_test::concurrent_poll=poll;}~Polls(){managed_test::concurrent_poll=nullptr;}};
struct NativeEvents {NativeEvents(){events.clear();events.reserve(32);native_test::scalar_poll=native_event;}~NativeEvents(){native_test::scalar_poll=nullptr;}};
void reject(const std::string& source,const std::string& expected){auto a=vd::analyze_source({"/operator-effects.void",source});check(!a.succeeded(),"unsafe operator effect accepted");bool found=false;for(const auto& d:a.diagnostics)found|=d.description.find(expected)!=std::string::npos;if(!found)throw vd::CompilationError(a.diagnostics);++checks;}
void systems(const std::filesystem::path& fixtures){
    auto combined=analyze(fixtures/"operator_systems.void");
    for(bool optimized:{false,true}){
        vd::Jit jit;auto code=jit.compile(*combined,optimized);auto runtime=code->runtime_owner();
        managed_test::Probe p;p.inspect_roots=true;p.collect_before=true;p.frames.reserve(1024);
        {managed_test::Observe observe(p);for(int input:{0,4,-3})check(code->invoke(input)==input,"combined fields/operators/hooks/time result");}
        roots(p);check(p.constructions>10 && p.polls>0,"combined allocation/GC probe not exercised");empty(*runtime);
        bool succeeded=false;unsigned failures=0;
        for(int budget=0;budget<=32768 && !succeeded;budget=budget<64?budget+1:budget*2){
            managed_test::Probe sweep;sweep.inspect_roots=true;sweep.frames.reserve(1024);int result=101;
            {managed_test::Observe observe(sweep);try{managed_test::AllocationFailure fault(budget);result=code->invoke(4);}catch(const std::bad_alloc&){++failures;}catch(const std::runtime_error&){++failures;}}
            check(result==4 || result==101,"allocation fault produced silently different successful result");
            succeeded=result==4;if(!succeeded)++failures;
            check(sweep.roots_valid && sweep.commits_valid && sweep.frames.empty(),"combined failure corrupted roots/commit/frame");empty(*runtime);
        }
        check(succeeded && failures>0,"combined allocation sweep failed to reach success");check(code->invoke(4)==4,"combined recovery after allocation sweep");empty(*runtime);
    }
    auto native=analyze(fixtures/"operator_native_systems.void");
    for(bool optimized:{false,true}){
        vd::Jit jit;auto code=jit.compile(*native,optimized);auto runtime=code->runtime_owner();
        for(int input:{0,4,-3}){
            managed_test::Probe p;p.inspect_roots=true;p.collect_before=true;p.capture=true;p.capture_at=0;p.frames.reserve(1024);
            p.before_construct=[&](std::size_t n,const void* data,std::size_t size){if(!p.capture_at && size==4){int value;std::memcpy(&value,data,4);if(value==4)p.capture_at=n;}};
            int result=101;std::string diagnostic;
            {managed_test::Observe observe(p);NativeEvents trace;try{result=code->invoke(input);}catch(const std::runtime_error& e){diagnostic=e.what();}}
            roots(p);check(bool(p.retained),"combined property storage not retained");
            check(events==(input<0?std::vector<int>{10,11,20,30,50,60,50,90}:std::vector<int>{10,11,20,30,12,40,50,90}),"combined getter/RHS/operator/setter/inequality/hook cleanup order");
            {vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);int value;std::memcpy(&value,p.retained->get(),4);check(value==(input<0?4:input+4),"failed hook committed captured property replacement");p.retained.reset();}
            if(input<0){auto primary=diagnostic.find("Format rejected: cross--1"),secondary=diagnostic.find("Format rejected: cross--2");check(primary!=std::string::npos && primary<secondary && diagnostic.find("Additional failure during defer")!=std::string::npos,"combined failed hook/cleanup reason order");check(diagnostic.find("operator +")!=std::string::npos && diagnostic.find("operator_native_systems.void")!=std::string::npos && diagnostic.find("holder.target.amount += rhs(input)")!=std::string::npos,"combined original operator/compound source frames");}
            else check(diagnostic.empty() && result==input,"combined native trace result");empty(*runtime);
        }
    }
    auto asynchronous=analyze(fixtures/"operator_systems_async.void");
    for(bool optimized:{false,true}){
        vd::Jit jit;auto code=jit.compile_async(*asynchronous,optimized);auto runtime=code->runtime_owner();runtime->async_worker_limit(2);owner=std::this_thread::get_id();collections=0;
        managed_test::Probe p;p.inspect_roots=true;p.collect_before=true;p.frames.reserve(1024);
        {Polls poll(collect_worker);managed_test::Observe observe(p);for(int input:{0,4,-3})check(code->invoke(input)==input,"combined suspension/worker/hook/result behavior");}
        roots(p);check(collections>0,"combined worker safepoint collection not exercised");empty(*runtime);
    }
    const auto path=fixtures/"operator_systems.void";const auto text=vd::read_source(path).text();
    auto overlay=[&](const std::string& source){vd::SourceOverrides files;files.emplace("/operator_systems.void",vd::SourceFile(path.string(),source));return vd::analyze_project(path,files);};
    auto replace=[](std::string text,const std::string& from,const std::string& to){auto at=text.find(from);if(at==std::string::npos)throw std::runtime_error("missing combined replacement");text.replace(at,from.size(),to);return text;};
    for(const auto& [source,expected]:std::vector<std::pair<std::string,std::string>>{
        {text+"func (self account) steal() -> i32:\n    return self.id\n","not accessible"},
        {text+"func (self account) mutate():\n    self.history[0] += 1\n","const stored field"},
        {replace(text,"return format(self.stored, pattern)","self.stored = 9\n    return format(self.stored, pattern)"),"const stored field"},
        {text+"func illegal(input: i32) -> i32:\n    return quantity<i32>(value = input).stored\n","not accessible"}
    }){auto a=overlay(source);check(!a.succeeded(),"combined field/hook policy accepted");bool found=false;for(const auto& d:a.diagnostics)if(d.description.find(expected)!=std::string::npos){found=true;check(d.primary.source && d.primary.begin<d.primary.source->text().size(),"combined policy source span missing");}if(!found)throw vd::CompilationError(a.diagnostics);++checks;}
    reject("struct cell:\n    value: i32\nstruct sealed<t>:\n    private const item: t\n    operator +(const left: sealed<t>, const right: i32) -> t:\n        return left.item\nfunc (self sealed<t>) init(item: t):\n    self.item = item\nfunc entry(input: i32) -> i32:\n    var owner = sealed<cell>(item = cell(value = input))\n    var alias = owner + 0\n    alias.value++\n    return input\n","readonly");
    vd::SourceOverrides unsafe_hook;unsafe_hook.emplace("/operator_systems.void",vd::SourceFile(path.string(),replace(text,"return format(self.stored, pattern)","var sample = now_utc()\n    return format(self.stored, pattern)")));
    auto prohibited_hook=vd::analyze_project(fixtures/"operator_systems_async.void",unsafe_hook);check(!prohibited_hook.succeeded(),"worker clock effect hidden through generic custom format hook");bool effect=false;for(const auto& d:prohibited_hook.diagnostics)effect|=d.description.find("Worker")!=std::string::npos;if(!effect)throw vd::CompilationError(prohibited_hook.diagnostics);++checks;
    auto changed=overlay(replace(text,"return Ok(input)","return Ok(input + 1)"));if(!changed.succeeded())throw vd::CompilationError(changed.diagnostics);
    auto incompatible=overlay(replace(text,"const spec = \"000\"","const spec = \"00\""));if(!incompatible.succeeded())throw vd::CompilationError(incompatible.diagnostics);
    check(combined->compatibility_contract()==changed.program->compatibility_contract(),"combined body edit changed compatibility");
    auto plan=vd::plan_invalidation(*combined,{"/operator_systems.void"},incompatible.program.get());check(plan.source_compatibility==vd::SourceCompatibility::restart_required && !plan.dependent_declarations.empty(),"combined format dependency did not invalidate");
    for(bool optimized:{false,true}){vd::Jit jit;auto old=jit.compile(*combined,optimized);vd::ReloadSlot slot(old);auto retained=slot.acquire();check(jit.replace(slot,*changed.program,optimized) && slot.invoke(4)==5 && retained->invoke(4)==4,"combined generation body lease");check(!jit.replace(slot,*incompatible.program,optimized) && slot.invoke(4)==5 && retained->invoke(4)==4,"combined format policy restart/old lease");empty(*slot.acquire()->runtime_owner());}
}

void four_system_policy(const std::filesystem::path& fixtures){
    const auto path=fixtures/"operator_console_systems.void";auto program=analyze(path);auto source=vd::read_source(path).text();
    auto test=[&](const std::string& candidate,const char* needle){vd::SourceOverrides files;files.emplace("/operator_console_systems.void",vd::SourceFile(path.string(),candidate));auto a=vd::analyze_project(path,files);check(!a.succeeded(),"combined unsafe source accepted");bool matched=false;for(const auto& d:a.diagnostics)if(d.description.find(needle)!=std::string::npos){matched=true;check(d.primary.source && d.primary.begin<d.primary.source->text().size(),"combined negative source location lost");}if(!matched)throw vd::CompilationError(a.diagnostics);++checks;};
    test(source+"func steal(value: model.number<i32>) -> i32:\n    return value.stored\n","not accessible");
    test(source+"func mutate(const value: result<model.packet, string>):\n    if value is Ok(var selected):\n        if selected is model.packet.data(var cargo):\n            cargo.marker = 4\n","readonly");
    test(source+"func keys():\n    var bad: {model.number<i32>: string} = {}\n","custom");
    test(source+"value struct nested:\n    private const item: model.number<i32>\nfunc keys():\n    var bad: {nested: string} = {}\n","custom");
    test(source+"func invalid():\n    var bad = format([model.number<i32>()], \"000\")\n","alignment");
    test(source+"func incorrect():\n    var value = model.number<i32>()\n    value += 1 as i64\n","operator");
    test("use \"operator_console_model.void\" as model\nfunc task(n: i32) -> i32:\n    return model.run(n)\nasync func entry(n: i32) -> i32:\n    return await spawn task(n)\n","Worker");
    {auto model=vd::read_source(fixtures/"operator_console_model.void").text();auto at=model.find("    self.value = value");check(at!=std::string::npos,"missing managed construction boundary");model.insert(at,"    print(self)\n");vd::SourceOverrides files;files.emplace("/operator_console_model.void",vd::SourceFile((fixtures/"operator_console_model.void").string(),model));auto a=vd::analyze_project(path,files);check(!a.succeeded(),"unfinished generic self displayed through composite adapter");bool located=false;for(const auto& d:a.diagnostics)located|=d.primary.module_identity=="/operator_console_model.void";check(located,"construction display diagnostic lost owner source");}
    auto module=program->lower();std::string remainder;for(const auto& f:module.functions)if(f.operator_glyph=="%" && f.name.find("number")!=std::string::npos){remainder=f.name;break;}
    check(!remainder.empty(),"missing combined remainder declaration");bool changed=false;for(auto& f:module.functions)for(auto& b:f.blocks)for(auto& i:b.instructions)if(!changed && i.operator_glyph=="+" && i.callee.find("number")!=std::string::npos){i.callee=remainder;changed=true;}
    bool denied=false;try{vd::ir::validate(module);}catch(const std::invalid_argument&){denied=true;}check(changed && denied,"combined forged selected operator target accepted");
}

}
int main(int argc,char** argv){try{
    if(argc==6 && (std::string(argv[1])=="--emit" || std::string(argv[1])=="--run" || std::string(argv[1])=="--measure")){
        auto program=analyze(argv[2]);bool optimized=std::string(argv[4])=="optimized";bool asynchronous=std::string(argv[5])=="async";
        if(std::string(argv[1])=="--emit") {if(asynchronous)vd::emit_async_object(program->lower(),argv[3],optimized);else vd::emit_object(program->lower(),argv[3],optimized,true);}
        else {auto start=std::chrono::steady_clock::now();vd::Jit jit;auto code=asynchronous?jit.compile_async(*program,optimized):jit.compile(*program,optimized);auto compiled=std::chrono::steady_clock::now();auto result=code->invoke(std::stoi(argv[3]));auto done=std::chrono::steady_clock::now();
            if(std::string(argv[1])=="--measure"){
                auto runtime=code->runtime_owner();std::size_t live=0;{vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);runtime->collect();live=runtime->snapshot().objects;}
                std::cout<<"{\"result\":"<<result<<",\"compile_ms\":"<<std::chrono::duration<double,std::milli>(compiled-start).count()<<",\"invoke_ms\":"<<std::chrono::duration<double,std::milli>(done-compiled).count()<<",\"objects_after_collection\":"<<live;
#if defined(__unix__) || defined(__APPLE__)
                rusage usage{};if(getrusage(RUSAGE_SELF,&usage)==0){auto kib=usage.ru_maxrss;
#if defined(__APPLE__)
                    kib/=1024;
#endif
                    std::cout<<",\"process_peak_rss_kib\":"<<kib;}
#endif
                std::cout<<"}\n";
            }else std::cout<<result<<'\n';}
        return 0;
    }
    if(argc==3 && std::string(argv[1])=="--four-system-policy"){four_system_policy(std::filesystem::path(argv[2])/"tests/stage10");std::cout<<checks<<" four-system policy checks passed\n";return 0;}
    if(argc!=2)throw std::invalid_argument("repository root required");auto fixtures=std::filesystem::path(argv[1])/"tests/stage10";
    systems(fixtures);four_system_policy(fixtures);
    auto lifetime=analyze(fixtures/"operator_lifetimes.void");
    for(bool optimized:{false,true}){
        vd::Jit jit;auto module=lifetime->lower();auto runtime=std::make_shared<vd::Runtime>(1);auto code=jit.compile(module,module.entry_symbol,optimized,{},runtime);
        managed_test::Probe p;p.inspect_roots=true;p.collect_before=true;p.frames.reserve(1024);unsigned rhs=0;
        p.before_construct=[&](std::size_t,const void* data,std::size_t size){if(size>=sizeof(void*)+sizeof(std::int32_t)){std::int32_t tag;std::memcpy(&tag,static_cast<const char*>(data)+sizeof(void*),sizeof(tag));if(tag==222)++rhs;}};
        auto before=runtime->snapshot().completed_epoch;
        {managed_test::Observe observe(p);check(code->invoke(7)==7,"operator RHS snapshot/managed return GC result");}
        roots(p);check(rhs==2 && p.constructions>=6 && runtime->snapshot().completed_epoch>before,"RHS construction/collection probe was not exercised");empty(*runtime);
        bool succeeded=false;unsigned failed=0;
        for(int budget=0;budget<=8192 && !succeeded;budget=budget<128?budget+1:budget*2){
            managed_test::Probe sweep;sweep.inspect_roots=true;sweep.frames.reserve(1024);
            {managed_test::Observe observe(sweep);try{managed_test::AllocationFailure fault(budget);if(code->invoke(7)!=7)throw std::logic_error("operator allocation retry result");succeeded=true;}catch(const std::bad_alloc&){++failed;}catch(const std::runtime_error&){++failed;}}
            check(sweep.roots_valid && sweep.commits_valid && sweep.frames.empty(),"operator allocation failure damaged roots/frames");empty(*runtime);
        }
        check(succeeded && failed>0,"operator allocation sweep did not reach failure and success");check(code->invoke(7)==7,"operator recovery after allocation sweep");empty(*runtime);
    }
    auto failure_text=vd::read_source(fixtures/"operator_failures.void").text();
    auto failure=vd::analyze_source({"/retained-operator.void",failure_text});if(!failure.succeeded())throw vd::CompilationError(failure.diagnostics);
    for(bool optimized:{false,true}){
        vd::Jit jit;auto code=jit.compile(*failure.program,optimized);auto runtime=code->runtime_owner();
        for(int capture:{10,20}){
            managed_test::Probe p;p.inspect_roots=true;p.collect_before=true;p.capture=true;p.capture_at=0;p.frames.reserve(1024);
            p.before_construct=[&](std::size_t n,const void* data,std::size_t size){if(size==sizeof(std::int32_t)){std::int32_t number;std::memcpy(&number,data,sizeof(number));if(number==capture)p.capture_at=n;}};
            std::string diagnostic;{managed_test::Observe observe(p);try{code->invoke(0);}catch(const std::runtime_error& e){diagnostic=e.what();}}
            roots(p);check(bool(p.retained),"reached operator/caller cleanup receiver not retained");
            auto primary=diagnostic.find("Division by zero"),additional=diagnostic.find("Additional failure during defer"),secondary=diagnostic.find("conversion is out of range");
            check(primary!=std::string::npos && primary<additional && additional<secondary,"operator primary/cleanup diagnostic retention order");
            check(diagnostic.find("operator ==")!=std::string::npos && diagnostic.find("retained-operator.void")!=std::string::npos && diagnostic.find("return 10 / left.value == right.value")!=std::string::npos && diagnostic.find("var different = number(value = input) != number(value = 4)")!=std::string::npos,"operator/caller original source frames missing");
            {vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);std::int32_t number;std::memcpy(&number,p.retained->get(),sizeof(number));check(number==capture+1,"failed cleanup skipped older reached defer or ran unreached defer");p.retained.reset();}empty(*runtime);
        }
        managed_test::Probe p;p.inspect_roots=true;p.collect_before=true;p.frames.reserve(1024);unsigned invocations=0;
        p.before_construct=[&](std::size_t,const void* data,std::size_t size){if(size==4){int value;std::memcpy(&value,data,4);if(value==20)++invocations;}};
        {managed_test::Observe observe(p);check(code->invoke(4)==4 && code->invoke(7)==7,"derived inequality complement/recovery result");}roots(p);check(invocations==2,"derived != did not invoke == exactly once per operation");empty(*runtime);
    }
    // Release caller-owned source/analysis before executing retained checked frames.
    {vd::Jit jit;auto code=jit.compile(*failure.program,true);failure.program.reset();failure_text.assign("changed source");std::string diagnostic;try{code->invoke(0);}catch(const std::runtime_error& e){diagnostic=e.what();}check(diagnostic.find("return 10 / left.value == right.value")!=std::string::npos,"operator failure source did not retain analysis snapshot");empty(*code->runtime_owner());}
    auto suspension=analyze(fixtures/"operator_suspension.void");
    for(bool optimized:{false,true}){vd::Jit jit;auto code=jit.compile_async(*suspension,optimized);managed_test::Probe p;p.inspect_roots=true;p.collect_before=true;p.frames.reserve(1024);{managed_test::Observe observe(p);check(code->invoke(7)==7,"operator left operand lost across RHS await/GC");}roots(p);check(p.constructions>0,"operator suspension collection probe did not run");empty(*code->runtime_owner());}
    auto workers=analyze(fixtures/"operator_workers.void");
    for(bool optimized:{false,true}){
        vd::Jit jit;auto code=jit.compile_async(*workers,optimized);auto runtime=code->runtime_owner();runtime->async_worker_limit(2);owner=std::this_thread::get_id();collections=0;
        {Polls poll(collect_worker);check(code->invoke(7)==7,"worker operator/generic value result under collection");}
        check(collections>0,"worker operator collection hook did not run");empty(*runtime);
    }
    const std::string header="value struct number:\n    value: i32\n    operator +(const left: number, const right: number) -> i32:\n";
    const std::string worker="func work(input: i32) -> i32:\n    return number(value = input) + number(value = 1)\nasync func entry(input: i32) -> i32:\n    return await spawn work(input)\n";
    reject(header+"        var callback = func() -> i32:\n            return left.value\n        return callback()\n"+worker,"Worker");
    reject("async func child(value: i32) -> i32:\n    return value\n"+header+"        var task = child(left.value)\n        return right.value\n"+worker,"Worker");
    reject("struct cell:\n    value: i32\nfunc mutate(owner: cell) -> i32:\n    owner.value++\n    return owner.value\nvalue struct number:\n    owner: cell\n    operator +(const left: number) -> i32:\n        return mutate(left.owner)\nfunc entry(input: i32) -> i32:\n    return +number(owner = cell(value = input))\n","readonly");
    const auto clock_path=fixtures/"operator_clock_probe.void";
    vd::SourceOverrides clock_source;clock_source.emplace("/operator_clock_probe.void",vd::SourceFile(clock_path.string(),"use time as clock\n"+header+"        var sample = clock.now_utc()\n        return left.value + right.value\n"+worker));
    auto clock_effect=vd::analyze_project(clock_path,clock_source);check(!clock_effect.succeeded(),"worker operator clock effect accepted");bool clock_rejected=false;for(const auto& d:clock_effect.diagnostics)clock_rejected|=d.description.find("Worker")!=std::string::npos;check(clock_rejected,"clock worker rejection unrelated to effects");
    auto cleanup=analyze(fixtures/"operator_cleanup.void");
    for(bool optimized:{false,true}){vd::Jit jit;auto code=jit.compile_async(*cleanup,optimized);std::string diagnostic;try{code->invoke(7);}catch(const std::runtime_error& e){diagnostic=e.what();}check(diagnostic.find("cannot enqueue during structured cleanup")!=std::string::npos,"operator indirectly published an async child during cleanup");empty(*code->runtime_owner());}
    auto owned_errors=analyze(fixtures/"operator_errors.void");
    for(bool optimized:{false,true}){vd::Jit jit;auto code=jit.compile(*owned_errors,optimized);std::string diagnostic;try{code->invoke(7);}catch(const std::runtime_error& e){diagnostic=e.what();}auto primary=diagnostic.find("Format rejected: owned-1"),secondary=diagnostic.find("Format rejected: owned-2");check(primary!=std::string::npos && primary<secondary && diagnostic.find("Additional failure during defer")!=std::string::npos,"operator dynamic primary error overwritten by cleanup error");empty(*code->runtime_owner());}
    auto worker_errors=analyze(fixtures/"operator_worker_failures.void");
    for(bool optimized:{false,true}){vd::Jit jit;auto code=jit.compile_async(*worker_errors,optimized);owner=std::this_thread::get_id();collections=0;std::string diagnostic;{Polls poll(collect_worker);try{code->invoke(7);}catch(const std::runtime_error& e){diagnostic=e.what();}}auto first=diagnostic.find("Format rejected: owned-1"),second=diagnostic.find("Format rejected: owned-2");check(first!=std::string::npos && first<second && diagnostic.find("operator ==")!=std::string::npos && collections>0,"worker operator primary/cleanup error lease or collection failed");empty(*code->runtime_owner());}
    auto native=analyze(fixtures/"operator_native.void");
    for(bool optimized:{false,true}){
        vd::Jit jit;auto code=jit.compile(*native,optimized);
        for(int input:{4,2,0,-1}){NativeEvents observe;std::string diagnostic;int result=-1;try{result=code->invoke(input);}catch(const std::runtime_error& e){diagnostic=e.what();}
            check(events==(input<0?std::vector<int>{10,20,9}:std::vector<int>{10,20,1,2,9}),"native operator operands/==/cleanup ran out of order or more than once");
            check(input>0?(diagnostic.empty() && result==(input==4?0:1)):diagnostic.find("Division by zero")!=std::string::npos,"native operator failure/result propagation");empty(*code->runtime_owner());
        }
    }
    auto prohibited=vd::analyze_project(fixtures/"operator_native_worker.void",vd::native_fixture_project());check(!prohibited.succeeded(),"worker operator native edge escaped reachability validation");bool worker_diagnostic=false;for(const auto& d:prohibited.diagnostics)worker_diagnostic|=d.description.find("Worker")!=std::string::npos;check(worker_diagnostic,"native worker rejection unrelated to effects");
    // A forged unknown call in a selected operator must be rejected by worker IR validation too.
    auto invalid=workers->lower();invalid.imports.push_back("unsafe_native");bool changed=false;
    for(auto& f:invalid.functions)if(f.operator_glyph=="+") {auto& instructions=f.blocks.back().instructions;vd::ir::Instruction constant{vd::ir::Op::constant};constant.immediate=1;auto index=instructions.size();instructions.push_back(constant);vd::ir::Instruction call{vd::ir::Op::call};call.callee="unsafe_native";call.arguments={static_cast<unsigned>(index)};instructions.push_back(call);changed=true;}
    bool rejected=false;try{vd::ir::validate(invalid);}catch(const std::invalid_argument& e){rejected=std::string(e.what()).find("worker has unknown native effect")!=std::string::npos;}check(changed && rejected,"malicious operator worker IR accepted unknown effects");
    std::cout<<checks<<" operator GC/effect/failure integration checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
