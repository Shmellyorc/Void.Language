#include "host_internal.hpp"
#ifdef _WIN32
#include "host_windows.hpp"
#include "host_save.hpp"
#include <winternl.h>
#include <cstring>
#include <mutex>
namespace vd::host {
namespace {
struct Node {
    win::Handle handle;
    std::shared_ptr<Node> ancestor;
    explicit Node(HANDLE value):handle(value){}
};
std::shared_ptr<Node> held(win::Handle handle,std::shared_ptr<Node> ancestor={}) {
    auto node=std::make_shared<Node>(handle.value);handle.release();node->ancestor=std::move(ancestor);return node;
}
struct Directory final : Object {
    std::shared_ptr<Node> node,parent;
    std::wstring name;
    ~Directory() override {close();}
    std::error_code close() noexcept override {
        std::error_code result;
        if(node && node.use_count()==1)result=node->handle.close();node.reset();
        if(parent && parent.use_count()==1){auto code=parent->handle.close();if(!result)result=code;}parent.reset();return result;
    }
};
struct Lock final : Object {
    win::Handle handle;
    std::shared_ptr<Node> parent;
    ~Lock() override {close();}
    std::error_code close() noexcept override {auto result=handle.close();parent.reset();return result;}
};
// Every relative name is one validated component, so there are no intermediate
// reparse points in NtCreateFile's name. Inspect the opened leaf before use.
win::Handle open_component(HANDLE parent,std::wstring_view name,bool directory,bool create,bool writable,win::PrivateAccess& access,bool lock=false,bool rename_authority=true) {
    UNICODE_STRING text{};text.Buffer=const_cast<wchar_t*>(name.data());text.Length=static_cast<USHORT>(name.size()*sizeof(wchar_t));text.MaximumLength=text.Length;
    OBJECT_ATTRIBUTES attributes{};attributes.Length=sizeof(attributes);attributes.RootDirectory=parent;attributes.ObjectName=&text;attributes.Attributes=0x40; // OBJ_CASE_INSENSITIVE
    if(create)attributes.SecurityDescriptor=&access.descriptor;
    IO_STATUS_BLOCK io{};HANDLE output=INVALID_HANDLE_VALUE;
    auto library=GetModuleHandleW(L"ntdll.dll");if(!library)win::fail();
    using Create=NTSTATUS(NTAPI*)(PHANDLE,ACCESS_MASK,POBJECT_ATTRIBUTES,PIO_STATUS_BLOCK,PLARGE_INTEGER,ULONG,ULONG,ULONG,ULONG,PVOID,ULONG);
    using Convert=ULONG(NTAPI*)(NTSTATUS);
    auto call=reinterpret_cast<Create>(GetProcAddress(library,"NtCreateFile"));
    auto convert=reinterpret_cast<Convert>(GetProcAddress(library,"RtlNtStatusToDosError"));
    if(!call || !convert)win::fail(ERROR_PROC_NOT_FOUND);
    // A persistent directory DELETE grant conflicts with the kernel's target
    // open during handle-relative rename. Acquire source DELETE only on demand.
    ACCESS_MASK rights=READ_CONTROL|SYNCHRONIZE|FILE_READ_ATTRIBUTES|(rename_authority && !directory?DELETE:0)|(directory?FILE_LIST_DIRECTORY:GENERIC_READ);
    if(writable)rights|=GENERIC_WRITE;
    constexpr ULONG synchronous=0x20,open_reparse=0x200000,directory_file=1,non_directory=0x40;
    auto status=call(&output,rights,&attributes,&io,nullptr,FILE_ATTRIBUTE_NORMAL,
        lock?0:FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,create?2:1, // FILE_CREATE / FILE_OPEN
        synchronous|open_reparse|(directory?directory_file:non_directory),nullptr,0);
    if(status<0)win::fail(convert(status));
    return win::Handle(output);
}
win::Handle relative(HANDLE parent,std::wstring_view name,bool directory,bool create,bool writable,win::PrivateAccess& access,bool lock=false,bool rename_authority=true) {
    if(!win::component(name))win::fail(ERROR_INVALID_NAME);
    auto handle=open_component(parent,name,directory,create,writable,access,lock,rename_authority);
    win::information(handle.value,directory);access.verify(handle.value);return handle;
}
win::Handle lock_file(HANDLE parent, std::wstring_view name, win::PrivateAccess& access) {
    win::Handle file;
    try {file.value=relative(parent,name,false,true,true,access,true).release();}
    catch(const std::system_error& e) {
        if(e.code().value()!=ERROR_FILE_EXISTS && e.code().value()!=ERROR_ALREADY_EXISTS)throw;
        file.value=relative(parent,name,false,false,true,access,true).release();
    }
    OVERLAPPED offset{};
    if(!LockFileEx(file.value,LOCKFILE_EXCLUSIVE_LOCK|LOCKFILE_FAIL_IMMEDIATELY,0,MAXDWORD,MAXDWORD,&offset))win::fail();
    return file;
}
std::shared_ptr<Directory> root(const std::string& path,bool create) {
    auto full=win::absolute(path);if(full.size()<=7)win::fail(ERROR_INVALID_NAME);
    win::PrivateAccess access;
    auto current=held(win::Handle(CreateFileW(full.substr(0,7).c_str(),FILE_LIST_DIRECTORY|FILE_READ_ATTRIBUTES|READ_CONTROL,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr)));
    if(current->handle.value==INVALID_HANDLE_VALUE)win::fail();
    win::information(current->handle.value,true);
    for(std::size_t start=7;start<full.size();) {
        access.verify(current->handle.value,true);
        auto end=full.find(L'\\',start);auto name=full.substr(start,end==full.npos?end:end-start);bool last=end==full.npos;
        if(last) {
            auto opened=relative(current->handle.value,name,true,create,true,access);
            auto value=std::make_shared<Directory>();value->node=held(std::move(opened),current);value->parent=current;value->name=std::move(name);return value;
        }
        // Ancestors need not be private; creation and final admission do.
        UNICODE_STRING text{};text.Buffer=name.data();text.Length=static_cast<USHORT>(name.size()*2);text.MaximumLength=text.Length;
        OBJECT_ATTRIBUTES attributes{};attributes.Length=sizeof(attributes);attributes.RootDirectory=current->handle.value;attributes.ObjectName=&text;attributes.Attributes=0x40;
        IO_STATUS_BLOCK io{};HANDLE raw=INVALID_HANDLE_VALUE;
        using Create=NTSTATUS(NTAPI*)(PHANDLE,ACCESS_MASK,POBJECT_ATTRIBUTES,PIO_STATUS_BLOCK,PLARGE_INTEGER,ULONG,ULONG,ULONG,ULONG,PVOID,ULONG);
        using Convert=ULONG(NTAPI*)(NTSTATUS);
        auto dll=GetModuleHandleW(L"ntdll.dll");auto call=reinterpret_cast<Create>(GetProcAddress(dll,"NtCreateFile"));auto convert=reinterpret_cast<Convert>(GetProcAddress(dll,"RtlNtStatusToDosError"));
        if(!call || !convert)win::fail(ERROR_PROC_NOT_FOUND);
        // NtCreateFile needs the metadata read right for information() below.
        auto status=call(&raw,FILE_LIST_DIRECTORY|FILE_READ_ATTRIBUTES|READ_CONTROL|SYNCHRONIZE,&attributes,&io,nullptr,FILE_ATTRIBUTE_NORMAL,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,1,1|0x20|0x200000,nullptr,0);
        if(status<0)win::fail(convert(status));win::Handle next(raw);win::information(raw,true);
        current=held(std::move(next),current);start=end+1;
    }
    win::fail(ERROR_INVALID_NAME);
}
Result sync(HANDLE handle) {if(!FlushFileBuffers(handle))return native_error(static_cast<int>(GetLastError()));return {};}
bool same(HANDLE a,HANDLE b) {
    BY_HANDLE_FILE_INFORMATION x{},y{};
    if(!GetFileInformationByHandle(a,&x) || !GetFileInformationByHandle(b,&y))win::fail();
    return x.dwVolumeSerialNumber==y.dwVolumeSerialNumber && x.nFileIndexHigh==y.nFileIndexHigh && x.nFileIndexLow==y.nFileIndexLow;
}
win::Handle rename_source(HANDLE directory,win::PrivateAccess& access) {
    // An empty native name reopens this held directory itself, without resolving
    // its former name or an absolute path. This is not a public component name.
    wchar_t zero=0;UNICODE_STRING name{};name.Buffer=&zero;
    OBJECT_ATTRIBUTES attributes{};attributes.Length=sizeof(attributes);attributes.RootDirectory=directory;attributes.ObjectName=&name;attributes.Attributes=0x40;
    IO_STATUS_BLOCK io{};HANDLE raw=INVALID_HANDLE_VALUE;
    using Create=NTSTATUS(NTAPI*)(PHANDLE,ACCESS_MASK,POBJECT_ATTRIBUTES,PIO_STATUS_BLOCK,PLARGE_INTEGER,ULONG,ULONG,ULONG,ULONG,PVOID,ULONG);
    using Convert=ULONG(NTAPI*)(NTSTATUS);
    auto dll=GetModuleHandleW(L"ntdll.dll");if(!dll)win::fail();
    auto call=reinterpret_cast<Create>(GetProcAddress(dll,"NtCreateFile"));auto convert=reinterpret_cast<Convert>(GetProcAddress(dll,"RtlNtStatusToDosError"));
    if(!call || !convert)win::fail(ERROR_PROC_NOT_FOUND);
    auto status=call(&raw,DELETE|SYNCHRONIZE|FILE_READ_ATTRIBUTES|READ_CONTROL,&attributes,&io,nullptr,FILE_ATTRIBUTE_NORMAL,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,1,1|0x20|0x200000,nullptr,0);
    if(status<0)win::fail(convert(status));win::Handle result(raw);
    win::information(raw,true);access.verify(raw);
    if(!same(directory,raw))win::fail(ERROR_INVALID_PARAMETER);
    return result;
}
Result rename(HANDLE source,HANDLE target,const std::wstring& name,bool replace) {
    // Allocate every byte before the namespace mutation. RootDirectory pins the
    // destination; the source is the held object, never a re-opened path.
    std::vector<unsigned char> storage(sizeof(FILE_RENAME_INFO)+name.size()*sizeof(wchar_t));
    auto* info=reinterpret_cast<FILE_RENAME_INFO*>(storage.data());info->ReplaceIfExists=replace;info->RootDirectory=target;
    info->FileNameLength=static_cast<DWORD>(name.size()*sizeof(wchar_t));std::memcpy(info->FileName,name.data(),info->FileNameLength);
    using Set=NTSTATUS(NTAPI*)(HANDLE,PIO_STATUS_BLOCK,PVOID,ULONG,FILE_INFORMATION_CLASS);
    using Convert=ULONG(NTAPI*)(NTSTATUS);
    auto dll=GetModuleHandleW(L"ntdll.dll");if(!dll)return native_error(static_cast<int>(GetLastError()));
    auto call=reinterpret_cast<Set>(GetProcAddress(dll,"NtSetInformationFile"));auto convert=reinterpret_cast<Convert>(GetProcAddress(dll,"RtlNtStatusToDosError"));
    if(!call || !convert)return native_error(ERROR_PROC_NOT_FOUND);
    // Runtime file handles and rename_source() are synchronous. The native
    // FileRenameInformation class preserves RootDirectory on Windows 10 too.
    IO_STATUS_BLOCK io{};auto status=call(source,&io,info,static_cast<ULONG>(storage.size()),static_cast<FILE_INFORMATION_CLASS>(10));
    if(status<0)return native_error(static_cast<int>(convert(status)));
    Result result;result.flag=true;return result;
}
Result published(Result result,HANDLE source,HANDLE target) {
    if(!result.flag)return result;
    auto first=sync(target),second=sync(source);
    if(first.kind){result.kind=first.kind;result.code=first.code;result.detail=second.code;}
    else if(second.kind){result.kind=second.kind;result.code=second.code;}return result;
}

constexpr std::wstring_view save_lock_name = L".void-save-lock";
constexpr std::wstring_view save_stage_name = L".void-save-stage";

bool equivalent_name(std::wstring_view a, std::wstring_view b) {
    return CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(), static_cast<int>(b.size()), TRUE)==CSTR_EQUAL;
}

Result local_save_volume(HANDLE directory) {
    struct DeviceInformation { ULONG type, characteristics; } device{};
    constexpr int file_fs_device_information = 4;
    constexpr ULONG file_remote_device = 0x10;
    using Query = NTSTATUS(NTAPI*)(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, int);
    using Convert = ULONG(NTAPI*)(NTSTATUS);
    auto dll = GetModuleHandleW(L"ntdll.dll");
    if (!dll) return native_error(GetLastError());
    auto query = reinterpret_cast<Query>(GetProcAddress(dll, "NtQueryVolumeInformationFile"));
    auto convert = reinterpret_cast<Convert>(GetProcAddress(dll, "RtlNtStatusToDosError"));
    if (!query || !convert) return native_error(ERROR_PROC_NOT_FOUND);
    IO_STATUS_BLOCK status{};
    const auto result = query(directory, &status, &device, sizeof(device), file_fs_device_information);
    if (result < 0) return native_error(convert(result));
    if (device.characteristics & file_remote_device) return error(11);
    wchar_t filesystem[32]{};
    if (!GetVolumeInformationByHandleW(directory, nullptr, 0, nullptr, nullptr, nullptr,
                                       filesystem, static_cast<DWORD>(std::size(filesystem))))
        return native_error(GetLastError());
    return std::wstring_view(filesystem) == L"NTFS" ? Result{} : error(11);
}

class SaveStage {
    Directory& directory;
    const std::wstring& name;
    win::PrivateAccess& access;
    HANDLE parent;
    win::Handle staged, previous, lock;

