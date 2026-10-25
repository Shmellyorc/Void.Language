#pragma once
#include "void/console.hpp"
#include <filesystem>
#include <fstream>
#include <cstring>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// Opt-in test diagnostics only. Never reads input, applies modes or sets code pages.
// Character payloads are losslessly recorded as UTF-8 hex, including NUL/control bytes.
class ConsoleWindowsObservation {
    std::ofstream log_;
    unsigned stop_=0;
    static void hex(std::ostream& out,std::string_view value) {
        for(unsigned char byte:value)out<<"0123456789abcdef"[byte>>4]<<"0123456789abcdef"[byte&15];
    }
    void finish(){log_<<"}\n";log_.flush();if(!log_)throw std::runtime_error("console observation write failed");}
public:
    ConsoleWindowsObservation(){
#ifdef _WIN32
        char path[32768];auto size=GetEnvironmentVariableA("VOID_CONSOLE_OBSERVATION",path,sizeof path);
        if(!size)return;
        if(size>=sizeof path || std::filesystem::exists(path))throw std::runtime_error("choose a fresh console observation path");
        log_.open(path,std::ios::binary);if(!log_)throw std::runtime_error("console observation open failed");
        char stop[32]{};GetEnvironmentVariableA("VOID_CONSOLE_CAPTURE_STOP",stop,sizeof stop);
        stop_=std::strcmp(stop,"up-release")==0?1:std::strcmp(stop,"unicode")==0?2:std::strcmp(stop,"eacute")==0?3:
            std::strcmp(stop,"A-release")==0?4:std::strcmp(stop,"f8-release")==0?5:std::strcmp(stop,"a-release")==0?6:0;
        state("before_runtime");
#endif
    }
    ~ConsoleWindowsObservation(){try{state("observation_end");}catch(...){}}
    bool capturing()const{return stop_!=0;}
    bool captured(const vd::console::KeyResult& key)const{
        return stop_==1?(key.key==vd::console::Key::up && key.key_down_known && !key.key_down):
            stop_==2?key.character=="🎮":stop_==3?key.character=="é":
            stop_==4?(key.character=="A" && key.key_down_known && !key.key_down):
            stop_==5?(key.key==vd::console::Key::f8 && key.key_down_known && !key.key_down):
            stop_==6?(key.character=="a" && key.key_down_known && !key.key_down):false;
    }
    void state(const char* label){
#ifdef _WIN32
        if(!log_.is_open())return;
        auto saved=GetLastError();
        log_<<"{\"type\":\"state\",\"label\":\""<<label<<"\",\"pid\":"<<GetCurrentProcessId()
            <<",\"input_cp\":"<<GetConsoleCP()<<",\"output_cp\":"<<GetConsoleOutputCP()<<",\"handles\":[";
        const DWORD names[]{STD_INPUT_HANDLE,STD_OUTPUT_HANDLE,STD_ERROR_HANDLE};
        for(unsigned n=0;n<3;++n){
            if(n)log_<<',';SetLastError(0);auto handle=GetStdHandle(names[n]);auto handle_error=GetLastError();
            DWORD mode=0;auto ok=GetConsoleMode(handle,&mode);auto error=ok?0:GetLastError();
            log_<<"{\"value\":"<<reinterpret_cast<std::uintptr_t>(handle)<<",\"get_handle_error\":"<<handle_error
                <<",\"mode_ok\":"<<(ok?"true":"false")<<",\"mode\":"<<mode<<",\"mode_error\":"<<error<<'}';
        }
        log_<<']';SetLastError(saved);finish();
#else
        (void)label;
#endif
    }
    void result(const char* label,const vd::console::Error& error,std::int64_t token=0){
        if(!log_.is_open())return;
        log_<<"{\"type\":\"result\",\"label\":\""<<label<<"\",\"kind\":"<<static_cast<unsigned>(error.kind)
            <<",\"os_code\":"<<error.os_code<<",\"cleanup_code\":"<<error.cleanup_code
            <<",\"partial_utf16_units\":"<<error.partial_utf16_units<<",\"token\":"<<token;finish();
    }
    void key(unsigned index,const vd::console::KeyResult& key){
        if(!log_.is_open())return;
        log_<<"{\"type\":\"key\",\"index\":"<<index<<",\"key\":"<<static_cast<unsigned>(key.key)
            <<",\"modifiers\":"<<key.modifiers<<",\"modifiers_known\":"<<(key.modifiers_known?"true":"false")
            <<",\"key_down\":"<<(key.key_down?"true":"false")<<",\"key_down_known\":"<<(key.key_down_known?"true":"false")
            <<",\"repeat\":"<<key.repeat<<",\"character_utf8_hex\":\"";hex(log_,key.character);
        log_<<"\",\"bytes_hex\":\"";for(auto byte:key.bytes)log_<<"0123456789abcdef"[byte>>4]<<"0123456789abcdef"[byte&15];
        log_<<"\",\"completed\":"<<key.transfer.completed<<",\"eof\":"<<(key.transfer.eof?"true":"false")
            <<",\"error_kind\":"<<static_cast<unsigned>(key.transfer.error.kind)<<",\"os_code\":"<<key.transfer.error.os_code
            <<",\"cleanup_code\":"<<key.transfer.error.cleanup_code<<",\"partial_utf16_units\":"<<key.transfer.error.partial_utf16_units;finish();
    }
};
