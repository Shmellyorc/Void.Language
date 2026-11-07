#include "host_internal.hpp"
#ifdef _WIN32
#include "host_windows.hpp"
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
win::Handle relative(HANDLE parent,const std::wstring& name,bool directory,bool create,bool writable,win::PrivateAccess& access,bool lock=false,bool rename_authority=true) {
    if(!win::component(name))win::fail(ERROR_INVALID_NAME);
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
    ACCESS_MASK rights=READ_CONTROL|SYNCHRONIZE|FILE_READ_ATTRIBUTES|(rename_authority?DELETE:0)|(directory?FILE_LIST_DIRECTORY:GENERIC_READ);
    if(writable)rights|=GENERIC_WRITE;
    constexpr ULONG synchronous=0x20,open_reparse=0x200000,directory_file=1,non_directory=0x40;
    auto status=call(&output,rights,&attributes,&io,nullptr,FILE_ATTRIBUTE_NORMAL,
        lock?0:FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,create?2:1, // FILE_CREATE / FILE_OPEN
        synchronous|open_reparse|(directory?directory_file:non_directory),nullptr,0);
    if(status<0)win::fail(convert(status));
    win::Handle handle(output);win::information(output,directory);access.verify(output);return handle;
}
std::shared_ptr<Directory> root(const std::string& path,bool create) {
    auto full=win::absolute(path);if(full.size()<=7)win::fail(ERROR_INVALID_NAME);
    win::PrivateAccess access;
    auto current=held(win::Handle(CreateFileW(full.substr(0,7).c_str(),FILE_LIST_DIRECTORY|READ_CONTROL,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr)));
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
        auto status=call(&raw,FILE_LIST_DIRECTORY|READ_CONTROL|SYNCHRONIZE,&attributes,&io,nullptr,FILE_ATTRIBUTE_NORMAL,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,1,1|0x20|0x200000,nullptr,0);
        if(status<0)win::fail(convert(status));win::Handle next(raw);win::information(raw,true);
        current=held(std::move(next),current);start=end+1;
    }
    win::fail(ERROR_INVALID_NAME);
}
Result sync(HANDLE handle) {if(!FlushFileBuffers(handle))return native_error(static_cast<int>(GetLastError()));return {};}
bool same(HANDLE a,HANDLE b) {
    BY_HANDLE_FILE_INFORMATION x{},y{};
    return GetFileInformationByHandle(a,&x) && GetFileInformationByHandle(b,&y) && x.dwVolumeSerialNumber==y.dwVolumeSerialNumber && x.nFileIndexHigh==y.nFileIndexHigh && x.nFileIndexLow==y.nFileIndexLow;
}
Result rename(HANDLE source,HANDLE target,const std::wstring& name,bool replace) {
    // Allocate every byte before the namespace mutation. RootDirectory pins the
    // destination; the source is the held object, never a re-opened path.
    std::vector<unsigned char> storage(offsetof(FILE_RENAME_INFO,FileName)+name.size()*sizeof(wchar_t));
    auto* info=reinterpret_cast<FILE_RENAME_INFO*>(storage.data());info->ReplaceIfExists=replace;info->RootDirectory=target;
    info->FileNameLength=static_cast<DWORD>(name.size()*sizeof(wchar_t));std::memcpy(info->FileName,name.data(),info->FileNameLength);
    if(!SetFileInformationByHandle(source,FileRenameInfo,info,static_cast<DWORD>(storage.size())))return native_error(static_cast<int>(GetLastError()));
    Result result;result.flag=true;return result;
}
Result published(Result result,HANDLE source,HANDLE target) {
    if(!result.flag)return result;
    auto first=sync(target),second=sync(source);
    if(first.kind){result.kind=first.kind;result.code=first.code;result.detail=second.code;}
    else if(second.kind){result.kind=second.kind;result.code=second.code;}return result;
}
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
            auto name=win::wide(q.texts[0]);win::Handle file;
            try {auto opened=relative(fd,name,false,true,true,access,true);file.value=opened.release();}
            catch(const std::system_error& e) {
                if(e.code().value()!=ERROR_FILE_EXISTS && e.code().value()!=ERROR_ALREADY_EXISTS)throw;
                auto opened=relative(fd,name,false,false,true,access,true);file.value=opened.release();
            }
            OVERLAPPED offset{};
            if(!LockFileEx(file.value,LOCKFILE_EXCLUSIVE_LOCK|LOCKFILE_FAIL_IMMEDIATELY,0,MAXDWORD,MAXDWORD,&offset))win::fail();
            auto value=std::make_shared<Lock>();value->handle.value=file.release();value->parent=d->node;q.acquired=value;return {};
        }
        auto target=std::static_pointer_cast<Directory>(q.objects[1]);if(!target->node)return error(7);
        if(q.operation==dir_publish) {
            auto name=win::wide(q.texts[0]);if(!win::component(name) || !d->parent)return error(4);
            // Rename the held source identity. Validated ancestor ACLs exclude
            // untrusted mutation; share-delete permits independent owned aliases.
            auto source=d->parent;auto result=rename(fd,target->node->handle.value,name,false);
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
