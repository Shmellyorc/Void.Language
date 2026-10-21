#pragma once
#include "void/utf8.hpp"
#include <chrono>
#include <iostream>
namespace sprint3_test {
using managed_test::require;
inline void native() {
    vd::Runtime runtime(1);vd::Runtime::Attachment attachment(runtime);vd::Runtime::Managed managed(runtime);
    void* values[5]{};constexpr std::size_t zero=0;vd::Runtime::Slot slots[5];for(unsigned n=0;n<5;++n)slots[n]={&values[n],sizeof(void*),&zero,1};vd::Runtime::Frame roots(runtime,slots,5);
    const std::string text("A\0\xf0\x9f\x8e\xae",6);runtime.string_into(&values[0],text);
    require(runtime.string_count(values[0],false)==3 && runtime.string_count(values[0],true)==6,"scalar/byte count mismatch");
    runtime.string_index_into(&values[1],values[0],vd::ScalarEncoding::i64,~std::uint64_t{});require(runtime.string_bytes(values[1])==std::string_view(text).substr(2),"native scalar index");
    runtime.string_encode_into(&values[2],values[0]);auto bytes=runtime.root_word(&values[2]);
    runtime.collect();require(runtime.string_decode_into(&values[3],bytes) && runtime.string_bytes(values[3])==text,"owned UTF-8 GC roundtrip");
    runtime.list_write(bytes,vd::ScalarEncoding::u8,0,66);require(runtime.string_bytes(values[0])==text && runtime.string_bytes(values[3])==text,"bytes mutation changed immutable text");
    runtime.list_write(bytes,vd::ScalarEncoding::u8,0,255);auto old=values[3];require(!runtime.string_decode_into(&values[3],bytes) && values[3]==old,"malformed decode replaced output");runtime.list_write(bytes,vd::ScalarEncoding::u8,0,65);
    for(unsigned operation=0;operation<2;++operation){bool success=false;unsigned failures=0;
        for(int budget=0;budget<512 && !success;++budget){runtime.string_into(&values[4],"previous");auto previous=values[4];
            {managed_test::AllocationFailure inject(budget);try{if(operation==0)runtime.string_encode_into(&values[4],values[0]);else require(runtime.string_decode_into(&values[4],bytes),"valid decode failed");success=true;}catch(const std::bad_alloc&){++failures;require(values[4]==previous,"failed UTF-8 conversion replaced output");}}
            runtime.collect();require(runtime.contains(values[0]) && runtime.contains(values[2]),"conversion failure lost rooted inputs");
        }require(success && failures>0,"UTF-8 conversion allocation sweep incomplete");std::cout<<"Sprint 3 native conversion "<<operation<<": "<<failures<<" failures; output/root preservation passed\n";
    }
    for(auto encoding:{vd::ScalarEncoding::i8,vd::ScalarEncoding::i16,vd::ScalarEncoding::i32,vd::ScalarEncoding::i64}){runtime.string_index_into(&values[1],values[0],encoding,~std::uint64_t{});require(runtime.string_count(values[1],false)==1,"negative index width");}
    bool rejected=false;try{runtime.string_index_into(&values[1],values[0],vd::ScalarEncoding::u64,~std::uint64_t{});}catch(const std::out_of_range&){rejected=true;}require(rejected,"unsigned string index wrapped");
    std::uint64_t observed=0;
    for(unsigned count:{64u,1024u,4096u}){std::string input;for(unsigned n=0;n<count;++n)input+="\xc3\xa9";runtime.string_into(&values[0],input);
        auto start=std::chrono::steady_clock::now();for(unsigned repeat=0;repeat<128;++repeat)observed+=runtime.string_count(values[0],false);auto middle=std::chrono::steady_clock::now();
        for(unsigned repeat=0;repeat<32;++repeat)runtime.string_index_into(&values[1],values[0],vd::ScalarEncoding::i64,count-1);auto stop=std::chrono::steady_clock::now();
        std::cout<<"Sprint 3 traversal scalars="<<count<<" count128_us="<<std::chrono::duration_cast<std::chrono::microseconds>(middle-start).count()<<" last_index32_us="<<std::chrono::duration_cast<std::chrono::microseconds>(stop-middle).count()<<'\n';
    }require(observed==128*(64+1024+4096),"traversal measurement optimized away");
}
inline void independent() {
    auto module=vd::compile_source({"/sprint3-ir.void","func entry(x: i32) -> i32:\n    var text = \"a\"\n    var count = text.length\n    var scalar = text[0]\n    var bytes = text.to_utf8()\n    var result = string.from_utf8(bytes)\n    return count as i32\n"});
    unsigned checked=0;
    for(unsigned f=0;f<module.functions.size();++f){for(unsigned b=0;b<module.functions[f].blocks.size();++b){for(unsigned n=0;n<module.functions[f].blocks[b].instructions.size();++n){auto op=module.functions[f].blocks[b].instructions[n].op;
        if(op!=vd::ir::Op::string_count && op!=vd::ir::Op::string_index && op!=vd::ir::Op::string_encode && op!=vd::ir::Op::string_decode)continue;
        auto bad=module;bad.functions[f].blocks[b].instructions[n].receiver=vd::ir::Place{0,{}};bool rejected=false;try{vd::ir::validate(bad);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"invalid text IR flags accepted");++checked;
    }}}
    require(checked==4,"text IR validation coverage incomplete");
}
inline void pop_versions(bool optimized) {
    const std::string text="struct holder:\n    items: [i32]\nfunc entry(input: i32) -> i32:\n    var owner = holder()\n    if input != 0:\n        owner.items.push(7)\n    owner.items.pop()\n    return owner.items.count as i32\n";
    auto analysis=vd::analyze_source({"/sprint3-version.void",text});if(!analysis.succeeded())throw vd::CompilationError(analysis.diagnostics);
    vd::Jit jit;auto code=jit.compile(*analysis.program,optimized);auto runtime=code->runtime_owner();
    for(int input:{0,1}){managed_test::Probe probe;probe.capture=true;probe.inspect_roots=true;
        {managed_test::Observe observe(probe);require(code->invoke(input)==0,"pop count result");}require(bool(probe.retained) && probe.roots_valid && probe.frames.empty(),"pop version capture/root failure");
        vd::Runtime::Attachment attachment(*runtime);vd::Runtime::Managed managed(*runtime);void* list=nullptr;std::memcpy(&list,probe.retained->get(),sizeof(list));constexpr std::size_t zero=0;vd::Runtime::Slot slot{&list,sizeof(list),&zero,1};vd::Runtime::Frame root(*runtime,&slot,1);
        require(runtime->list_version(runtime->root_word(&list),vd::ScalarEncoding::i32)==(input?2u:0u),"empty pop changed version or successful pop failed to increment it");
    }
}
inline void source(bool optimized) {
    const std::string text=R"(struct node:
    number: i32
func entry(input: i32) -> i32:
    var text = "A🎮\0e\u{301}"
    var bytes = text.to_utf8()
    var items = [node(number = 7)]
    var dictionary = {"x": items[0]}
    var count = 0
    for index in 0..<40:
        if string.from_utf8(bytes) is Ok(var copy):
            count += copy.length as i32
        else:
            return 90
        if items.get(-1) is Ok(var found):
            if found.number != 7:
                return 91
        if dictionary.get("x") is Ok(var item):
            if item.number != 7:
                return 92
    if items.pop() is Ok(var last):
        return count + last.number
    return 93
)";
    auto analysis=vd::analyze_source({"/sprint3.void",text});if(!analysis.succeeded())throw vd::CompilationError(analysis.diagnostics);
    vd::Jit jit;auto runtime=std::make_shared<vd::Runtime>(1);auto module=analysis.program->lower();auto code=jit.compile(module,module.entry_symbol,optimized,{},runtime);
    managed_test::Probe roots;roots.inspect_roots=true;{managed_test::Observe observe(roots);require(code->invoke(0)==207,"Sprint 3 GC source result");}require(roots.roots_valid && roots.frames.empty(),"Sprint 3 precise roots/frame cleanup");
    bool success=false;unsigned failures=0;
    for(int budget=0;budget<65536 && !success;budget=budget<64?budget+1:budget*2){managed_test::Probe state;state.inspect_roots=true;state.frames.reserve(128);
        {managed_test::Observe observe(state);try{managed_test::AllocationFailure inject(budget);require(code->invoke(0)==207,"Sprint 3 allocation-sweep value");success=true;}catch(const std::bad_alloc&){++failures;}catch(const std::runtime_error&){++failures;}}
        require(state.roots_valid && state.commits_valid && state.frames.empty(),"Sprint 3 failure roots/cleanup");vd::Runtime::Attachment attach(*runtime);vd::Runtime::Managed managed(*runtime);require(runtime->collect()==vd::Runtime::Collection::completed && runtime->snapshot().objects==0,"Sprint 3 failure heap leak");
    }require(success && failures>0,"Sprint 3 source sweep incomplete");require(code->invoke(0)==207,"Sprint 3 retry after failure");
    vd::ReloadSlot slot(jit.compile(*analysis.program,optimized));auto lease=slot.acquire();auto after=text;after.replace(after.find('7'),1,"8");auto next=vd::analyze_source({"/sprint3.void",after});require(next.succeeded() && jit.replace(slot,*next.program,optimized),"Sprint 3 compatible body reload rejected");require(lease->invoke(0)==207 && slot.invoke(0)==91,"Sprint 3 retained generation/reload result");
    std::cout<<"Sprint 3 source"<<(optimized?" O2":" normal")<<": "<<failures<<" allocation failures; roots/cleanup/retry/reload passed\n";
}
}
