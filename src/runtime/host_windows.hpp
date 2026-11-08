#pragma once
// Private OS mechanics shared by Phase A Runtime providers. No LLVM or handles
// in the source ABI. Native failures retain the Win32 error number.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>
namespace vd::host::win {
[[noreturn]] inline void fail(DWORD code=GetLastError()) {throw std::system_error(static_cast<int>(code),std::system_category());}
struct Handle {
    HANDLE value=INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE v=INVALID_HANDLE_VALUE):value(v){}
    Handle(const Handle&)=delete;
    Handle& operator=(const Handle&)=delete;
    Handle(Handle&& other) noexcept:value(std::exchange(other.value,INVALID_HANDLE_VALUE)){}
    ~Handle(){if(value!=INVALID_HANDLE_VALUE && value)CloseHandle(value);}
    HANDLE release() noexcept {return std::exchange(value,INVALID_HANDLE_VALUE);}
    std::error_code close() noexcept {
        auto old=release();return old!=INVALID_HANDLE_VALUE && old && !CloseHandle(old)?std::error_code(static_cast<int>(GetLastError()),std::system_category()):std::error_code{};
    }
};
inline std::wstring wide(std::string_view text) {
    if(text.find('\0')!=text.npos || text.size()>1048576)fail(ERROR_INVALID_PARAMETER);
    if(text.empty())return {};
    int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0);
    if(!count)fail();std::wstring out(count,0);
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),out.data(),count))fail();return out;
}
inline bool component(std::wstring_view text) {
    if(text.empty() || text.size()>255 || text==L"." || text==L".." || text.back()==L'.' || text.back()==L' ')return false;
    for(auto c:text)if(c<32 || std::wstring_view(L"\\/:*?\"<>|~").find(c)!=std::wstring_view::npos)return false;
    auto stem=std::wstring(text.substr(0,text.find(L'.')));
    for(auto& c:stem)if(c>=L'a' && c<=L'z')c-=32;
    if(stem==L"CON" || stem==L"PRN" || stem==L"AUX" || stem==L"NUL" || stem==L"CONIN$" || stem==L"CONOUT$")return false;
    return !(stem.size()==4 && (stem.substr(0,3)==L"COM" || stem.substr(0,3)==L"LPT") && ((stem[3]>=L'0' && stem[3]<=L'9') || stem[3]==0xb9 || stem[3]==0xb2 || stem[3]==0xb3));
}
// Deliberately bounded to drive-absolute local paths. No UNC, device namespaces,
// ADS, relative drives, dot traversal or Win32 trailing-dot/space normalization.
inline std::wstring absolute(std::string_view text) {
    auto path=wide(text);std::replace(path.begin(),path.end(),L'/',L'\\');
    if(path.starts_with(L"\\\\?\\"))path.erase(0,4);
    if(path.size()<3 || path.size()>32760 || !((path[0]>=L'A'&&path[0]<=L'Z')||(path[0]>=L'a'&&path[0]<=L'z')) || path[1]!=L':' || path[2]!=L'\\')fail(ERROR_INVALID_NAME);
    for(std::size_t start=3;start<path.size();) {
        auto end=path.find(L'\\',start);
        if(!component(std::wstring_view(path).substr(start,end==path.npos?end:end-start)))fail(ERROR_INVALID_NAME);
        start=end==path.npos?path.size():end+1;
        if(start==path.size() && end!=path.npos)fail(ERROR_INVALID_NAME);
    }
    return L"\\\\?\\"+path;
}
inline BY_HANDLE_FILE_INFORMATION information(HANDLE h,bool directory,bool single_link=true) {
    BY_HANDLE_FILE_INFORMATION info{};
    if(!GetFileInformationByHandle(h,&info))fail();
    if(GetFileType(h)!=FILE_TYPE_DISK || (info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT) || bool(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=directory || (!directory && single_link && info.nNumberOfLinks!=1))fail(ERROR_ACCESS_DENIED);
    return info;
}
struct PrivateAccess {
    std::vector<unsigned char> user,acl;
    SECURITY_DESCRIPTOR descriptor{};
    SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES),&descriptor,FALSE};
    PSID sid() const {return reinterpret_cast<const TOKEN_USER*>(user.data())->User.Sid;}
    PrivateAccess() {
        HANDLE raw=nullptr;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&raw))fail();Handle token(raw);DWORD count=0;
        GetTokenInformation(raw,TokenUser,nullptr,0,&count);if(!count)fail();user.resize(count);
        if(!GetTokenInformation(raw,TokenUser,user.data(),count,&count))fail();
        acl.resize(sizeof(ACL)+sizeof(ACCESS_ALLOWED_ACE)-sizeof(DWORD)+GetLengthSid(sid()));auto dacl=reinterpret_cast<PACL>(acl.data());
        if(!InitializeAcl(dacl,static_cast<DWORD>(acl.size()),ACL_REVISION) ||
           !AddAccessAllowedAceEx(dacl,ACL_REVISION,OBJECT_INHERIT_ACE|CONTAINER_INHERIT_ACE,FILE_ALL_ACCESS,sid()) ||
           !InitializeSecurityDescriptor(&descriptor,SECURITY_DESCRIPTOR_REVISION) || !SetSecurityDescriptorOwner(&descriptor,sid(),FALSE) ||
           !SetSecurityDescriptorDacl(&descriptor,TRUE,dacl,FALSE) || !SetSecurityDescriptorControl(&descriptor,SE_DACL_PROTECTED,SE_DACL_PROTECTED))fail();
    }
    void verify(HANDLE handle,bool ancestor=false) const {
        PSECURITY_DESCRIPTOR raw=nullptr;PACL dacl=nullptr;PSID owner=nullptr;
        auto status=GetSecurityInfo(handle,SE_FILE_OBJECT,OWNER_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION,&owner,nullptr,&dacl,nullptr,&raw);
        if(status!=ERROR_SUCCESS)fail(status);
        struct Free {void* p;~Free(){LocalFree(p);}} memory{raw};
        verify_descriptor(raw,ancestor);
    }
    // The handle path and controlled descriptor regressions share one policy.
    // This does not admit a descriptor as a filesystem capability.
    void verify_descriptor(PSECURITY_DESCRIPTOR raw,bool ancestor=false) const {
        PACL dacl=nullptr;PSID owner=nullptr;BOOL present=FALSE,defaulted=FALSE;
        if(!raw || !IsValidSecurityDescriptor(raw) ||
           !GetSecurityDescriptorOwner(raw,&owner,&defaulted) ||
           !GetSecurityDescriptorDacl(raw,&present,&dacl,&defaulted) ||
           !owner || !present || !dacl || !IsValidAcl(dacl))fail(ERROR_ACCESS_DENIED);
        struct Free {void* p;~Free(){LocalFree(p);}};
        std::array<unsigned char,SECURITY_MAX_SID_SIZE> system{},admins{};DWORD a=system.size(),b=admins.size();
        if(!CreateWellKnownSid(WinLocalSystemSid,nullptr,system.data(),&a) || !CreateWellKnownSid(WinBuiltinAdministratorsSid,nullptr,admins.data(),&b))fail();
        PSID installer=nullptr;
        if(!ConvertStringSidToSidW(L"S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464",&installer))fail();Free installer_memory{installer};
        auto trusted=[&](PSID candidate){return EqualSid(candidate,sid()) || EqualSid(candidate,system.data()) || EqualSid(candidate,admins.data()) || EqualSid(candidate,installer);};
        if(ancestor?!trusted(owner):!EqualSid(owner,sid()))fail(ERROR_ACCESS_DENIED);
        SECURITY_DESCRIPTOR_CONTROL control{};DWORD revision=0;
        if(!GetSecurityDescriptorControl(raw,&control,&revision) || (!ancestor && !(control&SE_DACL_PROTECTED)))fail(ERROR_ACCESS_DENIED);
        constexpr DWORD mutation=FILE_DELETE_CHILD|DELETE|WRITE_DAC|WRITE_OWNER|FILE_WRITE_ATTRIBUTES|FILE_WRITE_EA|GENERIC_WRITE|GENERIC_ALL;
        for(DWORD n=0;n<dacl->AceCount;++n) {
            void* entry=nullptr;if(!GetAce(dacl,n,&entry))fail();auto* header=static_cast<ACE_HEADER*>(entry);
            if(header->AceFlags&INHERIT_ONLY_ACE)continue;
            if(header->AceType==ACCESS_DENIED_ACE_TYPE)continue;
            if(header->AceType!=ACCESS_ALLOWED_ACE_TYPE)fail(ERROR_ACCESS_DENIED);
            auto* ace=static_cast<ACCESS_ALLOWED_ACE*>(entry);
            // OWNER_RIGHTS grants access to this object's already-verified owner.
            // It is valid for ancestors, never a substitute for private leaf ACLs.
            if(ancestor?((ace->Mask&mutation) && !trusted(&ace->SidStart) &&
                !IsWellKnownSid(&ace->SidStart,WinCreatorOwnerRightsSid)):
                !EqualSid(&ace->SidStart,sid()))fail(ERROR_ACCESS_DENIED);
        }
    }
};
inline std::vector<Handle> pin_directories(const std::wstring& path) {
    std::vector<Handle> pins;PrivateAccess access;
    auto pin=[&](const std::wstring& name) {
        Handle h(CreateFileW(name.c_str(),FILE_LIST_DIRECTORY|FILE_READ_ATTRIBUTES|READ_CONTROL,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
        if(h.value==INVALID_HANDLE_VALUE)fail();
        BY_HANDLE_FILE_INFORMATION info{};
        if(!GetFileInformationByHandle(h.value,&info))fail();
        if(!(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) || (info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))fail(ERROR_ACCESS_DENIED);
        access.verify(h.value,true);pins.push_back(std::move(h));
    };
    pin(path.substr(0,7));
    for(std::size_t start=7;start<path.size();) {
        auto end=path.find(L'\\',start);pin(path.substr(0,end));
        if(end==path.npos)break;start=end+1;
    }
    return pins;
}
}
#endif