    Result verify_directory() const {
        if (!directory.parent) return error(4);
        auto check = relative(directory.parent->handle.value, directory.name, true, false, false, access);
        if (!same(parent, check.value)) return error(4);
        win::information(parent, true);
        access.verify(parent);
        if (auto code = check.close()) return native_error(code.value());
        return {};
    }
    win::Handle target() const {
        return relative(parent, name, false, false, false, access, false, false);
    }
public:
    SaveStage(Directory& owner, const std::wstring& destination, win::PrivateAccess& security)
        : directory(owner), name(destination), access(security), parent(owner.node->handle.value) {}

    Result prepare(bool replace) {
        auto checked = verify_directory();
        if (checked.kind) return checked;
        checked = local_save_volume(parent);
        if (checked.kind) return checked;
        lock.value = lock_file(parent, save_lock_name, access).release();
        try { previous.value = target().release(); }
        catch (const std::system_error& e) {
            if (e.code().value() != ERROR_FILE_NOT_FOUND) throw;
            return {};
        }
        return replace ? Result{} : native_error(ERROR_FILE_EXISTS);
    }

    Result create() {
        staged.value = open_component(parent, save_stage_name, false, true, true, access).release();
        win::information(file(), false);
        access.verify(file());
        return {};
    }

    Result publish() {
        auto checked = verify_directory();
        if (checked.kind) return checked;
        auto source = relative(parent, save_stage_name, false, false, false, access, false, false);
        if (!same(file(), source.value)) return native_error(ERROR_INVALID_PARAMETER);
        if (auto code = source.close()) return native_error(code.value());
        const bool replace = previous.value != INVALID_HANDLE_VALUE;
        if (replace) {
            auto current = target();
            if (!same(previous.value, current.value)) return native_error(ERROR_INVALID_PARAMETER);
            if (win::information(parent, true).dwVolumeSerialNumber != win::information(current.value, false).dwVolumeSerialNumber)
                return native_error(ERROR_NOT_SAME_DEVICE);
            // Classic FileRenameInformation can reject an open destination.
            // Drop our inspection handles and keep the directory writer lock.
            if (auto code = current.close()) return native_error(code.value());
            if (auto code = previous.close()) return native_error(code.value());
        }
        return rename(file(), parent, name, replace);
    }

