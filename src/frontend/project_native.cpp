#include "project_native.hpp"
#include "project_files.hpp"
#include "void/path.hpp"
#include <llvm/BinaryFormat/COFF.h>
#include <llvm/BinaryFormat/ELF.h>
#include <llvm/Object/Archive.h>
#include <llvm/Object/COFF.h>
#include <llvm/Object/COFFImportFile.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Support/Process.h>
#include <llvm/Support/SHA256.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Program.h>
#include <fstream>
#include <set>

namespace vd::project {
namespace {
namespace fs = std::filesystem;

std::string native_hash(std::string_view bytes) {
    const auto digest = llvm::SHA256::hash({reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()});
    std::string hash;
    constexpr char hex[] = "0123456789abcdef";
    for (auto byte : digest) { hash += hex[byte >> 4]; hash += hex[byte & 15]; }
    return hash;
}

std::string verified_file(const fs::path& directory, const std::string& name, std::string_view expected_hash,
                          std::uint64_t size) {
    auto bytes = read_project_input(directory / utf8_path(name), static_cast<std::size_t>(size));
    if (bytes.size() != size || native_hash(bytes) != expected_hash)
        throw std::runtime_error("Managed native artifact identity mismatch: " + name);
    return bytes;
}

std::string verified_file(const fs::path& directory, const NativeArtifact& file) {
    return verified_file(directory, file.name, file.sha256, file.size);
}

void verify_image(std::string_view bytes, const std::string& name, bool windows) {
    const llvm::StringRef data(bytes.data(), bytes.size());
    auto file = llvm::object::ObjectFile::createObjectFile({data, name});
    if (!file) llvm::consumeError(file.takeError());
    else if ((*file)->getArch() == llvm::Triple::x86_64) {
        if (windows) {
            const auto* image = llvm::dyn_cast<llvm::object::COFFObjectFile>(file->get());
            if (image && image->getPE32PlusHeader() && (image->getCharacteristics() & llvm::COFF::IMAGE_FILE_DLL)) return;
        } else if (const auto* image = llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(file->get())) {
            const auto& header = image->getELFFile().getHeader();
            const auto os = header.e_ident[llvm::ELF::EI_OSABI];
            if (header.e_type == llvm::ELF::ET_DYN &&
                (os == llvm::ELF::ELFOSABI_NONE || os == llvm::ELF::ELFOSABI_GNU)) return;
        }
    }
    throw std::runtime_error("Managed native artifact has the wrong target or image type: " + name);
}

bool compatible_link_object(const llvm::object::Binary& binary, bool windows, std::string_view exported = {}) {
    using namespace llvm::object;
    if (windows) {
        if (const auto* import = llvm::dyn_cast<COFFImportFile>(&binary))
            return import->getData().size() >= sizeof(coff_import_header) &&
                import->getMachine() == llvm::COFF::IMAGE_FILE_MACHINE_AMD64;
        if (!llvm::isa<COFFObjectFile>(binary)) return false;
    } else if (!llvm::isa<ELF64LEObjectFile>(binary)) return false;

    const auto& object = llvm::cast<ObjectFile>(binary);
    if (!object.isRelocatableObject() || object.getArch() != llvm::Triple::x86_64) return false;
    // The selected input set is explicit. Keep object directives from adding files.
    for (const auto& section : object.sections()) {
        if (!windows) {
            const auto type = ELFSectionRef(section).getType();
            if (type == llvm::ELF::SHT_LLVM_DEPENDENT_LIBRARIES || type == llvm::ELF::SHT_LLVM_LINKER_OPTIONS)
                return false;
            continue;
        }
        auto name = section.getName();
        if (!name) {
            llvm::consumeError(name.takeError());
            return false;
        }
        if (*name == ".drectve") {
            auto contents=section.getContents();
            if(!contents) {llvm::consumeError(contents.takeError());return false;}
            if(exported.empty() || contents->trim()!="/EXPORT:"+std::string(exported))
                return false;
        }
    }
    return true;
}

bool compatible_link_input(llvm::MemoryBufferRef data, bool windows) {
    using namespace llvm::object;
    auto binary = createBinary(data);
    if (!binary) {
        llvm::consumeError(binary.takeError());
        return false;
    }
    const auto* archive = llvm::dyn_cast<Archive>(binary->get());
    if (!archive) return compatible_link_object(**binary, windows);
    if (archive->isThin()) return false;
    auto error = llvm::Error::success();
    bool has_members = false, compatible = true;
    for (const auto& child : archive->children(error)) {
        has_members = true;
        auto member = child.getAsBinary();
        if (!member) {
            llvm::consumeError(member.takeError());
            compatible = false;
            break;
        }
        if (!compatible_link_object(**member, windows)) {
            compatible = false;
            break;
        }
    }
    if (error) {
        llvm::consumeError(std::move(error));
        return false;
    }
    return has_members && compatible;
}

constexpr std::string_view managed_native_target() {
#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
    return "x86_64-pc-windows-msvc";
#elif defined(__linux__) && defined(__GLIBC__) && defined(__x86_64__)
    return "x86_64-unknown-linux-gnu";
#else
    return {};
#endif
}

struct NativeBuildDirectory {
    fs::path path;
    NativeBuildDirectory() {
        llvm::SmallString<128> temporary;
        if(auto error=llvm::sys::fs::createUniqueDirectory("void-package-native",temporary))
            throw std::system_error(error);
        path=utf8_path(temporary.str().str());
    }
    ~NativeBuildDirectory() {std::error_code ignored;fs::remove_all(path,ignored);}
};

void write_native_input(const fs::path& path,std::string_view bytes) {
    std::ofstream output(path,std::ios::binary);
    output.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));
    output.close();
    if(!output)throw std::runtime_error("Cannot stage declarative native build inputs.");
}

