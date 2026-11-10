#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

#include "explicit_abi.h"

namespace {
constexpr auto dependency_name = L"void_g4_probe_dep.dll";
constexpr auto provider_name = L"void_g4_probe_main.dll";
constexpr DWORD restricted = LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32;

[[noreturn]] void fail(const char* what) {
    std::printf("probe failed: %s (Win32 %lu)\n",what,GetLastError());
    ExitProcess(1);
}
void check(bool condition, const char* what) {
    if (!condition) fail(what);
}

struct File {
    HANDLE value;
    explicit File(const std::wstring& path, bool directory = false)
        : value(CreateFileW(path.c_str(),directory ? FILE_READ_ATTRIBUTES : GENERIC_READ,
            FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT |
            (directory ? FILE_FLAG_BACKUP_SEMANTICS : 0),nullptr)) {
        check(value != INVALID_HANDLE_VALUE,"hold approved file or directory");
        BY_HANDLE_FILE_INFORMATION info{};
        check(GetFileInformationByHandle(value,&info) &&
              !(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
              (directory || info.nNumberOfLinks == 1),"ordinary single-link file");
        std::printf("held %ls id=%08lx:%08lx%08lx\n",path.c_str(),
                    info.dwVolumeSerialNumber,info.nFileIndexHigh,info.nFileIndexLow);
    }
    ~File() { CloseHandle(value); }
    File(const File&) = delete;
};
struct Image {
    HMODULE value;
    explicit Image(const std::wstring& path) : value(LoadLibraryExW(path.c_str(),nullptr,restricted)) {
        check(value != nullptr,"restricted executable load");
    }
    ~Image() { FreeLibrary(value); }
    Image(const Image&) = delete;
};

struct Activation {
    HANDLE context = INVALID_HANDLE_VALUE;
    ULONG_PTR cookie = 0;
    bool active = false;
    void from(const std::wstring& manifest) {
        ACTCTXW info{};
        info.cbSize = sizeof(info);
        info.lpSource = manifest.c_str();
        context = CreateActCtxW(&info);
        check(context != INVALID_HANDLE_VALUE,"create activation context");
        active = ActivateActCtx(context,&cookie) != 0;
        check(active,"activate context");
    }
    void process_default() {
        active = ActivateActCtx(nullptr,&cookie) != 0;
        check(active,"push process default");
    }
    ~Activation() {
        if (active) DeactivateActCtx(0,cookie);
        if (context != INVALID_HANDLE_VALUE) ReleaseActCtx(context);
    }
};
bool redirected(const wchar_t* name) {
    ACTCTX_SECTION_KEYED_DATA data{};
    data.cbSize = sizeof(data);
    if (FindActCtxSectionStringW(0,nullptr,ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION,name,&data))
        return true;
    check(GetLastError() == ERROR_SXS_KEY_NOT_FOUND || GetLastError() == ERROR_SXS_SECTION_NOT_FOUND,
          "query active DLL redirection");
    return false;
}
bool compare_path(const char* label, const void* address, const File& file) {
    wchar_t expected[1024], mapped[1024];
    MEMORY_BASIC_INFORMATION region{};
    DWORD a = GetFinalPathNameByHandleW(file.value,expected,1024,VOLUME_NAME_NT);
    DWORD b = GetMappedFileNameW(GetCurrentProcess(),const_cast<void*>(address),mapped,1024);
    check(a && a < 1024 && b && b < 1024 && VirtualQuery(address,&region,sizeof(region)),
          "mapped path and memory region");
    bool match = region.Type == MEM_IMAGE && std::wstring_view(expected,a) == std::wstring_view(mapped,b);
    std::printf("%s match=%d MEM_IMAGE=%d\n  held=%ls\n  mapped=%ls\n",label,match,region.Type == MEM_IMAGE,expected,mapped);
    return match;
}
HMODULE owning_module(const void* address) {
    HMODULE owner = nullptr;
    check(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                             GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                             static_cast<LPCWSTR>(address),&owner),"module from address");
    return owner;
}
int marker(HMODULE owner) {
    auto* sentinel = reinterpret_cast<const volatile int*>(GetProcAddress(owner,"g4_init_marker"));
    return sentinel ? *sentinel : -1;
}

// Bounded inspection of the known, separately generated probe image. This
// reads the bound IAT directly and never asks the provider for its dependency.
struct PeView {
    const unsigned char* base;
    const IMAGE_NT_HEADERS64* header;
    explicit PeView(const void* image) : base(static_cast<const unsigned char*>(image)) {
        auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        check(dos->e_magic == IMAGE_DOS_SIGNATURE && dos->e_lfanew >= 64 && dos->e_lfanew < 4096,
              "probe DOS header");
        header = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
        check(header->Signature == IMAGE_NT_SIGNATURE && header->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 &&
              header->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC,"probe PE64 header");
    }
    template<class T>
    const T* at(DWORD rva, size_t count = 1) const {
        auto size = header->OptionalHeader.SizeOfImage;
        check(rva < size && count <= (size-rva)/sizeof(T),"bounded PE view");
        return reinterpret_cast<const T*>(base+rva);
    }
    bool named(DWORD rva, const char* expected) const {
        return std::memcmp(at<char>(rva,std::strlen(expected)+1),expected,std::strlen(expected)+1) == 0;
    }
    const void* imported() const {
        const auto dir = header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        auto* rows = at<IMAGE_IMPORT_DESCRIPTOR>(dir.VirtualAddress,dir.Size/sizeof(IMAGE_IMPORT_DESCRIPTOR));
        for (DWORD n=0; n<dir.Size/sizeof(*rows) && rows[n].Name; ++n) {
            if (!named(rows[n].Name,"void_g4_probe_dep.dll")) continue;
            for (DWORD index=0; index<32; ++index) {
                const auto lookup = *at<ULONGLONG>(rows[n].OriginalFirstThunk+8*index);
                if (!lookup) break;
                if (lookup < UINT32_MAX-2 && named(static_cast<DWORD>(lookup)+2,"g4_dependency")) {
                    const auto bound = *at<ULONGLONG>(rows[n].FirstThunk+8*index);
                    return reinterpret_cast<const void*>(bound);
                }
            }
        }
        fail("dependency IAT slot");
    }
    const int* initializer_export() const {
        const auto dir = header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        auto* row = at<IMAGE_EXPORT_DIRECTORY>(dir.VirtualAddress);
        auto* names = at<DWORD>(row->AddressOfNames,row->NumberOfNames);
        auto* ordinals = at<WORD>(row->AddressOfNameOrdinals,row->NumberOfNames);
        auto* addresses = at<DWORD>(row->AddressOfFunctions,row->NumberOfFunctions);
        for (DWORD n=0; n<row->NumberOfNames; ++n)
            if (named(names[n],"g4_init_marker") && ordinals[n] < row->NumberOfFunctions)
                return at<int>(addresses[ordinals[n]]);
        fail("initializer export in held data image");
    }
};
template<class F>
F export_from(const Image& image, const char* name) {
    auto address = GetProcAddress(image.value,name);
    check(address && owning_module(reinterpret_cast<const void*>(address)) == image.value,"owned exact export");
    return reinterpret_cast<F>(address);
}

void imports(const std::wstring& root, std::wstring_view mode) {
    const auto good = root+L"\\good", bad = root+L"\\alternate";
    const auto approved = good+L"\\"+dependency_name, rogue = bad+L"\\"+dependency_name;
    const auto provider = good+L"\\"+provider_name;
    File directory(good,true), main_file(provider), dep_file(approved);
    HANDLE write = CreateFileW(approved.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,
                               nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    const DWORD write_error = write == INVALID_HANDLE_VALUE ? GetLastError() : 0;
    if (write != INVALID_HANDLE_VALUE) CloseHandle(write);
    std::printf("write-open while held error=%lu\n",write_error);
    check(write_error == ERROR_SHARING_VIOLATION,"held file denies writer");

    const bool named_before = GetModuleHandleW(dependency_name) != nullptr;
    std::printf("dependency basename loaded before setup=%d\n",named_before);
    check(!named_before,"fresh application basename");
    HMODULE preloaded = nullptr;
    if (mode == L"collision" || mode == L"late") {
        if (mode == L"late")
            std::printf("preflight names and active redirects clear=%d\n",!redirected(dependency_name));
        preloaded = LoadLibraryExW(rogue.c_str(),nullptr,restricted);
        check(preloaded != nullptr,"load controlled competitor");
    }
    if (mode == L"preload-thread") {
        preloaded = LoadLibraryExW(approved.c_str(),nullptr,restricted);
        check(preloaded && compare_path("preloaded good dependency",preloaded,dep_file),"preload exact dependency");
    }
    Activation external, use_default;
    if (mode == L"thread" || mode == L"thread-default" || mode == L"thread-deny" || mode == L"preload-thread")
        external.from(bad+L"\\override-dependency.manifest");
    if (mode == L"thread-default" || mode == L"process-default" || mode == L"process-deny")
        use_default.process_default();
    const bool redirect = redirected(dependency_name), redirect_main = redirected(provider_name);
    std::printf("active redirect provider=%d dependency=%d\n",redirect_main,redirect);
    if (mode == L"thread-deny" || mode == L"process-deny") {
        check(redirect && !GetModuleHandleW(rogue.c_str()) && !GetModuleHandleW(provider.c_str()),
              "preflight must reject before either DLL is mapped");
        std::puts("denied active import redirect before executable load; alternate and provider absent");
        return;
    }

    Image image(provider);
    check(compare_path("provider",image.value,main_file),"approved top-level image");
    const auto* actual = PeView(image.value).imported();
    auto owner = owning_module(actual);
    const bool matched = compare_path("bound IAT owner",actual,dep_file);
    const int observed = marker(owner);
    std::printf("initializer marker at first post-load observation=%d\n",observed);
    const bool expected = mode == L"clean" || mode == L"thread-default";
    check(expected == matched && observed == (expected ? -1 : 1),"expected import binding and initializer");
    if (preloaded) FreeLibrary(preloaded);
}

void data_only(const std::wstring& root) {
    auto path = root+L"\\alternate\\"+dependency_name;
    File held(path);
    HANDLE section = CreateFileMappingW(held.value,nullptr,PAGE_READONLY|SEC_IMAGE_NO_EXECUTE,0,0,nullptr);
    check(section != nullptr,"create non-executing image view");
    void* view = MapViewOfFile(section,FILE_MAP_READ,0,0,0);
    check(view != nullptr,"map non-executing image view");
    const int initial = *PeView(view).initializer_export();
    const bool registered = GetModuleHandleW(path.c_str()) != nullptr;
    std::printf("held-file image view initializer=%d registered as executable module=%d\n",initial,registered);
    check(initial == 0 && !registered,"inspection must not initialize or register the image");
    UnmapViewOfFile(view);
    CloseHandle(section);
    Image loaded(path);
    std::printf("ordinary executable load initializer=%d\n",marker(loaded.value));
    check(marker(loaded.value) == 1,"control initializer");
}
void forwarding(const std::wstring& root) {
    const auto forwarder = root+L"\\good\\void_g4w_forwarder.dll";
    const auto approved = root+L"\\good\\"+dependency_name;
    const auto rogue = root+L"\\alternate\\"+dependency_name;
    File held(forwarder), dep(approved);
    Activation external;
    external.from(root+L"\\alternate\\override-dependency.manifest");
    check(redirected(dependency_name),"expected dependency redirection");
    Image module(forwarder);
    check(compare_path("forwarder image",module.value,held),"expected forwarder");
    check(!GetModuleHandleW(rogue.c_str()),"forwarder mapped a dependency before lookup");
    std::puts("alternate absent after loading import-free image");
    auto address = GetProcAddress(module.value,"forwarded");
    check(address != nullptr,"lookup forwarded export");
    const auto* target = reinterpret_cast<const void*>(address);
    const auto owner = owning_module(target);
    const int initialized = marker(owner);
    std::printf("forwarded export initializer after GetProcAddress=%d\n",initialized);
    check(!compare_path("forwarded export owner",target,dep) && initialized == 1,
          "forwarder must resolve alternate in this negative control");
}
void explicit_table(const std::wstring& root) {
    const auto main = root+L"\\good\\void_g4w_explicit.dll", leaf = root+L"\\good\\"+dependency_name;
    File main_file(main), leaf_file(leaf);
    check(!redirected(L"void_g4w_explicit.dll") && !redirected(dependency_name) &&
          !GetModuleHandleW(L"void_g4w_explicit.dll") && !GetModuleHandleW(dependency_name),
          "closed explicit fixture preflight");
    Image leaf_image(leaf), main_image(main);
    check(compare_path("explicit leaf",leaf_image.value,leaf_file) &&
          compare_path("explicit provider",main_image.value,main_file),"exact explicit fixture paths");
    const Table table{export_from<Bias>(leaf_image,"g4_dependency")};
    auto sample = export_from<Sample (*)(const Table*,Sample)>(main_image,"sample");
    auto fill = export_from<size_t (*)(const Table*,unsigned char*,size_t)>(main_image,"fill");
    auto acquire = export_from<Token* (*)(const Table*,int)>(main_image,"acquire");
    auto inspect = export_from<int (*)(Token*)>(main_image,"inspect");
    auto release = export_from<int (*)(Token*)>(main_image,"release");
    auto record = sample(&table,{5,1.25});
    unsigned char bytes[3]{};
    auto count = fill(&table,bytes,3);
    auto* token = acquire(&table,35);
    check(record.tag == 12 && record.value == 2.5 && count == 3 &&
          bytes[0] == 7 && bytes[1] == 8 && bytes[2] == 9 &&
          token && inspect(token) == 42 && release(token) == 0 && release(token) == 1,
          "exact typed table fixture result");
    std::puts("explicit table used C record, byte span, opaque token and terminal release");
}
}
int wmain(int argc, wchar_t** argv) {
    if (argc != 3) return 2;
    check(SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32),"narrow process search default");
    const std::wstring root = argv[2];
    const std::wstring_view mode = argv[1];
    if (mode == L"data") data_only(root);
    else if (mode == L"forward") forwarding(root);
    else if (mode == L"explicit") explicit_table(root);
    else imports(root,mode);
    return 0;
}
