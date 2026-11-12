#include "host_internal.hpp"
#ifdef _WIN32
#include "host_windows.hpp"
#include <bcrypt.h>
#include <condition_variable>
#include <mutex>
#include <set>
namespace vd::host {
namespace {
// Private diagnostic metadata in the existing Result.detail slot, on error only.
enum class LaunchStage : std::int64_t { paths=1,arguments,environment,image_ancestors,
    cwd_ancestors,image_open,image_metadata,cwd_open,cwd_metadata,stdio,attributes_init,
    attributes_update,create_process };
struct Child final : CaptureChild {
    std::mutex mutex;
    std::condition_variable changed;
    win::Handle process;
    std::array<win::Handle,2> readers;
    bool waited=false;
    DWORD status=0,wait_error=0;
    ~Child() override {close();}
    std::error_code terminate() noexcept {
        std::lock_guard lock(mutex);
        std::error_code error;
        if(!waited && process.value!=INVALID_HANDLE_VALUE) {
            // Direct-child ownership only. No PID lookup or process-tree claim.
            if(!TerminateProcess(process.value,1))
                error={static_cast<int>(GetLastError()),std::system_category()};
        }
        changed.notify_all();
        return error;
    }
    void stop() noexcept override {(void)terminate();}
    std::optional<Result> poll_locked() noexcept {
        if(!waited && !wait_error) {
            auto state=WaitForSingleObject(process.value,0);
            if(state==WAIT_OBJECT_0) {
                if(!GetExitCodeProcess(process.value,&status))wait_error=GetLastError();
                else waited=true;
            } else if(state==WAIT_FAILED)wait_error=GetLastError();
            else return {};
        }
        if(wait_error)return native_error(static_cast<int>(wait_error));
        Result result;result.number=status;return result;
    }
    std::optional<Result> poll_exit() noexcept override {
        std::lock_guard lock(mutex);
        return poll_locked();
    }
    Result wait_for_exit() noexcept {
        std::unique_lock lock(mutex);
        for(;;) {
            if(auto result=poll_locked())return *result;
            changed.wait_for(lock,std::chrono::milliseconds(10));
        }
    }
    PipeRead read(unsigned stream,std::span<char> buffer) noexcept override {
        DWORD size=0;
        if(ReadFile(readers[stream].value,buffer.data(),static_cast<DWORD>(buffer.size()),&size,nullptr))
            return {size};
        const auto code=GetLastError();
        if(code==ERROR_BROKEN_PIPE || code==ERROR_PIPE_NOT_CONNECTED)return {0,0,true};
        return {0,code==ERROR_NO_DATA?0:static_cast<int>(code)};
    }
    std::error_code close_readers() noexcept override {
        auto first=readers[0].close(),second=readers[1].close();
        return first?first:second;
    }
    std::error_code close() noexcept override {
        {std::lock_guard lock(mutex);if(process.value==INVALID_HANDLE_VALUE)return {};}
        const auto termination=terminate();
        auto result=wait_for_exit();
        // Do not release the process authority while a child may still execute.
        if(result.kind)
            return termination?termination:std::error_code(static_cast<int>(result.code),std::system_category());
        std::lock_guard lock(mutex);
        const auto closed=process.close();
        // TerminateProcess can report access denied while a natural exit is
        // in progress. Only the completed wait above confirms that race.
        return termination && termination.value()!=ERROR_ACCESS_DENIED?termination:closed;
    }
    std::error_code quiesce() noexcept override {return close();}
};
std::wstring quote(std::wstring_view value) {
    std::wstring out=L"\"";std::size_t slashes=0;
    for(auto c:value) {
        if(c==L'\\'){++slashes;continue;}
        out.append(slashes*(c==L'"'?2:1),L'\\');slashes=0;
        if(c==L'"')out.push_back(L'\\');out.push_back(c);
    }
    out.append(slashes*2,L'\\');out.push_back(L'"');return out;
}
bool name(std::string_view value) {
    if(value.empty())return false;
    for(std::size_t n=0;n<value.size();++n) {
        auto c=value[n];if(c!='_' && !(c>='A'&&c<='Z') && !(c>='a'&&c<='z') && !(n && c>='0'&&c<='9'))return false;
    }
    return true;
}
struct Attributes {
    std::vector<unsigned char> storage;
    LPPROC_THREAD_ATTRIBUTE_LIST list=nullptr;
    ~Attributes(){if(list)DeleteProcThreadAttributeList(list);}
};
win::Handle capture_pipe(win::Handle& reader) {
    std::array<unsigned char,16> nonce;
    if(BCryptGenRandom(nullptr,nonce.data(),static_cast<ULONG>(nonce.size()),BCRYPT_USE_SYSTEM_PREFERRED_RNG)!=0)
        win::fail(ERROR_GEN_FAILURE);
    constexpr std::wstring_view digits=L"0123456789abcdef";
    std::wstring name=LR"(\\.\pipe\void-capture-)";
    for(auto byte:nonce){name+=digits[byte>>4];name+=digits[byte&15];}
    win::PrivateAccess private_access;
    // Only the reader uses nonblocking mode. The child receives a regular
    // blocking write handle and can apply ordinary pipe backpressure.
    reader.value=CreateNamedPipeW(name.c_str(),PIPE_ACCESS_INBOUND|FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_NOWAIT|PIPE_REJECT_REMOTE_CLIENTS,1,0,8192,0,&private_access.attributes);
    if(reader.value==INVALID_HANDLE_VALUE)win::fail();
    win::Handle writer(CreateFileW(name.c_str(),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr));
    if(writer.value==INVALID_HANDLE_VALUE)win::fail();
    if(!ConnectNamedPipe(reader.value,nullptr) && GetLastError()!=ERROR_PIPE_CONNECTED)win::fail();
    return writer;
}
}
Result windows_process_execute(Request& q,const CaptureLimits* capture) {
    auto stage=LaunchStage::paths;
    try {
        if(q.operation==wait)return std::static_pointer_cast<Child>(q.objects[0])->wait_for_exit();
        if(q.operation!=spawn)throw std::logic_error("Invalid Windows process operation");
#if !defined(_M_X64) || !defined(_MSC_VER)
        if(capture)return error(11);
#endif
        auto executable=win::absolute(q.texts[0]),cwd=win::absolute(q.texts[1]);
        stage=LaunchStage::arguments;
        std::wstring command=quote(win::wide(q.texts[0]));
        for(const auto& arg:q.arrays[0]) {command+=L' ';command+=quote(win::wide(arg));if(command.size()>=32767)return error(12);}
        if(command.size()>=32767)return error(12);
        stage=LaunchStage::environment;
        std::vector<std::wstring> entries;std::set<std::string> names;
        for(const auto& entry:q.arrays[1]) {
            auto equals=entry.find('=');if(equals==entry.npos || !name(std::string_view(entry).substr(0,equals)))return error(20);
            auto key=entry.substr(0,equals);for(auto& c:key)if(c>='a'&&c<='z')c-=32;
            if(!names.insert(key).second)return error(20);entries.push_back(win::wide(entry));
        }
        std::sort(entries.begin(),entries.end(),[](const auto& a,const auto& b){return CompareStringOrdinal(a.data(),static_cast<int>(a.find(L'=')),b.data(),static_cast<int>(b.find(L'=')),TRUE)==CSTR_LESS_THAN;});
        std::vector<wchar_t> environment;
        for(const auto& entry:entries){environment.insert(environment.end(),entry.begin(),entry.end());environment.push_back(0);if(environment.size()>32765)return error(12);}
        if(environment.empty())environment.push_back(0);environment.push_back(0);
        stage=LaunchStage::image_ancestors;
        auto image_ancestors=win::pin_directories(executable.substr(0,std::max<std::size_t>(7,executable.find_last_of(L'\\'))));
        stage=LaunchStage::cwd_ancestors;
        auto cwd_ancestors=win::pin_directories(cwd);
        stage=LaunchStage::image_open;
        win::Handle image(CreateFileW(executable.c_str(),GENERIC_READ|FILE_EXECUTE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
        if(image.value==INVALID_HANDLE_VALUE)win::fail();
        stage=LaunchStage::image_metadata;win::information(image.value,false,false);
        stage=LaunchStage::cwd_open;
        win::Handle directory(CreateFileW(cwd.c_str(),FILE_LIST_DIRECTORY,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
        if(directory.value==INVALID_HANDLE_VALUE)win::fail();
        stage=LaunchStage::cwd_metadata;win::information(directory.value,true);
        auto child=std::make_shared<Child>();
        STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(startup);startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
        std::vector<win::Handle> standard;std::vector<HANDLE> inherited;standard.reserve(3);inherited.reserve(3);
        stage=LaunchStage::stdio;
        unsigned index=0;
        for(DWORD selected:{STD_INPUT_HANDLE,STD_OUTPUT_HANDLE,STD_ERROR_HANDLE}) {
            win::Handle writer;
            if(capture && index && (*capture)[index-1]>=0)
                writer.value=capture_pipe(child->readers[index-1]).release();
            auto original=writer.value==INVALID_HANDLE_VALUE?GetStdHandle(selected):writer.value;
            HANDLE duplicate=INVALID_HANDLE_VALUE;
            if(original && original!=INVALID_HANDLE_VALUE) {
                if(!DuplicateHandle(GetCurrentProcess(),original,GetCurrentProcess(),&duplicate,0,TRUE,DUPLICATE_SAME_ACCESS))win::fail();
                standard.emplace_back(duplicate);inherited.push_back(duplicate);
            }
            if(selected==STD_INPUT_HANDLE)startup.StartupInfo.hStdInput=duplicate;
            else if(selected==STD_OUTPUT_HANDLE)startup.StartupInfo.hStdOutput=duplicate;
            else startup.StartupInfo.hStdError=duplicate;
            ++index;
        }
        Attributes attributes;DWORD flags=CREATE_UNICODE_ENVIRONMENT;
        if(!inherited.empty()) {
            stage=LaunchStage::attributes_init;
            SIZE_T count=0;InitializeProcThreadAttributeList(nullptr,1,0,&count);attributes.storage.resize(count);
            auto* list=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.storage.data());
            if(!InitializeProcThreadAttributeList(list,1,0,&count))win::fail();attributes.list=list;
            stage=LaunchStage::attributes_update;
            if(!UpdateProcThreadAttribute(list,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited.data(),inherited.size()*sizeof(HANDLE),nullptr,nullptr))win::fail();
            startup.lpAttributeList=list;flags|=EXTENDED_STARTUPINFO_PRESENT;
        } else startup.StartupInfo.cb=sizeof(STARTUPINFOW);
        PROCESS_INFORMATION info{};
        stage=LaunchStage::create_process;
        if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,!inherited.empty(),flags,environment.data(),cwd.c_str(),&startup.StartupInfo,&info))win::fail();
        win::Handle thread(info.hThread);child->process.value=info.hProcess;
        q.acquired=std::move(child);return {};
    } catch(const std::system_error& e) {
        auto result=native_error(e.code().value());
        if(q.operation==spawn)result.detail=static_cast<std::int64_t>(stage);
        return result;
    }
}
}
#endif
