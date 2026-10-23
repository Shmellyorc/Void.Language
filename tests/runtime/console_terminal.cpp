#include "void/console.hpp"
#include "void/runtime.hpp"
#include <algorithm>
#include <atomic>
#include <fstream>
#include <future>
#include <functional>
#include <iostream>
#include <semaphore>
using namespace vd::console;
namespace {
unsigned checks=0;void check(bool b,const char* why){if(!b)throw std::runtime_error(why);++checks;}
struct Fake:Adapter {
    std::string pending,output;std::size_t at=0,chunk=4096;bool replying=true;std::string response="useré\033[12;34Rtail";Error fault{},read_fault{};unsigned prepares=0,writes=0;std::function<void(const Request&)> before_read;
    std::array<Endpoint,3> inspect() const override{return {{{Stream::input,Kind::terminal},{Stream::output,Kind::terminal},{Stream::error,Kind::pipe}}};}
    std::chrono::steady_clock::time_point now() const noexcept override{return std::chrono::steady_clock::now();}
    std::uint64_t capabilities(Stream s) override{return s==Stream::input?text_input:s==Stream::error?text_output:2034;}
    TerminalResult present(Stream,Presentation p,std::int64_t a,std::int64_t b,std::string_view text,const Request&) override {
        ++prepares;TerminalResult r;r.first=80;r.second=24;
        if(p==Presentation::size)return r;
        if(p==Presentation::cursor_position){r.sequence={27,'[','6','n'};r.response=true;return r;}
        if(p==Presentation::set_cursor && (a>=80 || b>=24)){r.transfer.error={ErrorKind::invalid_argument};return r;}
        std::string bytes=p==Presentation::title?std::string(text):"record;";r.sequence.assign(bytes.begin(),bytes.end());return r;
    }
    Transfer read(std::span<std::uint8_t> out,const Request& request) override {if(before_read)before_read(request);if(request.cancellation.stop_requested())return {0,false,{ErrorKind::closed}};if(at==pending.size())return {0,false,{request.polling?ErrorKind::would_block:ErrorKind::timed_out}};auto n=std::min({out.size(),chunk,pending.size()-at});std::copy_n(pending.begin()+at,n,out.begin());at+=n;return {n,false,request.polling?Error{}:read_fault};}
    Transfer write(Stream,std::span<const std::uint8_t> bytes,const Request&) override{++writes;if(fault){output.append(reinterpret_cast<const char*>(bytes.data()),std::min<std::size_t>(2,bytes.size()));return {std::min<std::size_t>(2,bytes.size()),false,fault};}output.append(reinterpret_cast<const char*>(bytes.data()),bytes.size());if(replying && std::string_view(reinterpret_cast<const char*>(bytes.data()),bytes.size())=="\033[6n")pending+=response;return {bytes.size()};}
};
void injected(){
    for(auto chunk:{1u,2u,4096u}){
        auto fake=std::make_shared<Fake>();fake->chunk=chunk;fake->pending="old\033[1;1R";auto context=std::make_shared<Context>(Streams::isolated(fake));
        auto r=context->present(Stream::output,Presentation::cursor_position);check(!r.transfer.error && r.first==33 && r.second==11 && r.transfer.completed==4,"query used stale report or lost fresh coordinates");
        std::string remaining;for(unsigned n=0;n<64;++n){auto part=context->read_bytes(1024,0);if(part.transfer.error)break;remaining.append(part.bytes.begin(),part.bytes.end());}check(remaining=="old\033[1;1Ruserétail","query discarded unrelated queued/native input");
    }
    {auto fake=std::make_shared<Fake>();fake->response="\033[12;34R";fake->read_fault={ErrorKind::io_failure,87,19};Context c(Streams::isolated(fake));auto result=c.present(Stream::output,Presentation::cursor_position);check(result.transfer.error.kind==ErrorKind::io_failure && result.transfer.error.os_code==87 && result.transfer.error.cleanup_code==19 && result.transfer.completed==4,"report swallowed its native read error");auto bytes=c.read_bytes(64,0);check(std::string(bytes.bytes.begin(),bytes.bytes.end())=="\033[12;34R" && bytes.transfer.error.os_code==87,"failed report discarded native bytes/error");}
    for(bool logical:{false,true}){auto fake=std::make_shared<Fake>();fake->pending="first\r\n";fake->response="\033[12;34R\n";Context c(Streams::isolated(fake));check(c.read_line().text=="first","CR setup failed");check(!c.present(Stream::output,Presentation::cursor_position).transfer.error,"query with pending CRLF failed");if(logical){auto line=c.read_line();check(line.text.empty() && !line.transfer.eof && line.transfer.completed==2,"query changed pending logical CRLF ownership");}else {auto raw=c.read_bytes(1);check(raw.bytes==Bytes{'\n'},"query discarded raw CRLF continuation");}}
    {auto fake=std::make_shared<Fake>();fake->response=std::string("\xff")+"\033[12;34R\xc3";Context c(Streams::isolated(fake));check(!c.present(Stream::output,Presentation::cursor_position).transfer.error,"query rejected unrelated invalid UTF8");auto raw=c.read_bytes(4);check(raw.bytes==Bytes{255,195},"query lost unrelated invalid/truncated user bytes");}
    auto fake=std::make_shared<Fake>();Context c(Streams::isolated(fake));
    for(auto args:{std::pair{-1LL,0LL},std::pair{80LL,0LL},std::pair{0LL,24LL}})check(c.present(Stream::output,Presentation::set_cursor,args.first,args.second).transfer.error.kind==ErrorKind::invalid_argument,"cursor bounds accepted");
    for(int color=-1;color<=16;++color)if(color<0 || color>15)check(c.present(Stream::output,Presentation::color,color,0).transfer.error.kind==ErrorKind::invalid_argument,"invalid palette value accepted");
    check(c.present(Stream::output,Presentation::cursor_visible,2).transfer.error.kind==ErrorKind::invalid_argument,"invalid visibility accepted");
    for(unsigned scalar=0;scalar<160;++scalar)if(scalar<32 || scalar>=127){std::string text="safe";if(scalar<128)text+=static_cast<char>(scalar);else {text+=static_cast<char>(0xc2);text+=static_cast<char>(scalar);}check(c.present(Stream::output,Presentation::title,0,0,text).transfer.error.kind==ErrorKind::invalid_argument,"title control injection accepted");}
    check(fake->writes==0,"rejected presentation wrote native bytes");
    for(int a=0;a<16;++a)for(int b=0;b<16;++b)check(!c.present(Stream::output,Presentation::color,a,b).transfer.error,"valid palette pair rejected");
    check(c.present(Stream::output,Presentation::title,0,0,"é🎮").transfer.completed==6,"Unicode title bytes changed");
    fake->fault={ErrorKind::io_failure,87,19,1};auto partial=c.present(Stream::output,Presentation::title,0,0,"abcdef");check(partial.transfer.completed==2 && partial.transfer.error.os_code==87 && partial.transfer.error.cleanup_code==19 && partial.transfer.error.partial_utf16_units==1,"presentation lost original native progress");fake->fault={};
    {auto input=c.acquire(Stream::input);check(c.present(Stream::output,Presentation::cursor_position).transfer.error.kind==ErrorKind::unsupported,"recursive query stole input owner");}
    std::binary_semaphore held(0),release(0);auto reader=std::async(std::launch::async,[&]{auto input=c.acquire(Stream::input);held.release();release.acquire();});held.acquire();auto writes=fake->writes;auto blocked=c.present(Stream::output,Presentation::cursor_position);check(blocked.transfer.error.kind==ErrorKind::would_block && fake->writes==writes,"query competed with another input reader");release.release();reader.get();
    fake->replying=false;fake->pending="\033[3;";fake->at=0;auto timeout=c.present(Stream::output,Presentation::cursor_position);check(timeout.transfer.error.kind==ErrorKind::timed_out,"incomplete response did not fail recoverably");auto preserved=c.read_bytes(1024,0);check(std::string(preserved.bytes.begin(),preserved.bytes.end())=="\033[3;","timed-out report bytes lost");
    std::vector<std::future<void>> jobs;fake->output.clear();for(unsigned n=0;n<8;++n)jobs.push_back(std::async(std::launch::async,[&]{for(unsigned k=0;k<16;++k){auto result=c.present(Stream::output,Presentation::clear);if(result.transfer.error)throw std::runtime_error("concurrent control failed");}}));for(auto& j:jobs)j.get();check(fake->output.size()==128*7,"concurrent presentation lost records");for(std::size_t n=0;n<fake->output.size();n+=7)check(fake->output.substr(n,7)=="record;","concurrent presentation split a record");
    {auto pending=std::make_shared<Fake>();pending->replying=false;Context closing(Streams::isolated(pending));std::binary_semaphore entered(0),cancelled(0),finish(0);std::atomic<bool> drained=false;pending->before_read=[&](const Request& request){if(!request.polling){std::stop_callback wake(request.cancellation,[&]{cancelled.release();});entered.release();finish.acquire();}};auto query=std::async(std::launch::async,[&]{return closing.present(Stream::output,Presentation::cursor_position);});entered.acquire();auto shutdown=std::async(std::launch::async,[&]{closing.close();drained=true;});cancelled.acquire();check(!drained && closing.snapshot().admitted==2,"pending query storage released before native completion");finish.release();auto r=query.get();shutdown.get();check(r.transfer.error.kind==ErrorKind::closed && r.transfer.completed==4 && closing.snapshot().admitted==0,"pending query cancellation/progress changed");}
    c.close();check(c.capabilities(Stream::output)==0 && c.present(Stream::output,Presentation::bell).transfer.error.kind==ErrorKind::closed,"closed presentation admitted output");
}
void probe(const char* path,const std::string& scenario){
    vd::Runtime runtime;auto c=runtime.console_context();std::ofstream report(path);
    for(auto stream:{Stream::input,Stream::output,Stream::error})report<<c->capabilities(stream)<<' ';report<<'\n';
    auto size=c->present(Stream::output,Presentation::size);report<<static_cast<unsigned>(size.transfer.error.kind)<<' '<<size.first<<' '<<size.second<<'\n';
    if(scenario=="query" || scenario=="timeout" || scenario=="redirected-query"){
        auto pos=c->present(Stream::output,Presentation::cursor_position);report<<static_cast<unsigned>(pos.transfer.error.kind)<<' '<<pos.first<<' '<<pos.second<<'\n';
        if(scenario=="query" || scenario=="timeout" || scenario=="redirected-query"){auto input=c->read_bytes(4096,0);for(auto byte:input.bytes)report<<"0123456789abcdef"[byte>>4]<<"0123456789abcdef"[byte&15];report<<'\n';}
    }else {
        for(auto operation:{Presentation::set_cursor,Presentation::cursor_visible,Presentation::color,Presentation::reset_color,Presentation::clear,Presentation::title,Presentation::bell}){auto r=c->present(Stream::output,operation,operation==Presentation::color?9:0,operation==Presentation::color?4:0,operation==Presentation::title?"VOID é🎮":"");report<<static_cast<unsigned>(r.transfer.error.kind)<<' ';}report<<'\n';
        auto bad=c->present(Stream::output,Presentation::title,0,0,"bad\033]2;injected\a");report<<static_cast<unsigned>(bad.transfer.error.kind)<<'\n';
    }
    runtime.close();check(bool(report),"probe report failed");
}
}
int main(int argc,char** argv){try{if(argc==3)probe(argv[1],argv[2]);else {injected();std::cout<<checks<<" terminal ownership/presentation checks passed\n";}}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