void run_native_build(const std::vector<std::string>& args,const fs::path& capture={}) {
    const std::vector<llvm::StringRef> refs(args.begin(),args.end());
    const auto filename=path_utf8(capture);
    std::vector<std::optional<llvm::StringRef>> redirects;
    if(!capture.empty())redirects={std::nullopt,filename,std::nullopt};
    std::string error;
    if(llvm::sys::ExecuteAndWait(args.front(),refs,std::nullopt,redirects,60,0,&error)!=0)
        throw std::runtime_error("Declarative native build failed. "+error);
}

}

const NativeRequirement& verified_raylib_requirement() {
    static const NativeRequirement requirement{"raylib", "org.voidlang.raylib", *parse_version("6.0.0"), 3};
    return requirement;
}

ProjectFiles prepare_native_package(const ProjectFiles& files,const NativeRequirement& requirement,
                                    const fs::path& clang,bool sanitize) {
    constexpr auto target=managed_native_target();
    if(target.empty())throw std::runtime_error("Declarative native packages are unsupported on this host.");
    const bool windows=target=="x86_64-pc-windows-msvc";
    const std::string descriptor_name=windows?"provider-windows.descriptor":"provider-linux.descriptor";
    const std::string library=windows?"void_provider.dll":"libvoid_provider.so";
    auto input=[&](const std::string& name,std::size_t limit) -> const std::string& {
        const auto source=files.find("native/"+name);
        if(source==files.end() || source->second.empty() || source->second.size()>limit)
            throw std::runtime_error("Missing or oversized declarative native input: native/"+name);
        return source->second;
    };
    const auto& descriptor=input(descriptor_name,c_conventional_descriptor_limit);
    const auto& notice=input("NOTICE.txt",256*1024);
    const auto adapter=CPrimitiveAdmission::generate_adapter(descriptor,"provider.h");
    NativeBuildDirectory work;
    const auto directory=fs::canonical(work.path);
    // Admit descriptor identity and physical declarations before using Clang.
    const auto admitted=CPrimitiveAdmission::admit_conventional(descriptor,directory,library,library);
    if(!admitted->physical_source() || admitted->provider()!=requirement.id || admitted->target()!=target ||
       admitted->abi_revision()!=requirement.abi || !files.contains(admitted->module().substr(1)))
        throw std::runtime_error("Declarative native descriptor must match the package provider, target, ABI and physical source.");
    write_native_input(directory/"provider.c",input("provider.c",1024*1024));
    write_native_input(directory/"provider.h",input("provider.h",1024*1024));
    write_native_input(directory/"adapter.c",adapter);
    const auto driver=path_utf8(clang);
    const auto linker=windows?clang.parent_path()/"lld-link.exe":clang.parent_path()/"ld.lld";
    const auto version_file=directory/"version.txt";
    if(!clang.is_absolute() || !fs::is_regular_file(linker))
        throw std::runtime_error("Declarative native preparation requires the configured Clang and matching LLD.");
    run_native_build({driver,"--version"},version_file);
    if(!read_project_input(version_file,4096).starts_with("clang version 23.1.4 "))
        throw std::runtime_error("Declarative native preparation requires Clang 23.1.4.");
    run_native_build({path_utf8(linker),"--version"},version_file);
    if(!read_project_input(version_file,4096).starts_with("LLD 23.1.4 "))
        throw std::runtime_error("Declarative native preparation requires LLD 23.1.4.");

    const std::vector<std::string> driver_options{driver,"--no-default-config","--target="+std::string(target),
        "-working-directory",path_utf8(directory)};
    for(const std::string unit:{"provider","adapter"}) {
        auto args=driver_options;
        args.insert(args.end(),{"-std=c11","-O2","-fno-autolink","-fno-asm","-c",unit+".c","-o",unit+".o"});
        if(windows)args.push_back("-fms-runtime-lib=static");
        else args.push_back("-fPIC");
        if(sanitize && !windows)
            args.insert(args.end(),{"-fsanitize=address,undefined","-fno-sanitize-recover=all"});
        run_native_build(args);
        const auto bytes=read_project_input(directory/(unit+".o"),16*1024*1024);
        auto object=llvm::object::ObjectFile::createObjectFile({llvm::StringRef(bytes),unit});
        if(!object) {llvm::consumeError(object.takeError());throw std::runtime_error("Invalid generated native object.");}
        if(!compatible_link_object(**object,windows,unit=="adapter"?admitted->transport():std::string_view{}))
            throw std::runtime_error("Declarative native object contains unsupported linker directives.");
    }
    auto link=driver_options;
    link.insert(link.end(),{"-shared","provider.o","adapter.o","-o",library});
    if(windows) {
        link.insert(link.end(),{"-B"+path_utf8(linker.parent_path()),"-fuse-ld=lld","-fms-runtime-lib=static",
            "-Wl,/Brepro,/defaultlib:libcmt,/defaultlib:libucrt,/defaultlib:libvcruntime"});
        for(const auto& binding:admitted->bindings())link.insert(link.end(),{"-Xlinker","/EXPORT:"+binding.export_name});
    } else {
        link.push_back("--ld-path="+path_utf8(linker));
    }
    if(!windows)link.push_back(sanitize?"-fsanitize=address,undefined":"-Wl,--no-undefined");
    run_native_build(link);

    const auto image=read_project_input(directory/library,16*1024*1024);
    verify_image(image,library,windows);
    const auto identity=[](std::string_view bytes) {return std::to_string(bytes.size())+":"+native_hash(bytes);};
    const auto catalog="format = 1\n[native]\nid = \""+requirement.id+"\"\nversion = \""+
        requirement.version.text()+"\"\nabi = "+std::to_string(requirement.abi)+"\ntarget = \""+std::string(target)+
        "\"\nrevision = \"void.c11.v1\"\ndescriptor = \""+descriptor_name+"\"\ndescriptor-identity = \""+
        identity(descriptor)+"\"\nlibrary = \""+library+"\"\nadapter = \""+library+
        "\"\n[native.runtime]\n\""+library+"\" = \""+identity(image)+
        "\"\n[native.link]\n[native.notices]\n\"NOTICE.txt\" = \""+identity(notice)+"\"\n";
    const std::string prefix=std::string(verified_raylib_catalog)+"/";
    return {{".void/",""},{prefix,""},{prefix+descriptor_name,descriptor},{prefix+library,image},
            {prefix+"NOTICE.txt",notice},{prefix+"void.native.toml",catalog}};
}

