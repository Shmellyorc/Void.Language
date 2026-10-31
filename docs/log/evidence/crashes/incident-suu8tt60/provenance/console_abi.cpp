#include "void/runtime_abi.hpp"
#include "void/console.hpp"
#include "void/console_ops.hpp"
#include <bit>
#include <limits>
namespace {
using namespace vd::console;
std::int64_t checked(std::size_t n){if(n>static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()))throw std::length_error("Console progress exceeds i64");return static_cast<std::int64_t>(n);}
constexpr const char* kinds[]={"success","invalid_argument","invalid_encoding","unsupported","would_block","timed_out","closed","broken_pipe","interrupted","io_failure"};
std::string describe(const Transfer& t,std::int64_t role,const char* operation){
    return std::string("console ")+operation+" "+(role==0?"stdin":role==1?"stdout":"stderr")+": "+kinds[static_cast<unsigned>(t.error.kind)]+"; native="+std::to_string(t.error.os_code)+"; completed="+std::to_string(t.completed)+"; partial_utf16_units="+std::to_string(t.error.partial_utf16_units)+"; cleanup="+std::to_string(t.error.cleanup_code);
}
bool role_valid(std::int64_t role){return role>=0 && role<=2;}
void exception_message(std::exception_ptr cause,vd::RuntimeFailure* failure,bool cleanup=false) noexcept {
    try{std::rethrow_exception(cause);}
    catch(const std::bad_alloc&){failure->message=cleanup?"Console cleanup allocation failed":"Console allocation failed";}
    catch(const std::length_error&){failure->message=cleanup?"Console cleanup extent is unrepresentable":"Console extent is unrepresentable";}
    catch(...){failure->message=cleanup?"Invalid console cleanup operation":"Invalid console operation";}
}
void exceptional_cleanup(const ModeCleanupFailure& error,vd::RuntimeFailure* primary) noexcept {
    exception_message(error.primary,primary);
    if(error.cleanup)try{auto message=describe(Transfer{0,false,error.cleanup},0,"restore_mode");vd::RuntimeFailure value;value.message=message.c_str();vd::RuntimeFailure retained;vd::retain_native_failure(value,&retained);vd_cleanup_failure(primary,&retained);}
    catch(...){vd::RuntimeFailure value;value.message="Terminal mode restoration failed";vd_cleanup_failure(primary,&value);}
    if(error.secondary){vd::RuntimeFailure value;exception_message(error.secondary,&value,true);vd_cleanup_failure(primary,&value);}
}

}
extern "C" void vd_console_print(void* record,std::int64_t role,bool flush,vd::RuntimeFailure* failure) noexcept {
    try {
        auto& runtime=vd::Runtime::current();auto text=runtime.string_bytes(record);
        Bytes bytes(text.begin(),text.end()); // Own every byte before NativeSafe/blocking.
        Transfer result;
        {vd::Runtime::Native safe(runtime);if(role!=1 && role!=2)result.error={ErrorKind::invalid_argument};else {try{result=runtime.console_context()->write(static_cast<Stream>(role),std::move(bytes));}catch(const Closed&){result.error={ErrorKind::closed};}}}
        // Writes have no long-lived VOID buffering; completion already drains it.
        (void)flush;
        if(result.error){auto message=describe(result,role,"print");vd::RuntimeFailure error;error.message=message.c_str();vd::retain_native_failure(error,failure);}
    }catch(const std::bad_alloc&){failure->message="Console allocation failed";}
    catch(const std::length_error&){failure->message="Console extent is unrepresentable";}
    catch(const std::exception&){failure->message="Invalid console print operation";}
    catch(...){std::terminate();}
}
extern "C" void vd_console_v2(void** text_out,void** bytes_out,std::uint64_t* words,unsigned operation,std::int64_t role,void** data,std::int64_t count,std::int64_t timeout,vd::RuntimeFailure* failure) noexcept {
    try {
        auto& runtime=vd::Runtime::current();std::string native;
        if(operation==0 || operation==16)native=runtime.string_bytes(*data);
        else if(operation==1)native=runtime.byte_array_bytes(runtime.root_word(data));
        // Allocate managed reply storage before a successful mode change.
        if(operation==20){runtime.string_into(text_out,"");runtime.bytes_into(bytes_out,"");}
        Transfer transfer;std::string text;Bytes bytes;std::int64_t first=0,second=0;
        {
            vd::Runtime::Native safe(runtime);
            if(!role_valid(role) || operation>=source_operations.size() || (output_operation(operation)?role==0:input_operation(operation)?role!=0:false))transfer.error={ErrorKind::invalid_argument};
            else try {
                auto context=runtime.console_context();
                if(operation<=1)transfer=context->write(static_cast<Stream>(role),Bytes(native.begin(),native.end()));
                else if(operation==5){auto read=context->read_bytes(count,timeout);bytes=std::move(read.bytes);transfer=read.transfer;}
                else if(operation<=4){auto read=operation==2?context->read_char(timeout):operation==3?context->read_text(count,timeout):context->read_line(timeout);text=std::move(read.text);bytes=std::move(read.partial);transfer=read.transfer;}
                else if(operation==6)transfer.completed=context->endpoint(static_cast<Stream>(role)).redirected()?1:0;
                else if(operation==8)transfer.completed=context->capabilities(static_cast<Stream>(role));
                else if(operation==18){auto key=context->read_key(count!=0,timeout);transfer=key.transfer;text=std::move(key.character);bytes=std::move(key.bytes);first=static_cast<unsigned>(key.key);second=key.modifiers | (key.modifiers_known?8:0) | (key.key_down_known?16:0) | (key.key_down?32:0) | (static_cast<std::int64_t>(key.repeat)<<8);}
                else if(operation==19){auto available=context->key_available();transfer=available.transfer;first=available.first;}
                else if(operation==20){auto mode=context->begin_mode({(count&1)!=0,(count&2)!=0,(count&4)!=0});transfer.error=mode.error;first=mode.token;}
                else if(operation>=21){transfer.error=context->end_mode(count);if(operation==22 && transfer.error.kind==ErrorKind::closed)transfer.error={};}
                else if(operation>=9){auto result=context->present(static_cast<Stream>(role),static_cast<Presentation>(operation-9),count,timeout,native);transfer=result.transfer;first=result.first;second=result.second;}
                else { // Serialize a drain acknowledgement with prior whole records.
                    auto lease=context->acquire(static_cast<Stream>(role));transfer.completed=1;
                }
            }catch(const Closed&){transfer.error={ErrorKind::closed};}
        }
        if(operation==22 && transfer.error){auto message=describe(transfer,role,"restore_mode");vd::RuntimeFailure error;error.message=message.c_str();vd::retain_native_failure(error,failure);return;}
        words[0]=static_cast<unsigned>(transfer.error.kind);words[1]=std::bit_cast<std::uint64_t>(transfer.error.os_code);
        words[2]=static_cast<std::uint64_t>(checked(transfer.completed));words[3]=std::bit_cast<std::uint64_t>(transfer.error.cleanup_code);
        words[4]=transfer.error.partial_utf16_units;words[5]=transfer.eof;words[6]=std::bit_cast<std::uint64_t>(first);words[7]=std::bit_cast<std::uint64_t>(second);
        if(transfer.error)text=kinds[static_cast<unsigned>(transfer.error.kind)];
        if(operation==20 && !transfer.error)return;
        runtime.string_into(text_out,text);
        runtime.bytes_into(bytes_out,{reinterpret_cast<const char*>(bytes.data()),bytes.size()});
    }catch(const ModeCleanupFailure& error){exceptional_cleanup(error,failure);}
    catch(const std::bad_alloc&){failure->message="Console allocation failed";}
    catch(const std::length_error&){failure->message="Console extent is unrepresentable";}
    catch(const std::exception&){failure->message="Invalid console operation";}
    catch(...){std::terminate();}
}

namespace vd {
void finish_console(Runtime& runtime,std::int64_t checkpoint,RuntimeFailure* primary) noexcept {
    auto error=runtime.console_finish(checkpoint);if(!error)return;
    try{auto text=describe(Transfer{0,false,error},0,"restore_mode");RuntimeFailure value;value.message=text.c_str();RuntimeFailure retained;retain_native_failure(value,&retained);vd_cleanup_failure(primary,&retained);}
    catch(...){RuntimeFailure value;value.message="Terminal mode restoration failed";vd_cleanup_failure(primary,&value);}
}
}
