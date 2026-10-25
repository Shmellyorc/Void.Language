#include "console_windows_trace.hpp"
#ifdef _WIN32
#include <filesystem>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <cstdint>
#include <cstring>
#include <atomic>

namespace {
std::mutex gate;
HANDLE active_thread=nullptr,active_handle=nullptr;
OVERLAPPED* active_overlap=nullptr;
const void* active_buffer=nullptr;
std::atomic<unsigned> next_write_fault=0;
std::atomic<bool> next_read_fault=false,last_read_success=false;
std::atomic<DWORD> last_read_error=0,last_read_count=0;
std::atomic<std::uint64_t> written_units=0;
std::ofstream& output(){
    static std::ofstream stream=[](){
        std::ofstream result;char path[32768];auto n=GetEnvironmentVariableA("VOID_CONSOLE_API_TRACE",path,sizeof path);
        if(n && n<sizeof path){if(std::filesystem::exists(path))throw std::runtime_error("choose a fresh native API trace");result.open(path,std::ios::binary);if(!result)throw std::runtime_error("native API trace open failed");}
        return result;
    }();return stream;
}
void event(const char* api,HANDLE handle,BOOL ok,DWORD error,DWORD requested=0,DWORD completed=0,const WCHAR* units=nullptr,OVERLAPPED* overlap=nullptr){
    auto saved=GetLastError();std::lock_guard lock(gate);auto& out=output();
    if(out.is_open()){
        out<<"{\"api\":\""<<api<<"\",\"tick_ms\":"<<GetTickCount64()<<",\"tid\":"<<GetCurrentThreadId()<<",\"handle\":"<<reinterpret_cast<std::uintptr_t>(handle)
           <<",\"success\":"<<(ok?"true":"false")<<",\"error\":"<<error<<",\"requested\":"<<requested<<",\"completed\":"<<completed
           <<",\"overlapped\":"<<reinterpret_cast<std::uintptr_t>(overlap)<<",\"utf16_units\":[";
        if(units)for(DWORD n=0;n<completed;++n){if(n)out<<',';out<<static_cast<unsigned>(units[n]);}
        out<<"]}\n";out.flush();
    }
    SetLastError(saved);
}
void active(HANDLE handle,OVERLAPPED* overlap,const void* buffer=nullptr){
    auto saved=GetLastError();std::lock_guard lock(gate);
    if(active_thread)CloseHandle(active_thread);
    active_thread=OpenThread(THREAD_QUERY_INFORMATION,FALSE,GetCurrentThreadId());active_handle=handle;active_overlap=overlap;active_buffer=buffer;
    SetLastError(saved);
}
void inactive(){auto saved=GetLastError();std::lock_guard lock(gate);if(active_thread)CloseHandle(active_thread);active_thread=nullptr;active_handle=nullptr;active_overlap=nullptr;active_buffer=nullptr;SetLastError(saved);}
void key_event(const char* api,HANDLE handle,const INPUT_RECORD* records,DWORD count){
    auto saved=GetLastError();std::lock_guard lock(gate);auto& out=output();
    if(out.is_open()){
    for(DWORD n=0;n<count;++n)if(records[n].EventType==KEY_EVENT){auto& k=records[n].Event.KeyEvent;
        out<<"{\"api\":\""<<api<<".KEY_EVENT\",\"tid\":"<<GetCurrentThreadId()<<",\"handle\":"<<reinterpret_cast<std::uintptr_t>(handle)
           <<",\"down\":"<<(k.bKeyDown?"true":"false")<<",\"repeat\":"<<k.wRepeatCount<<",\"vk\":"<<k.wVirtualKeyCode
           <<",\"scan\":"<<k.wVirtualScanCode<<",\"unicode_unit\":"<<static_cast<unsigned>(k.uChar.UnicodeChar)<<",\"control_state\":"<<k.dwControlKeyState<<"}\n";
    }
    out.flush();
    }SetLastError(saved);
}
}
bool vd_console_native_pending(){
    auto saved=GetLastError();std::lock_guard lock(gate);bool pending=false;
    if(active_overlap){DWORD count=0;pending=!GetOverlappedResult(active_handle,active_overlap,&count,FALSE) && GetLastError()==ERROR_IO_INCOMPLETE;}
    else if(active_thread){BOOL value=FALSE;pending=GetThreadIOPendingFlag(active_thread,&value) && value;}
    SetLastError(saved);return pending;
}
BOOL WINAPI vd_observe_ReadConsoleW(HANDLE h,LPVOID b,DWORD n,LPDWORD count,PCONSOLE_READCONSOLE_CONTROL control){
    active(h,nullptr,b);event("ReadConsoleW.enter",h,TRUE,0,n);
    if(next_read_fault.exchange(false)){CloseHandle(h);event("TEST.host_close_before_ReadConsoleW",h,TRUE,0,n);}
    auto ok=ReadConsoleW(h,b,n,count,control);auto saved=GetLastError();last_read_success=ok!=FALSE;last_read_error=ok?0:saved;last_read_count=count?*count:0;event("ReadConsoleW",h,ok,ok?0:saved,n,count?*count:0,static_cast<WCHAR*>(b));inactive();SetLastError(saved);return ok;
}
void vd_console_next_read_fault(){next_read_fault=true;}
void vd_console_last_wide_read(bool& ok,DWORD& error,DWORD& count){ok=last_read_success;error=last_read_error;count=last_read_count;}
std::uintptr_t vd_console_pending_buffer(){std::lock_guard lock(gate);return reinterpret_cast<std::uintptr_t>(active_buffer);}
void vd_console_next_write_fault(unsigned value){next_write_fault=value;}
std::uint64_t vd_console_written_units(){return written_units.load();}
BOOL WINAPI vd_observe_WriteConsoleW(HANDLE h,const VOID* b,DWORD n,LPDWORD count,LPVOID reserved){
    auto fault=next_write_fault.exchange(0);DWORD submitted=fault==2?1:n;
    if(fault==1){CloseHandle(h);event("TEST.host_close_before_WriteConsoleW",h,TRUE,0,n);}
    if(fault==2)event("TEST.clamp_WriteConsoleW_request",h,TRUE,0,n,submitted);
    auto ok=WriteConsoleW(h,b,submitted,count,reserved);auto saved=GetLastError();event("WriteConsoleW",h,ok,ok?0:saved,submitted,count?*count:0,static_cast<const WCHAR*>(b));if(count)written_units+=*count;SetLastError(saved);return ok;
}
BOOL WINAPI vd_observe_ReadConsoleInputW(HANDLE h,PINPUT_RECORD b,DWORD n,LPDWORD count){
    auto ok=ReadConsoleInputW(h,b,n,count);auto saved=GetLastError();event("ReadConsoleInputW",h,ok,ok?0:saved,n,count?*count:0);if(ok && count)key_event("ReadConsoleInputW",h,b,*count);SetLastError(saved);return ok;
}
BOOL WINAPI vd_observe_PeekConsoleInputW(HANDLE h,PINPUT_RECORD b,DWORD n,LPDWORD count){
    auto ok=PeekConsoleInputW(h,b,n,count);auto saved=GetLastError();event("PeekConsoleInputW",h,ok,ok?0:saved,n,count?*count:0);if(ok && count)key_event("PeekConsoleInputW",h,b,*count);SetLastError(saved);return ok;
}
BOOL WINAPI vd_observe_SetConsoleMode(HANDLE h,DWORD mode){auto ok=SetConsoleMode(h,mode);auto saved=GetLastError();event("SetConsoleMode",h,ok,ok?0:saved,mode);SetLastError(saved);return ok;}
BOOL WINAPI vd_observe_ReadFile(HANDLE h,LPVOID b,DWORD n,LPDWORD count,LPOVERLAPPED ov){
    active(h,ov,b);event("ReadFile.enter",h,TRUE,0,n,0,nullptr,ov);auto ok=ReadFile(h,b,n,count,ov);auto saved=GetLastError();event("ReadFile",h,ok,ok?0:saved,n,count?*count:0,nullptr,ov);if(!ov || (!ok && saved!=ERROR_IO_PENDING && saved!=ERROR_MORE_DATA))inactive();SetLastError(saved);return ok;
}
BOOL WINAPI vd_observe_WriteFile(HANDLE h,LPCVOID b,DWORD n,LPDWORD count,LPOVERLAPPED ov){
    active(h,ov,b);event("WriteFile.enter",h,TRUE,0,n,0,nullptr,ov);auto ok=WriteFile(h,b,n,count,ov);auto saved=GetLastError();event("WriteFile",h,ok,ok?0:saved,n,count?*count:0,nullptr,ov);if(!ov || (!ok && saved!=ERROR_IO_PENDING && saved!=ERROR_MORE_DATA))inactive();SetLastError(saved);return ok;
}
BOOL WINAPI vd_observe_GetOverlappedResult(HANDLE h,LPOVERLAPPED ov,LPDWORD count,BOOL wait){auto ok=GetOverlappedResult(h,ov,count,wait);auto saved=GetLastError();event("GetOverlappedResult",h,ok,ok?0:saved,0,count?*count:0,nullptr,ov);inactive();SetLastError(saved);return ok;}
BOOL WINAPI vd_observe_CancelIoEx(HANDLE h,LPOVERLAPPED ov){auto ok=CancelIoEx(h,ov);auto saved=GetLastError();event("CancelIoEx",h,ok,ok?0:saved,0,0,nullptr,ov);SetLastError(saved);return ok;}
BOOL WINAPI vd_observe_CancelSynchronousIo(HANDLE h){auto ok=CancelSynchronousIo(h);auto saved=GetLastError();event("CancelSynchronousIo",h,ok,ok?0:saved);SetLastError(saved);return ok;}
BOOL WINAPI vd_observe_CloseHandle(HANDLE h){auto ok=CloseHandle(h);auto saved=GetLastError();event("CloseHandle",h,ok,ok?0:saved);SetLastError(saved);return ok;}
namespace {
NTSTATUS NTAPI observe_query(HANDLE handle,PIO_STATUS_BLOCK status,PVOID data,ULONG size,FILE_INFORMATION_CLASS kind){
    using Query=NTSTATUS(NTAPI*)(HANDLE,PIO_STATUS_BLOCK,PVOID,ULONG,FILE_INFORMATION_CLASS);
    auto real=reinterpret_cast<Query>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtQueryInformationFile"));
    active(handle,nullptr);event("NtQueryInformationFile.enter",handle,TRUE,0,static_cast<DWORD>(kind));
    auto result=real(handle,status,data,size,kind);auto saved=GetLastError();
    event("NtQueryInformationFile",handle,result>=0,static_cast<DWORD>(result),static_cast<DWORD>(kind),static_cast<DWORD>(status->Information));
    inactive();SetLastError(saved);return result;
}
}
FARPROC WINAPI vd_observe_GetProcAddress(HMODULE module,LPCSTR name){
    auto address=GetProcAddress(module,name);auto saved=GetLastError();
    if(address && reinterpret_cast<std::uintptr_t>(name)>65535 && std::strcmp(name,"NtQueryInformationFile")==0)address=reinterpret_cast<FARPROC>(&observe_query);
    SetLastError(saved);return address;
}
#endif