SelectedNative select_managed_native(const Manifest& manifest, const fs::path& project_root) {
    const auto& requirement = *manifest.native_requirement();
    constexpr auto target=managed_native_target();
    if (target.empty()) throw std::runtime_error("Managed native providers are unsupported on this host.");
    fs::path selected;
    const auto& verified = verified_raylib_requirement();
    const auto dependency = manifest.dependencies().find(requirement.alias);
    const auto local_package=project_root/"libs"/utf8_path(requirement.alias);
    if(dependency!=manifest.dependencies().end() && dependency->second.id==requirement.id &&
       (dependency->second.git || dependency->second.path=="libs/"+requirement.alias) &&
       fs::exists(fs::symlink_status(local_package/utf8_path(std::string(native_build_recipe))))) {
        const auto recipe=read_project_input(local_package/utf8_path(std::string(native_build_recipe)),4096);
        if(parse_native_build_recipe(recipe)!=requirement.version || requirement.abi!=3)
            throw std::runtime_error("Declarative native recipe differs from the application requirement.");
        selected=local_package/verified_raylib_catalog;
    }
    if (requirement.alias == verified.alias && requirement.id == verified.id &&
        requirement.version == verified.version && requirement.abi == verified.abi &&
        dependency != manifest.dependencies().end() && !dependency->second.git &&
        dependency->second.id == verified.id && dependency->second.path == "libs/raylib" &&
        dependency->second.version.text() == "=0.1.0") {
        const auto local = project_root / "libs/raylib" / verified_raylib_catalog;
        // An installed catalog (including an invalid path) never falls back to
        // a different provider. Missing local catalogs retain the manual D4 route.
        if (fs::exists(fs::symlink_status(local)) || project_path_alias(local)) selected = local;
    }
    if (selected.empty()) {
        const auto environment = llvm::sys::Process::GetEnv("VOID_NATIVE_CATALOG");
        if (environment) selected = utf8_path(*environment);
        if (!selected.is_absolute())
            throw std::runtime_error("No native catalog selected for '" + requirement.id +
                "' " + requirement.version.text() + " (ABI " + std::to_string(requirement.abi) +
                "). Install the bundled raylib with 'voidc add raylib' or select an approved "
                "manual catalog with the absolute VOID_NATIVE_CATALOG path.");
    }
    const auto directory = inspect_creation_root(selected).path;
    const auto catalog_bytes = read_project_input(directory / "void.native.toml", native_catalog_byte_limit);
    auto plan = parse_native_catalog(catalog_bytes);
    if (plan.target != target)
        throw std::runtime_error("Managed native catalog target '" + plan.target +
            "' does not match this host ('" + std::string(target) + "').");
    if (plan.id != requirement.id)
        throw std::runtime_error("Managed native catalog provider ID '" + plan.id +
            "' does not match required ID '" + requirement.id + "'.");
    if (plan.version != requirement.version)
        throw std::runtime_error("Managed native catalog version '" + plan.version.text() +
            "' does not match required exact version '" + requirement.version.text() + "'.");
    if (plan.abi != requirement.abi)
        throw std::runtime_error("Managed native catalog ABI " + std::to_string(plan.abi) +
            " does not match required ABI " + std::to_string(requirement.abi) + ".");

    std::set<std::string> names{"void.native.toml", plan.descriptor.name};
    for (const auto* role : {&plan.runtime, &plan.link, &plan.notices})
        for (const auto& file : *role) names.insert(file.name);
    for (const auto& entry : fs::directory_iterator(directory)) {
        const auto name = path_utf8(entry.path().filename());
        if (!names.erase(name)) throw std::runtime_error("Unexpected managed native catalog entry: " + name);
    }
    if (!names.empty()) throw std::runtime_error("Missing managed native artifact: " + *names.begin());

    const auto descriptor = verified_file(directory, plan.descriptor);
    const bool windows = target == "x86_64-pc-windows-msvc";
    for (const auto& file : plan.runtime) {
        if (windows ? !file.name.ends_with(".dll") : file.name.find(".so") == file.name.npos)
            throw std::runtime_error("Invalid managed native runtime filename: " + file.name);
        verify_image(verified_file(directory, file), file.name, windows);
    }
    for (const auto* role : {&plan.link, &plan.notices})
        for (const auto& file : *role) (void)verified_file(directory, file);
    std::vector<DynamicClosure::Payload> payload;
    for (const auto* role : {&plan.runtime, &plan.notices})
        for (const auto& file : *role) payload.push_back({file.name, file.sha256, file.size});
    auto admission = CPrimitiveAdmission::admit_conventional(descriptor, directory, plan.library, plan.adapter, payload);
    if (admission->provider() != plan.id || admission->target() != plan.target || admission->abi_revision() != plan.abi)
        throw std::runtime_error("Managed native descriptor does not match the approved provider, target or ABI.");

    return {std::move(plan), std::move(admission), native_hash(catalog_bytes)};
}

std::map<std::string, std::string> read_managed_payload(const CPrimitiveAdmission& native) {
    std::map<std::string, std::string> files;
    for (const auto& file : native.dynamic()->payload())
        files.emplace(file.name, verified_file(native.dynamic_root(), file.name, file.sha256, file.size));
    return files;
}

std::map<std::string, std::string> read_managed_link_inputs(const CPrimitiveAdmission& native,
                                                          const std::vector<NativeArtifact>& inputs) {
    std::map<std::string, std::string> files;
    const bool windows = native.target() == "x86_64-pc-windows-msvc";
    for (const auto& file : inputs) {
        auto bytes = verified_file(native.dynamic_root(), file);
        if (!compatible_link_input({llvm::StringRef(bytes), file.name}, windows))
            throw std::runtime_error("Incompatible managed native link input for " + std::string(native.target()) +
                ": " + file.name + " (expected a self-contained native object or archive).");
        files.emplace(file.name, std::move(bytes));
    }
    return files;
}
}
