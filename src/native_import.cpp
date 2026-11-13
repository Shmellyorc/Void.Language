#include "void/native_import.hpp"
#include "void/c_primitive.hpp"
#include "void/path.hpp"
#include "frontend/project_files.hpp"
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/JSON.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/SHA256.h>
#include <llvm/Support/raw_ostream.h>
#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <fstream>
#include <functional>
#include <locale>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>

namespace vd {
namespace {
namespace fs=std::filesystem;
using Object=llvm::json::Object;
using Array=llvm::json::Array;
constexpr std::size_t header_limit=16*1024*1024, tool_limit=64*1024*1024;
std::string str(const Object& o,llvm::StringRef key) {return o.getString(key).value_or("").str();}
const Array& children(const Object& o) {static const Array empty;auto* a=o.getArray("inner");return a?*a:empty;}
std::string type(const Object& o,bool canonical=true) {
    auto* t=o.getObject("type");if(!t)return {};
    auto d=str(*t,"desugaredQualType");return canonical&&!d.empty()?d:str(*t,"qualType");
}
std::string hash(std::string_view s) {
    auto h=llvm::SHA256::hash({reinterpret_cast<const std::uint8_t*>(s.data()),s.size()});
    constexpr char digits[]="0123456789abcdef";std::string r;for(auto c:h){r+=digits[c>>4];r+=digits[c&15];}return r;
}
void write(const fs::path& p,std::string_view bytes) {
    std::ofstream file(p,std::ios::binary);file.write(bytes.data(),bytes.size());file.close();
    if(!file)throw std::runtime_error("Cannot write importer scratch: "+path_utf8(p));
}
struct Scratch {
    fs::path path;
    Scratch(){llvm::SmallString<128> p;if(auto e=llvm::sys::fs::createUniqueDirectory("void-native-import",p))throw std::system_error(e);path=utf8_path(p.str().str());}
    ~Scratch(){std::error_code ec;fs::remove_all(path,ec);}
};
struct Clang {
    fs::path driver,work;std::vector<std::string> base;
    std::string run(const std::vector<std::string>& args,bool allow_errors=false) const {
        std::vector<std::string> all{path_utf8(driver)};all.insert(all.end(),args.begin(),args.end());
        std::vector<llvm::StringRef> refs(all.begin(),all.end());auto output=path_utf8(work/"stdout"),errors=path_utf8(work/"stderr");
        // LLVM subprocess redirects may retain a longer previous file tail.
        fs::remove(work/"stdout");fs::remove(work/"stderr");
        std::vector<std::optional<llvm::StringRef>> redirects{std::nullopt,output,errors};std::string error;bool failed=false;
        auto status=llvm::sys::ExecuteAndWait(all[0],refs,std::nullopt,redirects,120,0,&error,&failed);
        auto text=project::read_project_input(work/"stdout",tool_limit);
        auto diagnostics=project::read_project_input(work/"stderr",tool_limit);
        if(failed||status<0||(!allow_errors&&status))throw std::runtime_error("Clang header import failed: "+error+diagnostics);
        return text;
    }
    llvm::json::Value ast(const fs::path& input,bool allow_errors=false) const {
        auto args=base;args.insert(args.end(),{"-fsyntax-only","-ferror-limit=0","-Xclang","-ast-dump=json",path_utf8(input)});
        auto text=run(args,allow_errors);auto value=llvm::json::parse(text);if(!value)throw std::runtime_error("Cannot read Clang AST for "+path_utf8(input)+": "+llvm::toString(value.takeError()));return std::move(*value);
    }
};
std::string include_path(const fs::path& p) {
    auto s=path_utf8(p);std::replace(s.begin(),s.end(),'\\','/');
    if(s.find_first_of("\"\r\n")!=s.npos)throw std::invalid_argument("Header path cannot contain quotes or line breaks");return s;
}
std::string identifier(std::string name) {
    std::string out;
    for(std::size_t i=0;i<name.size();++i) {
        auto c=name[i];if(c>='A'&&c<='Z') {
            if(i&&((name[i-1]>='a'&&name[i-1]<='z')||(name[i-1]>='0'&&name[i-1]<='9')||
                (i+1<name.size()&&name[i+1]>='a'&&name[i+1]<='z'&&name[i-1]>='A'&&name[i-1]<='Z')))out+='_';
            out+=char(c-'A'+'a');
        } else out+=c;
    }
    static const std::set<std::string> reserved{"native","main","func","var","const","use","as","if","else","elif","for","while","return","pass","defer","private","public","internal","struct","value","enum","union","async","await","yield","break","continue","in","is","and","or","not","true","false","none","string","bool","i8","i16","i32","i64","u8","u16","u32","u64","f32","f64","result","task"};
    if(out.empty()||out[0]=='_'||reserved.contains(out))out="c"+out;
    if(!std::regex_match(out,std::regex("[a-z][a-z0-9_]{0,63}")))throw std::runtime_error("Name is outside the existing descriptor identifier profile: "+name);
    return out;
}
std::optional<std::string> value(const Object& node) {
    if(str(node,"kind")=="ConstantExpr"&&!str(node,"value").empty())return str(node,"value");
    for(auto& v:children(node))if(auto* o=v.getAsObject())if(auto found=value(*o))return found;
    return {};
}
struct Shape {std::string descriptor,source,c;bool exact=true;unsigned size=0,align=0;};
struct Record {const Object* node;std::string name,c,original,descriptor;unsigned size=0,align=0;std::vector<std::pair<std::string,Shape>> fields;};
struct Layout {unsigned size=0,align=0;std::vector<unsigned> offsets;};
struct Importer {
    const NativeImportOptions& options;const Clang& clang;fs::path header;std::string bytes,preamble,prefix;
    Importer(const NativeImportOptions& o,const Clang& c,fs::path h,std::string b,std::string p,std::string d)
        :options(o),clang(c),header(std::move(h)),bytes(std::move(b)),preamble(std::move(p)),prefix(std::move(d)){}
    Array coverage;std::map<std::string,std::string> aliases,enums;std::map<std::string,Record> records;
    std::map<std::string,Layout> layouts;std::vector<const Object*> declarations;
    std::map<std::string,std::string> record_alias;std::set<std::string> names;
    std::string record_text,header_text,forwarders,api,constants,export_macro;unsigned bindings=0;
    std::set<std::string> c_names;
    void identifiers(const Object& n) {
        if(auto name=str(n,"name");!name.empty())c_names.insert(name);
        for(auto& v:children(n))if(auto* child=v.getAsObject())identifiers(*child);
    }
    std::string unique_c(const std::string& stem,bool type_pair=false) {
        auto name=stem;unsigned suffix=0;
        while(c_names.contains(name)||(type_pair&&c_names.contains(name+"_type")))name=stem+"_"+std::to_string(++suffix);
        c_names.insert(name);if(type_pair)c_names.insert(name+"_type");return name;
    }
    Object location(const Object& node) const {
        auto* loc=node.getObject("loc");if(loc)if(auto* e=loc->getObject("expansionLoc"))loc=e;
        auto offset=loc?loc->getInteger("offset").value_or(0):0;offset=std::clamp<std::int64_t>(offset,0,bytes.size());
        auto line=1+std::count(bytes.begin(),bytes.begin()+offset,'\n');auto previous=bytes.rfind('\n',offset?offset-1:0);
        return Object{{"file",path_utf8(header)},{"line",line},{"column",offset-(previous==bytes.npos?-1:static_cast<std::int64_t>(previous))},{"offset",offset}};
    }
    bool primary(const Object& node) const {
        auto* loc=node.getObject("loc");if(!loc)return false;if(auto* e=loc->getObject("expansionLoc"))loc=e;
        auto file=str(*loc,"file");return !loc->getObject("includedFrom")&&(file.empty()||utf8_path(file).lexically_normal()==header);
    }
    void report(const Object& n,std::string category,std::string reason,std::string generated={}) {
        Object row{{"kind",str(n,"kind")},{"name",str(n,"name")},{"c_type",type(n,false)},{"location",location(n)},{"category",std::move(category)},{"reason",std::move(reason)}};
        if(!generated.empty())row["void_name"]=generated;
        if(auto* range=n.getObject("range")) {
            auto* begin=range->getObject("begin");auto* end=range->getObject("end");
            if(begin&&end) {
                if(auto* e=begin->getObject("expansionLoc"))begin=e;if(auto* e=end->getObject("expansionLoc"))end=e;
                auto a=begin->getInteger("offset"),b=end->getInteger("offset");
                if(a&&b&&*a>=0&&*b>=*a&&static_cast<std::size_t>(*b)<bytes.size())row["c_declaration"]=bytes.substr(*a,*b-*a+end->getInteger("tokLen").value_or(1));
            }
        }
        coverage.emplace_back(std::move(row));
    }
    std::string resolve(std::string t) const {
        for(unsigned i=0;i<64;++i) {auto it=aliases.find(t);if(it==aliases.end()||it->second==t)break;t=it->second;}
        return t;
    }
    Shape shape(std::string t,bool field=false) const {
        auto original=t;t=resolve(t);
        if(t.starts_with("const ")) {if(field)throw std::runtime_error("const record fields need a reviewed value-copy adapter; descriptor fields are unqualified");t.erase(0,6);}
        if(t.find("volatile")!=t.npos||t.find("_Atomic")!=t.npos)throw std::runtime_error("volatile/atomic values need an explicitly reviewed synchronization adapter");
        if(t.find('*')!=t.npos||t.find('&')!=t.npos||t.find('(')!=t.npos)throw std::runtime_error("pointer/reference/callback ownership, direction, extent and lifetime are unresolved; review a G3/G4 resource/span or G5 callback adapter");
        std::smatch array;
        if(std::regex_match(t,array,std::regex("(.+)\\[([0-9]+)\\]"))) {
            if(!field)throw std::runtime_error("arrays are only admitted as fixed record fields");
            auto element=shape(array[1].str(),true);auto count=std::stoul(array[2].str());
            if(!count||count>32||element.descriptor.starts_with("array"))throw std::runtime_error("fixed arrays require one dimension and 1..32 elements");
            return {(element.descriptor.starts_with("record:")?"array_record:"+element.descriptor.substr(7):"array:"+element.descriptor)+":"+std::to_string(count)+":"+std::to_string(element.size),"["+element.source+", "+std::to_string(count)+"]",{},element.exact,element.size*static_cast<unsigned>(count),element.align};
        }
        if(t.find('[')!=t.npos)throw std::runtime_error("flexible, variable or multidimensional arrays are outside the existing fixed-array ABI");
        if(auto it=records.find(t);it!=records.end())return {"record:"+it->second.name,"native."+it->second.name,it->second.c,true,it->second.size,it->second.align};
        if(auto it=enums.find(t);it!=enums.end()) {auto s=shape(it->second,field);if(field&&options.language=="c++")s.exact=false;return s;}
        const bool win=prefix.find("msvc-x64")!=prefix.npos;
        const std::map<std::string,Shape> scalars{
            {"void",{"void","void","void",true,0,1}},
            {"char",{"c_char","i8","char",true,1,1}},
            {"signed char",{"i8","i8","int8_t",true,1,1}},{"unsigned char",{"u8","u8","uint8_t",true,1,1}},
            {"short",{"i16","i16","int16_t",true,2,2}},{"unsigned short",{"u16","u16","uint16_t",true,2,2}},
            {"int",{"c_int","i32","int",true,4,4}},{"unsigned int",{"u32","u32","uint32_t",true,4,4}},
            {"long",{"c_long",win?"i32":"i64","long",true,win?4u:8u,win?4u:8u}},
            {"unsigned long",{win?"u32":"u64",win?"u32":"u64",win?"uint32_t":"uint64_t",!win,win?4u:8u,win?4u:8u}},
            {"long long",{"i64","i64","int64_t",win,8,8}},{"unsigned long long",{"u64","u64","uint64_t",win,8,8}},
            {"float",{"f32","f32","float",true,4,4}},{"double",{"f64","f64","double",true,8,8}},
            {"_Bool",{"c_bool","bool","_Bool",true,1,1}},{"bool",{"c_bool","bool","_Bool",true,1,1}}};
        if(auto it=scalars.find(t);it!=scalars.end())return it->second;
        throw std::runtime_error("type '"+original+"' is not an admitted scalar or completed compatible POD record; opaque handles require an explicit acquire/release contract");
    }
    bool reserve(const std::string& n) {return names.insert(n).second;}
    void collect(const Object& n,bool c_linkage=false,bool forbidden=false) {
        auto kind=str(n,"kind");
        if(kind=="TranslationUnitDecl") {for(auto& v:children(n))if(auto* o=v.getAsObject())collect(*o);return;}
        // Resolve scalar typedefs from included headers, without importing their APIs.
        if(kind=="TypedefDecl"&&!forbidden)aliases[str(n,"name")]=type(n);
        if(!primary(n)||n.getBoolean("isImplicit").value_or(false))return;
        if(kind=="LinkageSpecDecl") {for(auto& v:children(n))if(auto* o=v.getAsObject())collect(*o,str(n,"language")=="C",forbidden);return;}
        if(kind=="NamespaceDecl"||kind.find("Template")!=kind.npos) {
            report(n,"unsupported","C++ namespaces/templates require an explicit non-overloaded extern C interface");
            for(auto& v:children(n))if(auto* o=v.getAsObject())collect(*o,c_linkage,true);return;
        }
        if(forbidden) {report(n,"unsupported","declaration belongs to a C++ namespace/template; supply an explicit C-compatible wrapper");return;}
        if(kind=="FunctionDecl"&&options.language=="c++"&&!c_linkage) {report(n,"unsupported","C++ functions/overloads require explicit extern C linkage and no escaping exceptions");return;}
        declarations.push_back(&n);
        if(kind=="TypedefDecl"||kind=="TypeAliasDecl") {
            aliases[str(n,"name")]=type(n);
            // Anonymous typedef-owned records have no tag in RecordDecl.
            std::function<void(const Object&)> visit=[&](const Object& o) {
                if(auto* d=o.getObject("decl"))if(str(*d,"kind")=="RecordDecl"||str(*d,"kind")=="CXXRecordDecl")record_alias[str(*d,"id")]=str(n,"name");
                if(auto* d=o.getObject("ownedTagDecl"))record_alias[str(*d,"id")]=str(n,"name");
                for(auto& v:children(o))if(auto* child=v.getAsObject())visit(*child);
            };visit(n);
        }
    }
    void load_layouts(std::string text) {
        std::istringstream lines(text);std::string line,current;
        while(std::getline(lines,line)) {
            std::smatch m;
            if(std::regex_match(line,m,std::regex("\\s*0 \\| (struct|class|union) (.+)")))current=m[2].str();
            else if(!current.empty()&&std::regex_match(line,m,std::regex("\\s*([0-9]+) \\|   \\S.*")))layouts[current].offsets.push_back(std::stoul(m[1].str()));
            else if(!current.empty()&&std::regex_search(line,m,std::regex("\\[sizeof=([0-9]+),.*align=([0-9]+)"))) {layouts[current].size=std::stoul(m[1].str());layouts[current].align=std::stoul(m[2].str());current.clear();}
        }
    }
    void enum_types() {
        std::string code=preamble;std::vector<std::pair<std::string,std::string>> keys;
        for(auto* n:declarations)if(str(*n,"kind")=="EnumDecl"&&!str(*n,"name").empty()) {
            auto c=(options.language=="c"?"enum ":"")+str(*n,"name"),key=unique_c("vdi_enum_"+std::to_string(keys.size()));keys.emplace_back(c,key);
            if(options.language=="c++")code+="typedef __underlying_type("+c+") "+key+";\n";
            else code+="typedef __typeof__(_Generic(("+c+")0, signed char:(signed char)0, unsigned char:(unsigned char)0, short:(short)0, unsigned short:(unsigned short)0, int:(int)0, unsigned int:(unsigned int)0, long:(long)0, unsigned long:(unsigned long)0, long long:(long long)0, unsigned long long:(unsigned long long)0, default:(void*)0)) "+key+";\n";
        }
        if(keys.empty())return;auto probe=clang.work/"enum-probe.h";write(probe,code);auto ast=clang.ast(probe);
        for(auto& v:children(*ast.getAsObject()))if(auto* n=v.getAsObject())for(auto& [c,key]:keys)if(str(*n,"name")==key)enums[c]=type(*n);
    }
    void emit_records() {
        std::set<const Object*> pending;
        for(auto* n:declarations)if(str(*n,"kind")=="RecordDecl"||str(*n,"kind")=="CXXRecordDecl")pending.insert(n);
        // Iterate in source order, postponing records whose value dependencies
        // appear later. The descriptor parser remains the layout authority.
        for(std::size_t round=0;round<=declarations.size()&&!pending.empty();++round) {
            bool progress=false;
            for(auto* n:declarations)if(pending.contains(n)) {
                auto original=str(*n,"name");if(original.empty())original=record_alias[str(*n,"id")];
                try {
                    if(str(*n,"tagUsed")!="struct"||!n->getBoolean("completeDefinition").value_or(false))throw std::runtime_error("union/class/incomplete record: provide an explicit C POD or opaque-owner adapter");
                    for(auto& v:children(*n))if(auto* child=v.getAsObject()) {
                        auto kind=str(*child,"kind");
                        if((kind=="CXXMethodDecl"||kind=="CXXConstructorDecl"||kind=="CXXDestructorDecl")&&!child->getBoolean("isImplicit").value_or(false))throw std::runtime_error("C++ methods/constructors/destructors require an explicit C wrapper; exceptions cannot cross the native boundary");
                        if(kind=="FieldDecl"&&(child->getBoolean("isBitfield").value_or(false)||str(*child,"name").empty()))throw std::runtime_error("bitfields/anonymous fields are outside the descriptor ABI");
                    }
                    if(auto* data=n->getObject("definitionData"))if(!data->getBoolean("isPOD").value_or(false))throw std::runtime_error("non-POD C++ layout needs an explicit C-compatible record");
                    auto layout=layouts.find(original);
                    if(layout==layouts.end()&&str(*n,"name").empty()) {
                        auto loc=location(*n);auto suffix=":"+std::to_string(loc.getInteger("line").value())+":"+std::to_string(loc.getInteger("column").value())+")";
                        auto spelling="(unnamed at "+include_path(header);
                        layout=std::find_if(layouts.begin(),layouts.end(),[&](const auto& entry){auto key=entry.first;std::replace(key.begin(),key.end(),'\\','/');return key.starts_with(spelling)&&key.ends_with(suffix);});
                    }
                    if(layout==layouts.end()||!layout->second.size)throw std::runtime_error("Clang did not report a named complete record layout; name the typedef/tag explicitly");
                    Record r{n,identifier(original),unique_c("vdi_record_"+std::to_string(records.size())),original,{},layout->second.size,layout->second.align,{}};
                    std::string fields;std::size_t index=0;
                    for(auto& v:children(*n))if(auto* field=v.getAsObject();field&&str(*field,"kind")=="FieldDecl") {
                        auto s=shape(type(*field),true);if(!s.exact)throw std::runtime_error("field '"+str(*field,"name")+"' has a C type distinct from the existing descriptor scalar aliases; review a value-copy adapter");
                        auto name=identifier(str(*field,"name"));if(index>=layout->second.offsets.size())throw std::runtime_error("missing Clang field offset");
                        fields+="field "+name+" "+str(*field,"name")+" "+s.descriptor+" "+std::to_string(layout->second.offsets[index++])+"\n";r.fields.emplace_back(name,s);
                    }
                    r.descriptor="record "+r.name+" "+r.c+" "+std::to_string(r.size)+" "+std::to_string(r.align)+" "+std::to_string(index)+"\n"+fields;
                    CPrimitiveAdmission::generate_declarations(prefix+record_text+r.descriptor+"bind vdi_layout_check vdi_layout_check void\n");
                    if(!reserve(r.name))throw std::runtime_error("duplicate or normalized VOID name collision");
                    const auto tag=str(*n,"name");auto c=tag.empty()?original:"struct "+tag;
                    header_text+="typedef "+c+" "+r.c+";\n";record_text+=r.descriptor;
                    records[original]=r;records["struct "+original]=r;if(!tag.empty())records["struct "+tag]=r;
                    if(auto a=record_alias.find(str(*n,"id"));a!=record_alias.end())records[a->second]=r;
                    report(*n,"generated","exact Clang natural layout, verified by existing descriptor/native-bind",r.name);
                    pending.erase(n);progress=true;
                } catch(const std::exception& e) {if(round==declarations.size()||(!progress&&round>0)) {report(*n,"reviewed_adapter_required",e.what());pending.erase(n);}}
            }
            if(!progress&&round==0)continue;
        }
    }
    void emit_functions() {
        for(auto* n:declarations)if(str(*n,"kind")=="FunctionDecl") {
            auto original=str(*n,"name");try {
                if(n->getBoolean("variadic").value_or(false))throw std::runtime_error("arbitrary C varargs are unsupported; supply a typed fixed-prototype adapter");
                if(!std::regex_match(original,std::regex("[A-Za-z_][A-Za-z0-9_]{0,63}")))throw std::runtime_error("C export name exceeds descriptor profile");
                if(str(*n,"storageClass")=="static")throw std::runtime_error("static/inline functions need an explicitly exported C wrapper");
                auto signature=type(*n,false);auto split=signature.find(" (");if(split==signature.npos)throw std::runtime_error("unsupported function prototype/calling convention");
                if(signature.find("__attribute__")!=signature.npos||signature.find("throw(")!=signature.npos)throw std::runtime_error("non-default calling convention/dynamic exception specification requires an explicit C-compatible prototype");
                if(options.language=="c"&&signature.ends_with("()"))throw std::runtime_error("C non-prototype declaration: write an explicit (void) or fixed parameter list");
                auto result_original=signature.substr(0,split);auto result=shape(result_original);auto name=identifier(original);
                std::vector<Shape> parameters;std::vector<std::string> original_parameters;
                for(auto& v:children(*n))if(auto* p=v.getAsObject();p&&str(*p,"kind")=="ParmVarDecl") {parameters.push_back(shape(type(*p)));original_parameters.push_back(type(*p,false));}
                if(parameters.size()>c_primitive_max_arguments)throw std::runtime_error("more than 16 fixed parameters exceeds the existing transport");
                if(names.contains(name))throw std::runtime_error("duplicate declaration or normalized name collision; the earlier declaration is retained");
                auto symbol=unique_c("vdi_call_"+std::to_string(bindings));std::string binding="bind "+name+" "+symbol+" "+result.descriptor,cparams,args,voidparams,callargs;
                for(std::size_t i=0;i<parameters.size();++i) {
                    auto index=std::to_string(i);binding+=" "+parameters[i].descriptor;
                    if(i){cparams+=", ";args+=", ";voidparams+=", ";callargs+=", ";}
                    cparams+=parameters[i].c+" p"+index;args+=parameters[i].descriptor.starts_with("record:")?"p"+index:"("+original_parameters[i]+")p"+index;
                    voidparams+="p"+index+": "+parameters[i].source;callargs+="p"+index;
                }
                binding+='\n';CPrimitiveAdmission::generate_declarations(prefix+record_text+forwarders+binding);
                reserve(name);forwarders+=binding;
                const auto prototype=result.c+" "+symbol+"("+(cparams.empty()?"void":cparams)+")";
                header_text+=export_macro+" "+prototype+";\n";
                definitions+=prototype+" { "+(result.descriptor=="void"?"":"return ")+original+"("+args+"); }\n";
                api+="func "+name+"("+voidparams+")"+(result.descriptor=="void"?"":" -> "+result.source)+":\n    "+(result.descriptor=="void"?"":"return ")+"native."+name+"("+callargs+")\n\n";
                report(*n,"generated","fixed C value forwarder; original symbols/types retained in report; review effects and no-escaping-exceptions contract",name);++bindings;
            }catch(const std::exception& e){report(*n,(n->getBoolean("variadic").value_or(false)||std::string(e.what()).starts_with("duplicate"))?"unsupported":"reviewed_adapter_required",e.what());}
        }
    }
    std::string definitions;
    void add_constant(const Object& n,std::string name,std::string number,std::string scalar) {
        try {auto normalized=identifier(name);if(!reserve(normalized))throw std::runtime_error("duplicate or normalized constant name collision");
            constants+="const "+normalized+": "+scalar+" = "+number+"\n";report(n,"generated","Clang-evaluated numeric constant",normalized);
        }catch(const std::exception& e){report(n,"unsupported",e.what());}
    }
    void emit_enums() {
        for(auto* n:declarations)if(str(*n,"kind")=="EnumDecl") {
            std::int64_t previous=-1;bool previous_valid=true;
            for(auto& v:children(*n))if(auto* enumerator=v.getAsObject();enumerator&&str(*enumerator,"kind")=="EnumConstantDecl") {
                try {auto explicit_value=value(*enumerator);if(explicit_value)previous=std::stoll(*explicit_value);else {if(!previous_valid||previous==INT64_MAX)throw std::runtime_error("enum value exceeds signed 64-bit constant profile");++previous;}
                    previous_valid=true;std::string scalar="i64";
                    try {auto named=str(*n,"name");scalar=shape(named.empty()?type(*enumerator):(options.language=="c"?"enum ":"")+named).source;}catch(const std::exception&){}
                    add_constant(*enumerator,str(*enumerator,"name"),std::to_string(previous),scalar);
                }catch(const std::exception&){previous_valid=false;report(*enumerator,"unsupported","enum constant exceeds the signed 64-bit constant profile; add a reviewed unsigned constant");}
            }
            report(*n,"generated","enumerator constants; underlying integer ABI derived by Clang when used");
        }
    }
};
} // namespace
namespace {
void emit_macros(Importer& importer,const std::string& preprocessed) {
    struct Macro {Object node;std::string key,scalar;bool active=true;};std::vector<Macro> macros;std::map<std::string,std::size_t> active;
    std::istringstream lines(preprocessed);std::string line,file;unsigned number=0;
    while(std::getline(lines,line)) {
        if(line.starts_with("# ")) {std::istringstream directive(line.substr(2));directive>>number>>std::quoted(file);continue;}
        if(line.starts_with("#define ")||line.starts_with("#undef ")) {
            auto definition=line.substr(line.starts_with("#define ")?8:7);auto end=definition.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_");auto name=definition.substr(0,end);
            if(auto previous=active.find(name);previous!=active.end()) {
                auto& old=macros[previous->second];old.active=false;importer.report(old.node,"unsupported","macro is superseded or undefined later in the translation unit");active.erase(previous);
            }
        }
        if(utf8_path(file).lexically_normal()==importer.header&&line.starts_with("#define ")) {
            auto rest=line.substr(8);auto end=rest.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_");
            auto name=rest.substr(0,end);if(!name.empty()) {
                std::size_t offset=0;for(unsigned i=1;i<number&&offset<importer.bytes.size();++i){auto next=importer.bytes.find('\n',offset);offset=next==std::string::npos?importer.bytes.size():next+1;}
                Object node{{"kind","MacroDefinition"},{"name",name},{"loc",Object{{"offset",static_cast<std::int64_t>(offset)}}}};
                if(end!=rest.npos&&rest[end]=='(')importer.report(node,"unsupported","function-like preprocessor macro: write an explicit typed VOID/C wrapper");
                else {active[name]=macros.size();macros.push_back({std::move(node),importer.unique_c("vdi_macro_"+std::to_string(macros.size()),true),{},true});}
            }
        }
        ++number;
    }
    if(macros.size()>2048)throw std::runtime_error("Header exceeds the 2048 object-macro import bound");
    if(macros.empty())return;
    auto probe=importer.clang.work/"macro-probe.h";std::string code=importer.preamble;
    for(auto& m:macros)if(m.active)code+="typedef __typeof__("+str(m.node,"name")+") "+m.key+"_type;\nenum { "+m.key+" = ("+str(m.node,"name")+") };\n";
    write(probe,code);auto ast=importer.clang.ast(probe,true);std::map<std::string,const Object*> types,values;
    auto index=[&](const Object& root){for(auto& v:children(root))if(auto* n=v.getAsObject()) {
        if(str(*n,"kind")=="TypedefDecl")types[str(*n,"name")]=n;
        if(str(*n,"kind")=="EnumDecl")for(auto& e:children(*n))if(auto* o=e.getAsObject())values[str(*o,"name")]=o;
    }};index(*ast.getAsObject());
    std::vector<Macro*> floats;
    for(auto& m:macros) {
        if(!m.active)continue;
        auto it=values.find(m.key);auto t=types.find(m.key+"_type");std::optional<std::string> number_value=it==values.end()?std::nullopt:value(*it->second);
        auto spelling=t==types.end()?std::string{}:type(*t->second);
        if(number_value) {
            try {auto s=importer.shape(spelling);if(s.source=="void"||!s.size)throw std::runtime_error("non-numeric macro");
                importer.add_constant(m.node,str(m.node,"name"),*number_value,s.source);
            }catch(const std::exception& e){importer.report(m.node,"unsupported",e.what());}
        } else if(spelling=="float"||spelling=="double") {m.scalar=spelling=="float"?"f32":"f64";floats.push_back(&m);}
        else importer.report(m.node,"unsupported","not an evaluable scalar numeric constant (string, pointer, configuration token or nonconstant expression); supply a typed wrapper/constant");
    }
    if(floats.empty())return;code=importer.preamble;
    for(auto* m:floats)code+="enum { "+m->key+" = __builtin_bit_cast(unsigned long long, (double)("+str(m->node,"name")+")) };\n";
    write(probe,code);auto floating=importer.clang.ast(probe,true);types.clear();values.clear();index(*floating.getAsObject());
    for(auto* m:floats) {
        auto it=values.find(m->key);auto bits=it==values.end()?std::nullopt:value(*it->second);
        try {if(!bits)throw std::runtime_error("floating macro is not a constant expression");auto d=std::bit_cast<double>(static_cast<std::uint64_t>(std::stoull(*bits)));
            if(!std::isfinite(d)||(std::signbit(d)&&d==0))throw std::runtime_error("non-finite or negative-zero constant requires explicit bit representation");
            std::ostringstream literal;literal.imbue(std::locale::classic());literal<<std::setprecision(std::numeric_limits<double>::max_digits10)<<d;
            auto text=literal.str();if(text.find_first_of(".eE")==text.npos)text+=".0";
            importer.add_constant(m->node,str(m->node,"name"),text,m->scalar);
        }catch(const std::exception& e){importer.report(m->node,"unsupported",e.what());}
    }
}
} // namespace
void import_native_header(const NativeImportOptions& options) {
    auto output=fs::absolute(options.output).lexically_normal();
    if(options.output.empty()||output.filename().empty()||options.output.filename()=="."||options.output.filename()==".."||fs::exists(fs::symlink_status(output)))
        throw std::invalid_argument("Native import requires a fresh output directory; existing output is never overwritten");
    auto parent=project::inspect_creation_root(output.parent_path());
    auto header=fs::absolute(options.header).lexically_normal();auto bytes=project::read_project_input(header,header_limit);
    if(options.language!="c"&&options.language!="c++")throw std::invalid_argument("Native import language must be c or c++");
    Scratch scratch;Clang clang{options.clang,scratch.path,{"-x",options.language,"-std="+std::string(options.language=="c"?"c11":"c++17")}};
#ifdef _WIN32
    const std::string target="x86_64-pc-windows-msvc msvc-x64",c_long="i32";
#else
    const std::string target="x86_64-unknown-linux-gnu sysv-x64",c_long="i64";
#endif
    clang.base.push_back("--target="+target.substr(0,target.find(' ')));
    for(const auto& flag:options.preprocessing) {
        if(!(flag.starts_with("-I")||flag.starts_with("-D")||flag.starts_with("-U"))||flag.size()<=2)throw std::invalid_argument("Only attached -Ipath, -Dname[=value] and -Uname preprocessing flags are supported");
        clang.base.push_back(flag);
    }
    auto version=clang.run({"--version"});if(!std::regex_search(version,std::regex("clang version 23\\.1\\.4([^0-9]|$)")))throw std::runtime_error("Native import requires pinned Clang 23.1.4");
    auto prefix="void-c-primitive 4 3\nprovider "+options.provider+"\ntarget "+target+"\nsource-file "+options.source+"\nthunk "+std::string(64,'0')+"\nfixture "+std::string(64,'0')+"\neffects direct-sync blocking no-callbacks\ncall c-default\naliases c_int:i32 c_long:"+c_long+" c_size:u64 c_bool:bool c_char:i8\n";
    // Validate caller identities using the existing descriptor parser before tools/publication.
    CPrimitiveAdmission::generate_declarations(prefix+"bind vdi_validate vdi_validate void\n");
    auto ast=clang.ast(header);auto* root=ast.getAsObject();if(!root)throw std::runtime_error("Clang did not produce a translation unit");
    Importer importer{options,clang,header,bytes,"#include \""+include_path(header)+"\"\n",prefix};
    importer.identifiers(*root);
    auto pp_args=clang.base;pp_args.insert(pp_args.end(),{"-E","-dD",path_utf8(header)});auto preprocessed=clang.run(pp_args);
    std::istringstream preprocessing_lines(preprocessed);std::string preprocessing_line;
    while(std::getline(preprocessing_lines,preprocessing_line))if(preprocessing_line.starts_with("#define ")) {
        auto name=preprocessing_line.substr(8);name.resize(name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")==name.npos?name.size():name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_"));importer.c_names.insert(name);
    }
    importer.export_macro=importer.unique_c("VDI_EXPORT");
    importer.collect(*root);if(importer.declarations.size()>8192)throw std::runtime_error("Header exceeds the 8192-declaration import bound");
    auto layout_args=clang.base;layout_args.insert(layout_args.end(),{"-fsyntax-only","-Xclang","-fdump-record-layouts-complete",path_utf8(header)});
    importer.load_layouts(clang.run(layout_args));importer.enum_types();importer.emit_records();
    for(auto* n:importer.declarations)if(str(*n,"kind")=="TypedefDecl"||str(*n,"kind")=="TypeAliasDecl") {
        try {auto s=importer.shape(type(*n));importer.report(*n,"generated","C typedef normalized to existing descriptor/VOID shape "+s.descriptor);}
        catch(const std::exception& e){importer.report(*n,"reviewed_adapter_required",e.what());}
    } else if(str(*n,"kind")!="FunctionDecl"&&str(*n,"kind")!="EnumDecl"&&str(*n,"kind")!="RecordDecl"&&str(*n,"kind")!="CXXRecordDecl")
        importer.report(*n,"unsupported","global objects/other language declarations are not callable bindings; add an explicit C accessor");
    importer.emit_functions();importer.emit_enums();
    emit_macros(importer,preprocessed);
    std::map<std::string,std::string> files;
    if(!importer.bindings) {
        // Constants remain usable without any native admission. A records-only
        // descriptor cannot yet be admitted; never add a dummy callable for it.
        if(!importer.constants.empty())files["api.void"]="# Numeric constants only; no native calls or approval.\n"+importer.constants;
        for(auto& row:importer.coverage)if(auto* n=row.getAsObject();n&&str(*n,"category")=="generated"&&
            (str(*n,"kind")=="RecordDecl"||str(*n,"kind")=="CXXRecordDecl")) {
            (*n)["category"]="reviewed_adapter_required";
            (*n)["reason"]="Existing native descriptors require at least one callable binding; add a reviewed fixed-prototype operation before exporting/admitting this record";
        }
    }
    if(importer.bindings) {
        auto descriptor=prefix+importer.record_text+importer.forwarders;
        const auto thunk=hash(descriptor+importer.header_text+importer.definitions+preprocessed),fixture=hash(preprocessed);
        descriptor.replace(descriptor.find("thunk ")+6,64,thunk);descriptor.replace(descriptor.find("fixture ")+8,64,fixture);
        files["provider.descriptor"]=descriptor;files["native.void"]=CPrimitiveAdmission::generate_declarations(descriptor);
        std::string constructors;std::set<std::string> seen;
        for(auto& [key,r]:importer.records)if(seen.insert(r.name).second) {
            std::string params,args;for(auto& [name,s]:r.fields){if(!params.empty()){params+=", ";args+=", ";}params+=name+": "+s.source;args+=name+" = "+name;}
            constructors+="func "+r.name+"("+params+") -> native."+r.name+":\n    return native."+r.name+"("+args+")\n\n";
        }
        files["api.void"]="# Editable source. This importer never runs during check/build/run.\nuse \"./native.void\" as native\n\n"+importer.constants+"\n"+constructors+importer.api;
        files["imported.h"]="#pragma once\n#ifdef __cplusplus\nusing _Bool = bool;\n#endif\n#include <stdint.h>\n#include <stddef.h>\n"+importer.preamble+
            "#ifdef _WIN32\n#define "+importer.export_macro+" __declspec(dllexport)\n#else\n#define "+importer.export_macro+" __attribute__((visibility(\"default\")))\n#endif\n"+"#ifdef __cplusplus\nextern \"C\" {\n#endif\n"+importer.header_text+"#ifdef __cplusplus\n}\n#endif\n";
        files[options.language=="c"?"imports.c":"imports.cpp"]=std::string("#include \"imported.h\"\n")+(options.language=="c++"?"extern \"C\" {\n":"")+importer.definitions+(options.language=="c++"?"}\n":"");
        if(options.language=="c++")files["checked.cpp"]="// Parse the supplied C++ header before entering C linkage.\n#include \"imported.h\"\nextern \"C\" {\n#include \"checked.c\"\n}\n";
    }
    std::map<std::string,unsigned> counts;for(auto& row:importer.coverage)++counts[str(*row.getAsObject(),"category")];Object totals;for(auto& [key,n]:counts)totals[key]=n;
    Array flags;for(auto& flag:options.preprocessing)flags.emplace_back(flag);
    Object report{{"schema",1},{"provider",options.provider},{"header",path_utf8(header)},{"header_sha256",hash(bytes)},{"preprocessed_sha256",hash(preprocessed)},{"clang",version},{"language",options.language},{"working_directory",path_utf8(fs::current_path())},{"target",target},{"source",options.source},{"preprocessing",std::move(flags)},{"descriptor_ready_for_review",bool(importer.bindings)},{"generated_functions",importer.bindings},{"export_macro",importer.export_macro},{"counts",std::move(totals)},{"declarations",std::move(importer.coverage)}};
    llvm::raw_string_ostream json(files["coverage.json"]);json<<llvm::formatv("{0:2}",llvm::json::Value(std::move(report)))<<'\n';
    files["README.import.txt"]="Candidate declarations only; no native code has been compiled, loaded or approved.\n"
        "Review coverage.json, exact C declarations, effects (blocking/no callbacks), pointer ownership and no escaping exceptions.\n"
        "Only declarations from the requested header are selected; include dependencies must remain available.\n"
        "native.void must occupy descriptor source-file "+options.source+" within the application/package root.\n"
        "Edit api.void normally. Review native.void against provider.descriptor; descriptor remains authoritative.\n"
        "Generate the checked adapter explicitly: voidc native-bind provider.descriptor imported.h -o checked.c\n"
        "Compile imports.c + checked.c (C11), or imports.cpp + checked.cpp (Clang C++17), with the original library.\n"
        "Use the same preprocessing flags as coverage.json. Record actual runtime/link/notice identities in the existing native catalog.\n"
        "Only then resolve/check and authorize execution with --allow-native. Source packages/locks do not grant native approval.\n"
        "No overwrite/update mode: regenerate to a fresh directory and review the diff.\n"
        +(importer.bindings?std::string{}:"No callable functions were generated. Only coverage and any pure numeric constants were published; complete an explicit reviewed adapter before native-bind.\n");
    std::map<std::string,std::string_view> views;for(auto& [name,text]:files)views[name]=text;
    project::publish_project_directory(parent,path_utf8(output.filename()),views,[&](const fs::path& stage,const project::ProjectDirectoryReader& reader) {
        if(stage==output)throw project::CreationCollision("Native import output already exists; use a fresh directory");
        for(auto& [name,text]:files)if(reader.read(stage/utf8_path(name),tool_limit)!=text)throw std::runtime_error("Native import output changed during publication");
    },[&]{if(project::read_project_input(header,header_limit)!=bytes)throw std::runtime_error("Header changed during import; no output published");});
    std::cout<<"Native import: "<<importer.bindings<<" supported functions; "<<counts["reviewed_adapter_required"]<<" declarations require reviewed adapters; "<<counts["unsupported"]<<" unsupported.\n"
             <<"Published "<<path_utf8(output)<<"/coverage.json"<<(importer.bindings?" and editable candidate declarations":" (coverage only; no callable stubs)")<<"\n";
}
} // namespace vd
