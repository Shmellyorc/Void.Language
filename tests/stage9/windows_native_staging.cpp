#include "../../src/native_windows.hpp"
#include <fstream>
#include <future>
#include <iostream>
#include <set>
#include <sddl.h>
using namespace vd::native_windows;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F> void rejects(F action){bool failed=false;try{action();}catch(const std::exception&){failed=true;}require(failed,"unsafe operation succeeded");}
fs::path occupied;
unsigned calls=0;
fs::path collide_once(){return calls++==0?occupied.filename():random_leaf();}
fs::path always_collide(){return occupied.filename();}
void write(const fs::path& p,const std::string& data){std::ofstream out(p,std::ios::binary);out<<data;require(bool(out),"fixture write failed");}
std::string bytes(const fs::path& p){std::ifstream in(p,std::ios::binary);return {(std::istreambuf_iterator<char>(in)),{}};}
void access_check(HANDLE directory) {
    PSECURITY_DESCRIPTOR descriptor=nullptr;
    auto error=GetSecurityInfo(directory,SE_FILE_OBJECT,OWNER_SECURITY_INFORMATION|GROUP_SECURITY_INFORMATION|
        DACL_SECURITY_INFORMATION,nullptr,nullptr,nullptr,nullptr,&descriptor);
    if(error!=ERROR_SUCCESS)fail("Cannot read test descriptor",error);
    struct Free{PSECURITY_DESCRIPTOR p;~Free(){LocalFree(p);}} memory{descriptor};
    HANDLE raw;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY|TOKEN_DUPLICATE,&raw))fail("Cannot open test token");
    Handle process(raw);HANDLE duplicate;
    // AccessCheck queries the impersonation token; CreateRestrictedToken also
    // needs TOKEN_DUPLICATE on this new handle (DuplicateToken omits it).
    if(!DuplicateTokenEx(process.value,TOKEN_QUERY|TOKEN_DUPLICATE,nullptr,
        SecurityImpersonation,TokenImpersonation,&duplicate))fail("Cannot duplicate test token");
    Handle token(duplicate);
    auto allowed=[&](HANDLE identity) {
        GENERIC_MAPPING mapping{FILE_GENERIC_READ,FILE_GENERIC_WRITE,FILE_GENERIC_EXECUTE,FILE_ALL_ACCESS};
        std::array<unsigned char,1024> privileges{};DWORD length=static_cast<DWORD>(privileges.size()),granted=0;BOOL result=FALSE;
        if(!AccessCheck(descriptor,identity,FILE_ADD_FILE|FILE_ADD_SUBDIRECTORY|FILE_DELETE_CHILD|DELETE,
            &mapping,reinterpret_cast<PRIVILEGE_SET*>(privileges.data()),&length,&granted,&result))fail("AccessCheck failed");
        return result!=FALSE;
    };
    require(allowed(token.value),"owner lacks effective directory access");
    std::array<unsigned char,SECURITY_MAX_SID_SIZE> everyone{};DWORD size=static_cast<DWORD>(everyone.size());
    if(!CreateWellKnownSid(WinWorldSid,nullptr,everyone.data(),&size))fail("Cannot create test SID");
    SID_AND_ATTRIBUTES restriction{everyone.data(),0};HANDLE restricted;
    if(!CreateRestrictedToken(token.value,DISABLE_MAX_PRIVILEGE,0,nullptr,0,nullptr,1,&restriction,&restricted))
        fail("Cannot create restricted test token");
    Handle other(restricted);
    require(!allowed(other.value),"Everyone restriction has effective mutation access");
}
bool same_directory(const BY_HANDLE_FILE_INFORMATION& a,const BY_HANDLE_FILE_INFORMATION& b) {
    return a.dwVolumeSerialNumber==b.dwVolumeSerialNumber &&
        a.nFileIndexHigh==b.nFileIndexHigh && a.nFileIndexLow==b.nFileIndexLow;
}
bool exists_directory(const fs::path& path) {
    auto attributes=GetFileAttributesW(path.c_str());
    if(attributes!=INVALID_FILE_ATTRIBUTES) {
        require((attributes & FILE_ATTRIBUTE_DIRECTORY)!=0,"probe path is not a directory");return true;
    }
    auto error=GetLastError();
    if(error!=ERROR_FILE_NOT_FOUND && error!=ERROR_PATH_NOT_FOUND)fail("Cannot inspect rename probe path",error);
    return false;
}
bool directory_identity(const fs::path& path,const BY_HANDLE_FILE_INFORMATION& expected) {
    Handle h(CreateFileW(path.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
    if(h.value==INVALID_HANDLE_VALUE)fail("Cannot inspect renamed directory identity");
    return same_directory(information(h.value),expected);
}
struct RenameResult {
    bool renamed,source_exists,destination_exists,source_original,destination_original,handles_original;
    DWORD error;
};
RenameResult rename_probe(const char* label,const fs::path& source,const fs::path& destination,
                          const std::vector<HANDLE>& handles) {
    require(!handles.empty() && !exists_directory(destination),"rename probe requires a fresh destination");
    std::vector<BY_HANDLE_FILE_INFORMATION> before;
    for(auto h:handles)before.push_back(information(h));
    require(directory_identity(source,before.back()),"probe handle did not identify source");
    const BOOL returned=MoveFileExW(source.c_str(),destination.c_str(),0);
    const bool renamed=returned!=FALSE;
    // GetLastError is meaningful only for a failed MoveFileExW.
    auto error=renamed?ERROR_SUCCESS:GetLastError();
    auto source_exists=exists_directory(source),destination_exists=exists_directory(destination);
    bool stable=true;
    for(std::size_t i=0;i<handles.size();++i)
        stable= same_directory(before[i],information(handles[i])) && stable;
    RenameResult result{renamed,source_exists,destination_exists,
        source_exists && directory_identity(source,before.back()),
        destination_exists && directory_identity(destination,before.back()),stable,error};
    std::cout<<"rename_probe="<<label<<" returned="<<returned<<" failure="<<error
        <<" source_exists="<<source_exists<<" destination_exists="<<destination_exists
        <<" source_original="<<result.source_original<<" destination_original="<<result.destination_original
        <<" handles_original="<<stable<<" handles="<<handles.size()
        <<" original_volume="<<before.back().dwVolumeSerialNumber
        <<" original_file_id="<<before.back().nFileIndexHigh<<':'<<before.back().nFileIndexLow<<std::endl;
    return result;
}
void metadata_reservation_control(const fs::path& home) {
    // Reproduce the old access mask inside our exclusively owned fixture. Its
    // parent remains reserved; only this control's child has metadata-only opens.
    const auto source=home/"metadata-control",moved=home/"metadata-moved";
    require(fs::create_directory(source),"cannot exclusively create metadata control");
    {
        std::vector<Handle> metadata;std::vector<HANDLE> observed;
        for(unsigned i=0;i<2;++i) {
            Handle h(CreateFileW(source.c_str(),FILE_READ_ATTRIBUTES|READ_CONTROL,
                FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
            if(h.value==INVALID_HANDLE_VALUE)fail("Cannot open metadata control");
            observed.push_back(h.value);metadata.push_back(std::move(h));
        }
        auto result=rename_probe("metadata_only",source,moved,observed);
        require(result.handles_original,"metadata handles changed directory identity");
        if(result.renamed) {
            require(!result.source_exists && result.destination_original,"metadata rename did not move the original directory");
            if(!MoveFileExW(moved.c_str(),source.c_str(),0))fail("Cannot restore owned metadata control");
        } else require(result.source_original && !result.destination_exists,"failed metadata rename changed paths");
    }
    // Positive control after closing handles rules out permissions or an invalid
    // rename as the reason a reservation attempt was refused.
    if(!MoveFileExW(source.c_str(),moved.c_str(),0))fail("Unreserved directory rename failed");
    if(!MoveFileExW(moved.c_str(),source.c_str(),0))fail("Cannot restore unreserved control");
    require(fs::remove(source),"metadata control cleanup failed");
}
void describe_root_acl(const char* label,HANDLE root) {
    PSECURITY_DESCRIPTOR descriptor=nullptr;
    auto error=GetSecurityInfo(root,SE_FILE_OBJECT,OWNER_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION,
        nullptr,nullptr,nullptr,nullptr,&descriptor);
    if(error!=ERROR_SUCCESS)fail("Cannot inspect test root ACL",error);
    struct Free {void* p;~Free(){LocalFree(p);}} memory{descriptor};
    LPWSTR text=nullptr;
    if(!ConvertSecurityDescriptorToStringSecurityDescriptorW(descriptor,SDDL_REVISION_1,
        OWNER_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION,&text,nullptr))fail("Cannot describe test root ACL");
    Free converted{text};
    // SDDL's SID/ACE syntax is ASCII, independent of the path/console encoding.
    std::cout<<"root_acl="<<label<<" sddl=";
    for(auto c=text;*c;++c)std::cout<<static_cast<char>(*c);
    std::cout<<std::endl;
}
void test_root_command(bool create) {
    // UTF-8 stdin preserves Unicode/% paths independently of main's narrow argv
    // encoding on Windows. The command name itself is ASCII.
    std::string text;
    require(bool(std::getline(std::cin,text)) && !text.empty(),"missing UTF-8 test root path");
    const auto path=vd::utf8_path(text);
    if(!create) {
        auto pins=pin_directories(path);describe_root_acl("inherited",pins.back().value);
        PrivateAccess access;
        try {access.verify_root(pins.back().value);std::cout<<"inherited_root_admitted=1\n";}
        catch(const std::runtime_error& error) {std::cout<<"inherited_root_admitted=0 reason="<<error.what()<<'\n';}
        return;
    }
    auto parents=pin_directories(path.parent_path());
    require(path.is_absolute() && path==path.lexically_normal(),"test root path must be absolute and normalized");
    PrivateAccess access;
    // Never adopt/re-ACL an existing directory. Apply the production private
    // descriptor during exclusive creation, before exposing the path as TMP/TEMP.
    if(!CreateDirectoryW(path.c_str(),&access.attributes))fail("Cannot create private test root");
    try {
        auto pins=pin_directories(path);
        access.verify(pins.back().value);access.verify_root(pins.back().value);
        access_check(pins.back().value);describe_root_acl("private",pins.back().value);
        std::cout<<"private_root_verified=1 owner_access=1 restricted_mutation=0\n";
    } catch(...) {
        // Only successful creation above grants ownership; this root is empty.
        if(!RemoveDirectoryW(path.c_str()))std::cerr<<"Private test root cleanup failed: "<<GetLastError()<<'\n';
        throw;
    }
    // The Python TemporaryDirectory fixture owns eventual cleanup after success.
}
int main(int argc,char** argv)try {
    if(argc==2 && (std::string(argv[1])=="--inspect-root" || std::string(argv[1])=="--create-private-root")) {
        test_root_command(std::string(argv[1])=="--create-private-root");return 0;
    }
    // A disposable configured driver exercises zero-exit malformed output and
    // transfer failures through the real compiler, without production test hooks.
    if(argc==2 && std::string(argv[1])=="--version") {
        std::cout<<"clang version 19.1.7\n";return 0;
    }
    if(argc>2 && std::string(argv[1])=="--driver-mode=g++") {
        fs::path work,collision;
        for(int i=2;i<argc;++i) {
            if(std::string(argv[i])=="-working-directory" && i+1<argc)work=vd::utf8_path(argv[++i]);
            else if(std::string(argv[i]).starts_with("--void-test-copy-collision="))
                collision=vd::utf8_path(std::string(argv[i]).substr(std::string("--void-test-copy-collision=").size()));
        }
        require(!work.empty(),"fake driver missing working directory");
        if(collision.empty())write(work/"program.exe","nonempty but invalid PE");
        else {
            fs::copy_file(vd::utf8_path(argv[0]),work/"program.exe");
            unsigned found=0;
            for(const auto& item:fs::directory_iterator(collision))
                if(item.is_directory() && vd::path_utf8(item.path().filename()).starts_with(".void-build-")) {
                    write(item.path()/"program.exe","owned stage sentinel");++found;
                }
            require(found==1,"fake copy failure must target exactly one owned stage");
        }
        return 0;
    }

    auto root=temporary_root();Directory fixture(root,true);
    auto home=fixture.directory;
    metadata_reservation_control(home);
    occupied=home/"collision";fs::create_directory(occupied);write(occupied/"sentinel","another owner");
    fs::path owned;
    {Directory d(home,true,collide_once);owned=d.directory;require(owned!=occupied,"adopted collision");
     PrivateAccess access;access.verify(d.pins.back().value);access_check(d.pins.back().value);
     // Observe BOOL/error, both names, and every live handle's native file ID.
     const auto original=d.directory,moved=home/"moved";
     std::vector<HANDLE> observed;for(const auto& h:d.pins)observed.push_back(h.value);
     auto result=rename_probe("list_directory",original,moved,observed);
     if(result.renamed && result.destination_original && !result.source_exists) {
         // Recover only the verified owned directory even if this gate fails.
         d.directory=moved;
         if(MoveFileExW(moved.c_str(),original.c_str(),0))d.directory=original;
     }
     require(!result.renamed,"directory reservation can be renamed");
     require(result.error==ERROR_SHARING_VIOLATION,"directory rename failed for a reason other than sharing");
     require(result.source_original && !result.destination_exists && result.handles_original,
         "directory reservation lost path or handle identity");}
    require(!fs::exists(owned) && bytes(occupied/"sentinel")=="another owner","ownership cleanup failed");
    rejects([&]{Directory d(home,true,always_collide);});
    require(bytes(occupied/"sentinel")=="another owner","collision exhaustion deleted foreign directory");
    std::vector<std::future<fs::path>> workers;
    for(unsigned i=0;i<16;++i)workers.push_back(std::async(std::launch::async,[&]{Directory d(home,true);return d.directory;}));
    std::set<fs::path> unique;
    for(auto& worker:workers){auto p=worker.get();require(!fs::exists(p),"concurrent cleanup failed");unique.insert(p);}
    require(unique.size()==16,"random concurrent directories collided");
    fs::path failed;
    rejects([&]{Directory d(home,true);failed=d.directory;throw std::runtime_error("compile/link failure");});
    require(!fs::exists(failed),"failure leaked owned directory");
    // Complete-path rejection, even with a short generated leaf.
    auto deep=home;
    while((deep/L"vd-0123456789abcdef"/L"program.exe").native().size()+1<=240)deep/=L"long-parent-0123456789";
    fs::create_directories(deep);rejects([&]{Directory d(deep,true);});
    require(fs::is_empty(deep),"length rejection created a child");
    auto source=home/"source",target=home/"target";
    std::string content(200003,'x');content[70000]='%';content[150000]='\0';write(source,content);
    auto valid=verified_executable(vd::utf8_path(argv[0]));
    rejects([&]{auto invalid=verified_executable(source);});
    copy_checked(source,target);require(bytes(target)==content,"copy differs");
    write(target,"previous executable");rejects([&]{copy_checked(source,target);});
    require(bytes(target)=="previous executable","copy overwrote previous artifact");
    {Handle locked(CreateFileW(source.c_str(),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr));
     require(locked.value!=INVALID_HANDLE_VALUE,"cannot lock fixture");rejects([&]{copy_checked(source,home/"blocked");});}
    require(!fs::exists(home/"blocked"),"source-open failure left partial artifact");
    // Hard-linked generated executables are never accepted for transfer.
    auto alias=home/"alias";fs::create_hard_link(source,alias);
    rejects([&]{copy_checked(source,home/"hardlink-copy",true);});fs::remove(alias);
    // Insecure roots fail closed instead of trusting a random leaf.
    auto insecure=home/"insecure";fs::create_directory(insecure);
    // Null DACL is intentionally confined to the disposable fixture directory.
    if(SetNamedSecurityInfoW(const_cast<wchar_t*>(insecure.c_str()),SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION|PROTECTED_DACL_SECURITY_INFORMATION,nullptr,nullptr,nullptr,nullptr)!=ERROR_SUCCESS)
        fail("Cannot make insecure test root");
    rejects([&]{Directory d(insecure,true);});
    // Optional second real volume supplied by Boss, never a fabricated same-volume pass.
    if(argc==2) {
        auto other=vd::utf8_path(argv[1]);auto pins=pin_directories(other);
        require(information(pins.back().value).dwVolumeSerialNumber!=information(fixture.pins.back().value).dwVolumeSerialNumber,
            "cross-volume test needs a different volume");
        Directory destination(other,false,staging_leaf);copy_checked(source,destination.directory/"copied");
        require(bytes(destination.directory/"copied")==content,"cross-volume bytes differ");
        std::cout<<"Cross-volume copy passed\n";
    } else std::cout<<"Cross-volume copy pending: supply a writable second-volume directory\n";
    std::cout<<"Windows staging ownership, collisions, cleanup, copies and effective access passed\n";
    return 0;
} catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
