#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <psapi.h>
#include <sddl.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
constexpr auto name = L"void_w2_gate.dll";
constexpr DWORD flags = LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32;
struct Refused { const char* stage; };
void check(bool okay, const char* step) {
    if (!okay) throw std::runtime_error(std::string(step) + ", Win32 " + std::to_string(GetLastError()));
}
struct Handle {
    HANDLE value;
    explicit Handle(HANDLE h) : value(h) {}
    Handle(Handle&& h) noexcept : value(std::exchange(h.value,nullptr)) {}
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    explicit operator bool() const { return value && value != INVALID_HANDLE_VALUE; }
};
Handle open_file(const std::wstring& path, DWORD access, DWORD share, bool directory = false) {
    return Handle(CreateFileW(path.c_str(),access,share,nullptr,OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT | (directory ? FILE_FLAG_BACKUP_SEMANTICS : 0),nullptr));
}
BY_HANDLE_FILE_INFORMATION attributes(const Handle& file) {
    BY_HANDLE_FILE_INFORMATION info{};
    check(file && GetFileInformationByHandle(file.value,&info),"file attributes");
    return info;
}
std::wstring final_name(const Handle& file, DWORD format) {
    wchar_t output[1024];
    const auto size = GetFinalPathNameByHandleW(file.value,output,1024,format);
    check(size > 0 && size < 1024,"canonical held name");
    return {output,size};
}
std::vector<Handle> hold_directories(const std::wstring& path) {
    wchar_t absolute[1024];
    const auto count = GetFullPathNameW(path.c_str(),1024,absolute,nullptr);
    if (!count || count >= 1024 || path != absolute || !path.starts_with(L"C:\\"))
        throw Refused{"path alias"};
    std::vector<Handle> held;
    for (auto end = path.find(L'\\',3); end != path.npos; end = path.find(L'\\',end+1)) {
        const auto parent = path.substr(0,end);
        auto file = open_file(parent,FILE_READ_ATTRIBUTES,FILE_SHARE_READ | FILE_SHARE_WRITE,true);
        const auto info = attributes(file);
        if (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) throw Refused{"reparse ancestor"};
        if (!(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
            final_name(file,VOLUME_NAME_DOS) != L"\\\\?\\"+parent) throw Refused{"directory alias"};
        held.push_back(std::move(file));
    }
    return held;
}
std::wstring sha256(const Handle& file) {
    LARGE_INTEGER size{};
    check(GetFileSizeEx(file.value,&size) && size.QuadPart > 0 && size.QuadPart < 1024*1024,"fixture size");
    std::vector<UCHAR> bytes(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    check(ReadFile(file.value,bytes.data(),static_cast<DWORD>(bytes.size()),&read,nullptr) &&
          read == bytes.size(),"read held bytes");
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    check(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0) >= 0,"open CNG SHA-256");
    std::array<UCHAR,32> digest{};
    const auto status = BCryptHash(algorithm,nullptr,0,bytes.data(),static_cast<ULONG>(bytes.size()),
                                   digest.data(),static_cast<ULONG>(digest.size()));
    BCryptCloseAlgorithmProvider(algorithm,0);
    check(status >= 0,"CNG SHA-256");
    std::wstring result;
    constexpr std::wstring_view hex = L"0123456789abcdef";
    for (auto byte : digest) { result.push_back(hex[byte >> 4]); result.push_back(hex[byte & 15]); }
    return result;
}

struct InspectView {
    Handle section;
    const unsigned char* image;
    explicit InspectView(const Handle& file)
        : section(CreateFileMappingW(file.value,nullptr,PAGE_READONLY|SEC_IMAGE_NO_EXECUTE,0,0,nullptr)),
          image(section ? static_cast<const unsigned char*>(MapViewOfFile(section.value,FILE_MAP_READ,0,0,0)) : nullptr) {
        check(image != nullptr,"non-executing held image view");
    }
    ~InspectView() { UnmapViewOfFile(image); }
};
DWORD inspect(const Handle& file) {
    InspectView view(file);
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(view.image);
    check(dos->e_magic == IMAGE_DOS_SIGNATURE && dos->e_lfanew >= 64 && dos->e_lfanew < 2048,"fixture DOS");
    auto* pe = reinterpret_cast<const IMAGE_NT_HEADERS64*>(view.image+dos->e_lfanew);
    check(pe->Signature == IMAGE_NT_SIGNATURE && pe->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 &&
          pe->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC,"fixture PE64");
    const auto& header = pe->OptionalHeader;
    if (header.AddressOfEntryPoint) throw Refused{"entry point"};
    for (auto entry : {IMAGE_DIRECTORY_ENTRY_IMPORT,IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT,
                       IMAGE_DIRECTORY_ENTRY_TLS,IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG}) {
        const auto& dir = header.DataDirectory[entry];
        if (dir.VirtualAddress || dir.Size) throw Refused{"execution-bearing PE directory"};
    }
    const auto dir = header.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    auto bounded = [&](DWORD rva, size_t bytes) {
        check(rva && rva < header.SizeOfImage && bytes <= header.SizeOfImage-rva,"fixture RVA");
        return view.image+rva;
    };
    bounded(dir.VirtualAddress,dir.Size);
    const auto* table = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(
        bounded(dir.VirtualAddress,sizeof(IMAGE_EXPORT_DIRECTORY)));
    check(table->NumberOfFunctions <= 32 && table->NumberOfNames <= 32,"fixture export count");
    const auto* functions = reinterpret_cast<const DWORD*>(bounded(table->AddressOfFunctions,
        table->NumberOfFunctions*sizeof(DWORD)));
    for (DWORD n=0;n<table->NumberOfFunctions;++n)
        if (functions[n] >= dir.VirtualAddress && functions[n]-dir.VirtualAddress < dir.Size)
            throw Refused{"forwarded export"};
    const auto* names = reinterpret_cast<const DWORD*>(bounded(table->AddressOfNames,table->NumberOfNames*sizeof(DWORD)));
    const auto* ordinals = reinterpret_cast<const WORD*>(bounded(table->AddressOfNameOrdinals,table->NumberOfNames*sizeof(WORD)));
    for (DWORD n=0;n<table->NumberOfNames;++n) {
        constexpr char symbol[] = "g4_dependency";
        if (!std::memcmp(bounded(names[n],sizeof(symbol)),symbol,sizeof(symbol))) {
            check(ordinals[n]<table->NumberOfFunctions,"fixture export ordinal");
            bounded(functions[ordinals[n]],1);
            return functions[ordinals[n]];
        }
    }
    throw Refused{"absent export"};
}

bool redirect() {
    ACTCTX_SECTION_KEYED_DATA data{};
    data.cbSize = sizeof(data);
    if (FindActCtxSectionStringW(0,nullptr,ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION,name,&data)) return true;
    check(GetLastError()==ERROR_SXS_KEY_NOT_FOUND || GetLastError()==ERROR_SXS_SECTION_NOT_FOUND,"activation query");
    return false;
}

struct Activation {
    HANDLE context = INVALID_HANDLE_VALUE;
    ULONG_PTR cookie = 0;
    explicit Activation(const std::wstring& path) {
        ACTCTXW input{};
        input.cbSize = sizeof(input);
        input.lpSource = path.c_str();
        context = CreateActCtxW(&input);
        check(context != INVALID_HANDLE_VALUE && ActivateActCtx(context,&cookie),"set test activation");
    }
    ~Activation() { DeactivateActCtx(0,cookie); ReleaseActCtx(context); }
};

struct Approved {
    std::vector<Handle> directories;
    Handle file;
    DWORD export_rva;
    Approved(const std::wstring& path, std::wstring_view expected)
        : directories(hold_directories(path)), file(open_file(path,GENERIC_READ,FILE_SHARE_READ)),
          export_rva(0) {
        const auto info = attributes(file);
        if (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) throw Refused{"reparse image"};
        if (info.nNumberOfLinks != 1) throw Refused{"hardlinked image"};
        if (final_name(file,VOLUME_NAME_DOS) != L"\\\\?\\"+path) throw Refused{"image alias"};
        const auto digest = sha256(file);
        std::printf("held file=%ls id=%08lx:%08lx%08lx sha256=%ls\n",path.c_str(),
                    info.dwVolumeSerialNumber,info.nFileIndexHigh,info.nFileIndexLow,digest.c_str());
        if (expected != digest) throw Refused{"image digest"};
        export_rva = inspect(file);
    }
};

void namespace_guard() {
    const bool active = redirect(), loaded = GetModuleHandleW(name)!=nullptr;
    const bool manifest = FindResourceW(nullptr,MAKEINTRESOURCEW(1),MAKEINTRESOURCEW(24))!=nullptr;
    std::printf("embedded manifest=%d active redirect=%d loaded basename=%d\n",manifest,active,loaded);
    if (active) throw Refused{"active manifest redirect"};
    if (loaded) throw Refused{"loaded basename"};
}
void execute(const std::wstring& path, const Approved& held) {
    HMODULE module = LoadLibraryExW(path.c_str(),nullptr,flags);
    check(module != nullptr,"executable load");
    wchar_t actual[1024];
    const auto size=GetMappedFileNameW(GetCurrentProcess(),module,actual,1024);
    MEMORY_BASIC_INFORMATION memory{};
    check(size && size < 1024 && VirtualQuery(module,&memory,sizeof(memory)) && memory.Type==MEM_IMAGE &&
          std::wstring_view(actual,size)==final_name(held.file,VOLUME_NAME_NT),"post-load mapped image");
    auto address = GetProcAddress(module,"g4_dependency");
    check(address && reinterpret_cast<const char*>(address) ==
          reinterpret_cast<const char*>(module)+held.export_rva,"mapped export ownership");
    const int value = reinterpret_cast<int(*)()>(address)();
    std::printf("mapped exact path=%ls result=%d\n",actual,value);
    check(value==7 && FreeLibrary(module),"approved result");
}

std::wstring user_sid(const Handle& token) {
    DWORD length=0;
    GetTokenInformation(token.value,TokenUser,nullptr,0,&length);
    std::vector<unsigned char> data(length);
    check(length && GetTokenInformation(token.value,TokenUser,data.data(),length,&length),"token SID");
    LPWSTR text=nullptr;
    check(ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(data.data())->User.Sid,&text),"SID string");
    std::wstring result(text);
    LocalFree(text);
    return result;
}
void acl_test(const std::wstring& directory, const std::wstring& path) {
    HANDLE primary=nullptr;
    check(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY|TOKEN_DUPLICATE,&primary),"open fixture owner token");
    Handle owner(primary);
    const auto acl=L"D:P(A;;FA;;;"+user_sid(owner)+L")(A;;GRGX;;;RC)(A;;GRGX;;;OW)";
    PSECURITY_DESCRIPTOR sd=nullptr;
    check(ConvertStringSecurityDescriptorToSecurityDescriptorW(acl.c_str(),SDDL_REVISION_1,&sd,nullptr),
          "fixture DACL");
    const bool protected_acl = SetFileSecurityW(directory.c_str(),
        DACL_SECURITY_INFORMATION|PROTECTED_DACL_SECURITY_INFORMATION,sd) &&
        SetFileSecurityW(path.c_str(),DACL_SECURITY_INFORMATION|PROTECTED_DACL_SECURITY_INFORMATION,sd);
    LocalFree(sd);
    check(protected_acl,"protect assignment-only objects");
    unsigned char sid[SECURITY_MAX_SID_SIZE];
    DWORD sid_size=sizeof(sid);
    check(CreateWellKnownSid(WinRestrictedCodeSid,nullptr,sid,&sid_size),"restricting SID");
    SID_AND_ATTRIBUTES restricting{sid,0};
    HANDLE narrowed=nullptr;
    check(CreateRestrictedToken(owner.value,DISABLE_MAX_PRIVILEGE|WRITE_RESTRICTED,
        0,nullptr,0,nullptr,1,&restricting,&narrowed),"restricted token");
    Handle writer(narrowed);
    check(ImpersonateLoggedOnUser(writer.value),"impersonate fixture writer");
    const auto share=FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE;
    auto read=open_file(path,GENERIC_READ,share);
    auto write=open_file(path,GENERIC_WRITE,share);
    auto erase=open_file(path,DELETE,share);
    auto change=open_file(path,WRITE_DAC,share);
    auto add=open_file(directory,FILE_ADD_FILE,share,true);
    check(RevertToSelf(),"leave restricted token");
    std::printf("restricted token read=%d write=%d delete=%d change-DACL=%d add-file=%d\n",
        bool(read),bool(write),bool(erase),bool(change),bool(add));
    check(read && !write && !erase && !change && !add,"scratch DACL separates this writer");
}
}

