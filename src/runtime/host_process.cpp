#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "host_internal.hpp"
#include "void/utf8.hpp"
#include <cerrno>
#include <condition_variable>
#include <set>
#ifndef _WIN32
#include <spawn.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
namespace vd::host {
#ifndef _WIN32
namespace {
struct Child final : Object {
    std::mutex mutex;
    std::condition_variable changed;
    pid_t pid=-1;
    int status=0;
    bool waited=false;
    int wait_error=0;
    ~Child() override {close();}
    void stop() noexcept override {
        std::lock_guard lock(mutex);
        if(pid>0)::kill(pid,SIGKILL);
        changed.notify_all();
    }
    Result wait_for_exit() noexcept {
        std::unique_lock lock(mutex);
        while(!waited && !wait_error) {
            int native=0;auto result=::waitpid(pid,&native,WNOHANG);
            if(result==pid) {status=native;pid=-1;waited=true;break;}
            if(result<0) {if(errno==EINTR)continue;wait_error=errno;pid=-1;break;}
            changed.wait_for(lock,std::chrono::milliseconds(10));
        }
        if(wait_error)return native_error(wait_error);
        Result r;
        if(WIFEXITED(status))r.number=WEXITSTATUS(status);
        else if(WIFSIGNALED(status)){r.flag=true;r.detail=WTERMSIG(status);}
        else return error(17);
        return r;
    }
    std::error_code close() noexcept override {
        {std::lock_guard lock(mutex);if(pid<0 && !waited)return wait_error?std::error_code(wait_error,std::generic_category()):std::error_code{};}
        stop();auto r=wait_for_exit();return r.kind?std::error_code(static_cast<int>(r.code),std::generic_category()):std::error_code{};
    }
    std::error_code quiesce() noexcept override {return close();}
};
bool text(const std::string& value) {return value.find('\0')==std::string::npos && valid_utf8(value);}
bool absolute(const std::string& value) {return !value.empty() && value[0]=='/' && text(value);}
bool name(std::string_view value) {
    if(value.empty())return false;
    for(std::size_t n=0;n<value.size();++n) {
        auto c=value[n];if(c!='_' && !(c>='A'&&c<='Z') && !(c>='a'&&c<='z') && !(n && c>='0'&&c<='9'))return false;
    }
    return true;
}
struct Actions {
    posix_spawn_file_actions_t value{};
    bool active=false;
    ~Actions(){if(active)posix_spawn_file_actions_destroy(&value);}
};
struct Attributes {
    posix_spawnattr_t value{};
    bool active=false;
    ~Attributes(){if(active)posix_spawnattr_destroy(&value);}
};
}
#endif
Result process_execute(Request& q) {
#ifdef _WIN32
    return windows_process_execute(q);
#else
    if(q.operation==wait)return std::static_pointer_cast<Child>(q.objects[0])->wait_for_exit();
    if(q.operation!=spawn)throw std::logic_error("Invalid process operation");
    if(!absolute(q.texts[0]) || !absolute(q.texts[1]))return error(4);
    std::vector<char*> argv,env;argv.reserve(q.arrays[0].size()+2);env.reserve(q.arrays[1].size()+1);
    std::set<std::string_view> names;std::size_t bytes=q.texts[0].size()+q.texts[1].size();
    if(bytes>1048576)return error(12);
    argv.push_back(q.texts[0].data());
    for(auto& value:q.arrays[0]) {
        if(!text(value))return error(20);
        if(value.size()+1>1048576-bytes)return error(12);bytes+=value.size()+1;argv.push_back(value.data());
    }
    argv.push_back(nullptr);
    for(auto& value:q.arrays[1]) {
        auto equals=value.find('=');
        if(!text(value) || equals==std::string::npos || !name(std::string_view(value).substr(0,equals)) || !names.insert(std::string_view(value).substr(0,equals)).second)return error(20);
        if(value.size()+1>1048576-bytes)return error(12);bytes+=value.size()+1;env.push_back(value.data());
    }
    env.push_back(nullptr);
    auto child=std::make_shared<Child>();Actions actions;Attributes attributes;
    int code=posix_spawn_file_actions_init(&actions.value);if(code)return native_error(code);actions.active=true;
    code=posix_spawnattr_init(&attributes.value);if(code)return native_error(code);attributes.active=true;
#if defined(__linux__) || defined(__APPLE__)
    code=posix_spawn_file_actions_addchdir_np(&actions.value,q.texts[1].c_str());if(code)return native_error(code);
#else
    return error(11);
#endif
    sigset_t empty,defaults;sigemptyset(&empty);sigemptyset(&defaults);
    for(int signal:{SIGINT,SIGTERM,SIGQUIT,SIGPIPE})sigaddset(&defaults,signal);
    if((code=posix_spawnattr_setsigmask(&attributes.value,&empty)) ||
       (code=posix_spawnattr_setsigdefault(&attributes.value,&defaults)) ||
       (code=posix_spawnattr_setflags(&attributes.value,POSIX_SPAWN_SETSIGMASK|POSIX_SPAWN_SETSIGDEF)))return native_error(code);
    pid_t pid=-1;code=posix_spawn(&pid,q.texts[0].c_str(),&actions.value,&attributes.value,argv.data(),env.data());
    if(code)return native_error(code);child->pid=pid;q.acquired=child;return {};
#endif
}
}
