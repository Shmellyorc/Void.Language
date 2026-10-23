#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/runtime.hpp"
#include "void/runtime_abi.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace {
unsigned checks=0;
void check(bool value,const std::string& why){if(!value)throw std::runtime_error(why);++checks;}
const std::string entry="func entry(input: i32) -> i32:\n";
std::string owner(std::string kind="value struct",std::string modifier="",std::string right="item"){
    return kind+" item:\n    value: i32\n    "+modifier+"operator ==(const left: item, const right: "+right+") -> bool:\n        return false\n";
}
auto analyze(const std::string& source){auto result=vd::analyze_source({"/keys.void",source});if(!result.succeeded())throw vd::CompilationError(result.diagnostics);++checks;return result.program;}
void reject(const std::string& source,bool equality=true){auto result=vd::analyze_source({"/keys.void",source});check(!result.succeeded(),"ineligible key accepted: "+source);if(equality){bool found=false;for(const auto& d:result.diagnostics)if(d.description.find("custom operator ==")!=std::string::npos){for(const auto& r:d.related)found|=r.span.source->text().substr(r.span.begin,r.span.end-r.span.begin)=="==";}check(found,"missing equality declaration diagnostic: "+source);}}
template<class F>void invalid(F action,const std::string& why,const std::string& message=""){bool rejected=false;try{action();}catch(const std::invalid_argument& e){rejected=std::string(e.what()).find(message)!=std::string::npos;if(!rejected)throw std::runtime_error(why+": "+e.what());}check(rejected,why);}
void source(){
    for(auto kind:{"value struct","struct"})for(auto modifier:{"","private ","internal "})for(auto right:{"item","i32"}){
        auto prefix=owner(kind,modifier,right);reject(prefix+entry+"    var bad: {item: i32}\n    return input\n");
        reject(prefix+entry+"    var bad = {item(value = input): 1}\n    return input\n");
    }
    reject(owner("struct")+"value struct nested:\n    private secret: item?\n"+entry+"    var bad: {nested: i32}\n    return input\n");
    reject(owner("struct")+entry+"    var bad: {item?: i32}\n    return input\n");
    reject(owner()+"value struct nested:\n    private secret: item\nvalue struct outer:\n    private inside: nested\n"+entry+"    var bad: {outer: i32}\n    return input\n");
    for(auto extent:{"0","3"}){
        reject(owner()+entry+"    var bad: {[item, "+extent+"]: i32}\n    return input\n");
        reject(owner()+"value struct nested:\n    private values: [item, "+extent+"]\n"+entry+"    var bad: {nested: i32}\n    return input\n");
    }
    const std::string generic="value struct box<t>:\n    value: t\n    operator ==(const left: box<t>, const right: box<t>) -> bool:\n        return false\n";
    reject("struct box<t>:\n    value: t\n    operator ==(const left: box<t>, const right: box<t>) -> bool:\n        return false\n"+entry+"    var bad: {box<i32>?: i32}\n    return input\n");
    reject(generic+entry+"    var bad: {box<i32>: i32}\n    return input\n");
    reject(generic+"value struct outer<t>:\n    private value: t\n"+entry+"    var bad: {outer<box<i32>>: i32}\n    return input\n");
    reject(owner()+"func make<t>(value: t) -> i32:\n    var bad: {t: i32}\n    return 0\n"+entry+"    return make(item())\n");
    reject(entry+"    var bad: {[i32, 3]: i32}\n    return input\n",false);
    for(auto type:{"i8","i16","i32","i64","u8","u16","u32","u64","bool","string"})check(vd::analyze_source({"/keys.void",entry+"    var legal: {"+type+": i32}\n    return input\n"}).succeeded(),"previous scalar key rejected");
    analyze(owner()+entry+"    var values: {i32: item} = {1: item(value = input)}\n    return values[1].value\n");
    analyze(owner()+"value struct wrapper:\n    private payload: item\n"+entry+"    var values: {string: wrapper}\n    return input\n");
}
void imported(const std::filesystem::path& root){
    auto path=root/"tests/stage10/operator_key_project.void";vd::SourceOverrides changes;
    changes.emplace("/operator_key_project.void",vd::SourceFile(path.string(),"use \"owner.void\" as owner\n"+entry+"    var bad: {owner.item: i32}\n    return input\n"));
    changes.emplace("/owner.void",vd::SourceFile("owner.void",owner("struct","private ")));
    auto result=vd::analyze_project(path,changes);check(!result.succeeded(),"foreign private equality allowed identity key");bool found=false;for(const auto& d:result.diagnostics)for(const auto& r:d.related)found|=r.span.source->text().substr(r.span.begin,r.span.end-r.span.begin)=="==";check(found,"foreign equality declaration reference missing");
}
void forged(){
    for(unsigned variant=0;variant<4;++variant){auto module=analyze(owner()+entry+"    var value = item(value = input)\n    return value.value\n")->lower();vd::ir::Type item;for(auto type:module.nominal_types)if(type.definition->name=="item")item=type;check(bool(item.definition),"missing owner");
        if(variant==0 || variant==3)module.coordinated=true;
        if(variant==0)module.functions.back().locals.push_back(vd::ir::dictionary_type(item,vd::ir::Type::i32));
        else if(variant==1)const_cast<vd::ir::ValueType*>(item.definition.get())->custom_equality=false;
        else if(variant==2){module.functions.erase(std::remove_if(module.functions.begin(),module.functions.end(),[](const auto& f){return f.operator_glyph=="==";}),module.functions.end());}
        else {auto nested=std::make_shared<vd::ir::ValueType>();nested->name=nested->identity="forged-wrapper";nested->fields={{"private",vd::ir::FieldType(item)}};module.functions.back().locals.push_back(vd::ir::dictionary_type(vd::ir::Type(nested),vd::ir::Type::i32));}
        invalid([&]{vd::ir::validate(module);},"forged equality/key metadata accepted",variant==0 || variant==3 ? "invalid dictionary key/value shape" : "custom equality metadata disagrees");
    }
    auto module=analyze("value struct item:\n    value: i32\n"+entry+"    return item(value = input).value\n")->lower();for(auto type:module.nominal_types)if(type.definition->name=="item")const_cast<vd::ir::ValueType*>(type.definition.get())->custom_equality=true;invalid([&]{vd::ir::validate(module);},"forged equality declaration accepted","custom equality metadata disagrees");
}
vd::ElementSchemaSpec schema(std::string identity,bool key){vd::ElementSchemaSpec spec;spec.identity=identity;spec.capability_profile="mutable";spec.lowering_layout_key="i32";spec.storage_domain=vd::StorageDomain::native();spec.nodes={vd::SchemaScalar{vd::ScalarEncoding::i32}};spec.dictionary_key_use=key;return spec;}
void native(){
    vd::Runtime runtime;vd::Runtime::Attachment attached(runtime);vd::Runtime::Managed managed(runtime);
    vd::NominalDescriptor policy{"custom","","shape",{4,{}}};policy.dictionary_key_schema="custom-key";policy.dictionary_key_forbidden=true;
    {vd::Runtime::Native native(runtime);for(unsigned flag:{1u,2u}){vd::RuntimeTypeRecord record{"bad-policy","","shape",4,nullptr,0,nullptr,flag};vd::RuntimeFailure failure;vd_runtime_admit(&runtime,&record,1,&failure);check(failure.message,"malformed Runtime ABI key policy admitted");}}
    auto bad=schema("custom-key",true);auto before=runtime.memory_snapshot();
    {vd::Runtime::Native native(runtime);invalid([&]{runtime.admit_metadata({&policy,1},{},{&bad,1});},"Runtime key schema accepted custom equality");}
    auto after=runtime.memory_snapshot();check(before.types==after.types && before.schemas==after.schemas,"rejected metadata published a partial catalog");
    auto value=schema("custom-key",false);{vd::Runtime::Native native(runtime);runtime.admit_metadata({&policy,1},{},{&value,1});}++checks;
    {vd::Runtime::Native native(runtime);invalid([&]{runtime.admit_metadata({},{},{&bad,1});},"existing value schema bypassed key admission");auto changed=policy;changed.dictionary_key_forbidden=false;invalid([&]{runtime.admit_types({&changed,1});},"Runtime policy downgrade accepted");}
    auto leaf=schema("managed-wrapper",true);leaf.nodes={vd::SchemaManaged{"custom",true},vd::SchemaRecord{sizeof(void*),alignof(void*),{{1,0,0}}}};leaf.root=1;leaf.lowering_layout_key="pointer-record";
    {vd::Runtime::Native native(runtime);invalid([&]{runtime.admit_metadata({},{},{&leaf,1});},"Runtime managed-leaf key admitted");leaf.dictionary_key_use=false;runtime.admit_metadata({},{},{&leaf,1});}++checks;
    {vd::Runtime::Native native(runtime);auto fixed=schema("fixed-key",true);fixed.nodes={vd::SchemaOwnedFixed{"custom-key",0,false}};invalid([&]{runtime.admit_metadata({},{},{&fixed,1});},"unsupported fixed key schema admitted");auto floating=schema("float-key",true);floating.nodes={vd::SchemaScalar{vd::ScalarEncoding::f64}};invalid([&]{runtime.admit_metadata({},{},{&floating,1});},"unsupported floating key schema admitted");}
    {vd::Runtime::Native native(runtime);auto native_policy=policy;native_policy.identity="native-custom";native_policy.dictionary_key_schema="native-key";runtime.admit_types({&native_policy,1});}
    struct Entry{std::int32_t key,value;std::uint64_t hash;};vd::NativeArrayField scalar{"",0,0,2,0};vd::NativeArrayElement key{"native-key",4,4,1,&scalar};vd::NativeArrayField fields[]={{"",0,0,2,0},{"",4,0,2,0},{"",8,0,7,0}};vd::NativeArrayElement pair{"custom-entry",sizeof(Entry),alignof(Entry),3,fields};vd::NativeDictionary dictionary{&pair,&key,4,8};void* result=nullptr;constexpr std::size_t zero=0;vd::Runtime::Slot slot{&result,sizeof(result),&zero,1};vd::Runtime::Frame root(runtime,&slot,1);invalid([&]{runtime.dictionary_new(&result,dictionary);},"Runtime native dictionary admitted forbidden key","custom equality type");check(!result,"failed native key published dictionary");
}
}
int main(int argc,char** argv){try{check(argc==2,"expected repository root");source();imported(argv[1]);forged();native();auto result=vd::analyze_project(std::filesystem::path(argv[1])/"tests/stage10/operator_keys.void");if(!result.succeeded())throw vd::CompilationError(result.diagnostics);for(bool optimized:{false,true}){vd::Jit jit;auto module=result.program->lower();auto runtime=std::make_shared<vd::Runtime>();auto code=jit.compile(module,module.entry_symbol,optimized,{},runtime);for(int input:{0,4,-3})check(code->invoke(input)==input,"legal key/value/pattern regression");for(auto type:module.nominal_types)if(type.kind==vd::ir::Type::aggregate || type.kind==vd::ir::Type::managed_ref){auto descriptor=runtime->descriptor(vd::ir::Type::key(type));check(descriptor && !descriptor->dictionary_key_schema.empty() && descriptor->dictionary_key_forbidden==vd::ir::dictionary_equality_owner(type).has_value(),"JIT catalog lost nominal/transitive key policy");}}std::cout<<checks<<" dictionary custom-equality checks passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
