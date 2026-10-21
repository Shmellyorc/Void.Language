#include "void/native_binding.hpp"
#include <stdexcept>
#include <set>
namespace vd {
const std::vector<NativeBinding>& native_fixture_catalog(){
    static const std::vector<NativeBinding> catalog=[] {
        std::vector<NativeBinding> out;for(auto t:{"i8","i16","i32","i64","u8","u16","u32","u64","f32","f64","bool"})out.push_back({std::string("scalar_")+t,std::string("vd_native_fixture_call#")+t,{t},t});
        for(auto b:std::vector<NativeBinding>{
            {"terminated","vd_native_fixture_call#terminated",{"string"},"u64"},
            {"length","vd_native_fixture_call#length",{"string"},"u64"},
            {"copy","vd_native_fixture_call#copy",{"string"},"string"},
            {"acquire","vd_native_fixture_call#acquire",{"bool"},"resource?"},
            {"close","vd_native_fixture_call#close",{"resource"},""},
            {"destroy","vd_native_fixture_call#destroy",{"resource"},""},
            {"retain","vd_native_fixture_call#retain",{"resource","string"},""},
            {"byte","vd_native_fixture_call#byte",{"resource","u64"},"i32"},
            {"release_text","vd_native_fixture_call#release_text",{"resource"},""},
            {"register","vd_native_fixture_call#register",{"resource","func(i32) -> i32"},""},
            {"invoke","vd_native_fixture_call#invoke",{"resource","i32"},"i32"},
            {"foreign","vd_native_fixture_call#foreign",{"resource","i32"},"i32"},
            {"unregister","vd_native_fixture_call#unregister",{"resource"},""}
        })out.push_back(std::move(b));
        for(unsigned n=11;n<out.size();++n){out[n].owning_thread=n>=14;out[n].blocking=n==15 || n==16 || n==21 || n==22 || n==23;out[n].callbacks=n==21 || n==22;}
        return out;
    }();return catalog;
}
const std::vector<NativeBinding>& native_sink_catalog(){
    static const auto catalog=[] {
        std::vector<NativeBinding> out;
        for(auto index:{2u,13u,14u,15u,16u,17u,18u,19u,20u,21u,23u}) {
            auto b=native_fixture_catalog().at(index);
            b.symbol="vd_native_sink_call#"+b.name;
            if(index==2){b.name="echo";b.symbol="vd_native_sink_call#echo";}
            out.push_back(std::move(b));
        }
        return out;
    }();return catalog;
}
const std::vector<NativeBinding>& native_catalog(std::string_view library){
    if(library=="void.fixture.v1")return native_fixture_catalog();
    if(library=="void.sink.v1")return native_sink_catalog();
    throw std::invalid_argument("Unsupported trusted native library");
}
std::string_view native_provider(std::string_view symbol){
    for(auto library:{"void.fixture.v1","void.sink.v1"})for(const auto& b:native_catalog(library))if(b.symbol==symbol)return library;
    throw std::invalid_argument("Unlinked or unknown trusted native symbol");
}
const NativeBinding& native_binding(std::string_view symbol){for(const auto& b:native_catalog(native_provider(symbol)))if(b.symbol==symbol)return b;throw std::logic_error("Native catalog invariant");}
unsigned native_operation(std::string_view symbol){
    const auto& binding=native_binding(symbol);
    if(binding.name=="echo")return 2;
    const auto& catalog=native_fixture_catalog();for(unsigned n=0;n<catalog.size();++n)if(catalog[n].name==binding.name)return n;
    throw std::logic_error("Native operation invariant");
}
NativeProject native_fixture_project(std::string module){NativeProject p;p.module=std::move(module);p.bindings=native_fixture_catalog();return p;}
NativeProject native_sink_project(std::string module){NativeProject p;p.library="void.sink.v1";p.module=std::move(module);p.bindings=native_sink_catalog();return p;}
void validate_native_project(const NativeProject& p){(void)native_catalog(p.library);std::set<std::string> names;
    for(const auto& b:p.bindings){const auto& known=native_binding(b.symbol);if(native_provider(b.symbol)!=p.library || !names.insert(b.name).second || b.name!=known.name || b.parameters!=known.parameters || b.result!=known.result || b.owning_thread!=known.owning_thread || b.blocking!=known.blocking || b.callbacks!=known.callbacks)throw std::invalid_argument("Native binding ABI/signature mismatch");}
}
std::string native_project_source(const NativeProject& p){validate_native_project(p);std::string text="# Explicit trusted compiled fixture descriptor v1\nstruct resource:\n    token: u64 = 0\n";
    for(const auto& b:p.bindings){text+="func "+b.name+"(";for(unsigned n=0;n<b.parameters.size();++n){if(n)text+=", ";text+="arg"+std::to_string(n)+": "+b.parameters[n];}text+=")";if(!b.result.empty())text+=" -> "+b.result;text+=":\n    pass\n";}return text;
}
bool native_signature(std::string_view symbol,const std::vector<ir::Type>& parameters,ir::Type result){
    const auto& b=native_binding(symbol);if(parameters.size()!=b.parameters.size())return false;
    auto matches=[&](ir::Type t,const std::string& s){if(s.empty())return t==ir::Type::none;if(s=="resource" || s=="resource?")return t.kind==ir::Type::managed_ref && t.definition && t.definition->native_resource && t.definition->native_provider==native_provider(symbol) && t.nullable==(s=="resource?") && !t.construction;
        if(s=="func(i32) -> i32")return t.kind==ir::Type::aggregate && !t.nullable && t.definition && t.definition->callable && t.definition->call_parameters.size()==1 && ir::Type(t.definition->call_parameters[0])==ir::Type::i32 && ir::Type(*t.definition->call_result)==ir::Type::i32;
        for(auto [name,kind]:{std::pair{"i8",ir::Type::i8}, {"i16",ir::Type::i16},{"i32",ir::Type::i32},{"i64",ir::Type::i64},{"u8",ir::Type::u8},{"u16",ir::Type::u16},{"u32",ir::Type::u32},{"u64",ir::Type::u64},{"f32",ir::Type::f32},{"f64",ir::Type::f64},{"bool",ir::Type::boolean},{"string",ir::Type::string}})if(s==name)return t.kind==kind && !t.nullable && !t.construction;
        return false;};
    if(!matches(result,b.result))return false;for(unsigned n=0;n<b.parameters.size();++n)if(!matches(parameters[n],b.parameters[n]))return false;return true;
}
}
