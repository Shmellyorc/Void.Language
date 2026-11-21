#include "void/crash_diagnostics.hpp"
#include <csignal>
#include <cstdio>
#include <thread>
#include <vector>
#include <llvm/Support/Program.h>
#include <cstdlib>
#include <limits>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif
// Maintained crash fixture: no production crash-injection flag or LSP method.
#if defined(__clang__)
__attribute__((noinline,optnone))
#endif
int exhaust(unsigned depth) {if(depth==10000000)return 0;volatile char bytes[8192];bytes[0]=static_cast<char>(depth);return bytes[0]+exhaust(depth+1);}
void sanitizer_failure() {
    auto bytes=static_cast<volatile char*>(std::malloc(1));
    bytes[2]=1;std::free(const_cast<char*>(bytes));
}
#ifdef _WIN32
LONG WINAPI recover_exception(EXCEPTION_POINTERS* error) {
    return error->ExceptionRecord->ExceptionCode==0xE0420090u
        ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH;
}
// Keep SEH in a function with no C++ object unwinding requirements.
int handled_exception() {
    __try {RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);}
    __except(EXCEPTION_EXECUTE_HANDLER) {return static_cast<int>(EXCEPTION_ACCESS_VIOLATION);}
    return 1;
}
#endif
int main(int argc,char** argv) {
#ifdef _WIN32
    SetErrorMode(SEM_NOGPFAULTERRORBOX|SEM_FAILCRITICALERRORS); // Controlled native fixture only.
#endif
    if(argc>1 && !std::strcmp(argv[1],"raw-abort")) {
#ifdef _WIN32
        RaiseException(EXCEPTION_ACCESS_VIOLATION,EXCEPTION_NONCONTINUABLE,0,nullptr);
#else
        std::raise(SIGABRT);
#endif
        return 1;
    }
    if(auto status=vd::crash::start("probe",argv[0],"fixture-build"))return *status;
    if(argc<2)return 2;
#ifndef _WIN32
    if(!std::strcmp(argv[1],"fork-context")) {
        vd::crash::context(vd::crash::Operation::frontend);
        auto child=fork();if(child<0)return 4;
        if(!child){vd::crash::context(vd::crash::Operation::jit_execute);_exit(0);}
        int status=0;if(waitpid(child,&status,0)!=child || status)return 5;
        std::raise(SIGABRT);return 6;
    }
    if(!std::strcmp(argv[1],"fork-sanitizer") || !std::strcmp(argv[1],"fork-sanitizer-nested") ||
       !std::strcmp(argv[1],"fork-sanitizer-parent-crash")) {
        vd::crash::context(vd::crash::Operation::frontend);
        auto child=fork();if(child<0)return 4;
        if(!child) {
            if(!std::strcmp(argv[1],"fork-sanitizer-nested")) {
                auto grandchild=fork();if(grandchild<0)_exit(4);
                if(grandchild) {int status=0;waitpid(grandchild,&status,0);_exit(0);}
            }
            sanitizer_failure();_exit(1);
        }
        int status=0;if(waitpid(child,&status,0)!=child)return 5;
        std::printf("parent normal; observed child status=%d\n",status);
        if(!std::strcmp(argv[1],"fork-sanitizer-parent-crash"))std::raise(SIGABRT);
        return 0;
    }
#else
    if(!std::strcmp(argv[1],"top-level-filter")) {
        SetUnhandledExceptionFilter(recover_exception);
        RaiseException(0xE0420090u,0,0,nullptr);
        return 17;
    }
    if(!std::strcmp(argv[1],"handled"))return handled_exception();
    if(!std::strcmp(argv[1],"exit-process"))ExitProcess(EXCEPTION_ACCESS_VIOLATION);
    if(!std::strcmp(argv[1],"fail-fast"))RaiseFailFastException(nullptr,nullptr,0);
    if(!std::strcmp(argv[1],"custom-exception"))RaiseException(0xE0420090u,EXCEPTION_NONCONTINUABLE,0,nullptr);
    if(!std::strcmp(argv[1],"teardown-exception")) {
        vd::crash::context(vd::crash::Operation::finished);
        RaiseException(EXCEPTION_ACCESS_VIOLATION,EXCEPTION_NONCONTINUABLE,0,nullptr);
    }
#endif
    if(!std::strcmp(argv[1],"args")) {for(int i=2;i<argc;++i)std::printf("%s\n",argv[i]);return 0;}
    if(!std::strcmp(argv[1],"exit"))return argc>2?std::atoi(argv[2]):42;
    if(!std::strcmp(argv[1],"wait")) {
        std::puts("ready");std::fflush(stdout);
        for(;;) {
#ifdef _WIN32
            Sleep(1000);
#else
            pause();
#endif
        }
    }
    if(!std::strcmp(argv[1],"child")) {
        std::vector<llvm::StringRef> arguments{argv[0],"raw-abort"};
        auto status=llvm::sys::ExecuteAndWait(argv[0],arguments);
        vd::crash::child_termination(argv[0],status,false);return 0;
    }
    if(!std::strcmp(argv[1],"stack-thread")) {std::thread thread([]{exhaust(0);});thread.join();return 0;}
    if(!std::strcmp(argv[1],"stack"))return exhaust(0);
    if(!std::strcmp(argv[1],"ubsan")) {volatile int maximum=std::numeric_limits<int>::max();return maximum+argc;}
    if(!std::strcmp(argv[1],"sanitizer")) {
        sanitizer_failure();return 0;
    }
    for(unsigned i=0;i<1000;++i)vd::crash::context(vd::crash::Operation::frontend);
    vd::crash::context(vd::crash::Operation::jit_execute);
#ifdef _WIN32
    RaiseException(EXCEPTION_ACCESS_VIOLATION,EXCEPTION_NONCONTINUABLE,0,nullptr);
#else
    std::raise(SIGABRT);
#endif
    return 0;
}
