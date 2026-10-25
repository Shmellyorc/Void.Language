#include "void/console.hpp"
#include "void/runtime.hpp"
#include <fstream>
#include <atomic>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <semaphore>
#include <algorithm>
#include <future>
#include <functional>
#include <iostream>
#include <stdexcept>
#include "console_windows_observation.hpp"
#include "console_windows_native.hpp"
using namespace vd::console;
namespace {
std::atomic<unsigned> checks=0;void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
struct Fake:Adapter {
    std::string pending,output;std::size_t at=0,chunk=4096;unsigned mode=7,restores=0;bool allocation_fault=false,fault_once=false,eof=false,restore_once=false;std::function<void(const Request&)> before;std::function<void()> saving,restoring,describing;std::optional<PhysicalKey> physical;Error fault,restore_fault,apply_fault,payload_fault;mutable std::chrono::steady_clock::time_point clock{};
    std::array<Endpoint,3> inspect()const override{return {{{Stream::input,Kind::terminal},{Stream::output,Kind::terminal},{Stream::error,Kind::pipe}}};}
    auto now()const noexcept->std::chrono::steady_clock::time_point override{return clock;}
    std::uint64_t capabilities(Stream s)override{return s==Stream::input?text_input|key_input|input_modes:text_output;}
    Error save_mode(InputMode& saved)override{if(saving)saving();saved.native[0]=mode;return {};}
    Error apply_mode(const InputMode&,ModeOptions o)override{mode=(o.echo?1:0)|(o.line_input?2:0)|(o.control_c_as_input?0:4);return apply_fault;}
    Error restore_mode(const InputMode& saved)override{++restores;if(restoring)restoring();if(restore_fault){auto error=restore_fault;if(restore_once)restore_fault={};return error;}mode=saved.native[0];return {};}
    std::vector<std::pair<std::string,Key>> key_sequences()override{if(describing)describing();if(allocation_fault)throw std::bad_alloc();return {{"\033[A",Key::up},{"\033[B",Key::down},{"\033OP",Key::f1},{"\033[24~",Key::f12}};}
    Transfer read(std::span<std::uint8_t> out,const Request& request)override{if(before)before(request);if(fault){auto error=fault;if(fault_once)fault={};return {0,false,error};}if(physical && at==pending.size()){auto key=physical;physical.reset();return {0,false,{},key};}if(at==pending.size()){if(eof)return {0,true};if(request.deadline && !request.polling)clock=*request.deadline;return {0,false,{request.polling?ErrorKind::would_block:ErrorKind::timed_out}};}auto n=std::min({out.size(),chunk,pending.size()-at});std::copy_n(pending.begin()+at,n,out.begin());at+=n;auto key=physical;physical.reset();return {n,false,payload_fault,key};}
    Transfer write(Stream,std::span<const std::uint8_t> bytes,const Request&)override{output.append(bytes.begin(),bytes.end());return {bytes.size()};}
};
void probe(unsigned count,bool unclosed){
    ConsoleWindowsObservation observation;
    {vd::Runtime runtime;auto c=runtime.console_context();observation.state("after_runtime");
    auto outer=c->begin_mode({false,false,true});observation.result("begin_outer",outer.error,outer.token);observation.state("after_outer_apply");check(!outer.error,"native mode failed");
    auto inner=c->begin_mode({false,true,false});observation.result("begin_inner",inner.error,inner.token);observation.state("after_inner_apply");check(!inner.error,"native nested mode failed");
    auto non_lifo=c->end_mode(outer.token);observation.result("reject_non_lifo",non_lifo,outer.token);observation.state("after_non_lifo");check(non_lifo.kind==ErrorKind::would_block,"native non-LIFO accepted");
    auto restored=c->end_mode(inner.token);observation.result("end_inner",restored,inner.token);observation.state("after_inner_restore");check(!restored,"native inner restore failed");
    auto available=c->key_available();check(!available.transfer.error && !available.first,"native initial availability failed");std::cout<<"READY\n"<<std::flush;
    bool captured=!observation.capturing();
    for(unsigned n=0;n<count;++n){auto key=c->read_key(false,2000);observation.key(n,key);observation.state("after_key");check(!key.transfer.error,"native key failed");std::cout<<static_cast<unsigned>(key.key)<<' '<<key.modifiers_known<<' '<<key.key_down_known<<' '<<key.repeat<<' ';for(auto byte:key.bytes)std::cout<<"0123456789abcdef"[byte>>4]<<"0123456789abcdef"[byte&15];std::cout<<'\n';if(observation.captured(key)){captured=true;break;}}
    if(observation.capturing())check(captured,"native opt-in capture did not reach requested event");
    if(!unclosed){auto error=c->end_mode(outer.token);observation.result("end_outer",error,outer.token);observation.state("after_outer_restore");check(!error,"native outer restore failed");}
    observation.state("before_runtime_close");auto closed=runtime.close();observation.result("runtime_close",closed);observation.state("after_runtime_close");}
    observation.state("after_runtime_destructor");
}
void default_probe(){vd::Runtime runtime;auto c=runtime.console_context();std::cout<<"READY\n"<<std::flush;auto key=c->read_key(true,2000);check(!key.transfer.error,"default read failed");std::cout<<static_cast<unsigned>(key.key)<<'\n';runtime.close();}

void shared_probe(){
    vd::Runtime first(1),second(1);auto a=first.console_context(),b=second.console_context();check(a->streams()==b->streams(),"real process queue split");
    std::binary_semaphore owned(0);auto read=std::async(std::launch::async,[&]{
        vd::Runtime::Attachment attachment(second);vd::Runtime::Managed managed(second);void* root=nullptr;std::size_t offset=0;
        vd::Runtime::Slot slot{&root,sizeof(root),&offset,1};vd::Runtime::Frame frame(second,&slot,1);second.allocate_into(&root,{8,{}});
        std::string result;{vd::Runtime::Native safe(second);auto outer=b->begin_mode({false,false,true});check(!outer.error,"real outer mode failed");auto inner=b->begin_mode({false,false,true});check(!inner.error,"real inner mode failed");owned.release();
            auto key=b->read_key(false,5000);check(!key.transfer.error,"real shared key failed");auto line=b->read_line(5000),tail=b->read_line(5000);
            check(!line.transfer.error && !tail.transfer.error && line.text.empty() && tail.text=="Z","real mixed key/line queue ownership failed");result=key.character;
            check(!b->end_mode(inner.token) && !b->end_mode(outer.token),"real nested restoration failed");}
        check(second.contains(root),"real shared input GC root lost");return result;
    });owned.acquire();
    check(a->read_line(0).transfer.error.kind==ErrorKind::would_block && a->key_available().transfer.error.kind==ErrorKind::would_block && a->begin_mode().error.kind==ErrorKind::would_block,"real foreign input owner accepted");
    check(a->present(Stream::output,Presentation::cursor_position).transfer.error.kind==ErrorKind::would_block,"real foreign cursor query accepted");
    {vd::Runtime::Attachment attachment(second);vd::Runtime::Managed managed(second);check(second.collect(std::chrono::seconds(5))==vd::Runtime::Collection::completed,"real shared read blocked GC");}
    check(!first.close(),"real independent shutdown failed");vd::Runtime third;auto c=third.console_context();
    auto ready=std::string("READY\n");check(!c->write(Stream::output,Bytes(ready.begin(),ready.end())).error,"real readiness write failed");
    auto writer=[&](std::shared_ptr<Context> context,Stream stream,char label){for(unsigned n=0;n<32;++n){auto text=std::string(1,label)+std::to_string(n)+":é🎮\n";auto result=context->write(stream,Bytes(text.begin(),text.end()));if(result.error)throw std::runtime_error("real concurrent output failed");}};
    auto output=std::async(std::launch::async,writer,b,Stream::output,'O'),errors=std::async(std::launch::async,writer,c,Stream::error,'E');output.get();errors.get();
    auto key=read.get();check(!second.close() && !third.close(),"real shared Runtime close failed");std::cout<<"DONE:"<<key<<"\n";
}
void run(){
    {struct ClosedAdapter:Fake {std::uint64_t capabilities(Stream)override{return 0;}Error save_mode(InputMode&)override{return {ErrorKind::closed,55};}};auto f=std::make_shared<ClosedAdapter>();Context c(Streams::isolated(f));auto mode=c.begin_mode();check(mode.error.kind==ErrorKind::closed && mode.error.os_code==55,"capability precheck hid original closed-input error");}
    {auto f=std::make_shared<Fake>();{Context c(Streams::isolated(f));check(!c.begin_mode({false,false,true}).error,"abandoned session setup failed");}check(f->mode==7,"native session destruction failed to restore");}
    {auto f=std::make_shared<Fake>();f->pending="q";f->physical=PhysicalKey{Key::character,5,3,true,true,true};Context c(Streams::isolated(f));check(c.key_available().first==1,"physical availability lost event");check(c.read_char(0).transfer.error.kind==ErrorKind::unsupported,"repeated physical event silently became one character");auto k=c.read_key();check(k.key==Key::character && k.character=="q" && k.repeat==3 && k.modifiers==5 && k.modifiers_known && k.key_down_known,"physical metadata lost across speculative availability");}
    {auto f=std::make_shared<Fake>();f->physical=PhysicalKey{Key::up,2,1,true,false,true};Context c(Streams::isolated(f));check(c.key_available().first==1,"key-up availability lost");check(c.read_bytes(1,0).transfer.error.kind==ErrorKind::unsupported,"key-up silently discarded by raw read");auto k=c.read_key();check(k.key==Key::up && k.key_down_known && !k.key_down && k.modifiers==2,"key-up metadata changed");}
    {auto f=std::make_shared<Fake>();Context c(Streams::isolated(f));auto mode=c.begin_mode({false,false,true});f->restore_fault={ErrorKind::io_failure,77};check(c.close().os_code==77 && f->mode==0,"shutdown restoration failure not reported");f->restore_fault={};check(!c.close() && f->mode==7,"shutdown restoration retry lost original mode");}
    {auto f=std::make_shared<Fake>();f->apply_fault={ErrorKind::io_failure,66};Context c(Streams::isolated(f));auto r=c.begin_mode({false,false,true});check(r.error.os_code==66 && f->mode==7,"partially applied failed mode was not restored");}
    {auto f=std::make_shared<Fake>();f->pending="q";f->physical=PhysicalKey{Key::character,0,1,true,false,true};Context c(Streams::isolated(f));auto k=c.read_key(true);check(!k.transfer.error && f->output.empty(),"physical key-up was echoed twice");}
    {auto f=std::make_shared<Fake>();f->pending="q";f->physical=PhysicalKey{Key::character,0,3,true,true,true};Context c(Streams::isolated(f));auto k=c.read_key(true);check(!k.transfer.error && f->output=="qqq","physical repeat echo count lost");}
    {auto f=std::make_shared<Fake>();f->pending="q";f->physical=PhysicalKey{Key::character,0,1,true,true,true};f->payload_fault={ErrorKind::io_failure,88};Context c(Streams::isolated(f));auto k=c.read_key();check(k.transfer.error.os_code==88 && k.transfer.completed==1 && k.bytes==Bytes{'q'},"physical event swallowed native error/progress");}
    {auto f=std::make_shared<Fake>();f->fault={ErrorKind::io_failure,88};f->fault_once=true;f->pending="q";Context c(Streams::isolated(f));auto first=c.read_key();auto second=c.read_key();check(first.transfer.error.os_code==88 && !second.transfer.error && second.character=="q","delivered native key error poisoned later reads");}
    {auto f=std::make_shared<Fake>();f->pending=std::string("\xff")+"q";Context c(Streams::isolated(f));auto first=c.read_key();auto second=c.read_key();check(first.transfer.error.kind==ErrorKind::invalid_encoding && first.bytes==Bytes{255} && first.transfer.completed==1 && second.character=="q","invalid key encoding lost partial bytes or recovery");}
    {auto f=std::make_shared<Fake>();f->pending="\x9f\x8e\xae";f->physical=PhysicalKey{Key::character};Context c(Streams::isolated(f));auto key=c.read_key();check(key.transfer.error.kind==ErrorKind::invalid_encoding && key.bytes==Bytes{159,142,174} && key.transfer.completed==3 && key.character.empty(),"raw scalar tail became invalid physical key text");}
    {auto f=std::make_shared<Fake>();f->eof=true;Context c(Streams::isolated(f));check(c.read_key().transfer.error.kind==ErrorKind::closed && c.read_line().transfer.eof && c.read_bytes(1).transfer.eof,"key EOF changed ordinary shared-reader EOF ownership");}
    {auto f=std::make_shared<Fake>();f->fault={ErrorKind::interrupted,4};f->fault_once=true;f->pending="q";Context c(Streams::isolated(f));auto key=c.read_key(false,50);check(!key.transfer.error && key.character=="q" && f->mode==7,"interrupted key read failed recovery/restoration");}
    {auto f=std::make_shared<Fake>();f->pending="\033";f->allocation_fault=true;f->restore_fault={ErrorKind::io_failure,99};f->restore_once=true;Context c(Streams::isolated(f));bool original=false;std::int64_t code=0;try{c.read_key();}catch(const ModeCleanupFailure& failure){code=failure.cleanup.os_code;try{std::rethrow_exception(failure.primary);}catch(const std::bad_alloc&){original=true;}}auto error=c.finish_modes(1);check(original && code==99 && !error && f->mode==7,"exceptional restoration failure vanished after a successful retry");}
    {auto f=std::make_shared<Fake>();f->pending="\033";f->allocation_fault=true;f->restore_fault={ErrorKind::io_failure,99};f->restore_once=true;Context c(Streams::isolated(f));std::binary_semaphore caught(0),finish(0);auto owner=std::async(std::launch::async,[&]{try{c.read_key();}catch(const ModeCleanupFailure& error){caught.release();finish.acquire();auto restored=c.finish_modes(1);return error.cleanup.os_code==99 && !restored;}return false;});caught.acquire();check(!c.finish_modes(1) && f->mode!=7,"another entry stole exceptional cleanup or mode ownership");finish.release();check(owner.get() && f->mode==7,"concurrent exceptional caller lost diagnostic/restoration");}
    {auto f=std::make_shared<Fake>();Context c(Streams::isolated(f));std::binary_semaphore entered(0),finish(0);f->saving=[&]{entered.release();finish.acquire();};auto mode=std::async(std::launch::async,[&]{return c.begin_mode({false,false,true});});entered.acquire();auto closing=std::async(std::launch::async,[&]{c.begin_close();});bool prompt=closing.wait_for(std::chrono::seconds(1))==std::future_status::ready;finish.release();closing.get();auto result=mode.get();check(prompt && result.error.kind==ErrorKind::closed && f->mode==7,"entered mode query blocked queued cancellation or applied after close");c.close();}
    {auto f=std::make_shared<Fake>();Context c(Streams::isolated(f));check(!c.begin_mode({false,false,true}).error,"shutdown mode setup failed");std::binary_semaphore entered(0),finish(0);f->restoring=[&]{entered.release();finish.acquire();};auto closing=std::async(std::launch::async,[&]{return c.close();});entered.acquire();auto rejected=std::async(std::launch::async,[&]{return c.write(Stream::output,Bytes{'x'},0);});bool prompt=rejected.wait_for(std::chrono::seconds(1))==std::future_status::ready;finish.release();auto result=closing.get();check(prompt && !result && rejected.get().error.kind==ErrorKind::closed && f->mode==7,"entered mode restore held metadata lock over cancellation");}
    {auto f=std::make_shared<Fake>();f->pending="q";f->saving=[&]{f->clock+=std::chrono::milliseconds(20);};Context c(Streams::isolated(f));auto key=c.read_key(false,10);check(key.transfer.error.kind==ErrorKind::timed_out && f->mode==7 && f->at==0,"mode preparation restarted or exceeded the original key deadline");}
    for(auto chunk:{1u,2u,4096u}){
        auto f=std::make_shared<Fake>();f->chunk=chunk;f->pending="é🎮\r\t\177\033[A\033[B\033OP\033[24~\033[999~";Context c(Streams::isolated(f));
        auto available=c.key_available();check(!available.transfer.error && available.first==1 && f->mode==7,"availability changed mode");
        for(auto [key,text]:{std::pair{Key::character,"é"},{Key::character,"🎮"},{Key::enter,""},{Key::tab,""},{Key::backspace,""},{Key::up,""},{Key::down,""},{Key::f1,""},{Key::f12,""},{Key::unknown,""}}){auto r=c.read_key(true);check(!r.transfer.error && r.key==key && r.character==text && !r.modifiers_known && !r.key_down_known && r.repeat==1,"key decode mismatch");check(f->mode==7,"key read left mode changed");if(key==Key::unknown)check(std::string(r.bytes.begin(),r.bytes.end())=="\033[999~","unknown bytes lost");}
        check(f->output=="é🎮","echo duplicated or changed bytes");check(c.key_available().first==0,"empty availability true");
    }
    // Terminfo/decoder work may consume the continuation budget after one native
    // read has already queued the complete sequence. A deadline must not split it.
    {auto f=std::make_shared<Fake>();f->pending="\033[Aq";f->describing=[&]{f->clock+=std::chrono::milliseconds(31);};Context c(Streams::isolated(f));auto key=c.read_key();check(!key.transfer.error && key.key==Key::up && key.bytes==Bytes{27,'[','A'},"buffered Up Arrow split after terminfo work");check(c.read_char(0).text=="q" && f->mode==7,"buffered continuation lost shared input or restoration");}
    // With no buffered continuation, the same expired absolute budget still
    // returns Escape; a later shared reader owns the untouched remaining bytes.
    {auto f=std::make_shared<Fake>();f->chunk=1;f->pending="\033[Aq";f->describing=[&]{f->clock+=std::chrono::milliseconds(31);};Context c(Streams::isolated(f));auto key=c.read_key();check(!key.transfer.error && key.key==Key::escape && key.bytes==Bytes{27},"unbuffered continuation extended the 30ms deadline");std::string tail;for(unsigned n=0;n<3;++n)tail+=c.read_char(0).text;check(tail=="[Aq" && f->mode==7,"expired continuation discarded input bytes");}
    {auto f=std::make_shared<Fake>();f->clock=std::chrono::steady_clock::time_point::max()-std::chrono::milliseconds(10);f->pending="\033";Context c(Streams::isolated(f));auto k=c.read_key();check(k.key==Key::escape && !k.transfer.error,"escape deadline overflowed near clock limit");}
    for(auto timeout:{-1,0,10,100}){
        auto f=std::make_shared<Fake>();f->pending="\033";Context c(Streams::isolated(f));auto r=c.read_key(false,timeout);check(!r.transfer.error && r.key==Key::escape,"escape continuation failed");auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(f->clock.time_since_epoch()).count();check(ms==(timeout==0?0:timeout==10?10:30),"escape did not retain bounded absolute deadline");
    }
    {auto f=std::make_shared<Fake>();f->pending="\033[";Context c(Streams::isolated(f));auto r=c.read_key();check(r.key==Key::unknown && r.bytes==Bytes{27,'['},"incomplete escape lost bytes");}
    {auto f=std::make_shared<Fake>();f->pending="line\r\nZ\n";Context c(Streams::isolated(f));check(c.read_line().text=="line","line setup failed");check(c.key_available().first==1,"pending CR availability failed");check(c.read_line().text=="Z","availability changed CRLF continuation");}
    {auto f=std::make_shared<Fake>();f->pending="é\n";Context c(Streams::isolated(f));check(c.key_available().first==1 && c.read_char().text=="é" && c.read_line().text.empty(),"availability stole character bytes");}
    {auto f=std::make_shared<Fake>();Context c(Streams::isolated(f));auto outer=c.begin_mode({false,false,true});check(!outer.error && f->mode==0,"outer mode failed");auto inner=c.begin_mode({true,true,false});check(!inner.error && f->mode==7,"nested mode failed");check(c.end_mode(outer.token).kind==ErrorKind::would_block && f->mode==7,"non-LIFO restore accepted");auto other=std::async(std::launch::async,[&]{return c.begin_mode();}).get();check(other.error.kind==ErrorKind::would_block,"foreign session owner accepted");check(!c.end_mode(inner.token) && f->mode==0,"inner exact restoration failed");check(!c.end_mode(outer.token) && f->mode==7,"outer exact restoration failed");check(c.end_mode(outer.token).kind==ErrorKind::closed,"closed mode reused");auto a=c.begin_mode({false,false,true});auto b=c.begin_mode({true,false,true});c.close();check(f->mode==7 && c.snapshot().admitted==0,"shutdown did not restore nested modes");}
    {auto f=std::make_shared<Fake>();f->fault={ErrorKind::io_failure,88};Context c(Streams::isolated(f));auto r=c.read_key();check(r.transfer.error.os_code==88 && f->mode==7,"native failure lost restoration");}
    {auto f=std::make_shared<Fake>();f->pending="\033";f->allocation_fault=true;Context c(Streams::isolated(f));bool caught=false;try{c.read_key();}catch(const std::bad_alloc&){caught=true;}check(caught && f->mode==7,"allocation exception skipped exact mode restoration");}
    {auto f=std::make_shared<Fake>();f->fault={ErrorKind::io_failure,88};f->restore_fault={ErrorKind::io_failure,99};Context c(Streams::isolated(f));auto r=c.read_key();check(r.transfer.error.os_code==88 && r.transfer.error.cleanup_code==99,"restoration overwrote primary failure");f->restore_fault={};c.close();check(f->mode==7,"shutdown lost failed restoration state");}
    {auto f=std::make_shared<Fake>();Context c(Streams::isolated(f));std::binary_semaphore entered(0),cancelled(0),finish(0);f->before=[&](const Request& request){std::stop_callback stop(request.cancellation,[&]{cancelled.release();});entered.release();finish.acquire();};auto read=std::async(std::launch::async,[&]{return c.read_key();});entered.acquire();auto close=std::async(std::launch::async,[&]{c.close();});cancelled.acquire();check(f->mode!=7 && c.snapshot().admitted==1,"shutdown released entered read mode early");finish.release();auto r=read.get();close.get();check(f->mode==7 && c.snapshot().admitted==0,"shutdown race lost original mode");}
}
}
int main(int argc,char** argv){try{
#ifdef _WIN32
if(argc==3 && vd_windows_console_native(argv[1],argv[2]))return 0;
#endif
#ifndef _WIN32
if(argc==2 && std::string(argv[1])=="canonical-availability"){vd::Runtime runtime;auto context=runtime.console_context();auto available=context->key_available();std::cout<<"AVAILABLE "<<available.first<<"\n"<<std::flush;auto line=context->read_line(2000);if(line.transfer.error)throw std::runtime_error("canonical read failed");std::cout<<line.text<<"\n";runtime.close();return 0;}
if(argc==2 && std::string(argv[1])=="closed-input"){vd::Runtime runtime;auto context=runtime.console_context();::close(0);auto result=context->begin_mode();std::cout<<static_cast<unsigned>(result.error.kind)<<' '<<result.error.os_code<<'\n';runtime.close();return 0;}
#endif
if(argc==2 && std::string(argv[1])=="shared"){shared_probe();return 0;}if(argc==2){default_probe();return 0;}if(argc==3){probe(static_cast<unsigned>(std::stoul(argv[1])),std::string(argv[2])=="unclosed");return 0;}run();std::cout<<checks<<" key/mode checks passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