    HANDLE file() const { return staged.value; }
    bool has_file() const { return file() != INVALID_HANDLE_VALUE; }
    Result sync_file() const { return sync(file()); }
    Result sync_directory() const { return sync(parent); }
    int remove() const {
        try {
            auto source = open_component(parent, save_stage_name, false, false, false, access, false, false);
            if (!same(file(), source.value)) return ERROR_INVALID_PARAMETER;
            if (auto code = source.close()) return code.value();
            FILE_DISPOSITION_INFO disposition{TRUE};
            if (!SetFileInformationByHandle(file(), FileDispositionInfo, &disposition, sizeof(disposition)))
                return static_cast<int>(GetLastError());
            return 0;
        } catch (const std::system_error& e) { return e.code().value(); }
          catch (const std::bad_alloc&) { return ERROR_NOT_ENOUGH_MEMORY; }
    }
    int close_file() { return staged.close().value(); }
    int close_target() { return previous.close().value(); }
    int close_lock() { return lock.close().value(); }
};
}
Result windows_directory_execute(Request& q) {
    try {
        if(q.operation==file_mode_matches) {
            if(q.numbers[0]!=0 && q.numbers[0]!=1)return error(6);
            auto native=filesystem::private_mode_matches(*q.file->file(),q.numbers[0]!=0);
            auto result=error(static_cast<std::int64_t>(native.error),native.os_code);result.number=native.number;return result;
        }
        if(q.operation==file_mode) {
            if(q.numbers[0]!=0 && q.numbers[0]!=1)return error(6);
            auto r=filesystem::private_mode(*q.file->file(),q.numbers[0]!=0);return error(static_cast<std::int64_t>(r.error),r.os_code);
        }
        if(q.operation==dir_create || q.operation==dir_admit){q.acquired=root(q.texts[0],q.operation==dir_create);return {};}
        auto d=std::static_pointer_cast<Directory>(q.objects[0]);if(!d->node)return error(7);auto fd=d->node->handle.value;
        if(q.operation==dir_sync)return sync(fd);
        win::PrivateAccess access;
        if(q.operation==dir_save) {
            auto name=win::wide(q.texts.front());
            if(!win::component(name) || equivalent_name(name,save_lock_name) || equivalent_name(name,save_stage_name))return error(4);
            SaveStage stage(*d,name,access);
            return save_atomic(stage,q);
        }
        if(q.operation==dir_child || q.operation==dir_open) {
            auto name=win::wide(q.texts[0]);auto child=relative(fd,name,true,q.operation==dir_child,true,access);
            auto value=std::make_shared<Directory>();value->node=held(std::move(child),d->node);value->parent=d->node;value->name=std::move(name);q.acquired=std::move(value);return {};
        }
        if(q.operation==dir_file) {
            if(q.numbers[0]!=0 && q.numbers[0]!=1)return error(6);bool create=q.numbers[0]==1;
            auto file=relative(fd,win::wide(q.texts[0]),false,create,create,access);
            q.acquired_file=filesystem::adopt_native_handle(file.release(),create);return {};
        }
        if(q.operation==dir_lock) {
            auto file=lock_file(fd,win::wide(q.texts[0]),access);
            auto value=std::make_shared<Lock>();value->handle.value=file.release();value->parent=d->node;q.acquired=value;return {};
        }
        auto target=std::static_pointer_cast<Directory>(q.objects[1]);if(!target->node)return error(7);
        if(q.operation==dir_publish) {
            auto name=win::wide(q.texts[0]);if(!win::component(name) || !d->parent)return error(4);
            // Another owned alias may have moved this directory. Reject stale
            // parent bookkeeping before mutation, so durability covers both parents.
            {auto check=relative(d->parent->handle.value,d->name,true,false,false,access,false,false);if(!same(fd,check.value))return error(4);}
            // Rename the held source identity. Validated ancestor ACLs exclude
            // untrusted mutation; share-delete permits independent owned aliases.
            auto source=d->parent;Result result;
            {auto identity=rename_source(fd,access);result=rename(identity.value,target->node->handle.value,name,false);}
            if(result.flag){d->parent=target->node;d->node->ancestor=target->node;d->name.swap(name);}
            return published(result,source->handle.value,target->node->handle.value);
        }
        if(q.operation==dir_replace) {
            if(q.numbers[0]!=0 && q.numbers[0]!=1)return error(6);
            auto name=win::wide(q.texts[1]);if(!win::component(name))return error(4);
            // The borrowed IO handle owns the rename; verify its source name.
            auto check=relative(fd,win::wide(q.texts[0]),false,false,false,access,false,false);
            auto file=filesystem::native_handle(*q.file->file());if(!same(file,check.value))return error(4);access.verify(file);check.close();
            auto ready=sync(file);if(ready.kind)return ready;
            auto result=rename(file,target->node->handle.value,name,q.numbers[0]!=0);
            return published(result,fd,target->node->handle.value);
        }
        throw std::logic_error("Invalid Windows directory operation");
    } catch(const std::system_error& e) {return native_error(e.code().value());}
}
}
#endif
