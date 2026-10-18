#include "../managed/probe.hpp"
#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/tooling.hpp"
#include <cstring>
#include <iostream>
using namespace vd;
using managed_test::require;
namespace {
void empty(Runtime& r){Runtime::Attachment a(r);Runtime::Managed m(r);require(r.collect()==Runtime::Collection::completed && !r.snapshot().objects,"dictionary heap/root leak");}
void diagnostics(){
    auto metadata=analyze_source({"dictionary-metadata.void","const values: {i32: i32} = {1: 2}\nfunc entry(x: i32) -> i32:\n    return values[1] + values.count as i32\n"});require(metadata.succeeded(),"dictionary constant analysis");for(const auto& d:metadata.program->declarations())if(d.kind==AnalyzedDeclaration::Kind::module_constant)require(d.type.kind==ir::Type::dictionary && !d.value,"dictionary advertised a scalar constant value");for(const auto& r:metadata.program->references())require(!r.to.starts_with("$dict."),"builtin dictionary query advertised a nonexistent source symbol");
    const std::string entry="func entry(x: i32) -> i32:\n";
    for(const auto& text:{
        "    var d = {}\n    return x\n",
        "    var d: {f32: i32}\n    return x\n",
        "    var d: {[i32]: i32}\n    return x\n",
        "    var d = {true: 1}\n    return d[1]\n",
        "    var d = {1: 2}\n    d[true] = 3\n    return x\n",
        "    const d = {1: 2}\n    d[1] = 3\n    return x\n",
        "    const d = {1: 2}\n    var alias = d\n    alias.clear()\n    return x\n",
        "    const d = {1: 2}\n    d.remove(1)\n    return x\n",
        "    var d = {1: 2}\n    return d.get(1)\n",
        "    var d = {1: 2}\n    d.push(3)\n    return x\n",
        "    var d = {1: 2}\n    var bad = d[0..1]\n    return x\n",
        "    var d = {1: 2}\n    d.count = 3\n    return x\n",
        "    var d = {health = 1}\n    return x\n",
        "    var d = {unknown: 1}\n    return x\n"
    })require(!analyze_source({"invalid-dictionary.void",entry+text}).succeeded(),"invalid dictionary accepted");
    for(const auto& text:{
        "struct node:\n    value: i32\nfunc entry(x: i32) -> i32:\n    const d = {1: node()}\n    var n = d[1]\n    n.value = x\n    return x\n",
        "struct node:\n    value: i32\nfunc entry(x: i32) -> i32:\n    const d = {1: [node()]}\n    for items, key in d:\n        items.push(node())\n    return x\n",
        "struct node:\n    value: i32\nfunc entry(x: i32) -> i32:\n    const d = {node(): 1}\n    for item, key in d:\n        key.value = x\n    return x\n",
        "value struct key:\n    value: f64\nfunc entry(x: i32) -> i32:\n    var d: {key: i32}\n    return x\n",
        "interface key:\n    func read() -> i32\nfunc entry(x: i32) -> i32:\n    var d: {key: i32}\n    return x\n"
    })require(!analyze_source({"readonly-dictionary.void",text}).succeeded(),"invalid key/readonly authority accepted");
}
void source(const std::filesystem::path& root,bool optimized){
    for(auto [name,answer]:{std::pair{"main",10},{"keys",7},{"nested",27},{"order",17},{"large",14},{"transactions",14},{"literal",9}}){
        auto program=compile_project(root/"examples/dictionaries"/(std::string(name)+".void"));auto runtime=std::make_shared<Runtime>(1);Jit jit;auto code=jit.compile(program.module,program.module.entry_symbol,optimized,{},runtime);managed_test::Probe probe;probe.inspect_roots=true;
        {managed_test::Observe observe(probe);require(code->invoke(7)==answer,"dictionary source result");}
        require(probe.roots_valid && probe.roots_checked && probe.commits_valid && probe.frames.empty(),"dictionary roots/publication invalid");empty(*runtime);
        if(std::string(name)=="literal"){require(program.module.coordinated && code->invoke(1)==1,"literal-only dictionary Runtime activation");empty(*runtime);}
        if(std::string(name)=="order"){
            for(int mode:{1,2,3,4,5,9}){bool failed=false;try{code->invoke(mode);}catch(const std::runtime_error& e){failed=std::string(e.what()).find("order.void:")!=std::string::npos;}require(failed,"missing checked dictionary failure");empty(*runtime);require(code->invoke(7)==17,"dictionary failure poisoned retry");empty(*runtime);}
            managed_test::Probe effects;effects.capture=true;{managed_test::Observe observe(effects);bool failed=false;try{code->invoke(2);}catch(const std::runtime_error&){failed=true;}require(failed && bool(effects.retained),"dictionary RHS capture missing");}
            {Runtime::Attachment a(*runtime);Runtime::Managed m(*runtime);void* captured=nullptr;auto descriptor=runtime->descriptor("/order.void::state");std::memcpy(&captured,static_cast<const char*>(effects.retained->get())+descriptor->layout.references.at(0),sizeof(captured));constexpr std::size_t zero=0;Runtime::Slot slot{&captured,sizeof(captured),&zero,1};Runtime::Frame roots(*runtime,&slot,1);
                struct Pair{std::int32_t key,value;std::uint64_t hash;};auto type=ir::dictionary_type(ir::Type::i32,ir::Type::i32);auto identity="void.array.element.v1/"+std::to_string(ir::Type::aggregate)+":"+ir::Type::key(ir::array_element(type));NativeArrayField key_field{"",0,0,2,0};NativeArrayElement key{"void.fixed.scalar.v1/2",4,4,1,&key_field};NativeArrayField fields[]={{"",offsetof(Pair,key),0,2,0},{"",offsetof(Pair,value),0,2,0},{"",offsetof(Pair,hash),0,7,0}};NativeArrayElement entry{identity.c_str(),sizeof(Pair),alignof(Pair),3,fields};NativeDictionary metadata{&entry,&key,offsetof(Pair,value),offsetof(Pair,hash)};std::int32_t selected=1;Pair out{};runtime->dictionary_read(&out,runtime->root_word(&captured),metadata,&selected);require(out.value==2,"invalidated dictionary write published RHS value");selected=9;runtime->dictionary_read(&out,runtime->root_word(&captured),metadata,&selected);require(out.value==99 && runtime->dictionary_count(runtime->root_word(&captured),metadata)==3,"failed selected write lost prior RHS effects");}
            effects.retained.reset();empty(*runtime);
            require(code->invoke(6)==0 && code->invoke(8)==1,"dictionary break/return invalidation policy");empty(*runtime);
            ReloadSlot slot(code);auto original=read_source(root/"examples/dictionaries/order.void");auto text=original.text();text.replace(text.rfind("return made[0] + alias[1]"),std::string("return made[0] + alias[1]").size(),"return made[0] + alias[1] + 1");auto changed=analyze_project(root/"examples/dictionaries/order.void",{{"/order.void",SourceFile(original.path(),text)}});if(!changed.succeeded())throw CompilationError(changed.diagnostics);require(jit.replace(slot,*changed.program,optimized) && slot.invoke(7)==18 && code->invoke(7)==17,"dictionary reload/old lease");empty(*runtime);
        }
    }
}
void failures(const std::filesystem::path& root,bool optimized){for(auto name:{"main","transactions"}){
    auto program=compile_project(root/"examples/dictionaries"/(std::string(name)+".void"));auto runtime=std::make_shared<Runtime>(1);Jit jit;auto code=jit.compile(program.module,program.module.entry_symbol,optimized,{},runtime);const int answer=std::string(name)=="main"?10:14;require(code->invoke(7)==answer,"dictionary sweep warmup");empty(*runtime);unsigned failures=0;bool success=false;
    for(int budget=0;budget<16384 && !success;++budget){managed_test::Probe probe;probe.inspect_roots=true;probe.frames.reserve(192);{managed_test::Observe observe(probe);try{managed_test::AllocationFailure inject(budget);require(code->invoke(7)==answer,"dictionary sweep result");success=true;}catch(const std::bad_alloc&){++failures;}catch(const std::runtime_error&){++failures;}}require(probe.roots_valid && probe.commits_valid && probe.frames.empty(),"dictionary failure roots/publication cleanup");empty(*runtime);require(code->invoke(7)==answer,"dictionary allocation failure poisoned retry");empty(*runtime);}
    require(success && failures>0,"dictionary allocating sweep incomplete");std::cout<<name<<(optimized?" O2: ":" normal: ")<<failures<<" allocation failures; cleanup/retry passed\n";
}}
struct Entry {std::uint64_t key;void* value;std::uint64_t hash;};
struct Native {
    Runtime r{1};Runtime::Attachment attachment{r};Runtime::Managed managed{r};static constexpr std::size_t zero=0;void* dict=nullptr;void* item=nullptr;void* fresh=nullptr;
    Runtime::Slot slots[3]={{&dict,sizeof(dict),&zero,1},{&item,sizeof(item),&zero,1},{&fresh,sizeof(fresh),&zero,1}};Runtime::Frame roots{r,slots,3};
    NativeArrayField key_field{"",0,0,7,0};NativeArrayElement key{"void.fixed.scalar.v1/7",8,8,1,&key_field};
    NativeArrayField fields[3]={{"",0,0,7,0},{"dict-node",offsetof(Entry,value),0,11,0},{"",offsetof(Entry,hash),0,7,0}};
    NativeArrayElement entry{"dictionary-native-entry",sizeof(Entry),alignof(Entry),3,fields};NativeDictionary metadata{&entry,&key,offsetof(Entry,value),offsetof(Entry,hash)};
    Native(){NominalDescriptor spec{"dict-node","","node-v1",{8,{}}};{Runtime::Native n(r);r.admit_types({&spec,1});}auto d=r.descriptor("dict-node");auto l=d->layout;l.descriptor=d;std::uint64_t v=42;r.construct_into(&item,l,&v);r.dictionary_new(&dict,metadata);}
    auto token(){return r.root_word(&dict);}void put(std::uint64_t key){Entry value{key,item,0};constexpr std::size_t ref=offsetof(Entry,value);Runtime::Slot slot{&value,sizeof(value),&ref,1};Runtime::Frame input(r,&slot,1);r.dictionary_write(token(),metadata,&value,r.dictionary_count(token(),metadata,true));}
    void finish(){dict=item=fresh=nullptr;require(r.collect()==Runtime::Collection::completed && !r.snapshot().objects,"native dictionary cleanup");}
};
void retirement(const std::filesystem::path& root,bool optimized){
    auto runtime=std::make_shared<Runtime>(1);managed_test::Probe probe;probe.capture=true;std::string entry_key,node_key;
    {auto program=compile_project(root/"examples/dictionaries/nested.void");for(auto t:program.module.nominal_types)if(t.kind==ir::Type::managed_ref && t.definition->name=="node"){t.nullable=true;node_key=ir::Type::key(t);entry_key="void.array.element.v1/"+std::to_string(ir::Type::aggregate)+":"+ir::Type::key(ir::array_element(ir::dictionary_type(ir::Type::string,t)));}Jit jit;auto code=jit.compile(program.module,program.module.entry_symbol,optimized,{},runtime);managed_test::Observe observe(probe);require(code->invoke(7)==27 && bool(probe.retained),"dictionary retirement capture");}
    require(!entry_key.empty(),"missing retained dictionary shape");
    {Runtime::Attachment a(*runtime);Runtime::Managed m(*runtime);require(runtime->collect()==Runtime::Collection::completed && runtime->snapshot().objects,"retirement lost live graph");auto descriptor=runtime->descriptor(node_key);void* dict=nullptr;void* text=nullptr;constexpr std::size_t zero=0;Runtime::Slot slots[]={{&dict,sizeof(dict),&zero,1},{&text,sizeof(text),&zero,1}};Runtime::Frame roots(*runtime,slots,2);std::memcpy(&dict,static_cast<const char*>(probe.retained->get())+descriptor->layout.references.at(1),sizeof(dict));runtime->string_into(&text,"self");
        struct Pair{void* key;void* value;std::uint64_t hash;};NativeArrayField key_field{"",0,0,12,0};std::string string_key="void.array.element.v1/"+std::to_string(ir::Type::string)+":";NativeArrayElement key{string_key.c_str(),sizeof(void*),alignof(void*),1,&key_field};NativeArrayField fields[]={{"",offsetof(Pair,key),0,12,0},{node_key.c_str(),offsetof(Pair,value),0,11,1},{"",offsetof(Pair,hash),0,7,0}};NativeArrayElement entry{entry_key.c_str(),sizeof(Pair),alignof(Pair),3,fields};NativeDictionary metadata{&entry,&key,offsetof(Pair,value),offsetof(Pair,hash)};Pair result{};runtime->dictionary_read(&result,runtime->root_word(&dict),metadata,&text);require(result.value==probe.retained->get(),"retired dictionary metadata/reachability lost");
    }
    {auto program=compile_project(root/"examples/dictionaries/nested.void");Jit jit;auto fresh=jit.compile(program.module,program.module.entry_symbol,optimized,{},runtime);require(fresh->invoke(7)==27,"fresh generation after dictionary retirement");}
    probe.retained.reset();empty(*runtime);
}
void native(){
    {Native f;auto bad=f.metadata;bad.hash_offset=0;RuntimeFailure error;vd_dict_new(&f.fresh,&bad,&error);require(error.message && !f.fresh,"malformed dictionary geometry accepted");f.finish();}
    for(unsigned op=0;op<7;++op){bool success=false;unsigned failures=0;for(int budget=0;budget<2048 && !success;++budget){Native f;for(unsigned n=0;n<(op==1?4u:3u);++n)f.put(n*8);auto before=f.dict;auto count=f.r.dictionary_count(f.token(),f.metadata),version=f.r.dictionary_count(f.token(),f.metadata,true);Entry value{op==3?0u:32u,f.item,0};constexpr std::size_t ref=offsetof(Entry,value);Runtime::Slot slot{&value,sizeof(value),&ref,1};Runtime::Frame input(f.r,&slot,1);std::uint64_t key=8;RuntimeFailure error;
        {managed_test::AllocationFailure inject(budget);if(op==0)vd_dict_new(&f.fresh,&f.metadata,&error);else if(op<4)vd_dict_write(&f.dict,&f.metadata,&value,version,&error);else if(op==4)vd_dict_remove(&f.dict,&f.metadata,&key,&error);else if(op==5)vd_dict_clear(&f.dict,&f.metadata,&error);else vd_dict_count(&f.dict,&f.metadata,0,&error);}
        if(error.message){++failures;require(!f.fresh && f.dict==before && f.r.dictionary_count(f.token(),f.metadata)==count && f.r.dictionary_count(f.token(),f.metadata,true)==version,"failed dictionary operation published state");}else {success=true;require(f.dict==before,"dictionary identity changed");require(f.r.dictionary_count(f.token(),f.metadata)==(op==1||op==2?count+1:op==4?count-1:op==5?0:count),"native dictionary count");require(f.r.dictionary_count(f.token(),f.metadata,true)==version+(op==1||op==2||op==4||op==5),"native dictionary version");}
        require(f.r.collect()==Runtime::Collection::completed,"dictionary collection after live transaction");if(error.message || (op!=4 && op!=5)){for(std::uint64_t n=0;n<count;++n){auto k=n*8;Entry out{};f.r.dictionary_read(&out,f.token(),f.metadata,&k);require(out.key==k && out.value==f.item,"dictionary collision/element integrity");}}value.value=nullptr;f.finish();
    }require(success && ((op!=0 && op!=1) || failures>0),"native dictionary required allocation sweep incomplete");std::cout<<"native dictionary "<<op<<": "<<failures<<" failures; rollback/version/collection passed\n";}
    Native f;f.put(0);auto version=f.r.dictionary_count(f.token(),f.metadata,true);f.r.dictionary_clear(f.token(),f.metadata);f.r.dictionary_clear(f.token(),f.metadata);require(f.r.dictionary_count(f.token(),f.metadata,true)==version+1,"empty clear changed dictionary version");f.item=nullptr;require(f.r.collect()==Runtime::Collection::completed && f.r.snapshot().objects==3,"dictionary clear retained removed references or lost capacity");f.finish();
}
}
int main(int argc,char** argv){try{require(argc==2,"expected repository root");diagnostics();native();for(bool optimized:{false,true}){source(argv[1],optimized);failures(argv[1],optimized);retirement(argv[1],optimized);}std::cout<<"Dictionaries: source/GC/order/readonly/rollback/reload passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