int wmain(int argc, wchar_t** argv) try {
    if (argc!=5) return 2;
    const std::wstring_view mode=argv[1], digest=argv[4];
    const std::wstring path=argv[2], other=argv[3];
    check(SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32),"process search policy");

    if (mode==L"acl") { acl_test(other,path); return 0; }
    const char* expected=nullptr;
    if (mode==L"alias") expected="path alias";
    if (mode==L"junction") expected="reparse ancestor";
    if (mode==L"symlink") expected="reparse image";
    if (mode==L"hardlink") expected="hardlinked image";
    if (mode==L"forward") expected="forwarded export";
    if (mode==L"guarded-substitute") expected="image digest";
    if (mode==L"late") expected="loaded basename";
    if (mode==L"thread" || mode==L"external" || mode==L"embedded") expected="active manifest redirect";
    if (mode==L"unguarded-substitute" || mode==L"guarded-substitute") {
        { Approved original(path,digest); }
        check(MoveFileExW(other.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING),"swap after releasing validation handle");
        std::puts("approved no-entry image replaced after dropping the held identity");
        if (mode==L"unguarded-substitute") {
            std::fflush(stdout);
            LoadLibraryExW(path.c_str(),nullptr,flags);
            throw std::runtime_error("unguarded load returned instead of reaching initializer sentinel");
        }
    }
    try {
        Approved held(path,digest);
        if (mode==L"pin") {
            const auto parent=path.substr(0,path.find_last_of(L'\\'));
            const BOOL replaced=MoveFileExW(other.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING);
            const DWORD replace_error=replaced ? 0 : GetLastError();
            const BOOL renamed=MoveFileExW(parent.c_str(),(parent+L"-renamed").c_str(),0);
            const DWORD rename_error=renamed ? 0 : GetLastError();
            std::printf("pinned replacement error=%lu ancestor rename error=%lu\n",replace_error,rename_error);
            check(!replaced && !renamed,"pins refused rename and replacement");
        }
        if (mode==L"late") {
            namespace_guard();
            std::puts("initial namespace observation clear");
            auto loaded=LoadLibraryExW(other.c_str(),nullptr,flags);
            check(loaded!=nullptr,"load controlled no-entry collision");
        }
        if (mode==L"thread") {
            Activation active(other);
            namespace_guard();
        } else namespace_guard();
        check(!expected,"negative was accepted before executable load");
        execute(path,held);
    } catch (const Refused& error) {
        check(expected && std::string_view(expected)==error.stage,"expected denial stage");
        std::printf("denied before executable load: %s\n",error.stage);
    }
    return 0;
} catch (const std::exception& error) {
    std::fprintf(stderr,"%s\n",error.what());
    return 1;
}
