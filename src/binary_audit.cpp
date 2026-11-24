#include "void/binary_audit.hpp"
#include "frontend/project_digest.hpp"
#include <llvm/Object/ObjectFile.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Object/COFF.h>
#include <llvm/Object/MachO.h>
#include <llvm/Support/JSON.h>
#include <llvm/Support/raw_ostream.h>
#include <map>
#include <stdexcept>

namespace vd::audit {
namespace {
struct Invalid : std::runtime_error { using std::runtime_error::runtime_error; };
struct Limited : Invalid { using Invalid::Invalid; };
template<class T> T checked(llvm::Expected<T> value) {
    if (!value) throw Invalid(llvm::toString(value.takeError()));
    return std::move(*value);
}
void check(llvm::Error error) { if (error) throw Invalid(llvm::toString(std::move(error))); }
void count(std::size_t& n) { if (++n > record_limit) throw Limited("Record budget exceeded."); }
void bounded(std::string_view s, std::uint64_t offset, std::uint64_t size) {
    if (offset > s.size() || size > s.size() - offset) throw Invalid("Truncated or out-of-bounds image record.");
}
std::uint64_t number(std::string_view s, std::uint64_t offset, unsigned width) {
    bounded(s, offset, width); std::uint64_t value = 0;
    for (unsigned i = 0; i < width; ++i) value |= std::uint64_t(static_cast<unsigned char>(s[offset+i])) << (i*8);
    return value;
}
std::string cstring(std::string_view s, std::uint64_t offset) {
    bounded(s, offset, 1); auto end = s.find('\0', offset);
    if (end == s.npos) throw Invalid("Unterminated image string.");
    if (end-offset > 4096) throw Limited("Image string budget exceeded.");
    return std::string(s.substr(offset, end-offset));
}
// COFF dependency-name accessors use C strings. Validate their terminators in
// file-backed RVA ranges before LLVM can inspect them; caller bytes need not
// carry an accessible sentinel beyond the supplied string_view.
void pe_bounds(std::string_view bytes) {
    bounded(bytes,0,64);
    const auto pe=number(bytes,60,4);bounded(bytes,pe,24);
    if(bytes.substr(pe,4)!=std::string_view("PE\0\0",4)) throw Invalid("Invalid PE signature.");
    const auto sections=number(bytes,pe+6,2), optional=pe+24, optional_size=number(bytes,pe+20,2);
    bounded(bytes,optional,optional_size);
    if(optional_size<112 || number(bytes,optional,2)!=0x20b) throw Invalid("Require a complete PE32+ optional header.");
    const auto directories=number(bytes,optional+108,4);
    if(directories>16 || directories>(optional_size-112)/8) throw Invalid("Invalid PE directory count.");
    const auto table=optional+optional_size;
    bounded(bytes,table,sections*40);
    for(std::uint64_t i=0;i<sections;++i) {
        const auto section=table+i*40;
        bounded(bytes,number(bytes,section+20,4),number(bytes,section+16,4));
    }
    auto mapped=[&](std::uint64_t rva)->std::string_view {
        const auto headers=number(bytes,optional+60,4);
        std::optional<std::string_view> result;
        if(rva<headers) {bounded(bytes,0,headers);result=bytes.substr(rva,headers-rva);}
        for(std::uint64_t i=0;i<sections;++i) {
            const auto section=table+i*40, address=number(bytes,section+12,4), size=number(bytes,section+16,4);
            if(rva>=address && rva-address<size) {
                if(result) throw Invalid("Ambiguous PE RVA mapping.");
                const auto delta=rva-address;
                result=bytes.substr(number(bytes,section+20,4)+delta,size-delta);
            }
        }
        if(!result) throw Invalid("Unmapped PE dependency RVA.");
        return *result;
    };
    std::size_t budget=0;
    for(const auto index:{1u,13u}) {
        if(index>=directories) continue;
        const auto directory=optional+112+index*8, rva=number(bytes,directory,4), size=number(bytes,directory+4,4);
        if(!rva && !size) continue;
        if(!rva || !size) throw Invalid("Incomplete PE import directory.");
        const auto stride=index==1?20u:32u;
        auto data=mapped(rva);bounded(data,0,size);data=data.substr(0,size);
        bool terminated=false;
        for(std::uint64_t offset=0;offset<size;offset+=stride) {
            count(budget);bounded(data,offset,stride);
            const auto record=data.substr(offset,stride);
            if(record.find_first_not_of('\0')==record.npos) {terminated=true;break;}
            if(index==13 && number(record,0,4)!=1) throw Invalid("Unsupported PE delay import VA addressing.");
            const auto name=number(record,index==1?12:4,4);
            if(!name || cstring(mapped(name),0).empty()) throw Invalid("Empty PE dependency name.");
        }
        if(!terminated) throw Invalid("Unterminated PE import directory.");
    }
    const auto symbol_offset=number(bytes,pe+12,4), symbols=number(bytes,pe+16,4);
    if(!symbol_offset && symbols) throw Invalid("Invalid PE symbol table.");
    if(symbols>record_limit) throw Limited("PE symbol budget exceeded.");
    if(symbol_offset) {
        bounded(bytes,symbol_offset,symbols*18+4);
        const auto strings=symbol_offset+symbols*18, size=number(bytes,strings,4);
        if(size<4) throw Invalid("Invalid PE symbol string table.");
        bounded(bytes,strings,size);
        if(size>4 && bytes[strings+size-1]!='\0') throw Invalid("Unterminated PE symbol string table.");
    }
}
// Mach-O symbol names also use a C-string accessor. Require every declared
// symbol name to terminate within its declared string table before LLVM reads it.
void macho_bounds(std::string_view bytes) {
    bounded(bytes,0,32);
    const auto commands=number(bytes,16,4), size=number(bytes,20,4);
    bounded(bytes,32,size);auto data=bytes.substr(32,size);
    std::uint64_t cursor=0;bool symbols_seen=false;
    for(std::uint64_t i=0;i<commands;++i) {
        bounded(data,cursor,8);
        const auto tag=number(data,cursor,4), length=number(data,cursor+4,4);
        if(length<8) throw Invalid("Invalid Mach-O command size.");
        bounded(data,cursor,length);
        if(tag==llvm::MachO::LC_SYMTAB) {
            if(symbols_seen || length!=24) throw Invalid("Invalid Mach-O symbol command.");
            symbols_seen=true;
            const auto offset=number(data,cursor+8,4), count=number(data,cursor+12,4);
            const auto strings=number(data,cursor+16,4), string_size=number(data,cursor+20,4);
            if(count>record_limit) throw Limited("Mach-O symbol budget exceeded.");
            bounded(bytes,offset,count*16);bounded(bytes,strings,string_size);
            const auto names=bytes.substr(strings,string_size);
            if(!names.empty() && names.back()!='\0') throw Invalid("Unterminated Mach-O symbol string table.");
            for(std::uint64_t j=0;j<count;++j) (void)cstring(names,number(bytes,offset+j*16,4));
        }
        cursor+=length;
    }
    if(cursor!=size) throw Invalid("Inconsistent Mach-O command extent.");
}
// Program headers remain authoritative when section tables have been removed.
void elf(std::string_view bytes, Report& report) {
    bounded(bytes, 0, 64);
    if (number(bytes,4,1)!=2 || number(bytes,5,1)!=1) throw Invalid("Unsupported ELF encoding; require ELF64 little endian.");
    auto type=number(bytes,16,2);
    if (type!=2 && type!=3) throw Invalid("ELF is not an executable or shared image.");
    report.image_kind=type==2?"executable":"shared-or-pie";
    if (number(bytes,52,2)!=64 || number(bytes,54,2)!=56) throw Invalid("Invalid ELF header sizes.");
    const auto ph=number(bytes,32,8), n=number(bytes,56,2);
    if (!n || n==65535) throw Invalid("Unsupported ELF program header count.");
    if (n>record_limit) throw Limited("ELF program header budget exceeded.");
    bounded(bytes,ph,n*56);
    struct Load { std::uint64_t address, offset, size; };
    std::vector<Load> loads;
    std::optional<std::pair<std::uint64_t,std::uint64_t>> dynamic;
    for (std::uint64_t i=0;i<n;++i) {
        auto p=ph+i*56, tag=number(bytes,p,4), off=number(bytes,p+8,8), size=number(bytes,p+32,8);
        bounded(bytes,off,size);
        if (size>number(bytes,p+40,8)) throw Invalid("ELF file segment exceeds memory size.");
        if (tag==1) loads.push_back({number(bytes,p+16,8),off,size});
        if (tag==2) {
            if (dynamic) throw Invalid("Duplicate ELF dynamic segment.");
            dynamic={{off,size}};
        }
        if (tag==3) {
            if (!report.interpreter.empty() || !size) throw Invalid("Invalid ELF interpreter segment.");
            report.interpreter=cstring(bytes.substr(off,size),0);
            if(report.interpreter.empty()) throw Invalid("Empty ELF interpreter.");
        }
    }
    auto mapped=[&](std::uint64_t address)->std::string_view {
        std::optional<Load> result;
        for (const auto& load:loads) if (address>=load.address && address-load.address<load.size) {
            if (result) throw Invalid("Ambiguous overlapping ELF virtual mapping.");
            result=load;
        }
        if (!result) throw Invalid("Unmapped ELF dynamic address.");
        auto delta=address-result->address;
        return bytes.substr(result->offset+delta,result->size-delta);
    };
    if (!dynamic) return;
    auto [offset,size]=*dynamic;
    if (size%16) throw Invalid("Invalid ELF dynamic table.");
    if (size/16>record_limit) throw Limited("ELF dynamic table budget exceeded.");
    std::vector<std::pair<std::uint64_t,std::uint64_t>> entries;
    std::map<std::uint64_t,std::uint64_t> tags;
    bool terminated=false;
    for (std::uint64_t i=0;i<size;i+=16) {
        auto tag=number(bytes,offset+i,8), value=number(bytes,offset+i+8,8);
        if (!tag) { terminated=true; break; }
        entries.emplace_back(tag,value);
        if (tag==5 || tag==10 || tag==0x6ffffffe || tag==0x6fffffff) {
            if (!tags.emplace(tag,value).second) throw Invalid("Duplicate ELF dynamic singleton.");
        }
    }
    if (!terminated) throw Invalid("Missing ELF dynamic terminator.");
    bool needs_strings=false;
    for (auto [tag,value]:entries) { (void)value; if(tag==1 || tag==15 || tag==29) needs_strings=true; }
    if (tags.contains(0x6ffffffe) || tags.contains(0x6fffffff)) needs_strings=true;
    if (!needs_strings) return;
    if (!tags.contains(5) || !tags.contains(10)) throw Invalid("Missing ELF dynamic string table.");
    auto strings=mapped(tags.at(5)); bounded(strings,0,tags.at(10)); strings=strings.substr(0,tags.at(10));
    for (auto [tag,value]:entries) {
        if (tag==1) report.dependencies.push_back(cstring(strings,value));
        if (tag==15 || tag==29) report.loader_paths.push_back(cstring(strings,value));
    }
    if (tags.contains(0x6ffffffe)!=tags.contains(0x6fffffff)) throw Invalid("Incomplete ELF version requirements.");
    if (!tags.contains(0x6ffffffe)) return;
    auto versions=mapped(tags.at(0x6ffffffe)); auto total=tags.at(0x6fffffff);
    if (!total) throw Invalid("Invalid ELF version count.");
    if (total>record_limit) throw Limited("ELF version budget exceeded.");
    std::uint64_t cursor=0; std::size_t budget=0;
    for (std::uint64_t i=0;i<total;++i) {
        bounded(versions,cursor,16);
        if (number(versions,cursor,2)!=1) throw Invalid("Unsupported ELF version need revision.");
        auto count_aux=number(versions,cursor+2,2), aux=number(versions,cursor+8,4), next=number(versions,cursor+12,4);
        auto dependency=cstring(strings,number(versions,cursor+4,4));
        if (count_aux && aux<16) throw Invalid("Invalid ELF version auxiliary offset.");
        auto a=cursor+aux;
        for (std::uint64_t j=0;j<count_aux;++j) {
            count(budget); bounded(versions,a,16);
            report.versions.push_back(dependency+":"+cstring(strings,number(versions,a+8,4)));
            auto advance=number(versions,a+12,4);
            if (j+1<count_aux) { if (advance<16 || advance>versions.size()-a) throw Invalid("Invalid ELF auxiliary chain."); a+=advance; }
            else if (advance) throw Invalid("Excess ELF auxiliary chain.");
        }
        if (i+1<total) { if(next<16 || next>versions.size()-cursor) throw Invalid("Invalid ELF version chain."); cursor+=next; }
        else if (next) throw Invalid("Excess ELF version chain.");
    }
}
void macho(const llvm::object::MachOObjectFile& image, std::string_view bytes, Report& report) {
    if (!image.is64Bit() || !image.isLittleEndian()) throw Invalid("Unsupported Mach-O encoding.");
    auto type=image.getHeader64().filetype;
    if (type!=llvm::MachO::MH_EXECUTE && type!=llvm::MachO::MH_DYLIB && type!=llvm::MachO::MH_BUNDLE)
        throw Invalid("Mach-O is not an executable, dylib or bundle.");
    report.image_kind=type==llvm::MachO::MH_EXECUTE?"executable":"shared";
    std::size_t budget=0;
    for (const auto& command:image.load_commands()) {
        count(budget);
        auto offset=static_cast<std::size_t>(command.Ptr-bytes.data());
        auto size=command.C.cmdsize; bounded(bytes,offset,size);
        if (size<8) throw Invalid("Invalid Mach-O load command size.");
        auto cmd=command.C.cmd;
        bool dylib=cmd==llvm::MachO::LC_LOAD_DYLIB || cmd==llvm::MachO::LC_LOAD_WEAK_DYLIB || cmd==llvm::MachO::LC_REEXPORT_DYLIB || cmd==llvm::MachO::LC_LOAD_UPWARD_DYLIB || cmd==llvm::MachO::LC_LAZY_LOAD_DYLIB;
        if (dylib || cmd==llvm::MachO::LC_RPATH) {
            if (size<(dylib?24u:12u)) throw Invalid("Truncated Mach-O path command.");
            auto start=number(bytes,offset+8,4);
            if(start<(dylib?24u:12u)) throw Invalid("Invalid Mach-O path offset.");
            auto value=cstring(bytes.substr(offset,size),start);
            (dylib?report.dependencies:report.loader_paths).push_back(std::move(value));
        }
    }
}
}
Report inspect(std::string_view bytes, const Policy& policy) {
    Report report;
    auto finding=[&](std::string code,std::string detail,std::uint64_t offset=0) {
        if(report.findings.size()<record_limit) report.findings.push_back({std::move(code),std::move(detail),offset});
        else {
            report.inspected=false;
            report.findings.back()={"limit","Finding budget exceeded; inspection is incomplete.",0};
        }
    };
    if (policy.revision!=1 || (policy.target!="linux-x64" && policy.target!="windows-x64" && policy.target!="macos-arm64") || policy.forbidden_strings.size()>64) {
        finding("unsupported-policy","Require policy revision 1 and an existing target spelling; at most 64 canaries."); return report;
    }
    std::size_t canary_bytes=0;
    for(const auto& s:policy.forbidden_strings) {
        if(s.empty() || s.size()>256 || canary_bytes+s.size()>4096) {finding("unsupported-policy","Canaries must be nonempty, at most 256 bytes each and 4096 bytes total.");return report;}
        canary_bytes+=s.size();
    }
    if(bytes.size()>byte_limit) {finding("limit","Image byte budget exceeded.");return report;}
    project::Digest digest;digest.bytes(bytes);report.sha256=digest.finish();
    try {
        const bool is_elf=bytes.starts_with("\x7f" "ELF"), is_pe=bytes.starts_with("MZ"), is_macho=bytes.starts_with("\xcf\xfa\xed\xfe");
        if(!is_elf && !is_pe && !is_macho) {finding("unsupported-format","Require ELF64LE, PE32+ or thin Mach-O64LE.");return report;}
        // Bound table counts before asking LLVM to allocate format structures.
        if(is_elf) {
            bounded(bytes,0,64);
            if(number(bytes,4,1)!=2 || number(bytes,5,1)!=1) {finding("unsupported-format","Require ELF64 little endian.");return report;}
            if(number(bytes,60,2)>record_limit || number(bytes,56,2)>record_limit) throw Limited("ELF table budget exceeded.");
            if(number(bytes,60,2)==0 && number(bytes,40,8)!=0) throw Invalid("Extended ELF section counts are outside this inspection scope.");
        }
        if(is_pe) {
            bounded(bytes,0,64); auto pe=number(bytes,60,4);bounded(bytes,pe,24);
            if(number(bytes,pe+6,2)>record_limit) throw Limited("PE section budget exceeded.");
            const auto optional=pe+24;bounded(bytes,optional,2);
            if(number(bytes,optional,2)!=0x20b) {finding("unsupported-format","Require a PE32+ image.");return report;}
            pe_bounds(bytes);
        }
        if(is_macho) {
            bounded(bytes,0,32);if(number(bytes,16,4)>record_limit) throw Limited("Mach-O command budget exceeded.");
            macho_bounds(bytes);
        }
        llvm::MemoryBufferRef buffer(llvm::StringRef(bytes.data(),bytes.size()),"<audit>");
        auto object=checked(llvm::object::ObjectFile::createObjectFile(buffer));
        report.architecture=llvm::Triple::getArchTypeName(object->getArch()).str();
        if (auto* e=llvm::dyn_cast<llvm::object::ELFObjectFileBase>(object.get())) {
            (void)e;report.format="ELF";elf(bytes,report);
        } else if(auto* pe=llvm::dyn_cast<llvm::object::COFFObjectFile>(object.get())) {
            report.format="PE";
            if(!pe->getPE32PlusHeader()) {finding("unsupported-format","Require a PE32+ image.");return report;}
            if(!(pe->getCharacteristics() & llvm::COFF::IMAGE_FILE_EXECUTABLE_IMAGE)) throw Invalid("PE image lacks executable-image characteristic.");
            report.image_kind=pe->getCharacteristics() & llvm::COFF::IMAGE_FILE_DLL?"shared":"executable";
            std::size_t budget=0;
            for(const auto& dependency:pe->import_directories()) {count(budget);llvm::StringRef name;check(dependency.getName(name));if(name.size()>4096)throw Limited("PE import name budget exceeded.");report.dependencies.push_back(name.str());}
            for(const auto& dependency:pe->delay_import_directories()) {count(budget);llvm::StringRef name;check(dependency.getName(name));if(name.size()>4096)throw Limited("PE delay import name budget exceeded.");report.dependencies.push_back(name.str());}
            report.debug=!pe->debug_directories().empty();
        } else if(auto* m=llvm::dyn_cast<llvm::object::MachOObjectFile>(object.get())) {report.format="Mach-O";macho(*m,bytes,report);}
        else {finding("unsupported-format","Object is outside supported image encodings.");return report;}
        std::size_t sections=0;
        for(const auto& section:object->sections()) {
            count(sections);auto name=checked(section.getName());
            if(section.isDebugSection() || name.starts_with(".debug") || name.starts_with(".zdebug") || name.starts_with("__debug") || name==".gnu_debuglink" || name==".gnu_debugdata") report.debug=true;
            if(name==".symtab" || name==".dynsym") report.symbols=true;
            if(!section.isBSS() && !section.isVirtual()) {auto contents=checked(section.getContents());(void)contents;}
        }
        std::size_t symbols=0;
        for(const auto& symbol:object->symbols()) {count(symbols);(void)checked(symbol.getName());report.symbols=true;}
        report.inspected=true;
        const auto expected=policy.target=="macos-arm64"?"aarch64":"x86_64";
        const auto expected_format=policy.target=="linux-x64"?"ELF":policy.target=="windows-x64"?"PE":"Mach-O";
        if(report.architecture!=expected || report.format!=expected_format) finding("target-mismatch","Image format or architecture differs from policy target.");
        if(policy.reject_debug && report.debug) finding("debug-metadata","Debug sections or external debug declarations are present; compressed contents are not decoded.");
        if(policy.reject_symbols && report.symbols) finding("symbol-table","Symbol metadata is present; imports/exports are not classified for removal.");
        if(policy.reject_loader_paths) for(const auto& path:report.loader_paths) { (void)path;finding("loader-path","Declared loader search path is forbidden by this policy."); }
        // Scan full bounded bytes, including ordinary data. Offsets only: never
        // echo potentially secret canaries or surrounding source text.
        for(std::size_t i=0;i<policy.forbidden_strings.size();++i) {
            const auto& s=policy.forbidden_strings[i];auto offset=bytes.find(s);
            if(offset!=bytes.npos)finding("forbidden-string","Canary index "+std::to_string(i)+" (byte form)",offset);
            std::string wide;for(unsigned char c:s){wide+=char(c);wide+='\0';}
            offset=bytes.find(wide);
            if(offset!=bytes.npos)finding("forbidden-string","Canary index "+std::to_string(i)+" (UTF-16LE ASCII form)",offset);
        }
    } catch(const Limited& error) {report.inspected=false;finding("limit",error.what());}
      catch(const Invalid& error) {report.inspected=false;finding("malformed-image",error.what());}
    return report;
}
std::string json(const Report& r) {
    std::string result;llvm::raw_string_ostream stream(result);
    llvm::json::OStream out(stream);
    out.object([&] {
        out.attribute("schema",r.schema);out.attribute("scope","image-declarations-only");out.attribute("inspected",r.inspected);
        out.attribute("format",r.format);out.attribute("architecture",r.architecture);out.attribute("image_kind",r.image_kind);out.attribute("sha256",r.sha256);out.attribute("interpreter",r.interpreter);
        out.attribute("symbols",r.symbols);out.attribute("debug",r.debug);
        auto strings=[&](const char* key,const auto& values){out.attributeArray(key,[&]{for(const auto& v:values)out.value(v);});};
        strings("dependencies",r.dependencies);strings("versions",r.versions);strings("loader_paths",r.loader_paths);
        out.attributeArray("findings",[&]{for(const auto& f:r.findings)out.object([&]{out.attribute("code",f.code);out.attribute("detail",f.detail);out.attribute("offset",f.offset);});});
    });
    return result;
}
}
