#include "void/detail/console_wide.hpp"
#include "void/runtime.hpp"
#include <atomic>
#include <codecvt>
#include <cstdlib>
#include <deque>
#include <functional>
#include <future>
#include <iostream>
#include <locale>
#include <new>
#include <semaphore>
using namespace vd::console;
using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;
thread_local bool fail_one_allocation=false;
void* operator new(std::size_t n){if(fail_one_allocation){fail_one_allocation=false;throw std::bad_alloc();}if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}
namespace {
std::atomic<unsigned> checks=0;
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);++checks;}
Bytes bytes(std::string_view s){return {s.begin(),s.end()};}
struct Unit {std::optional<char16_t> value;Error error{};bool eof=false;};
struct Script:Adapter {
    detail::WideConsole wide;
    std::deque<Unit> units;
    std::deque<Transfer> completions;
    std::u16string written;
    unsigned reads=0,writes=0;
    std::function<void()> read_action,write_action;
    Request last;
    bool fail_result=false;
    std::array<Endpoint,3> inspect() const override{return {{{Stream::input,Kind::terminal},{Stream::output,Kind::terminal},{Stream::error,Kind::terminal}}};}
    std::chrono::steady_clock::time_point now() const noexcept override{return std::chrono::steady_clock::now();}
    Transfer read(std::span<std::uint8_t> out,const Request& request) override {
        auto result=wide.read(out,request,[&](std::span<char16_t> u,const Request& req){
            check(u.size()==1,"wide input over-read");++reads;last=req;
            if(read_action)read_action();
            if(units.empty())return Transfer{0,true,{}};
            auto step=units.front();units.pop_front();if(step.value)u[0]=*step.value;
            return Transfer{step.value?1u:0u,step.eof,step.error};
        });
        if(fail_result){fail_result=false;fail_one_allocation=true;}
        return result;
    }
    Transfer write(Stream,std::span<const std::uint8_t> data,const Request& request) override {
        return detail::WideConsole::write(data,request,[&](std::span<const char16_t> u,const Request& req){
            check(!u.empty() && u.size()<=2048,"wide native count not bounded");
            for(std::size_t n=0;n<u.size();++n){
                auto c=u[n];if(c>=0xd800 && c<=0xdbff){check(n+1<u.size() && u[n+1]>=0xdc00 && u[n+1]<=0xdfff,"write divided surrogate pair");++n;}
                else check(c<0xdc00 || c>0xdfff,"isolated low surrogate submitted");
            }
            ++writes;last=req;if(write_action)write_action();
            Transfer part{u.size(),false,{}};
            if(!completions.empty()){part=completions.front();completions.pop_front();}
            if(part.completed<=u.size())written.append(u.data(),part.completed);
            return part;
        });
    }
    void add(std::u16string_view s){for(auto c:s)units.push_back({c,{},false});}
};
struct Harness {
    std::shared_ptr<Script> a=std::make_shared<Script>();
    std::shared_ptr<Streams> streams=Streams::isolated(a);
    vd::Runtime runtime{0,streams};std::shared_ptr<Context> c=runtime.console_context();
};
void text(const TextResult& r,std::string_view s){check(!r.transfer.error && !r.transfer.eof && r.text==s && r.transfer.completed==s.size(),"wide text/progress mismatch");}
std::string reference(std::u16string_view text){
    std::codecvt_utf8_utf16<char16_t> codec;std::mbstate_t state{};
    std::string result(text.size()*4,0);const char16_t* next=nullptr;char* end=nullptr;
    auto status=codec.out(state,text.data(),text.data()+text.size(),next,result.data(),result.data()+result.size(),end);
    check(status==std::codecvt_base::ok && next==text.data()+text.size(),"test oracle failed");result.resize(end-result.data());return result;
}
void mixed(){
    Harness h;h.a->add(u"Aé🎮\0\ufeff\r\nZ"sv);
    text(h.c->read_char(),"A");
    check(h.c->read_bytes(1).bytes==Bytes{0xc3},"raw UTF8 prefix mismatch");
    auto invalid=h.c->read_char();check(invalid.transfer.error.kind==ErrorKind::invalid_encoding && invalid.partial==Bytes{0xa9},"shared UTF8 decoder bypassed");
    text(h.c->read_char(),"🎮");text(h.c->read_char(),std::string(1,'\0'));text(h.c->read_char(),"\xef\xbb\xbf");
    auto before=h.a->reads;auto cr=h.c->read_line();check(!cr.transfer.error && cr.text.empty() && cr.transfer.completed==1,"wide CR line mismatch"); // CR, no LF lookahead.
    check(h.a->reads==before+1,"wide CR waited for following input");
    check(h.c->read_bytes(1).bytes==Bytes{'\n'},"wide mixed read silently discarded LF");text(h.c->read_char(),"Z");
    check(h.c->read_char().transfer.eof,"wide EOF missing");
    Harness line;line.a->add(u"\r\nZ\n");check(line.c->read_line().transfer.completed==1,"wide CR count");
    auto r=line.c->read_line();check(r.text=="Z" && r.transfer.completed==3,"wide CRLF continuation mismatch");
}
void unicode(){
    
    // Every BMP scalar, plus every high/low surrogate boundary family.
    for(unsigned u=0;u<=0xffff;++u){
        if(u>=0xd800 && u<=0xdfff)continue;
        detail::WideConsole wide;char16_t unit=static_cast<char16_t>(u);std::array<std::uint8_t,4> out{};unsigned calls=0;
        auto r=wide.read(out,{},[&](std::span<char16_t> data,const Request&){++calls;data[0]=unit;return Transfer{1,false,{}};});
        auto expected=reference({&unit,1});
        check(!r.error && r.completed==expected.size() && std::equal(expected.begin(),expected.end(),reinterpret_cast<const char*>(out.data())) && calls==1,"BMP input conversion differs from oracle");
    }
    for(unsigned hi:{0xd800u,0xd801u,0xdabcu,0xdbffu})for(unsigned lo=0xdc00;lo<=0xdfff;++lo){
        std::u16string source{static_cast<char16_t>(hi),static_cast<char16_t>(lo)};auto expected=reference(source);
        for(unsigned cap=1;cap<=4;++cap){
            detail::WideConsole wide;std::array<std::uint8_t,4> out{};std::string encoded;std::size_t at=0;
            auto read=[&](std::span<char16_t> data,const Request&){check(at<2,"pair reread native data");data[0]=source[at++];return Transfer{1,false,{}};};
            while(encoded.size()<4){auto r=wide.read(std::span(out).first(cap),{},read);check(!r.error && r.completed,"pair conversion stalled");encoded.append(reinterpret_cast<char*>(out.data()),r.completed);}
            check(encoded==expected && at==2,"split surrogate/input byte ownership differs");
        }
    }
    for(unsigned u=0xd800;u<=0xdfff;++u){
        Harness h;h.a->units.push_back({static_cast<char16_t>(u),{},false});h.a->add(u"Z");
        auto r=h.c->read_char();check(r.transfer.error.kind==ErrorKind::invalid_encoding && r.transfer.error.os_code==1113 && r.partial.empty(),"isolated surrogate accepted");text(h.c->read_char(),"Z");
    }
}
void errors(){
    Harness h;h.a->add(u"\xd83c");h.a->units.push_back({{}, {ErrorKind::timed_out,995},false});h.a->add(u"\xdfaeZ");
    auto r=h.c->read_char();check(r.transfer.error.kind==ErrorKind::timed_out && r.transfer.error.os_code==995,"high-surrogate timeout lost");text(h.c->read_char(),"🎮");text(h.c->read_char(),"Z");
    Harness eof;eof.a->add(u"\xd800");auto e=eof.c->read_char();check(e.transfer.error.kind==ErrorKind::invalid_encoding,"truncated wide EOF accepted");check(eof.c->read_char().transfer.eof,"truncated wide EOF order lost");eof.a->add(u"Q");text(eof.c->read_char(),"Q");
    Harness native;native.a->units.push_back({u'\xdfff',{ErrorKind::io_failure,123,77},false});native.a->add(u"R");
    check(native.c->read_char().transfer.error.kind==ErrorKind::invalid_encoding,"bad unit missing encoding event");
    auto n=native.c->read_char();check(n.transfer.error.os_code==123 && n.transfer.error.cleanup_code==77,"ordered original native error lost");text(native.c->read_char(),"R");
    Harness partial;partial.a->units.push_back({u'\xd83c',{},false});partial.a->units.push_back({u'\xdfae',{ErrorKind::io_failure,8,9},false});
    auto p=partial.c->read_char();check(p.partial==bytes("🎮") && p.transfer.completed==4 && p.transfer.error.os_code==8 && p.transfer.error.cleanup_code==9,"UTF16 native progress/error mismatch");
    Harness rawerror;rawerror.a->units.push_back({static_cast<char16_t>(0xd83c),{},false});rawerror.a->units.push_back({static_cast<char16_t>(0xdfae),{ErrorKind::timed_out,995},false});
    auto raw=rawerror.c->read_bytes(1);check(raw.bytes==Bytes{0xf0} && raw.transfer.error.kind==ErrorKind::timed_out && raw.transfer.error.os_code==995,"partial raw conversion deferred native control failure");
    rawerror.runtime.close();vd::Runtime fresh(0,rawerror.streams);auto rest=fresh.console_context()->read_bytes(3);check(rest.bytes==Bytes({0x9f,0x8e,0xae}) && !rest.transfer.error,"old control event poisoned retained native carry");
    Harness prefix;prefix.a->add(u"abc\xd800Z\n");auto malformed=prefix.c->read_line();
    check(malformed.partial==bytes("abc") && malformed.transfer.completed==3 && malformed.error_offset==3 && malformed.transfer.error.os_code==1113,"wide invalid-unit diagnostic offset/progress lost");
    auto recovery=prefix.c->read_line();check(recovery.text=="Z" && recovery.transfer.completed==2,"wide malformed unit swallowed recovery line");
    Harness closed;closed.a->add(u"\xd83c");closed.a->units.push_back({{}, {ErrorKind::closed,995},false});closed.a->add(u"\xdfae");
    closed.a->read_action=[&]{if(closed.a->reads==2)closed.c->begin_close();};
    check(closed.c->read_char().transfer.error.kind==ErrorKind::closed,"wide stopped high-surrogate request lost failure");closed.runtime.close();
    vd::Runtime next(0,closed.streams);text(next.console_context()->read_char(),"🎮");
    Harness highhigh;highhigh.a->add(u"\xd800\xd83c\xdfae");check(highhigh.c->read_char().transfer.error.kind==ErrorKind::invalid_encoding,"high-high accepted");text(highhigh.c->read_char(),"🎮");
}
void output(){
    const std::string input=std::string("Aé🎮\0",8)+"\xef\xbb\xbf";
    const std::u16string expected=u"Aé🎮\0\ufeff"s;
    Harness h;auto r=h.c->write(Stream::output,bytes(input));check(!r.error && r.completed==input.size() && h.a->written==expected,"wide output differs");
    for(std::size_t units:{0u,1u,2u,3u,4u,5u,6u}){
        Harness shorted;shorted.a->completions.push_back({units,false,{ErrorKind::io_failure,123,456}});
        auto s=shorted.c->write(Stream::error,bytes(input));
        const std::size_t expectedbytes[]={0,1,3,3,7,8,11};
        check(s.completed==expectedbytes[units] && s.error.os_code==123 && s.error.cleanup_code==456 && s.error.partial_utf16_units==(units==3),"wide native unit/UTF8 partial progress wrong");
        check(shorted.a->writes==1,"failed wide output retried");
    }
    Harness half;half.a->completions.push_back({3,false,{ErrorKind::interrupted,995}});
    auto bad=half.c->write(Stream::output,bytes(input));check(bad.completed==3 && bad.error.partial_utf16_units==1 && bad.error.os_code==995 && half.a->writes==1,"half scalar was automatically retried");
    Harness shorted;shorted.a->completions.push_back({2,false,{}});shorted.a->completions.push_back({2,false,{}});
    auto s=shorted.c->write(Stream::output,bytes(input));check(!s.error && s.completed==input.size() && shorted.a->written==expected && shorted.a->writes==3,"short writes duplicated output");
    Harness zero;zero.a->completions.push_back({0,false,{}});check(zero.c->write(Stream::output,bytes("x")).error.kind==ErrorKind::io_failure,"zero progress spin");
    Harness halfsuccess;halfsuccess.a->completions.push_back({1,false,{}});auto hs=halfsuccess.c->write(Stream::output,bytes("🎮"));check(hs.completed==0 && hs.error.partial_utf16_units==1 && hs.error.os_code==29,"half successful completion unreported");
    for(auto invalid:{std::string("\xff"),std::string("ok\xc0\x80"),std::string("\xed\xa0\x80"),std::string("\xf4\x90\x80\x80"),std::string("\xc3"),std::string("\x80"),std::string("\xe0\x80\x80")}){
        Harness badutf;auto b=badutf.c->write(Stream::output,bytes(invalid));check(b.error.kind==ErrorKind::unsupported && b.completed==0 && badutf.a->writes==0,"invalid console binary produced output");
    }
    Harness large;std::string source(2047,'x');source+="🎮";source+=std::string(4099,'y');source+="🎮";
    auto l=large.c->write(Stream::output,bytes(source));
    check(!l.error && l.completed==source.size() && reference(large.a->written)==source && large.a->writes==4,"native chunk boundary changed supplementary scalar");
}
void allocation(){
    Harness h;h.a->add(u"abcdefghijklmnopqrstuvwx"); // Force non-SSO result allocation.
    h.a->fail_result=true;bool failed=false;
    try{h.c->read_bytes(1);}catch(const std::bad_alloc&){failed=true;}fail_one_allocation=false;
    check(failed && h.a->reads==1 && h.c->snapshot().admitted==0,"wide read allocation fault not reached/safely released");
    text(h.c->read_char(),"a");check(h.a->reads==1,"native byte lost on result allocation failure");
    // Transcoding itself never allocates, even after entering native IO.
    detail::WideConsole w;std::array<std::uint8_t,1> out{};unsigned calls=0;
    auto read=[&](std::span<char16_t> u,const Request&){u[0]=calls++?0xdfae:0xd83c;fail_one_allocation=true;return Transfer{1,false,{}};};
    for(unsigned n=0;n<4;++n){auto r=w.read(out,{},read);fail_one_allocation=false;check(!r.error && r.completed==1,"transcoder allocated after native input");}
    bool entered=false;fail_one_allocation=true;
    auto r=detail::WideConsole::write(bytes(""),{},[&](std::span<const char16_t>,const Request&){entered=true;return Transfer{};});
    fail_one_allocation=false;check(!entered && !r.error,"empty wide output allocated or called native");
}
void contracts(){
    Harness h;h.a->add(u"🎮");text(h.c->read_char(1000),"🎮");
    check(h.a->last.deadline.has_value() && !h.a->last.polling,"wide input lost original deadline");
    auto request=h.a->last;Bytes source=bytes("abc🎮é");unsigned calls=0;
    auto result=detail::WideConsole::write(source,request,[&](std::span<const char16_t> u,const Request& req){
        check(req.deadline==request.deadline && req.cancellation==request.cancellation,"wide output replaced original request");++calls;
        fail_one_allocation=true;return Transfer{u.size(),false,{}};
    });
    bool no_allocation=fail_one_allocation;fail_one_allocation=false;
    check(no_allocation && calls==1 && !result.error && result.completed==source.size(),"wide output allocated after native completion");
    detail::WideConsole w;std::array<std::uint8_t,4> out{};bool rejected=false;
    try{w.read(out,{},[](std::span<char16_t>,const Request&){return Transfer{2,false,{}};});}catch(const std::logic_error&){rejected=true;}
    check(rejected,"invalid native input count accepted");
    for(auto bad:{Transfer{100,false,{}},Transfer{0,true,{}}}){
        rejected=false;try{detail::WideConsole::write(source,{},[&](std::span<const char16_t>,const Request&){return bad;});}catch(const std::logic_error&){rejected=true;}
        check(rejected,"invalid native output count/EOF accepted");
    }
    Harness gate;vd::Runtime other(0,gate.streams);auto c=other.console_context();
    std::string a(20000,'a'),b(20000,'b');a+="🎮";b+="é";
    auto one=std::async(std::launch::async,[&]{return gate.c->write(Stream::output,bytes(a));});
    auto two=std::async(std::launch::async,[&]{return c->write(Stream::error,bytes(b));});
    auto ra=one.get(),rb=two.get();auto actual=reference(gate.a->written);
    check(!ra.error && !rb.error && ra.completed==a.size() && rb.completed==b.size() && (actual==a+b || actual==b+a),"wide chunks escaped stdout/stderr record gate");
}
void lifetime(){
    Harness h;h.a->add(u"\xd83c\xdfae");
    std::binary_semaphore entered(0),cancelled(0),release(0);std::atomic<bool> done=false;
    h.a->read_action=[&]{if(h.a->reads==2){std::stop_callback notify(h.a->last.cancellation,[&]{cancelled.release();});entered.release();release.acquire();}};
    auto call=std::async(std::launch::async,[&]{vd::Runtime::Attachment a(h.runtime);vd::Runtime::Managed m(h.runtime);void* root=nullptr;std::size_t offset=0;vd::Runtime::Slot slot{&root,sizeof(root),&offset,1};vd::Runtime::Frame frame(h.runtime,&slot,1);h.runtime.allocate_into(&root,{8,{}});vd::Runtime::Native safe(h.runtime);auto r=h.c->read_char();done=true;return r;});
    entered.acquire();
    {vd::Runtime::Attachment a(h.runtime);vd::Runtime::Managed m(h.runtime);check(h.runtime.collect(5s)==vd::Runtime::Collection::completed && h.runtime.snapshot().objects==1,"wide native wait lost precise roots or blocked GC");}
    auto close=std::async(std::launch::async,[&]{h.c->close();});cancelled.acquire();
    check(!done && close.wait_for(10ms)==std::future_status::timeout,"entered native wait freed carry/storage early");
    release.release();text(call.get(),"🎮");close.get();
    check(h.c->snapshot().admitted==0,"wide close leaked lease");
    // An entered output completion wins cancellation, retaining conversion storage.
    Harness output;std::binary_semaphore wentered(0),wrelease(0);output.a->write_action=[&]{wentered.release();wrelease.acquire();};
    auto write=std::async(std::launch::async,[&]{return output.c->write(Stream::error,bytes("🎮"));});wentered.acquire();output.c->begin_close();wrelease.release();auto result=write.get();
    check(!result.error && result.completed==4 && output.a->written==u"🎮","cancelled entered wide write lost native storage/completion");output.c->close();
    Harness carry;carry.a->add(u"🎮");check(carry.c->read_bytes(1).bytes==Bytes{0xf0},"carry initial byte");carry.runtime.close();vd::Runtime later(0,carry.streams);auto tail=later.console_context()->read_bytes(3);check(tail.bytes==Bytes({0x9f,0x8e,0xae}),"carry not shared after Runtime close");
}
}
int main(){try{mixed();unicode();errors();output();allocation();contracts();lifetime();std::cout<<"console wide checks: "<<checks<<'\n';return 0;}catch(const std::exception& e){fail_one_allocation=false;std::cerr<<e.what()<<'\n';return 1;}}
