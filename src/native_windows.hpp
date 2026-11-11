#pragma once
// Windows-only native staging primitives. Shared with the focused native test.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <llvm/Object/COFF.h>
#include <llvm/Support/MemoryBuffer.h>
#include <windows.h>
#include <aclapi.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/ADT/SmallString.h>
#include <algorithm>
#include <cstdint>
#include <array>
#include <filesystem>
#include <stdexcept>
#include <vector>
#include <utility>
#include "void/path.hpp"
namespace vd::native_windows {
namespace fs = std::filesystem;
[[noreturn]] inline void fail(const char* operation, DWORD code=GetLastError()) {
    throw std::runtime_error(std::string(operation)+" (Windows error "+std::to_string(code)+").");
}
struct Handle {
    HANDLE value=INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE h=INVALID_HANDLE_VALUE):value(h){}
    Handle(const Handle&)=delete;
    Handle& operator=(const Handle&)=delete;
    Handle(Handle&& h) noexcept:value(std::exchange(h.value,INVALID_HANDLE_VALUE)){}
    ~Handle(){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
};
inline BY_HANDLE_FILE_INFORMATION information(HANDLE h) {
    BY_HANDLE_FILE_INFORMATION info{};
    if(!GetFileInformationByHandle(h,&info))fail("Cannot inspect native artifact");
    if(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
        throw std::runtime_error("Native temporary path contains a reparse point.");
    return info;
}
inline bool same_filename(const fs::path& left,const fs::path& right) {
    const auto result=CompareStringOrdinal(left.c_str(),-1,right.c_str(),-1,TRUE);
    if(!result)fail("Cannot compare Windows filenames");
    return result==CSTR_EQUAL;
}
// Pin a directory with directory read access and no delete sharing.
// Metadata-only opens do not establish the read/write/delete sharing reservation.
// OPEN_REPARSE_POINT inspects the component itself, never its redirect target.
inline Handle pin_directory(const fs::path& path) {
    Handle h(CreateFileW(path.c_str(),FILE_LIST_DIRECTORY|FILE_READ_ATTRIBUTES|READ_CONTROL,
        FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
    if(h.value==INVALID_HANDLE_VALUE)fail("Cannot pin native temporary directory");
    if(!(information(h.value).dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        throw std::runtime_error("Native temporary ancestor is not a directory.");
    return h;
}
// A private stage's owner already holds DELETE without delete sharing. Borrow
// that reservation only below its admitted path, and only while it is alive.
struct DirectoryReservation {
    const fs::path& path;
    HANDLE handle;
    const DirectoryReservation* parent;
};
inline std::vector<Handle> pin_directories(const fs::path& path,
        const DirectoryReservation* reservation=nullptr,HANDLE* leaf=nullptr) {
    if(!path.is_absolute() || path!=path.lexically_normal())
        throw std::runtime_error("Native temporary root must be absolute and normalized.");
    if(reservation) {
        const auto relative=path.lexically_relative(reservation->path);
        if(relative.empty() || *relative.begin()==L"..")
            throw std::runtime_error("Path is outside the reserved project stage.");
    }
    std::vector<Handle> result;
    auto pin=[&](const fs::path& current) {
        HANDLE handle;
        auto owner=reservation;
        while(owner && owner->path!=current)owner=owner->parent;
        if(owner) {
            // This metadata-only open checks identity. The owner retains the
            // actual read/delete reservation throughout the nested operation.
            Handle observed(CreateFileW(current.c_str(),FILE_READ_ATTRIBUTES,
                FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
            if(observed.value==INVALID_HANDLE_VALUE)fail("Cannot check project staging directory");
            const auto actual=information(observed.value), expected=information(owner->handle);
            if(actual.dwVolumeSerialNumber!=expected.dwVolumeSerialNumber ||
               actual.nFileIndexHigh!=expected.nFileIndexHigh || actual.nFileIndexLow!=expected.nFileIndexLow)
                throw std::runtime_error("Project staging directory identity changed.");
            handle=owner->handle;
        } else {
            result.push_back(pin_directory(current));
            handle=result.back().value;
        }
        if(leaf)*leaf=handle;
    };
    auto current=path.root_path();pin(current);
    for(const auto& part:path.relative_path()){current/=part;pin(current);}
    return result;
}
struct PrivateAccess {
    std::vector<unsigned char> user;
    std::vector<unsigned char> acl;
    SECURITY_DESCRIPTOR descriptor{};
    SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES),&descriptor,FALSE};
    PrivateAccess() {
        HANDLE token_value;
        if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token_value))fail("Cannot read native staging identity");
        Handle token(token_value);DWORD size=0;
        GetTokenInformation(token.value,TokenUser,nullptr,0,&size);
        if(!size)fail("Cannot size native staging identity");
        user.resize(size);
        if(!GetTokenInformation(token.value,TokenUser,user.data(),size,&size))fail("Cannot read native staging identity");
        auto sid=reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid;
        acl.resize(sizeof(ACL)+sizeof(ACCESS_ALLOWED_ACE)-sizeof(DWORD)+GetLengthSid(sid));
        auto dacl=reinterpret_cast<PACL>(acl.data());
        if(!InitializeAcl(dacl,static_cast<DWORD>(acl.size()),ACL_REVISION) ||
           !AddAccessAllowedAceEx(dacl,ACL_REVISION,OBJECT_INHERIT_ACE|CONTAINER_INHERIT_ACE,FILE_ALL_ACCESS,sid) ||
           !InitializeSecurityDescriptor(&descriptor,SECURITY_DESCRIPTOR_REVISION) ||
           !SetSecurityDescriptorOwner(&descriptor,sid,FALSE) ||
           !SetSecurityDescriptorDacl(&descriptor,TRUE,dacl,FALSE) ||
           !SetSecurityDescriptorControl(&descriptor,SE_DACL_PROTECTED,SE_DACL_PROTECTED))
            fail("Cannot construct private native staging DACL");
    }
    void verify_root(HANDLE root) const {
        PSECURITY_DESCRIPTOR raw=nullptr;PACL dacl=nullptr;PSID owner=nullptr;
        auto code=GetSecurityInfo(root,SE_FILE_OBJECT,OWNER_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION,
            &owner,nullptr,&dacl,nullptr,&raw);
        if(code!=ERROR_SUCCESS)fail("Cannot verify native temporary root",code);
        struct Free {PSECURITY_DESCRIPTOR p;~Free(){LocalFree(p);}} memory{raw};
        std::array<unsigned char,SECURITY_MAX_SID_SIZE> system{},administrators{};
        DWORD a=static_cast<DWORD>(system.size()),b=static_cast<DWORD>(administrators.size());
        if(!CreateWellKnownSid(WinLocalSystemSid,nullptr,system.data(),&a) ||
           !CreateWellKnownSid(WinBuiltinAdministratorsSid,nullptr,administrators.data(),&b))
            fail("Cannot verify native temporary root identities");
        auto trusted=[&](PSID sid) {
            return EqualSid(sid,reinterpret_cast<const TOKEN_USER*>(user.data())->User.Sid) ||
                EqualSid(sid,system.data()) || EqualSid(sid,administrators.data());
        };
        if(!owner || !trusted(owner) || !dacl || !IsValidAcl(dacl))
            throw std::runtime_error("Native temporary root has untrusted ownership or access.");
        constexpr DWORD mutation=FILE_ADD_FILE|FILE_ADD_SUBDIRECTORY|FILE_DELETE_CHILD|DELETE|
            WRITE_DAC|WRITE_OWNER|FILE_WRITE_ATTRIBUTES|FILE_WRITE_EA|GENERIC_WRITE|GENERIC_ALL;
        for(DWORD i=0;i<dacl->AceCount;++i) {
            void* entry=nullptr;if(!GetAce(dacl,i,&entry))fail("Cannot inspect native temporary root ACL");
            const auto header=static_cast<ACE_HEADER*>(entry);
            if(header->AceFlags & INHERIT_ONLY_ACE)continue;
            if(header->AceType==ACCESS_DENIED_ACE_TYPE)continue;
            if(header->AceType!=ACCESS_ALLOWED_ACE_TYPE)
                throw std::runtime_error("Unsupported native temporary root ACL.");
            const auto ace=static_cast<ACCESS_ALLOWED_ACE*>(entry);
            if((ace->Mask & mutation) && !trusted(&ace->SidStart))
                throw std::runtime_error("Native temporary root permits untrusted modification.");
        }
    }
    void verify(HANDLE directory) const {
        PSECURITY_DESCRIPTOR raw=nullptr;PACL dacl=nullptr;PSID owner=nullptr;
        auto code=GetSecurityInfo(directory,SE_FILE_OBJECT,OWNER_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION,
            &owner,nullptr,&dacl,nullptr,&raw);
        if(code!=ERROR_SUCCESS)fail("Cannot verify native staging DACL",code);
        struct Free {PSECURITY_DESCRIPTOR p;~Free(){LocalFree(p);}} memory{raw};
        SECURITY_DESCRIPTOR_CONTROL control{};DWORD revision=0;void* entry=nullptr;
        auto sid=reinterpret_cast<const TOKEN_USER*>(user.data())->User.Sid;
        if(!GetSecurityDescriptorControl(raw,&control,&revision) || !(control & SE_DACL_PROTECTED) ||
           !owner || !EqualSid(owner,sid) || !dacl || !IsValidAcl(dacl) || dacl->AceCount!=1 ||
           !GetAce(dacl,0,&entry))
            throw std::runtime_error("Native staging DACL is not private.");
        const auto ace=static_cast<ACCESS_ALLOWED_ACE*>(entry);
        if(ace->Header.AceType!=ACCESS_ALLOWED_ACE_TYPE ||
           ace->Header.AceFlags!=(OBJECT_INHERIT_ACE|CONTAINER_INHERIT_ACE) ||
           ace->Mask!=FILE_ALL_ACCESS || !EqualSid(&ace->SidStart,sid))
            throw std::runtime_error("Native staging DACL grants unexpected access.");
    }
};
inline fs::path temporary_root() {
    std::vector<wchar_t> buffer(32768);
    auto size=GetTempPathW(static_cast<DWORD>(buffer.size()),buffer.data());
    if(!size || size>=buffer.size())fail("Cannot locate native temporary root");
    auto root=fs::path(buffer.data()).lexically_normal();
    // Drop a trailing separator so ancestry pinning does not add an empty leaf.
    if(root.has_relative_path() && root.filename().empty())root=root.parent_path();
    return root;
}
inline fs::path unique_leaf(const char* model) {
    llvm::SmallString<32> name;
    llvm::sys::fs::createUniquePath(model,name,true);
    return utf8_path(name.str().str()).filename();
}
inline fs::path random_leaf(){return unique_leaf("vd-%%%%%%%%%%%%%%%%");}
inline fs::path staging_leaf(){return unique_leaf(".void-build-%%%%%%%%%%%%%%%%");}
struct Directory {
    fs::path directory;
    std::vector<Handle> pins;
    // A candidate supplier permits deterministic genuine collisions in the test;
    // production always uses LLVM's existing random leaf generator.
    using Candidate=fs::path(*)();
    Directory(const fs::path& root,bool short_path,Candidate candidate=random_leaf) {
        pins=pin_directories(root);
        PrivateAccess access;
        if(short_path)access.verify_root(pins.back().value);
        for(unsigned attempt=0;attempt<32;++attempt) {
            auto leaf=candidate();
            if(leaf.empty() || leaf!=leaf.filename() || leaf=="." || leaf=="..")
                throw std::runtime_error("Invalid native staging leaf.");
            auto next=root/leaf;
            // Count the entire UTF-16 linker path, including filename and NUL.
            if(short_path && (next/L"program.exe").native().size()+1>240)
                throw std::runtime_error("Windows temporary root is too long for native linking.");
            if(!CreateDirectoryW(next.c_str(),&access.attributes)) {
                auto error=GetLastError();
                if(error==ERROR_ALREADY_EXISTS || error==ERROR_FILE_EXISTS)continue;
                fail("Cannot create private native staging directory",error);
            }
            // Only successful exclusive creation confers cleanup ownership.
            directory=std::move(next);
            try {
                auto child=pin_directories(directory);
                access.verify(child.back().value);
                for(auto& h:child)pins.push_back(std::move(h));
            } catch(...) {pins.clear();std::error_code ignored;fs::remove_all(directory,ignored);throw;}
            return;
        }
        throw std::runtime_error("Cannot reserve a unique native staging directory.");
    }
    Directory(const Directory&)=delete;
    Directory& operator=(const Directory&)=delete;
    ~Directory(){pins.clear();std::error_code ignored;fs::remove_all(directory,ignored);}
};
inline Handle read_file(const fs::path& path,bool single_link=false) {
    Handle h(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_SEQUENTIAL_SCAN,nullptr));
    if(h.value==INVALID_HANDLE_VALUE)fail("Cannot open native artifact");
    const auto info=information(h.value);
    if((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || (single_link && info.nNumberOfLinks!=1))
        throw std::runtime_error("Native artifact is not an ordinary private file.");
    return h;
}
inline std::uint64_t size(HANDLE h) {
    LARGE_INTEGER bytes{};
    if(!GetFileSizeEx(h,&bytes))fail("Cannot size native artifact");
    return static_cast<std::uint64_t>(bytes.QuadPart);
}
inline void rewind(HANDLE h) {
    LARGE_INTEGER zero{};
    if(!SetFilePointerEx(h,zero,nullptr,FILE_BEGIN))fail("Cannot rewind native artifact");
}
inline DWORD read(HANDLE h,void* buffer,DWORD count) {
    DWORD got=0;if(!ReadFile(h,buffer,count,&got,nullptr))fail("Cannot read native artifact");return got;
}
// Byte comparison is independent of copy success/length; handles deny mutation
// and deletion while copying/verifying. CREATE_NEW never overwrites a stage file.
inline void copy_checked(const fs::path& from,const fs::path& to,bool executable=false) {
    auto source=read_file(from,executable);
    if(!size(source.value))throw std::runtime_error("Native artifact is empty.");
    Handle target(CreateFileW(to.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
    if(target.value==INVALID_HANDLE_VALUE)fail("Cannot create native artifact copy");
    std::array<unsigned char,65536> a{},b{};
    while(auto got=read(source.value,a.data(),static_cast<DWORD>(a.size()))) {
        DWORD written=0;
        if(!WriteFile(target.value,a.data(),got,&written,nullptr) || written!=got)fail("Cannot write native artifact copy");
    }
    if(!FlushFileBuffers(target.value))fail("Cannot flush native artifact copy");
    if(size(source.value)!=size(target.value))throw std::runtime_error("Native artifact copy size mismatch.");
    rewind(source.value);rewind(target.value);
    for(;;) {
        auto got=read(source.value,a.data(),static_cast<DWORD>(a.size()));
        auto copied=read(target.value,b.data(),static_cast<DWORD>(b.size()));
        if(got!=copied || !std::equal(a.begin(),a.begin()+got,b.begin()))
            throw std::runtime_error("Native artifact copy contents mismatch.");
        if(!got)break;
    }
}
inline void check_executable(llvm::MemoryBufferRef buffer, bool amd64=false) {
    auto pe=llvm::object::COFFObjectFile::create(buffer);
    if(!pe)throw std::runtime_error("Invalid native executable: "+llvm::toString(pe.takeError()));
    if(!(*pe)->getDOSHeader() || (!(*pe)->getPE32Header() && !(*pe)->getPE32PlusHeader()) ||
       !((*pe)->getCharacteristics() & IMAGE_FILE_EXECUTABLE_IMAGE) ||
       ((*pe)->getCharacteristics() & IMAGE_FILE_DLL) || !(*pe)->getNumberOfSections())
        throw std::runtime_error("Native linker did not produce a PE executable.");
    const auto* header=amd64 ? (*pe)->getPE32PlusHeader() : nullptr;
    if(amd64) {
        if(!header || (*pe)->getMachine()!=IMAGE_FILE_MACHINE_AMD64)
            throw std::runtime_error("Application package requires a PE32+ AMD64 executable.");
        const auto count=(*pe)->getNumberOfSections();
        const auto optional=(*pe)->getSizeOfOptionalHeader();
        const auto table_end=std::uint64_t((*pe)->getDOSHeader()->AddressOfNewExeHeader) + 4 +
            sizeof(llvm::object::coff_file_header) + optional + count*sizeof(llvm::object::coff_section);
        if(count>96 || optional<sizeof(llvm::object::pe32plus_header) ||
           header->SizeOfHeaders<table_end || header->SizeOfHeaders>buffer.getBufferSize())
            throw std::runtime_error("Invalid native executable headers.");
    }
    bool executable_entry=false;
    for(const auto& section:(*pe)->sections()) {
        auto contents=section.getContents();
        if(!contents)throw std::runtime_error("Invalid native executable section: "+llvm::toString(contents.takeError()));
        if(!header)continue;
        const auto* raw=(*pe)->getCOFFSection(section);
        if(raw->SizeOfRawData && (raw->PointerToRawData<header->SizeOfHeaders ||
           std::uint64_t(raw->PointerToRawData)+raw->SizeOfRawData>buffer.getBufferSize()))
            throw std::runtime_error("Invalid native executable section extent.");
        if((raw->Characteristics & IMAGE_SCN_MEM_EXECUTE) && header->AddressOfEntryPoint>=raw->VirtualAddress &&
           std::uint64_t(header->AddressOfEntryPoint)-raw->VirtualAddress<contents->size())
            executable_entry=true;
    }
    if(header && !executable_entry)throw std::runtime_error("Invalid native executable entry point.");
}
inline Handle verified_executable(const fs::path& executable) {
    auto pinned=read_file(executable,true);
    auto buffer=llvm::MemoryBuffer::getFile(path_utf8(executable));
    if(!buffer)throw std::runtime_error("Cannot read native executable for verification.");
    check_executable((*buffer)->getMemBufferRef());
    return pinned;
}
} // namespace vd::native_windows
#endif
