#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/runtime.hpp"
#include "../managed/probe.hpp"
#include <cstring>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#ifdef __linux__
#include <sys/resource.h>
#endif

namespace {
void require(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
auto analyze(const std::string& text){return vd::analyze_source({"/await-syntax.void",text});}
auto checked(const std::string& text){auto result=analyze(text);if(!result.succeeded())throw vd::CompilationError(result.diagnostics);return result.program;}
// Compare the actual typed CFG, excluding source locations which necessarily
// differ when parentheses are inserted. Both versions independently validate.
std::string shape(const vd::ir::Module& module){
    vd::ir::validate(module);std::ostringstream out;
    for(const auto& f:module.functions){out<<f.name<<':'<<vd::ir::type_name(f.result)<<':'<<f.asynchronous<<';';
        for(auto t:f.parameters)out<<vd::ir::type_name(t)<<',';
        for(auto t:f.locals)out<<vd::ir::type_name(t)<<',';
        for(const auto& b:f.blocks){for(const auto& i:b.instructions){out<<static_cast<int>(i.op)<<':'<<i.a<<':'<<i.b<<':'<<i.immediate<<':'<<i.callee<<':'<<vd::ir::type_name(i.type)<<':'<<i.initialization<<':'<<i.array_repeat<<':'<<i.operator_glyph<<';';for(auto a:i.arguments)out<<a<<',';}
            out<<static_cast<int>(b.end.kind)<<':'<<b.end.value<<':'<<b.end.yes<<':'<<b.end.no<<'\n';}}
    return out.str();
}
const std::string prefix="func worker(value: i32) -> result<i32, string>:\n    return Ok(value)\nasync func child(value: i32) -> result<i32, string>:\n    return Ok(value)\n";
std::string source(const std::string& expression){return prefix+"async func compute(value: i32) -> result<i32, string>:\n    var pending = child(value)\n    return Ok("+expression+")\nasync func entry(value: i32) -> i32:\n    var outcome = await compute(value)\n    match outcome:\n        Ok(var answer):\n            return answer\n        Err(_):\n            return -1\n";}
}
void async_syntax_tests(const std::filesystem::path& root,bool small){
    if(small){
#ifdef __linux__
        rlimit limit{1048576,1048576};require(setrlimit(RLIMIT_STACK,&limit)==0,"set actual 1 MiB stack");
        rlimit actual{};require(getrlimit(RLIMIT_STACK,&actual)==0 && actual.rlim_cur==1048576 && actual.rlim_max==1048576,"actual stack limit");
#else
        throw std::runtime_error("actual 1 MiB stack test requires Linux");
#endif
    }
    unsigned positive=0,negative=0,recovery=0,executions=0;
    for(const auto& [sugar,legacy]:{
        std::pair{"await pending?","(await pending)?"},
        std::pair{"await child(value)?","(await child(value))?"},
        std::pair{"await spawn worker(value)?","(await spawn worker(value))?"},
        std::pair{"await (child(value))?","(await (child(value)))?"},
        std::pair{"await child(value)? + 1","(await child(value))? + 1"},
        std::pair{"2 * await child(value)? + await pending?","2 * (await child(value))? + (await pending)?"},
        std::pair{"(await child(value)?) * 2","((await child(value))?) * 2"}}){
        auto a=checked(source(sugar)),b=checked(source(legacy));
        require(shape(a->lower())==shape(b->lower()),std::string("typed CFG differs: ")+sugar);++positive;
    }
    auto reject=[&](const std::string& text,const std::string& message,const std::string& token){
        auto result=analyze(text);require(!result.succeeded(),"invalid await/propagation accepted");
        bool found=false;for(const auto& d:result.diagnostics)if(d.description.find(message)!=std::string::npos){
            require(d.code==vd::DiagnosticCode::type_mismatch || d.code==vd::DiagnosticCode::invalid_value || d.code==vd::DiagnosticCode::unsupported_type,"unexpected diagnostic code");
            require(d.primary.source && text.substr(d.primary.begin,d.primary.end-d.primary.begin)==token,"diagnostic lost exact source span: "+d.description);found=true;}
        require(found,"missing diagnostic: "+message);++negative;
    };
    reject(source("await (pending?)"),"? requires a result","?");
    reject(source("await child(value)??"),"? requires a result","?");
    reject(source("await value?"),"await requires","await");
    reject(source("await (value?)"),"? requires a result","?");
    reject("async func plain(value: i32) -> i32:\n    return value\n"+source("await plain(value)?"),"? requires a result","?");
    reject(prefix+"async func entry(value: i32) -> i32:\n    return await child(value)?\n","? requires a result","?");
    reject(prefix+"func compute(value: i32) -> result<i32, string>:\n    return Ok(await child(value)?)\nfunc entry(value: i32) -> i32:\n    return value\n","await requires","await");
    reject(prefix+"async func compute(value: i32) -> result<i32, i32>:\n    return Ok(await child(value)?)\nasync func entry(value: i32) -> i32:\n    return value\n","exactly matching error types","?");
    const std::string derived="struct base_error:\n    code: i32\nstruct child_error extends base_error:\n    detail: i32\nasync func child(value: i32) -> result<i32, child_error>:\n    return Err(child_error(code = value, detail = 1))\n";
    for(const auto& expression:{"await child(value)?","(await child(value))?"})
        reject(derived+"async func compute(value: i32) -> result<i32, base_error>:\n    return Ok("+expression+")\nasync func entry(value: i32) -> i32:\n    return value\n","exactly matching error types","?");
    reject(prefix+"async func compute(value: i32) -> result<i32, string>:\n    defer {\n        var invalid = await child(value)?\n    }\n    return Ok(value)\nasync func entry(value: i32) -> i32:\n    return value\n","await requires","await");
    // Same ordinary object upcast remains legal; exact E is a result policy.
    checked(derived+"func entry(value: i32) -> i32:\n    var derived = child_error(code = value, detail = 1)\n    var base: base_error = derived\n    return base.code\n");++positive;
    for(const auto& expression:{"await ?","await child(value)? +","await (child(value)?","await child(value)?[]","await child(value)?(","await child(value)?.."}){
        auto text=prefix+"async func broken(value: i32) -> result<i32, string>:\n    var damaged = "+expression+"\n    return Ok(value)\nfunc later() -> i32:\n    return missing_after_syntax\nfunc entry(value: i32) -> i32:\n    return value\n";
        auto result=analyze(text);require(!result.succeeded(),"malformed await syntax accepted");bool syntax=false,later=false;
        for(const auto& d:result.diagnostics){syntax|=d.code==vd::DiagnosticCode::syntax;later|=d.code==vd::DiagnosticCode::unknown_identifier && d.description.find("missing_after_syntax")!=std::string::npos;require(d.primary.begin<=d.primary.end && d.primary.end<=text.size(),"invalid recovered source span");}
        // An unmatched delimiter keeps later lines in the lexer's continued
        // expression, as before. Balanced damage must recover later bindings.
        const bool unclosed=std::string(expression)=="await (child(value)?" || std::string(expression)=="await child(value)?(";
        require(syntax && (unclosed || later),"malformed await lost syntax diagnostic or later declaration recovery: "+std::string(expression));++recovery;
    }
    // Existing depth guard and bounded diagnostics on the actual limited stack.
    for(unsigned count:{40U,400U}){
        auto text=source(std::string(count,'(')+"await child(value)?"+std::string(count,')'));
        auto result=analyze(text);if(count==40){require(result.succeeded(),"grouped await depth regression");++positive;}
        else{require(!result.succeeded(),"excessive await nesting accepted");bool nesting=false;for(const auto& d:result.diagnostics)nesting|=d.code==vd::DiagnosticCode::nesting;require(nesting,"excessive await nesting lost bounded diagnostic");++negative;}
    }
    for(auto [fixture,answer]:{std::pair{"values",130},std::pair{"errors",7},std::pair{"cancelled",0},std::pair{"failure",0},std::pair{"spawn",9}}){
        auto result=vd::analyze_project(root/"tests/async/syntax"/(std::string(fixture)+".void"));
        if(!result.succeeded())throw vd::CompilationError(result.diagnostics);++positive;
        for(bool optimize:{false,true})for(auto emission:{vd::ir::Emission::all,vd::ir::Emission::entry}){
            auto module=result.program->lower(emission);module.async_root=true;vd::ir::validate(module);vd::Jit jit;
            auto code=jit.compile(module,result.program->entry_symbol(),optimize,{},std::make_shared<vd::Runtime>());
            auto runtime=code->runtime_owner();managed_test::Probe p;p.inspect_roots=true;p.collect_before=true;p.frames.reserve(1024);
            const bool fails=std::string(fixture)=="cancelled" || std::string(fixture)=="failure";p.capture=fails;
            bool expected_failure=false;{managed_test::Observe observe(p);try{auto actual=code->invoke(7);require(actual==answer,std::string(fixture)+" expected "+std::to_string(answer)+", got "+std::to_string(actual));}catch(const std::runtime_error& e){if(!fails)throw;expected_failure=std::string(e.what()).find(std::string(fixture)=="cancelled"?"Async task cancelled":"Division by zero")!=std::string::npos;}}
            require(!fails || expected_failure,"cancellation/runtime failure converted to a result or swallowed");
            if(fails){vd::Runtime::Attachment attachment(*runtime);vd::Runtime::Managed managed(*runtime);require(bool(p.retained),"missing cleanup observer");std::int32_t cleanups=0;std::memcpy(&cleanups,p.retained->get(),sizeof(cleanups));require(cleanups==111,"await propagation bypassed reached cleanup");p.retained.reset();}
            require(p.roots_checked>0 && p.frames.empty() && p.roots_valid && p.commits_valid,"await propagation precise roots/ownership");
            {vd::Runtime::Attachment attachment(*runtime);vd::Runtime::Managed managed(*runtime);require(runtime->collect()==vd::Runtime::Collection::completed && runtime->snapshot().objects==0,"await propagation retained managed roots");}
            if(std::string(fixture)=="errors" && emission==vd::ir::Emission::entry){
                bool complete=false;unsigned failures=0;
                for(int budget=0;budget<2048 && !complete;++budget){
                    managed_test::Probe sweep;sweep.inspect_roots=true;sweep.frames.reserve(1024);
                    {managed_test::Observe observe(sweep);try{managed_test::AllocationFailure inject(budget);require(code->invoke(7)==7,"allocation retry error result");complete=true;}catch(const std::bad_alloc&){++failures;}catch(const std::runtime_error&){++failures;}}
                    require(sweep.frames.empty() && sweep.roots_valid && sweep.commits_valid,"await allocation failure root imbalance");
                    vd::Runtime::Attachment attachment(*runtime);vd::Runtime::Managed managed(*runtime);require(runtime->collect()==vd::Runtime::Collection::completed && runtime->snapshot().objects==0,"await allocation failure retained roots");
                }
                require(complete && failures>0,"await allocation failure sweep incomplete");
                std::cout<<"await error allocation sweep: "<<failures<<" injected failures passed; O2="<<optimize<<'\n';
            }
            ++executions;
        }
    }
    std::cout<<"await syntax: "<<positive<<" positive source/CFG checks, "<<negative<<" rejected cases, "<<recovery<<" recovery checks, "<<executions<<" JIT/GC executions passed; stack="<<(small?"1048576/1048576":"default")<<'\n';
}
