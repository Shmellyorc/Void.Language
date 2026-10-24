#include "void/console.hpp"
#include "void/runtime.hpp"
#include "console_posix.hpp"
#include "void/utf8.hpp"
#include <atomic>
#include <barrier>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <new>
#include <semaphore>
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/syscall.h>
#endif
#endif
// Same test-only checked allocation fault technique as runtime/failures.cpp.
thread_local bool fail_one_allocation=false;
void* operator new(std::size_t n){if(fail_one_allocation){fail_one_allocation=false;throw std::bad_alloc();}if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}
using namespace vd::console;
using namespace std::chrono_literals;
namespace {
std::atomic<unsigned> checks=0;
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);++checks;}
Bytes bytes(std::string_view text){return {text.begin(),text.end()};}
struct Step {Bytes data;Error error;std::int64_t advance=0;};
struct Script:Adapter {
    std::deque<Step> steps;std::size_t offset=0,reads=0;
    std::int64_t tick=0;bool no_wait=false,fail_result=false;
    std::vector<Request> requests;std::function<void()> action;
    std::array<Endpoint,3> inspect() const override{return {{{Stream::input,Kind::pipe},{Stream::output,Kind::pipe},{Stream::error,Kind::pipe}}};}
    std::chrono::steady_clock::time_point now() const noexcept override{return std::chrono::steady_clock::time_point(std::chrono::milliseconds(tick));}
    Transfer read(std::span<std::uint8_t> out,const Request& request) override {
        ++reads;requests.push_back(request);
        if(steps.empty()){
            if(request.polling)return {0,false,{ErrorKind::would_block}};
            if(no_wait)throw std::logic_error("unexpected blocking input after available data");
            return {0,true,{}};
        }
        auto& step=steps.front();auto count=std::min(out.size(),step.data.size()-offset);
        std::copy_n(step.data.begin()+offset,count,out.begin());offset+=count;
        Error error;
        if(offset==step.data.size()){error=step.error;tick+=step.advance;steps.pop_front();offset=0;}
        if(action)action();
        if(fail_result){fail_result=false;fail_one_allocation=true;}
        return {count,count==0 && !error,error};
    }
    Transfer write(Stream,std::span<const std::uint8_t>,const Request&) override {throw std::logic_error("unexpected input fixture write");}
    void add(std::string_view data,Error error={},std::int64_t advance=0){steps.push_back({bytes(data),error,advance});}
};
struct Harness {
    std::shared_ptr<Script> adapter=std::make_shared<Script>();
    std::shared_ptr<Streams> streams=Streams::isolated(adapter);
    vd::Runtime runtime{0,streams};std::shared_ptr<Context> c=runtime.console_context();
};
void text(const TextResult& result,std::string_view expected,std::size_t count){
    check(!result.transfer.error && !result.transfer.eof && result.text==expected && result.partial.empty() && result.transfer.completed==count && vd::valid_utf8(result.text),"text/progress/UTF8 mismatch");
}
void error(const TextResult& result,ErrorKind kind,std::string_view partial,std::size_t at){
    check(result.transfer.error.kind==kind && result.text.empty() && !result.transfer.eof && result.partial==bytes(partial) && result.transfer.completed==partial.size() && result.error_offset==at,"text failure/partial/offset mismatch");
}
void eof(const TextResult& result){check(result.transfer.eof && !result.transfer.error && result.text.empty() && result.partial.empty(),"text EOF mismatch");}
void mixed(){
    Harness h;h.adapter->add("A\xc3\xa9\xf0\x9f\x8e\xae"); // Add NUL separately in the same native batch.
    h.adapter->steps.back().data.insert(h.adapter->steps.back().data.end(),{0,'\r','\n','B','\n','l','a','s','t'});
    text(h.c->read_char(),"A",1);
    check(h.c->read_bytes(1).bytes==Bytes{0xc3},"raw byte split changed bytes");
    error(h.c->read_char(),ErrorKind::invalid_encoding,"\xa9",0);
    text(h.c->read_text(2),std::string("\xf0\x9f\x8e\xae\0",5),5);
    text(h.c->read_line(),"",1);
    check(h.c->read_bytes(1).bytes==Bytes{'\n'},"raw read discarded pending LF");
    text(h.c->read_line(),"B",2);text(h.c->read_text(2),"la",2);
    {auto op=h.c->acquire(Stream::input);Bytes out(2);auto result=op.read(out,Request{op.cancellation(),{},false});check(out==bytes("st") && result.completed==2 && !result.error,"leased raw read bypassed queue");}
    check(h.adapter->reads==1,"mixed readers reread native input");eof(h.c->read_char());
    check(!h.c->read_bytes(0).transfer.eof && !h.c->read_text(0).transfer.eof,"zero count probed EOF");
    check(h.c->read_text(-1).transfer.error.kind==ErrorKind::invalid_argument && h.c->read_line(-2).transfer.error.kind==ErrorKind::invalid_argument,"invalid text controls accepted");
}
void partitions(Harness& h,const std::string& input,unsigned bits){
    std::size_t start=0;
    for(std::size_t n=1;n<input.size();++n)if(bits&(1u<<(n-1))){h.adapter->add(std::string_view(input).substr(start,n-start));start=n;}
    h.adapter->add(std::string_view(input).substr(start));
}
void unicode(){
    std::vector<std::string> scalars={"X",std::string(1,'\0'),"\xc2\x80","\xdf\xbf","\xe0\xa0\x80","\xed\x9f\xbf","\xee\x80\x80","\xef\xbf\xbf","\xef\xbb\xbf","\xf0\x90\x80\x80","\xf4\x8f\xbf\xbf","\xf0\x9f\x8e\xae"};
    for(const auto& scalar:scalars){
        for(unsigned mask=0;mask<(1u<<(scalar.size()-1));++mask){Harness h;partitions(h,scalar,mask);text(h.c->read_char(),scalar,scalar.size());eof(h.c->read_char());}
        auto input="x"+scalar+"\r";
        for(unsigned mask=0;mask<(1u<<(input.size()-1));++mask){Harness h;partitions(h,input,mask);h.adapter->no_wait=true;text(h.c->read_line(),"x"+scalar,input.size());}
    }
    for(unsigned n=0;n<256;++n){
        auto lead=vd::utf8_sequence_bytes(static_cast<unsigned char>(n));
        check((n<128 && lead==1) || (n>=0xc2 && n<=0xdf && lead==2) || (n>=0xe0 && n<=0xef && lead==3) || (n>=0xf0 && n<=0xf4 && lead==4) || (n>=128 && !(n>=0xc2 && n<=0xf4) && !lead),"shared UTF8 lead grammar changed");
    }
}
void malformed(){
    std::vector<std::string> bad={"\xc0\xaf","\xc1\xbf","\xe0\x80\x80","\xed\xa0\x80","\xed\xbf\xbf","\xf0\x80\x80\x80","\xf4\x90\x80\x80","\xf5\x80\x80\x80","\xff\x80\x80","\xe2\xff\x80"};
    for(unsigned n=128;n<256;++n)if(!vd::utf8_sequence_bytes(static_cast<unsigned char>(n)))bad.push_back(std::string(1,static_cast<char>(n))+"\x80\xbf");
    for(const auto& value:bad)for(unsigned mask=0;mask<(1u<<(value.size()-1));++mask){
        Harness h;h.adapter->add("ok");partitions(h,value,mask);h.adapter->add("Z\n");
        error(h.c->read_line(),ErrorKind::invalid_encoding,"ok"+value,2);text(h.c->read_char(),"Z",1);text(h.c->read_line(),"",1);eof(h.c->read_line());
    }
    for(const auto& full:std::vector<std::string>{"\xc2\xa2","\xe2\x82\xac","\xf0\x9f\x8e\xae"})for(std::size_t n=1;n<full.size();++n){
        Harness h;h.adapter->add("ok"+full.substr(0,n));error(h.c->read_line(),ErrorKind::invalid_encoding,"ok"+full.substr(0,n),2);eof(h.c->read_char());eof(h.c->read_char());
    }
    {Harness h;h.adapter->add("\xe2\x82\xc2\xa2");error(h.c->read_char(),ErrorKind::invalid_encoding,"\xe2\x82",0);text(h.c->read_char(),"\xc2\xa2",2);}
    {Harness h;h.adapter->add("\xff\x80");h.adapter->no_wait=true;error(h.c->read_char(),ErrorKind::invalid_encoding,"\xff\x80",0);}
    {Harness h;h.adapter->add(std::string("\xc2\0",2));error(h.c->read_char(),ErrorKind::invalid_encoding,"\xc2",0);text(h.c->read_char(),std::string(1,'\0'),1);}
    {Harness h;h.adapter->add("\xff");h.adapter->add("",{ErrorKind::io_failure,5});h.adapter->add("Z");error(h.c->read_char(),ErrorKind::invalid_encoding,"\xff",0);auto r=h.c->read_char();error(r,ErrorKind::io_failure,"",0);check(r.transfer.error.os_code==5,"queued native error lost after encoding failure");text(h.c->read_char(),"Z",1);}
}
void lines(){
    for(unsigned next=0;next<4;++next)for(bool lf:{false,true}){
        Harness h;h.adapter->add("a\r");h.adapter->add(lf?"\nB\n":"B\n");
        text(h.c->read_line(),"a",2);check(h.adapter->reads==1,"CR waited for following native read");
        check(h.c->read_bytes(0).bytes.empty() && h.c->read_text(0).text.empty(),"zero read changed continuation");
        if(next==0)text(h.c->read_line(),"B",lf?3:2);
        else {
            std::string expected=lf?"\n":"B";
            if(next==1)check(h.c->read_bytes(1).bytes==bytes(expected),"raw continuation ownership");
            if(next==2)text(h.c->read_char(),expected,1);
            if(next==3)text(h.c->read_text(1),expected,1);
            text(h.c->read_line(),lf?"B":"",lf?2:1);
        }
        eof(h.c->read_line());
    }
    for(unsigned method=0;method<4;++method){
        Harness h;h.adapter->add("a\r");text(h.c->read_line(),"a",2);h.adapter->add("",{ErrorKind::would_block,11});
        if(method==0){auto result=h.c->read_bytes(1,0);check(result.transfer.error.kind==ErrorKind::would_block && result.bytes.empty(),"failed raw read consumed continuation");}
        else {auto result=method==1?h.c->read_char(0):method==2?h.c->read_text(1,0):h.c->read_line(0);error(result,ErrorKind::would_block,"",0);}
        h.adapter->add("\nB\n");text(h.c->read_line(),"B",3);
    }
    Harness h;h.adapter->add("\n\r\r\n\nfinal");text(h.c->read_line(),"",1);text(h.c->read_line(),"",1);text(h.c->read_line(),"",1);text(h.c->read_line(),"",2);text(h.c->read_line(),"final",5);eof(h.c->read_line());
    Harness shared;shared.adapter->add("a\r\nB\n");text(shared.c->read_line(),"a",2);shared.runtime.close();
    vd::Runtime other(0,shared.streams);auto next=other.console_context();text(next->read_line(),"B",3);eof(next->read_line());
    check(shared.c->read_line().transfer.error.kind==ErrorKind::closed,"closed session consumed buffered input");
}
void progress(){
    {Harness h;h.adapter->add("AZ",{ErrorKind::io_failure,5});text(h.c->read_char(),"A",1);auto raw=h.c->read_bytes(1);check(raw.bytes==bytes("Z") && raw.transfer.error.kind==ErrorKind::io_failure && raw.transfer.error.os_code==5,"native error lost between typed/raw reads");eof(h.c->read_char());}
    {Harness h;h.adapter->add("",{ErrorKind::invalid_encoding,111});auto r=h.c->read_char();error(r,ErrorKind::invalid_encoding,"",0);check(r.transfer.error.os_code==111,"native encoding error code lost");h.adapter->add("Z");text(h.c->read_char(),"Z",1);}
    {Harness h;h.adapter->add("part");h.adapter->add("",{ErrorKind::timed_out,123});error(h.c->read_line(),ErrorKind::timed_out,"part",4);h.adapter->add("next\n");text(h.c->read_line(),"next",5);}
    {Harness h;h.adapter->add("\xe2");h.adapter->add("",{ErrorKind::timed_out,123});auto r=h.c->read_char();error(r,ErrorKind::timed_out,"\xe2",1);check(r.transfer.error.os_code==123,"timeout lost native code");h.adapter->add("\x82\xacZ");check(h.c->read_bytes(2).bytes==bytes("\x82\xac"),"partial scalar secretly retained after error");text(h.c->read_char(),"Z",1);}
    {Harness h;h.adapter->add("ok",{ErrorKind::io_failure,5,22});auto r=h.c->read_line();error(r,ErrorKind::io_failure,"ok",2);check(r.transfer.error.os_code==5 && r.transfer.error.cleanup_code==22,"native cleanup error lost");h.adapter->add("x\n");text(h.c->read_line(),"x",2);}
    {Harness h;h.adapter->add("A\xe2\x82");h.adapter->no_wait=true;text(h.c->read_text(10),"A",1);check(h.adapter->reads==1,"available text tried to fill/incomplete scalar waited");h.adapter->add("\xacZ");text(h.c->read_char(),"\xe2\x82\xac",3);text(h.c->read_char(),"Z",1);}
    {Harness h;h.adapter->add("A\xe2\x82",{ErrorKind::io_failure,5});error(h.c->read_text(10),ErrorKind::io_failure,"A\xe2\x82",3);}
    {Harness h;h.adapter->add("a",{},4);h.adapter->add("b",{},4);h.adapter->add("c",{},4);auto r=h.c->read_line(10);error(r,ErrorKind::timed_out,"abc",3);check(h.adapter->requests.size()==3,"line deadline renewed");for(const auto& req:h.adapter->requests)check(req.deadline==h.adapter->requests.front().deadline,"different per-byte deadlines");}
    {Harness h;h.adapter->add("a\n");h.adapter->action=[&]{h.c->begin_close();};text(h.c->read_line(),"a",2);}
    {Harness h;h.adapter->add("ab");h.adapter->action=[&]{h.c->begin_close();};error(h.c->read_line(),ErrorKind::closed,"ab",2);h.adapter->action={};vd::Runtime other(0,h.streams);h.adapter->add("\nx\n");text(other.console_context()->read_line(),"",1);text(other.console_context()->read_line(),"x",2);}
}
void concurrent(){
    Harness h;std::string input;for(unsigned n=0;n<128;++n)input+="\xc3\xa9";h.adapter->add(input);h.adapter->no_wait=true;
    vd::Runtime other(0,h.streams);auto c=other.console_context();std::barrier start(3);
    auto first=std::async(std::launch::async,[&]{start.arrive_and_wait();for(unsigned n=0;n<64;++n)text(h.c->read_char(),"\xc3\xa9",2);});
    auto second=std::async(std::launch::async,[&]{start.arrive_and_wait();for(unsigned n=0;n<64;++n)text(c->read_text(1),"\xc3\xa9",2);});
    start.arrive_and_wait();first.get();second.get();check(h.adapter->reads==1,"concurrent typed readers split ownership/native reads");
    vd::Runtime::Attachment a(h.runtime);vd::Runtime::Managed m(h.runtime);
    for(unsigned method=0;method<3;++method){bool rejected=false;try{if(method==0)h.c->read_char();else if(method==1)h.c->read_text(1);else h.c->read_line();}catch(const std::logic_error&){rejected=true;}check(rejected,"Managed shared text read accepted");}
}
void allocation(){
    for(unsigned mode=0;mode<4;++mode){
        Harness h;std::string prefix(mode==2?4096:1024,'R');
        if(mode==0)h.adapter->add(prefix+"\r\nTAIL\n");
        if(mode==1)h.adapter->add(prefix+"\xff\n");
        if(mode==2){h.adapter->add(prefix);h.adapter->add("\n");}
        if(mode==3)h.adapter->add(prefix,{ErrorKind::timed_out,77});
        h.adapter->fail_result=true;bool failed=false;
        try{h.c->read_line();}catch(const std::bad_alloc&){failed=true;}
        fail_one_allocation=false;check(failed && h.c->snapshot().admitted==0,"failed result lost input lease/allocation channel");
        if(mode==1){error(h.c->read_line(),ErrorKind::invalid_encoding,prefix+"\xff",prefix.size());text(h.c->read_line(),"",1);}
        else {if(mode==3)h.adapter->add("\n");text(h.c->read_line(),prefix,prefix.size()+1);if(mode==0)text(h.c->read_line(),"TAIL",6);}
    }
    Harness h;h.adapter->add("01234567890123456789012345678901");h.adapter->fail_result=true;bool failed=false;
    try{h.c->read_bytes(32);}catch(const std::bad_alloc&){failed=true;}check(failed,"raw result failure injection missed");
    check(h.c->read_bytes(32).bytes==bytes("01234567890123456789012345678901") && h.adapter->reads==1,"raw allocation failure lost/reread bytes");
}
#ifndef _WIN32
void frame(Context& c,char kind,std::span<const std::uint8_t> data){
    Bytes record{static_cast<std::uint8_t>(kind)};auto length=static_cast<std::uint64_t>(data.size());for(unsigned n=0;n<8;++n)record.push_back(static_cast<std::uint8_t>(length>>(8*n)));record.insert(record.end(),data.begin(),data.end());auto result=c.write(Stream::output,std::move(record));check(!result.error,"native input report write");
}
void frame(Context& c,const TextResult& result){
    if(result.transfer.error){check(result.text.empty(),"invalid text returned");frame(c,'E',result.partial);}
    else frame(c,result.transfer.eof?'F':'T',{reinterpret_cast<const std::uint8_t*>(result.text.data()),result.text.size()});
}
void report(const char* path){std::ofstream out(path);out<<checks<<'\n';check(bool(out),"input report");}
using console_native_test::flags;
using console_native_test::flags_unchanged;
void native_lines(const char* path,std::int64_t count,bool recover=false){
    auto before=flags();vd::Runtime runtime;auto c=runtime.console_context();
    for(std::int64_t n=0;count<0 || n<count;++n){auto r=c->read_line();check(!r.transfer.error || (recover && r.transfer.error.kind==ErrorKind::invalid_encoding),"native line error");frame(*c,r);if(r.transfer.eof)break;}
    runtime.close();check(flags_unchanged(before,path),"input changed borrowed descriptors");report(path);
}
void live(const char* path,std::string method,bool timeout,bool character){
    vd::Runtime runtime;auto c=runtime.console_context();
    if(timeout){auto prefix=c->read_char();check(prefix.text=="@","timeout fixture prefix missing");frame(*c,'R',{});}
    auto first=timeout?(character?c->read_char(10):c->read_line(10)):c->read_line();
    check(timeout?first.transfer.error.kind==ErrorKind::timed_out:!first.transfer.error,"live initial read");frame(*c,first);
    if(method=="bytes"){auto r=c->read_bytes(character?2:1);check(!r.transfer.error,"live raw error");frame(*c,'B',r.bytes);}
    else frame(*c,method=="char"?c->read_char():method=="text"?c->read_text(1):c->read_line());
    if(method!="line" && !timeout)frame(*c,c->read_line());
    report(path);
}
#ifdef __linux__
bool in_poll(long tid){std::ifstream in("/proc/self/task/"+std::to_string(tid)+"/syscall");long call=-1;in>>call;return in && call==SYS_poll;}
#endif
void blocked(const char* path){
    vd::Runtime runtime;auto c=runtime.console_context();frame(*c,c->read_line());std::atomic<long> tid=0;TextResult result;
    auto call=std::async(std::launch::async,[&]{vd::Runtime::Attachment a(runtime);vd::Runtime::Managed m(runtime);void* root=nullptr;std::size_t off=0;vd::Runtime::Slot slot{&root,sizeof(root),&off,1};vd::Runtime::Frame f(runtime,&slot,1);runtime.allocate_into(&root,{8,{}});{vd::Runtime::Native safe(runtime);
#ifdef __linux__
        tid=syscall(SYS_gettid);
#endif
        result=c->read_line();}if(!runtime.contains(root))throw std::runtime_error("queue wait lost root");});
    auto end=std::chrono::steady_clock::now()+10s;
    while(!c->snapshot().admitted
#ifdef __linux__
          || !in_poll(tid)
#endif
    ){if(std::chrono::steady_clock::now()>end)std::_Exit(3);std::this_thread::yield();}
    {vd::Runtime::Attachment a(runtime);vd::Runtime::Managed m(runtime);check(runtime.collect(5s)==vd::Runtime::Collection::completed && runtime.snapshot().objects==1,"buffered blocked input prevented GC");}
    runtime.close();call.get();check(result.transfer.error.kind==ErrorKind::closed,"line stop not observed");
    vd::Runtime next;auto other=next.console_context();frame(*other,result);auto available=other->read_char(0);check(available.transfer.error.kind==ErrorKind::would_block,"stopped session poisoned shared queue");report(path);
}
#endif
}
int main(int argc,char** argv){try{
#ifndef _WIN32
    if(argc>=3){std::string mode=argv[1];if(mode=="--lines" || mode=="--recover")native_lines(argv[2],argc>3?std::stoll(argv[3]):-1,mode=="--recover");else if(mode=="--blocked")blocked(argv[2]);else live(argv[2],argc>3?argv[3]:"line",mode=="--timeout-line" || mode=="--timeout-char",mode=="--timeout-char");return 0;}
#endif
    mixed();unicode();malformed();lines();progress();concurrent();allocation();std::cout<<checks<<" shared input/UTF8/line/progress/allocation checks passed\n";
}catch(const std::exception& e){fail_one_allocation=false;std::cerr<<e.what()<<'\n';return 1;}}
