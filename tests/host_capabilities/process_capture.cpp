#include "../../src/runtime/host_internal.hpp"
#include "void/path.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#include "../../src/runtime/host_windows.hpp"
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <spawn.h>
#include <unistd.h>
extern char** environ;
#ifdef VOID_CAPTURE_READ_FAULT
namespace { thread_local int reads_before_fault=-1; }
extern "C" ssize_t __real_read(int,void*,std::size_t);
extern "C" ssize_t __wrap_read(int fd,void* output,std::size_t size) {
    if(reads_before_fault==0) {
        reads_before_fault=-1;errno=EIO;return -1;
    }
    const auto result=__real_read(fd,output,size);
    if(result>0 && reads_before_fault>0)--reads_before_fault;
    return result;
}
#endif
#endif

using namespace std::chrono_literals;
using namespace vd::host;
namespace fs=std::filesystem;
namespace {
void require(bool condition,const char* message) {
    if(!condition)throw std::runtime_error(message);
}
std::string pattern(std::size_t size,unsigned stream) {
    std::string bytes(size,'\0');
    for(std::size_t n=0;n<size;++n)bytes[n]=static_cast<char>((n+stream*17)&255);
    return bytes;
}
void write_output(unsigned stream,std::string_view bytes) {
    while(!bytes.empty()) {
#ifdef _WIN32
        DWORD written=0;
        if(!WriteFile(GetStdHandle(stream?STD_ERROR_HANDLE:STD_OUTPUT_HANDLE),bytes.data(),
                      static_cast<DWORD>(bytes.size()),&written,nullptr) || !written)std::_Exit(90);
#else
        const auto written=::write(static_cast<int>(stream)+1,bytes.data(),bytes.size());
        if(written<0 && errno==EINTR)continue;
        if(written<=0)std::_Exit(90);
#endif
        bytes.remove_prefix(static_cast<std::size_t>(written));
    }
}
unsigned long pid() {
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return static_cast<unsigned long>(::getpid());
#endif
}
bool exited(unsigned long id) {
#ifdef _WIN32
    win::Handle process(OpenProcess(SYNCHRONIZE,FALSE,static_cast<DWORD>(id)));
    if(!process.value)return GetLastError()==ERROR_INVALID_PARAMETER;
    return WaitForSingleObject(process.value,0)==WAIT_OBJECT_0;
#else
    return ::kill(static_cast<pid_t>(id),0)<0 && errno==ESRCH;
#endif
}
std::size_t handles() {
#ifdef _WIN32
    DWORD count=0;require(GetProcessHandleCount(GetCurrentProcess(),&count),"handle count");
    return count;
#else
    return static_cast<std::size_t>(std::distance(fs::directory_iterator("/proc/self/fd"),fs::directory_iterator{}));
#endif
}
void marker(const std::string& path) {
    std::ofstream output(vd::utf8_path(path),std::ios::binary);
    output<<pid();
    require(bool(output),"child marker");
}
void descendant(const std::string& self,const std::string& path) {
#ifdef _WIN32
    auto image=win::wide(self);
    std::wstring command=L"\""+image+L"\" --late \""+win::wide(path)+L"\"";
    STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES;
    startup.hStdInput=GetStdHandle(STD_INPUT_HANDLE);startup.hStdOutput=GetStdHandle(STD_OUTPUT_HANDLE);
    startup.hStdError=GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION info{};
    require(CreateProcessW(image.c_str(),command.data(),nullptr,nullptr,TRUE,0,nullptr,nullptr,&startup,&info)!=0,"descendant launch");
    win::Handle process(info.hProcess),thread(info.hThread);
#else
    pid_t child;
    std::array argv{const_cast<char*>(self.c_str()),const_cast<char*>("--late"),const_cast<char*>(path.c_str()),static_cast<char*>(nullptr)};
    require(::posix_spawn(&child,self.c_str(),nullptr,nullptr,argv.data(),environ)==0,"descendant launch");
#endif
}
int fixture(const std::vector<std::string>& args) {
    const auto& mode=args.at(1);
    if(mode=="--io") {
        auto first=pattern(std::stoul(args.at(2)),0),second=pattern(std::stoul(args.at(3)),1);
        std::thread stdout_writer([&]{write_output(0,first);});
        write_output(1,second);
        stdout_writer.join();
        return std::stoi(args.at(4));
    }
    if(mode=="--args") {
        require(args.size()==8 && args[2].empty() && args[3]=="two words" &&
            args[4]=="\"quote\"" && args[5]=="tail\\" && args[6]=="雪😀","literal arguments");
#ifdef _WIN32
        auto value=::_wgetenv(L"VOID_CAPTURE");
        require(value && std::wstring_view(value)==L"雪=value" && !::_wgetenv(L"VOID_PARENT"),"complete environment");
#else
        auto value=std::getenv("VOID_CAPTURE");
        require(value && std::string_view(value)=="雪=value" && !std::getenv("VOID_PARENT"),"complete environment");
#endif
        require(fs::equivalent(fs::current_path(),vd::utf8_path(args[7])),"explicit child cwd");
        write_output(0,"verified");
        return 0;
    }
    if(mode=="--abnormal") {
#ifdef _WIN32
        TerminateProcess(GetCurrentProcess(),0xc0000005u);
#else
        std::raise(SIGTERM);
#endif
        std::_Exit(91);
    }
    if(mode=="--inheritance") {
        const auto handle=std::stoull(args.at(2));
#ifdef _WIN32
        DWORD flags=0;
        require(!GetHandleInformation(reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(handle)),&flags),"unrelated handle inherited");
#else
        require(::fcntl(static_cast<int>(handle),F_GETFD)<0 && errno==EBADF,"unrelated descriptor inherited");
#endif
        return 0;
    }
    if(mode=="--linger") {
        write_output(0,"ready");write_output(1,"ready");
        // Write beyond each native pipe's capacity before publishing the
        // cancellation marker. Both readers must have made progress first.
        if(args.at(3)!="deadline") {
            write_output(0,pattern(131072,0));
            write_output(1,pattern(131072,1));
        }
        marker(args.at(2));
        const auto end=std::chrono::steady_clock::now()+8s;
        while(std::chrono::steady_clock::now()<end) {
            if(args.at(3)=="active"){write_output(0,pattern(1024,0));write_output(1,pattern(1024,1));}
            std::this_thread::sleep_for(5ms);
        }
        return 92;
    }
    if(mode=="--descendant"){descendant(args[0],args.at(2));return 31;}
    if(mode=="--late"){std::this_thread::sleep_for(3s);marker(args.at(2));return 0;}
    throw std::runtime_error("unknown child fixture");
}
void wait_for(const fs::path& path) {
    const auto deadline=std::chrono::steady_clock::now()+5s;
    while(std::chrono::steady_clock::now()<deadline) {
        if(fs::exists(path) && fs::file_size(path)>0)return;
        std::this_thread::sleep_for(5ms);
    }
    throw std::runtime_error("child marker was not written");
}
unsigned long read_pid(const fs::path& path) {
    unsigned long id=0;
    std::ifstream(path)>>id;
    require(id!=0,"child marker PID");
    return id;
}
void native(const std::string& executable,const std::string& cwd) {
    vd::Runtime runtime;
    std::atomic<bool> cancelled=false;
    auto run=[&](std::vector<std::string> arguments,CaptureLimits limits,
                 std::string image={}) {
        Request request{};request.operation=spawn;
        request.texts={image.empty()?executable:std::move(image),cwd};
        request.arrays={std::move(arguments),{"VOID_CAPTURE=雪=value"}};
        auto reply=capture_execute(request,limits,runtime,cancelled);
        require(!request.acquired,"one-shot child ownership retired");
        return reply;
    };
    auto done=[](const CaptureResult& result,int code) {
        require(!result.launch.kind && result.end==CaptureEnd::complete && result.exited &&
                result.status.number==code && !result.status.flag && !result.cleanup_code,"normal capture status");
    };
#ifdef _WIN32
    // First launch initializes process-wide CNG and tracing state. Formatting
    // the first launch error also loads the Win32 message resource.
    done(run({"--io","0","0","0"},{0,0,0,2000}),0);
    require(run({"--io","0","0","0"},{0,0,0,3000},executable+".absent").launch.kind==1,"launch failure warmup");
#endif
    const auto baseline=handles();
    auto result=run({"--io","0","0","0"},{0,0,0,2000});
    done(result,0);require(result.eof[0] && result.eof[1],"zero-budget EOF");
    result=run({"--io","256","0","23"},{256,-1,256,2000});
    done(result,23);require(result.output[0]==pattern(256,0) && result.eof[0] && !result.eof[1],"stdout-only binary capture");
    result=run({"--io","0","256","0"},{-1,256,256,2000});
    done(result,0);require(result.output[1]==pattern(256,1) && result.eof[1] && !result.eof[0],"stderr-only binary capture");
    result=run({"--io","262144","262144","23"},{262144,262144,524288,5000});
    done(result,23);require(result.output[0]==pattern(262144,0) && result.output[1]==pattern(262144,1) &&
        result.eof[0] && result.eof[1],"two concurrent writers and exact budgets");
    result=run({"--args","","two words","\"quote\"","tail\\","雪😀",cwd},{32,32,64,3000});
    done(result,0);require(result.output[0]=="verified","verified literal launch output");
    result=run({"--abnormal"},{0,0,0,3000});
    require(!result.launch.kind && result.end==CaptureEnd::complete && result.exited,"abnormal capture status");
#ifdef _WIN32
    require(!result.status.flag && result.status.number==0xc0000005u,"full Windows native status");
    {
        SECURITY_ATTRIBUTES inheritable{sizeof(inheritable),nullptr,TRUE};
        win::Handle event(CreateEventW(&inheritable,FALSE,FALSE,nullptr));
        require(event.value!=nullptr,"inheritable event fixture");
        done(run({"--inheritance",std::to_string(reinterpret_cast<std::uintptr_t>(event.value))},{0,0,0,3000}),0);
    }
#else
    require(result.status.flag && result.status.detail==SIGTERM && result.status.number==0,"separate POSIX signal");
    {
        const int fd=::fcntl(STDOUT_FILENO,F_DUPFD,200);
        require(fd>=200,"inheritable descriptor fixture");
        auto inheritance=run({"--inheritance",std::to_string(fd)},{0,0,0,3000});
        ::close(fd);done(inheritance,0);
    }
#endif
    require(run({"--io","0","0","0"},{-1,-1,0,2000}).launch.kind==6,"invalid options");
    for(unsigned n=0;n<12;++n) {
        done(run({"--io","4","4","0"},{4,4,8,2000}),0);
        result=run({"--io","262144","262144","0"},{64,64,95,3000});
        require(!result.launch.kind && result.end==CaptureEnd::output_limit && !result.cleanup_code &&
            result.output[0].size()+result.output[1].size()<=95 &&
            (result.output[0].size()==64 || result.output[1].size()==64 ||
             result.output[0].size()+result.output[1].size()==95),"bounded overflow cleanup");
#ifdef _WIN32
        result=run({"--io","20","20","0"},{4,4,8,3000});
        require(!result.launch.kind && result.end==CaptureEnd::output_limit && !result.cleanup_code &&
            (result.output[0].size()==4 || result.output[1].size()==4),"short-lived child overflow cleanup");
#endif
        require(run({"--io","0","0","0"},{0,0,0,3000},executable+".absent").launch.kind==1,"launch failure");
    }
    result=run({"--io","4000","4000","0"},{4000,4000,7999,3000});
    require(result.end==CaptureEnd::output_limit && result.output[0].size()+result.output[1].size()==7999,"exact aggregate prefix");
#ifdef VOID_CAPTURE_READ_FAULT
    reads_before_fault=2;
    result=run({"--io","262144","262144","0"},{262144,262144,524288,5000});
    require(result.end==CaptureEnd::io_failure && result.io_code==EIO && !result.cleanup_code &&
        result.output[0].size()+result.output[1].size()>0,"injected read failure retains partial output");
#endif
    const auto path=vd::utf8_path(cwd)/"capture marker";
    const auto path_text=vd::path_utf8(path);
    result=run({"--linger",path_text,"deadline"},{64,64,128,1200});
    require(result.end==CaptureEnd::deadline && !result.exited && result.output[0]=="ready" &&
        result.output[1]=="ready" && !result.cleanup_code && exited(read_pid(path)),"deadline retains partial output and reaps");
    fs::remove(path);
    for(const auto* mode:{"quiet","active"}) {
        std::atomic<bool> observed=false;
        std::jthread interrupter([&]{
            try {wait_for(path);observed=true;}catch(const std::exception&){}
            cancelled=true;
        });
        result=run({"--linger",path_text,mode},{1048576,1048576,2097152,7000});
        interrupter.join();cancelled=false;
        require(observed && result.end==CaptureEnd::cancelled && !result.cleanup_code,"entered capture cancelled");
        require(result.output[0].starts_with("ready") && result.output[1].starts_with("ready"),"cancellation retains two partial streams");
        require(exited(read_pid(path)),"cancellation reaps the direct child");
        fs::remove(path);
    }
    result=run({"--descendant",path_text},{0,0,0,1200});
    require(result.end==CaptureEnd::deadline && result.exited && result.status.number==31 &&
        !result.eof[0] && !result.eof[1] && !result.cleanup_code,"inherited descendant writers reach deadline");
    wait_for(path);fs::remove(path);
    require(handles()==baseline,"no retained handles or descriptors");
    std::cout<<"native capture ownership, limits, cancellation and descendant controls passed\n";
}
int run(const std::vector<std::string>& args) {
    try {
        if(args.at(1)=="--native") {native(args.at(2),args.at(3));return 0;}
        if(args.at(1)=="--reaped")return exited(std::stoul(args.at(2)))?0:1;
        return fixture(args);
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv) {
    std::vector<std::string> args;
    for(int n=0;n<argc;++n) {
        const auto count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[n],-1,nullptr,0,nullptr,nullptr);
        if(!count)return 3;
        std::string value(count,'\0');
        if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[n],-1,value.data(),count,nullptr,nullptr))return 3;
        value.pop_back();args.push_back(std::move(value));
    }
    return run(args);
}
#else
int main(int argc,char** argv){return run(std::vector<std::string>(argv,argv+argc));}
#endif
