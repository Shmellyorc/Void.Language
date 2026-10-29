#include "console_windows_native.hpp"
#ifdef _WIN32
#include "console_windows_trace.hpp"
#include "console_windows_observation.hpp"
#include "void/runtime.hpp"
#include <atomic>
#include <future>
#include <iostream>
#include <semaphore>
#include <string>
#include <vector>
using namespace vd::console;
using namespace std::chrono_literals;
namespace {
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<class F>void until(F f){auto end=std::chrono::steady_clock::now()+10s;while(!f()){require(std::chrono::steady_clock::now()<end,"native Windows rendezvous deadline");std::this_thread::yield();}}
struct Handle {HANDLE value=nullptr;~Handle(){if(value && value!=INVALID_HANDLE_VALUE)CloseHandle(value);}Handle()=default;explicit Handle(HANDLE h):value(h){};Handle(const Handle&)=delete;};
struct Standard {DWORD role;HANDLE original;Standard(DWORD r,HANDLE h):role(r),original(GetStdHandle(r)){require(SetStdHandle(r,h),"install native borrowed endpoint");}~Standard(){SetStdHandle(role,original);}};
struct Pipe {
    Handle borrowed,peer;
    Pipe(bool overlapped,bool writing,bool message=false){
        if(!overlapped && !writing && !message){require(CreatePipe(&borrowed.value,&peer.value,nullptr,4096),"native synchronous pipe creation");return;}
        auto name=L"\\\\.\\pipe\\void-console-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(reinterpret_cast<std::uintptr_t>(this));
        borrowed.value=CreateNamedPipeW(name.c_str(),(writing?PIPE_ACCESS_OUTBOUND:PIPE_ACCESS_INBOUND)|(overlapped?FILE_FLAG_OVERLAPPED:0),(message?PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE:PIPE_TYPE_BYTE|PIPE_READMODE_BYTE)|PIPE_WAIT,1,4096,4096,0,nullptr);
        require(borrowed.value!=INVALID_HANDLE_VALUE,"native named pipe creation");
        peer.value=CreateFileW(name.c_str(),writing?GENERIC_READ:GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);
        require(peer.value!=INVALID_HANDLE_VALUE,"native named pipe client");
        // The client connection can win before ConnectNamedPipe is issued.
        if(overlapped){Handle event(CreateEventW(nullptr,TRUE,FALSE,nullptr));OVERLAPPED ov{};ov.hEvent=event.value;auto ok=ConnectNamedPipe(borrowed.value,&ov);auto error=ok?0:GetLastError();require(ok || error==ERROR_PIPE_CONNECTED,"native overlapped pipe connection");}
        else require(ConnectNamedPipe(borrowed.value,nullptr) || GetLastError()==ERROR_PIPE_CONNECTED,"native pipe connection");
    }
};
void modes(const char* path){
    ConsoleWindowsObservation observation;std::ofstream report(path);require(bool(report),"mode report open");
    DWORD baseline=0;require(GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE),&baseline),"native mode baseline");
    auto input_cp=GetConsoleCP(),output_cp=GetConsoleOutputCP();
    {vd::Runtime runtime;auto c=runtime.console_context();
    for(unsigned bits=0;bits<8;++bits){
        ModeOptions options{bool(bits&1),bool(bits&2),bool(bits&4)};auto result=c->begin_mode(options);
        observation.result("matrix_begin",result.error,result.token);observation.state("matrix_applied");
        DWORD actual=0;require(GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE),&actual),"matrix mode readback");
        report<<bits<<' '<<static_cast<unsigned>(result.error.kind)<<' '<<result.error.os_code<<' '<<actual<<'\n';report.flush();
        if(!result.error){auto expected=baseline & ~(ENABLE_ECHO_INPUT|ENABLE_LINE_INPUT|ENABLE_PROCESSED_INPUT);expected|=(options.echo?ENABLE_ECHO_INPUT:0)|(options.line_input?ENABLE_LINE_INPUT:0)|(options.control_c_as_input?0:ENABLE_PROCESSED_INPUT);require(actual==expected,"native applied mode mask differs");auto error=c->end_mode(result.token);observation.result("matrix_end",error,result.token);require(!error,"matrix restoration error");}
        else require(actual==baseline,"failed native apply did not restore exact baseline");
        require(GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE),&actual) && actual==baseline,"matrix exact restoration");observation.state("matrix_restored");
        require(GetConsoleCP()==input_cp && GetConsoleOutputCP()==output_cp,"matrix changed native code page");
    }
    auto error=runtime.close();observation.result("runtime_close",error);require(!error,"matrix Runtime close");}
    observation.state("after_runtime_destructor");report<<"PASS\n";
}
void queue(std::u16string_view text){
    std::vector<INPUT_RECORD> records;
    for(auto unit:text){INPUT_RECORD record{};record.EventType=KEY_EVENT;record.Event.KeyEvent.bKeyDown=TRUE;record.Event.KeyEvent.wRepeatCount=1;record.Event.KeyEvent.wVirtualKeyCode=VK_PACKET;record.Event.KeyEvent.uChar.UnicodeChar=unit;records.push_back(record);}
    DWORD count=0;require(WriteConsoleInputW(GetStdHandle(STD_INPUT_HANDLE),records.data(),static_cast<DWORD>(records.size()),&count) && count==records.size(),"explicit scripted Unicode queue supply");
}
void wide(const char* path){
    ConsoleWindowsObservation observation;std::ofstream report(path);require(bool(report),"wide report open");
    vd::Runtime runtime;auto c=runtime.console_context();auto mode=c->begin_mode({false,false,true});require(!mode.error,"wide raw console mode");
    observation.state("wide_raw");
    // Native input records are not POSIX byte streams: this host discards LF
    // VK_PACKET records. Use real CR key units, and retain the original failed
    // CRLF attempt/API trace rather than assuming an LF reached the Runtime.
    queue(u"Aé🎮\ufeff\rZ\r");
    auto a=c->read_char(2000);require(!a.transfer.error && a.text=="A","native wide ASCII scalar");
    auto first=c->read_bytes(1,2000);require(first.bytes==Bytes{0xc3},"native wide UTF8 prefix");
    auto tail=c->read_char(2000);require(tail.transfer.error.kind==ErrorKind::invalid_encoding && tail.partial==Bytes{0xa9},"native raw/text carry error");
    auto emoji=c->read_char(2000);require(!emoji.transfer.error && emoji.text=="🎮" && emoji.transfer.completed==4,"native supplementary scalar and byte count");
    auto bom=c->read_char(2000);require(!bom.transfer.error && bom.text=="\xef\xbb\xbf","native BOM preserved as data");
    auto cr=c->read_line(2000);require(!cr.transfer.error && cr.text.empty() && cr.transfer.completed==1,"native CR completes without LF read-ahead");
    auto line=c->read_line(2000);require(!line.transfer.error && line.text=="Z" && line.transfer.completed==2,"native CR line ownership");
    queue(u"\xd800X\xdc00Y");
    auto high=c->read_char(2000);require(high.transfer.error.kind==ErrorKind::invalid_encoding && high.transfer.error.os_code==1113,"native malformed high surrogate error");
    auto replay=c->read_char(2000);require(!replay.transfer.error && replay.text=="X","native high-surrogate replay");
    auto low=c->read_char(2000);require(low.transfer.error.kind==ErrorKind::invalid_encoding && low.transfer.error.os_code==1113,"native isolated low surrogate error");
    require(c->read_char(2000).text=="Y","native low-surrogate recovery");
    const std::string output("Aé🎮\0\xef\xbb\xbf\n",12);auto result=c->write(Stream::output,Bytes(output.begin(),output.end()));
    require(!result.error && result.completed==output.size(),"native Unicode output progress including NUL/BOM");
    observation.result("wide_write",result.error);require(!c->end_mode(mode.token),"native wide mode restore");observation.state("wide_restored");
    require(!runtime.close(),"wide Runtime close");report<<"PASS actual ReadConsoleW/WriteConsoleW; scripted native text records, not physical input\n";
}
void cancel(const char* path,bool overlapped,bool writing){
    std::ofstream report(path);require(bool(report),"native cancellation report open");
    Pipe pipe(overlapped,writing);Standard standard(writing?STD_OUTPUT_HANDLE:STD_INPUT_HANDLE,pipe.borrowed.value);
    vd::Runtime runtime(0);auto c=runtime.console_context();SetStdHandle(standard.role,standard.original);
    Handle foreign_event(CreateEventW(nullptr,TRUE,FALSE,nullptr));OVERLAPPED foreign{};foreign.hEvent=foreign_event.value;
    std::atomic<DWORD> foreign_tid=0;std::atomic<bool> foreign_done=false;DWORD foreign_count=0;char foreign_byte=0;
    std::future<ReadResult> call;
    std::binary_semaphore foreign_start(overlapped?1:0);
    auto foreign_read=std::async(std::launch::async,[&]{foreign_start.acquire();foreign_tid=GetCurrentThreadId();BOOL ok;
        if(overlapped){ok=ReadFile(pipe.borrowed.value,&foreign_byte,1,nullptr,&foreign);auto error=ok?0:GetLastError();require(ok || error==ERROR_IO_PENDING,"foreign native pending read");ok=GetOverlappedResult(pipe.borrowed.value,&foreign,&foreign_count,TRUE);}
        else ok=ReadFile(pipe.borrowed.value,&foreign_byte,1,&foreign_count,nullptr);
        require(ok,"unrelated foreign request was cancelled");foreign_done=true;
    });
    struct FailureCleanup {
        vd::Runtime& runtime;HANDLE handle;OVERLAPPED* overlap;std::atomic<DWORD>& tid;std::binary_semaphore& start;bool finished=false;
        ~FailureCleanup(){if(finished)return;if(!tid)start.release();if(overlap)CancelIoEx(handle,overlap);else {until([&]{return tid.load()!=0;});Handle thread(OpenThread(THREAD_TERMINATE,FALSE,tid));if(thread.value)CancelSynchronousIo(thread.value);}runtime.close();}
    } failure_cleanup{runtime,pipe.borrowed.value,overlapped?&foreign:nullptr,foreign_tid,foreign_start};
    // Read isolation uses the SAME borrowed handle and a distinct request/thread.
    // Write scenarios are dispatched separately below, never mislabeled reads.
    require(!writing,"write cancellation probe requires its own full-pipe protocol");
    if(overlapped)until([&]{DWORD n=0;return !GetOverlappedResult(pipe.borrowed.value,&foreign,&n,FALSE) && GetLastError()==ERROR_IO_INCOMPLETE;});
    std::atomic<bool> rooted=false;
    call=std::async(std::launch::async,[&]{vd::Runtime::Attachment attachment(runtime);vd::Runtime::Managed managed(runtime);void* root=nullptr;std::size_t offset=0;vd::Runtime::Slot slot{&root,sizeof(root),&offset,1};vd::Runtime::Frame frame(runtime,&slot,1);runtime.allocate_into(&root,{64,{}});ReadResult result;
        {vd::Runtime::Native safe(runtime);result=c->read(16);}
        rooted=runtime.contains(root);return result;
    });
    until([]{return vd_console_native_pending();});
    auto retained_buffer=vd_console_pending_buffer();require(retained_buffer!=0,"native pending read buffer missing");
    // Synchronous file-object operations serialize. Start the foreign call only
    // after the Runtime's ReadFile is genuinely pending; starting it first tests
    // an externally blocked metadata query, not entered stream cancellation.
    if(!overlapped){foreign_start.release();until([&]{return foreign_tid.load()!=0;});}
    require(c->snapshot().admitted==1,"real pending request lost admission lease");
    {vd::Runtime::Attachment attachment(runtime);vd::Runtime::Managed managed(runtime);require(runtime.collect(5s)==vd::Runtime::Collection::completed && runtime.snapshot().objects==1,"real pending native request lost GC root");}
    require(vd_console_native_pending(),"request completed before pending GC observation");
    require(vd_console_pending_buffer()==retained_buffer,"pending native read changed buffer through GC");
    auto closed=runtime.close();auto result=call.get();require(!closed && rooted,"native cancellation lost close result/root");
    require(result.transfer.error.kind==ErrorKind::closed && result.transfer.error.os_code==ERROR_OPERATION_ABORTED && result.bytes.empty(),"native cancelled read result/error/progress");
    require(!foreign_done,"cancellation completed foreign request");
    DWORD valid=0;require(GetHandleInformation(pipe.borrowed.value,&valid),"Runtime closed borrowed native handle");
    DWORD wrote=0;require(WriteFile(pipe.peer.value,"F",1,&wrote,nullptr) && wrote==1,"foreign completion supply");foreign_read.get();
    require(foreign_done && foreign_count==1 && foreign_byte=='F',"foreign read completion changed");
    require(WriteFile(pipe.peer.value,"P",1,&wrote,nullptr),"post-close borrowed handle supply");
    char post=0;DWORD got=0;OVERLAPPED after{};after.hEvent=foreign_event.value;ResetEvent(foreign_event.value);
    auto ok=ReadFile(pipe.borrowed.value,&post,1,&got,overlapped?&after:nullptr);if(overlapped && !ok && GetLastError()==ERROR_IO_PENDING)ok=GetOverlappedResult(pipe.borrowed.value,&after,&got,TRUE);
    require(ok && got==1 && post=='P',"borrowed handle unusable after Runtime close");
    require(c->snapshot().admitted==0,"native cancellation lease not drained");
    failure_cleanup.finished=true;
    report<<"PASS "<<(overlapped?"overlapped":"synchronous")<<" read; actual pending OS request, GC root, native error 995, same-handle foreign isolation and post-close reuse\n";
}
void cancel_write(const char* path,bool overlapped){
    std::ofstream report(path);require(bool(report),"native write report open");
    Pipe pipe(overlapped,true);Standard standard(STD_OUTPUT_HANDLE,pipe.borrowed.value);
    vd::Runtime runtime(0);auto c=runtime.console_context();SetStdHandle(standard.role,standard.original);
    Handle event(CreateEventW(nullptr,TRUE,FALSE,nullptr));OVERLAPPED fill{};fill.hEvent=event.value;Bytes prefix(4096,'F');DWORD count=0;
    auto filled=WriteFile(pipe.borrowed.value,prefix.data(),static_cast<DWORD>(prefix.size()),&count,overlapped?&fill:nullptr);
    if(overlapped && !filled && GetLastError()==ERROR_IO_PENDING)filled=GetOverlappedResult(pipe.borrowed.value,&fill,&count,TRUE);
    require(filled && count==prefix.size(),"native pipe fill");
    std::atomic<bool> rooted=false;
    auto call=std::async(std::launch::async,[&]{vd::Runtime::Attachment attachment(runtime);vd::Runtime::Managed managed(runtime);void* root=nullptr;std::size_t offset=0;vd::Runtime::Slot slot{&root,sizeof(root),&offset,1};vd::Runtime::Frame frame(runtime,&slot,1);runtime.allocate_into(&root,{64,{}});Transfer result;
        {vd::Runtime::Native safe(runtime);result=c->write(Stream::output,Bytes(65536,'W'));}rooted=runtime.contains(root);return result;
    });
    struct Drain {vd::Runtime& runtime;bool finished=false;~Drain(){if(!finished)runtime.close();}} drain{runtime};
    until([]{return vd_console_native_pending();});auto buffer=vd_console_pending_buffer();require(buffer!=0,"native write buffer missing");
    {vd::Runtime::Attachment attachment(runtime);vd::Runtime::Managed managed(runtime);require(runtime.collect(5s)==vd::Runtime::Collection::completed && runtime.snapshot().objects==1,"native pending write GC root");}
    require(vd_console_native_pending() && vd_console_pending_buffer()==buffer && c->snapshot().admitted==1,"pending write buffer/lease changed through GC");
    auto close=runtime.close();auto result=call.get();drain.finished=true;
    require(!close && rooted && result.error.kind==ErrorKind::closed && result.error.os_code==995 && result.completed<65536,"native write cancellation result");
    DWORD flags=0;require(GetHandleInformation(pipe.borrowed.value,&flags),"Runtime closed borrowed write handle");
    Bytes actual(4096);require(ReadFile(pipe.peer.value,actual.data(),static_cast<DWORD>(actual.size()),&count,nullptr) && count==4096 && actual==prefix,"native cancellation damaged prior output");
    DWORD remaining=0;require(PeekNamedPipe(pipe.peer.value,nullptr,0,nullptr,&remaining,nullptr),"post-cancel native pipe state");
    while(remaining){Bytes tail(remaining);require(ReadFile(pipe.peer.value,tail.data(),remaining,&count,nullptr),"drain actual cancelled-write prefix");require(PeekNamedPipe(pipe.peer.value,nullptr,0,nullptr,&remaining,nullptr),"drain native pipe state");}
    OVERLAPPED post{};post.hEvent=event.value;ResetEvent(event.value);auto ok=WriteFile(pipe.borrowed.value,"P",1,&count,overlapped?&post:nullptr);
    if(overlapped && !ok && GetLastError()==ERROR_IO_PENDING)ok=GetOverlappedResult(pipe.borrowed.value,&post,&count,TRUE);
    require(ok && count==1,"borrowed write handle unusable after close");char received=0;require(ReadFile(pipe.peer.value,&received,1,&count,nullptr) && count==1 && received=='P',"post-close native output mismatch");
    require(c->snapshot().admitted==0,"native write lease not drained");report<<"PASS "<<(overlapped?"overlapped":"synchronous")<<" write; native pending, GC root, stable buffer, error "<<result.error.os_code<<", completed "<<result.completed<<", borrowed handle reuse\n";
}
void cleanup_failure(const char* path){
    std::ofstream report(path);require(bool(report),"cleanup failure report open");
    auto original=GetStdHandle(STD_INPUT_HANDLE);DWORD baseline=0;require(GetConsoleMode(original,&baseline),"cleanup original input");
    Handle input(CreateFileW(L"CONIN$",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr));require(input.value!=INVALID_HANDLE_VALUE,"cleanup private console handle");
    Standard standard(STD_INPUT_HANDLE,input.value);ConsoleWindowsObservation observation;
    {vd::Runtime runtime;auto c=runtime.console_context();auto mode=c->begin_mode({false,false,true});require(!mode.error,"cleanup mode apply");observation.state("cleanup_applied");
    require(CloseHandle(input.value),"host closes its private borrowed handle");input.value=nullptr;
    auto primary=c->read_bytes(1,0);observation.result("host_invalidated_read",primary.transfer.error);require(primary.transfer.error.kind==ErrorKind::closed && primary.transfer.error.os_code==6,"closed native input error lost");
    auto end=c->end_mode(mode.token);observation.result("failed_end_mode",end);observation.state("failed_end_mode");require(end.kind==ErrorKind::closed && end.os_code==6,"native cleanup failure lost");
    auto close=runtime.close();observation.result("failed_runtime_close",close);require(close.kind==ErrorKind::closed && close.os_code==6,"Runtime suppressed native restore error");}
    // Explicit host recovery is separate from, and must not be counted as, a
    // successful Runtime restoration of a handle the embedding host invalidated.
    SetStdHandle(STD_INPUT_HANDLE,original);require(SetConsoleMode(original,baseline),"test host restores its original console state");observation.state("host_recovery_after_reported_failure");
    report<<"PASS expected native read/end_mode/Runtime-close errors 6 after host invalidated its private handle; final recovery performed by host, not Runtime\n";
}
void nested_shutdown(const char* path){
    ConsoleWindowsObservation observation;std::ofstream report(path);require(bool(report),"nested shutdown report");
    auto input=GetStdHandle(STD_INPUT_HANDLE);DWORD before=0;require(GetConsoleMode(input,&before),"nested shutdown baseline");auto input_cp=GetConsoleCP(),output_cp=GetConsoleOutputCP();
    {vd::Runtime runtime;auto c=runtime.console_context();auto outer=c->begin_mode({false,false,true});observation.result("begin_outer",outer.error,outer.token);require(!outer.error,"nested shutdown outer");observation.state("shutdown_outer_active");
    auto inner=c->begin_mode({false,true,false});observation.result("begin_inner",inner.error,inner.token);require(!inner.error,"nested shutdown inner");observation.state("shutdown_inner_active");
    auto reject=c->end_mode(outer.token);observation.result("reject_non_lifo",reject);require(reject.kind==ErrorKind::would_block,"nested shutdown non-LIFO");observation.state("shutdown_non_lifo_rejected");
    auto error=runtime.close();observation.result("runtime_close_both_unclosed",error);require(!error,"nested unclosed Runtime restoration error");observation.state("after_nested_runtime_close");}
    DWORD after=0;require(GetConsoleMode(input,&after) && before==after && GetConsoleCP()==input_cp && GetConsoleOutputCP()==output_cp,"nested shutdown native state mismatch");observation.state("after_runtime_destructor");report<<"PASS both nested sessions left unclosed; Runtime restored exact native input mask and code pages\n";
}
void wide_fault(const char* path,bool partial){
    ConsoleWindowsObservation observation;std::ofstream report(path);require(bool(report),"native wide fault report");
    Handle output(CreateFileW(L"CONOUT$",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr));require(output.value!=INVALID_HANDLE_VALUE,"private native output handle");
    Standard standard(STD_OUTPUT_HANDLE,output.value);vd::Runtime runtime;auto c=runtime.console_context();SetStdHandle(STD_OUTPUT_HANDLE,standard.original);
    vd_console_next_write_fault(partial?2:1);auto result=c->write(Stream::output,Bytes{0xf0,0x9f,0x8e,0xae});
    observation.result(partial?"explicit_native_request_clamp":"host_closed_output_at_native_boundary",result.error);
    require(result.completed==0,"native fault invented completed UTF8 scalar bytes");
    if(partial)require(result.error.kind==ErrorKind::io_failure && result.error.os_code==29 && result.error.partial_utf16_units==1,"native accepted high surrogate lost partial-unit diagnostic");
    else {output.value=nullptr;require(result.error.kind==ErrorKind::closed && result.error.os_code==6 && result.error.partial_utf16_units==0,"genuine WriteConsoleW invalid-handle error changed");}
    require(!runtime.close(),"wide fault Runtime drain");
    report<<(partial?"PASS explicitly clamped native request: actual WriteConsoleW requested/accepted one high-surrogate unit; original UTF8 scalar incomplete, zero UTF8 bytes, partial_utf16_units=1, error 29. This is NOT a naturally short OS return.\n":"PASS host invalidated its private output handle at native boundary; actual WriteConsoleW failed with error 6 and zero progress.\n");
}
void partial_pipe(const char* path,bool overlapped){
    std::ofstream report(path);require(bool(report),"native partial pipe report");Pipe pipe(overlapped,false,true);Standard standard(STD_INPUT_HANDLE,pipe.borrowed.value);
    vd::Runtime runtime;auto c=runtime.console_context();SetStdHandle(STD_INPUT_HANDLE,standard.original);
    std::atomic<DWORD> writer_tid=0;auto writer=std::async(std::launch::async,[&]{writer_tid=GetCurrentThreadId();Bytes message(8192,'M');DWORD count=0;require(WriteFile(pipe.peer.value,message.data(),static_cast<DWORD>(message.size()),&count,nullptr) && count==message.size(),"native message write");});
    struct Cleanup {std::atomic<DWORD>& tid;bool finished=false;~Cleanup(){if(!finished){until([&]{return tid.load()!=0;});Handle thread(OpenThread(THREAD_TERMINATE,FALSE,tid));if(thread.value)CancelSynchronousIo(thread.value);}}}cleanup{writer_tid};
    auto first=c->read_bytes(4096,2000);
    report<<"first completed "<<first.transfer.completed<<" kind "<<static_cast<unsigned>(first.transfer.error.kind)<<" native "<<first.transfer.error.os_code<<'\n';report.flush();
    require(first.bytes==Bytes(4096,'M') && first.transfer.completed==4096 && first.transfer.error.os_code==ERROR_MORE_DATA,"native message partial/error progress lost");
    auto second=c->read_bytes(4096,2000);require(second.bytes==Bytes(4096,'M') && second.transfer.completed==4096 && !second.transfer.error,"native message partial recovery");writer.get();cleanup.finished=true;
    require(!runtime.close(),"native partial pipe Runtime close");report<<"PASS actual message pipe returned ERROR_MORE_DATA with 4096 bytes; remaining 4096 recovered\n";
}
void wide_progress(const char* path){
    ConsoleWindowsObservation observation;std::ofstream report(path);require(bool(report),"wide native progress report");
    vd::Runtime runtime(0);auto c=runtime.console_context();std::atomic<bool> rooted=false;
    auto call=std::async(std::launch::async,[&]{vd::Runtime::Attachment attachment(runtime);vd::Runtime::Managed managed(runtime);void* root=nullptr;std::size_t offset=0;vd::Runtime::Slot slot{&root,sizeof(root),&offset,1};vd::Runtime::Frame frame(runtime,&slot,1);runtime.allocate_into(&root,{64,{}});Transfer result;
        Bytes source;source.reserve(4*1024*1024);for(unsigned n=0;n<1024*1024;++n)source.insert(source.end(),{0xf0,0x9f,0x8e,0xae});
        {vd::Runtime::Native safe(runtime);result=c->write(Stream::output,source);}rooted=runtime.contains(root);return result;
    });
    struct Drain {vd::Runtime& runtime;~Drain(){runtime.close();}}drain{runtime};
    until([]{return vd_console_written_units()>=2048;});
    {vd::Runtime::Attachment attachment(runtime);vd::Runtime::Managed managed(runtime);require(runtime.collect(5s)==vd::Runtime::Collection::completed && runtime.snapshot().objects==1,"native wide output lost pending GC root");}
    c->begin_close();auto close=runtime.close();auto result=call.get();auto units=vd_console_written_units();
    observation.result("native_wide_partial_record",result.error);
    report<<"native_utf16_units "<<units<<" utf8_completed "<<result.completed<<" kind "<<static_cast<unsigned>(result.error.kind)<<" native "<<result.error.os_code<<" partial_utf16 "<<result.error.partial_utf16_units<<'\n';report.flush();
    require(!close && rooted && result.completed>0 && result.completed<4*1024*1024 && result.completed==units*2 && !result.error.partial_utf16_units,"genuine wide record prefix count/root mismatch");
    require(result.error.kind==ErrorKind::closed && (result.error.os_code==0 || result.error.os_code==995),"genuine wide partial cancellation error mismatch");
    report<<"PASS real WriteConsoleW prefix progress followed by cancellation; no request clamp or synthetic completion; native UTF16 count exactly matches Runtime UTF8 prefix\n";
}
void wide_read_cancel(const char* path){
    ConsoleWindowsObservation observation;std::ofstream report(path);require(bool(report),"native wide read cancellation report");
    DWORD baseline=0;require(GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE),&baseline) && (baseline&ENABLE_LINE_INPUT),"cooked native input baseline required");
    vd::Runtime runtime(0);auto c=runtime.console_context();
    // A genuine cooked read has entered ReadConsoleW but cannot finish its line.
    // The queued character is explicitly SCRIPTED, never a physical key claim.
    queue(u"Q");std::atomic<bool> rooted=false;
    auto call=std::async(std::launch::async,[&]{vd::Runtime::Attachment attachment(runtime);vd::Runtime::Managed managed(runtime);void* root=nullptr;std::size_t offset=0;vd::Runtime::Slot slot{&root,sizeof(root),&offset,1};vd::Runtime::Frame frame(runtime,&slot,1);runtime.allocate_into(&root,{64,{}});TextResult result;
        {vd::Runtime::Native safe(runtime);result=c->read_char(2000);}rooted=runtime.contains(root);return result;
    });
    struct Drain {vd::Runtime& runtime;~Drain(){runtime.close();}}drain{runtime};
    until([]{return vd_console_native_pending();});auto buffer=vd_console_pending_buffer();require(buffer!=0,"pending ReadConsoleW buffer missing");
    {vd::Runtime::Attachment attachment(runtime);vd::Runtime::Managed managed(runtime);require(runtime.collect(5s)==vd::Runtime::Collection::completed && runtime.snapshot().objects==1,"pending ReadConsoleW root lost");}
    require(vd_console_native_pending() && buffer==vd_console_pending_buffer(),"ReadConsoleW buffer moved/completed during GC");
    auto closed=runtime.close();auto result=call.get();observation.result("actual_ReadConsoleW_cancel",result.transfer.error);
    bool native_ok=false;DWORD native_error=0,native_count=0;vd_console_last_wide_read(native_ok,native_error,native_count);
    require(!closed && rooted && native_count==0 && result.transfer.completed==0 && result.text.empty(),"actual ReadConsoleW cancellation/progress mismatch");
    // Windows 10 conhost can return TRUE/zero after CancelSynchronousIo. Under
    // the existing completion-wins rule that is EOF, not an invented error 995.
    if(native_ok)require(!result.transfer.error && result.transfer.eof,"successful zero-unit ReadConsoleW did not preserve EOF");
    else require(native_error==995 && result.transfer.error.kind==ErrorKind::closed && result.transfer.error.os_code==native_error,"failed ReadConsoleW native error changed");
    DWORD after=0;require(GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE),&after) && after==baseline,"native cooked read changed mode");
    observation.state("after_cooked_native_read_cancel");report<<"PASS actual pending cooked ReadConsoleW, stable native buffer and live GC root; targeted cancellation: native success "<<native_ok<<", error "<<native_error<<", units "<<native_count<<", Runtime EOF "<<result.transfer.eof<<", unchanged console mode\n";
}
void wide_read_error(const char* path){
    ConsoleWindowsObservation observation;std::ofstream report(path);require(bool(report),"native wide read error report");
    Handle input(CreateFileW(L"CONIN$",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr));require(input.value!=INVALID_HANDLE_VALUE,"private native read handle");
    Standard standard(STD_INPUT_HANDLE,input.value);vd::Runtime runtime;auto c=runtime.console_context();SetStdHandle(STD_INPUT_HANDLE,standard.original);
    queue(u"Q\r");vd_console_next_read_fault();auto result=c->read_char(2000);input.value=nullptr;
    bool native_ok=false;DWORD native_error=0,native_count=0;vd_console_last_wide_read(native_ok,native_error,native_count);
    observation.result("host_invalidated_actual_ReadConsoleW",result.transfer.error);
    require(!native_ok && native_error==6 && native_count==0 && result.transfer.error.kind==ErrorKind::closed && result.transfer.error.os_code==6 && result.transfer.completed==0 && result.text.empty(),"actual ReadConsoleW invalid-handle error/progress changed");
    require(!runtime.close(),"native wide read error Runtime close");report<<"PASS host invalidated private handle at actual ReadConsoleW boundary; native FALSE/error 6/zero units preserved as Runtime closed/6/zero bytes\n";
}
}
void vd_windows_console_closed_peers(){
    auto verify=[](HANDLE writer){
        Standard standard(STD_OUTPUT_HANDLE,writer);
        vd::Runtime runtime;
        auto result=runtime.console_context()->write(Stream::output,Bytes{'x'},1000);
        require(result.completed==0 && !result.eof && result.error.kind==ErrorKind::broken_pipe,
                "closed native pipe write must fail as broken_pipe without retrying to deadline");
        require(result.error.os_code==ERROR_NO_DATA || result.error.os_code==ERROR_BROKEN_PIPE ||
                result.error.os_code==ERROR_PIPE_NOT_CONNECTED,"closed pipe lost native error code");
        require(!runtime.close(),"closed pipe Runtime cleanup");
        DWORD flags=0;require(GetHandleInformation(writer,&flags),"Runtime closed borrowed pipe writer");
        std::cerr<<"closed native pipe: completed="<<result.completed
                 <<" os_code="<<result.error.os_code<<" borrowed_writer_open=1\n";
    };
    Handle reader,writer;
    require(CreatePipe(&reader.value,&writer.value,nullptr,4096),"closed anonymous pipe fixture");
    require(CloseHandle(reader.value),"close anonymous pipe peer");reader.value=nullptr;
    verify(writer.value);
    for(bool overlapped:{false,true}){
        Pipe pipe(overlapped,true);
        require(CloseHandle(pipe.peer.value),"close named pipe peer");pipe.peer.value=nullptr;
        verify(pipe.borrowed.value);
    }
}
bool vd_windows_console_native(const char* mode,const char* report){
    std::string value(mode);
    if(value=="--windows-modes")modes(report);
    else if(value=="--windows-wide")wide(report);
    else if(value=="--windows-cancel-sync-read")cancel(report,false,false);
    else if(value=="--windows-cancel-overlapped-read")cancel(report,true,false);
    else if(value=="--windows-cancel-sync-write")cancel_write(report,false);
    else if(value=="--windows-cancel-overlapped-write")cancel_write(report,true);
    else if(value=="--windows-cleanup-failure")cleanup_failure(report);
    else if(value=="--windows-nested-shutdown")nested_shutdown(report);
    else if(value=="--windows-wide-partial")wide_fault(report,true);
    else if(value=="--windows-wide-error")wide_fault(report,false);
    else if(value=="--windows-partial-sync-read")partial_pipe(report,false);
    else if(value=="--windows-partial-overlapped-read")partial_pipe(report,true);
    else if(value=="--windows-wide-progress")wide_progress(report);
    else if(value=="--windows-wide-read-cancel")wide_read_cancel(report);
    else if(value=="--windows-wide-read-error")wide_read_error(report);
    else return false;
    return true;
}
#endif
