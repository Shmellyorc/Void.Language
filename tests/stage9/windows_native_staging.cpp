#include "../../src/native_windows.hpp"
#include <fstream>
#include <future>
#include <iostream>
#include <set>
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
int main(int argc,char** argv)try {
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
    occupied=home/"collision";fs::create_directory(occupied);write(occupied/"sentinel","another owner");
    fs::path owned;
    {Directory d(home,true,collide_once);owned=d.directory;require(owned!=occupied,"adopted collision");
     PrivateAccess access;access.verify(d.pins.back().value);access_check(d.pins.back().value);
     // Directory and ancestors cannot be renamed while reserved.
     require(!MoveFileExW(d.directory.c_str(),(home/"moved").c_str(),0),"directory reservation can be renamed");}
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
