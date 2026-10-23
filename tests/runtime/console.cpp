#include "void/console.hpp"
#include "void/runtime.hpp"
#include "void/callback.hpp"
#include <atomic>
#include <algorithm>
#include <sstream>
#include <functional>
#include <barrier>
#include <fstream>
#include <future>
#include <iostream>
#include <optional>
#include <type_traits>
#include <semaphore>
#include <vector>
#ifndef _WIN32
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif
using namespace vd::console;
using namespace std::chrono_literals;
namespace {
std::atomic<unsigned> checks{0};
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);++checks;}
template<class F> void rejected(F&& f){bool failed=false;try{f();}catch(const std::logic_error&){failed=true;}check(failed,"invalid console ownership/state accepted");}
template<class F> void closed(F&& f){bool failed=false;try{f();}catch(const Closed&){failed=true;}check(failed,"closed context admitted an operation");}
template<class F> void until(F&& f){auto end=std::chrono::steady_clock::now()+10s;while(!f()){if(std::chrono::steady_clock::now()>end)throw std::runtime_error("console synchronization deadline");std::this_thread::yield();}}
struct Tracking {std::atomic<unsigned> destroyed{0};std::atomic<bool> managed_cleanup{false};};
struct Fake final:Adapter {
    std::shared_ptr<Tracking> tracking;
    std::array<Endpoint,3> endpoints{{{Stream::input,Kind::terminal},{Stream::output,Kind::pipe},{Stream::error,Kind::file}}};
    std::vector<unsigned char> native_buffer{1,2,3,4};
    std::atomic<std::int64_t> tick{7};
    explicit Fake(std::shared_ptr<Tracking> t=std::make_shared<Tracking>()):tracking(std::move(t)){}
    ~Fake(){if(auto* runtime=vd::Runtime::attached();runtime && runtime->is_managed())tracking->managed_cleanup=true;++tracking->destroyed;}
    Transfer read(std::span<std::uint8_t>,const Request&) override {throw std::logic_error("unexpected foundation read");}
    Transfer write(Stream,std::span<const std::uint8_t>,const Request&) override {throw std::logic_error("unexpected foundation write");}
    std::array<Endpoint,3> inspect() const override {return endpoints;}
    std::chrono::steady_clock::time_point now() const noexcept override {return std::chrono::steady_clock::time_point(std::chrono::nanoseconds(tick.load()));}
};

// Shared native state deliberately combines the existing record/input/mode gates.
// The one-byte writes expose any accidental gate release between native chunks.
struct Integrated:Adapter {
    std::string output,pending="é\r\nZ\n";std::size_t at=0;
    std::atomic<unsigned> mode{7},restored{0};
    std::function<void(const Request&)> entering;
    std::array<Endpoint,3> inspect()const override{return {{{Stream::input,Kind::terminal},{Stream::output,Kind::terminal},{Stream::error,Kind::terminal}}};}
    auto now()const noexcept->std::chrono::steady_clock::time_point override{return std::chrono::steady_clock::now();}
    Error save_mode(InputMode& saved)override{saved.native[0]=mode;return {};}
    Error apply_mode(const InputMode&,ModeOptions options)override{mode=(options.echo?1:0)|(options.line_input?2:0)|(options.control_c_as_input?0:4);return {};}
    Error restore_mode(const InputMode& saved)override{mode=saved.native[0];++restored;return {};}
    TerminalResult present(Stream,Presentation p,std::int64_t,std::int64_t,std::string_view text,const Request&)override{TerminalResult r;if(p==Presentation::cursor_position){r.transfer.error={ErrorKind::unsupported};return r;}r.sequence.assign(text.begin(),text.end());return r;}
    Transfer read(std::span<std::uint8_t> bytes,const Request& request)override{if(entering)entering(request);auto n=std::min(bytes.size(),pending.size()-at);std::copy_n(pending.begin()+at,n,bytes.begin());at+=n;return {n,!n};}
    Transfer write(Stream,std::span<const std::uint8_t> bytes,const Request&)override{if(bytes.empty())return {};output+=static_cast<char>(bytes[0]);std::this_thread::yield();return {1};}
};
void shared_records(){
    auto fake=std::make_shared<Integrated>();auto streams=Streams::isolated(fake);vd::Runtime first(1,streams),second(1,streams);auto a=first.console_context(),b=second.console_context();
    std::barrier start(9);std::vector<std::future<void>> jobs;std::vector<std::string> expected;
    for(unsigned n=0;n<8;++n){for(unsigned k=0;k<32;++k)expected.push_back(std::to_string(n)+":"+std::to_string(k)+":é🎮");jobs.push_back(std::async(std::launch::async,[&,n]{
        auto& runtime=n%2?first:second;auto c=n%2?a:b;vd::Runtime::Attachment attachment(runtime);vd::Runtime::Managed managed(runtime);start.arrive_and_wait();
        for(unsigned k=0;k<32;++k){auto text=std::to_string(n)+":"+std::to_string(k)+":é🎮\n";vd::Runtime::Native safe(runtime);Transfer result;
            if(n%3==0)result=c->present(n%2?Stream::output:Stream::error,Presentation::title,0,0,std::string_view(text).substr(0,text.size()-1)).transfer;
            else result=c->write(n%2?Stream::output:Stream::error,Bytes(text.begin(),text.end()));
            if(result.error)throw std::runtime_error("shared record failed");
        }
    }));}
    start.arrive_and_wait();for(auto& job:jobs)job.get();
    // Native writes retain each record. Separate titles have no LF, so use the
    // common supplementary UTF8 scalar as the exact record boundary.
    std::vector<std::string> actual;std::size_t at=0;
    while(at<fake->output.size()){auto end=fake->output.find("🎮",at);check(end!=std::string::npos,"shared record has corrupt UTF8 boundary");end+=std::string("🎮").size();actual.push_back(fake->output.substr(at,end-at));at=end;if(at<fake->output.size() && fake->output[at]=='\n')++at;}
    std::sort(actual.begin(),actual.end());std::sort(expected.begin(),expected.end());check(actual==expected,"stdout/stderr/presentation records interleaved or lost");
    check(!first.close() && !second.close(),"shared record Runtime cleanup failed");
}
void shared_modes_shutdown(){
    auto fake=std::make_shared<Integrated>();auto streams=Streams::isolated(fake);vd::Runtime first(1,streams),second(1,streams);auto a=first.console_context(),b=second.console_context();
    std::binary_semaphore entered(0),cancelled(0),release(0);std::atomic<bool> drained=false;
    fake->entering=[&](const Request& request){std::stop_callback stop(request.cancellation,[&]{cancelled.release();});entered.release();release.acquire();};
    auto reader=std::async(std::launch::async,[&]{vd::Runtime::Attachment attachment(second);vd::Runtime::Managed managed(second);
        void* root=nullptr;std::size_t offset=0;vd::Runtime::Slot slot{&root,sizeof(root),&offset,1};vd::Runtime::Frame frame(second,&slot,1);second.allocate_into(&root,{8,{}});
        KeyResult key;{vd::Runtime::Native safe(second);auto outer=b->begin_mode({false,false,true});check(!outer.error,"shared mode admission failed");auto inner=b->begin_mode({false,false,false});check(!inner.error,"nested shared mode failed");key=b->read_key();}
        check(second.contains(root),"shutdown/GC lost admitted reader root");return key;
    });entered.acquire();check(fake->mode==4,"temporary key mode lost enclosing interrupt policy");
    check(a->read_line(0).transfer.error.kind==ErrorKind::would_block,"foreign line reader stole scoped input");
    check(a->key_available().transfer.error.kind==ErrorKind::would_block,"foreign availability stole scoped input");
    check(a->begin_mode().error.kind==ErrorKind::would_block,"foreign mode owner accepted");
    check(a->present(Stream::output,Presentation::cursor_position).transfer.error.kind==ErrorKind::would_block && fake->output.empty(),"foreign cursor query emitted bytes");
    check(!a->write(Stream::error,Bytes{'o','k'}).error,"owned key input unnecessarily blocked output");
    {vd::Runtime::Attachment attachment(second);vd::Runtime::Managed managed(second);check(second.collect(5s)==vd::Runtime::Collection::completed && second.snapshot().objects==1,"shared-mode native wait blocked GC");}
    check(!first.close() && fake->mode==4,"closing other Runtime restored an active owner's modes");
    std::future<Error> shutdown;
    {vd::Runtime::Attachment attachment(second);vd::Runtime::Managed managed(second);
        {vd::Runtime::Native safe(second);shutdown=std::async(std::launch::async,[&]{auto error=second.close();drained=true;return error;});cancelled.acquire();}
        // Already-admitted source participants may resume/collect while close is
        // draining NativeSafe I/O; a fresh host attachment is correctly rejected.
        check(second.collect(5s)==vd::Runtime::Collection::completed && second.snapshot().objects==1,"GC while closing lost frozen key input roots");
    }
    check(!drained && b->snapshot().admitted==1 && fake->mode==4,"shutdown discarded entered key state");
    check(b->write(Stream::output,Bytes{'x'},0).error.kind==ErrorKind::closed,"closing Runtime admitted output");release.release();
    auto key=reader.get();auto error=shutdown.get();check(!error && !key.transfer.error && key.character=="é" && fake->mode==7 && fake->restored==3,"entered key completion/nested shutdown restoration changed");
    fake->entering={};vd::Runtime later(1,streams);auto c=later.console_context();check(c->read_line().text.empty() && c->read_line().text=="Z","shared queued CRLF bytes lost across Runtime shutdown");check(fake->output=="ok","shutdown emitted unexpected bytes");
}
void identity(){
    vd::Runtime first,second;auto a=first.console_context(),b=second.console_context();
    check(a!=b && a->streams()==b->streams() && a->streams()==Streams::process(),"process contexts duplicate input ownership");
    for(auto stream:{Stream::input,Stream::output,Stream::error})check(a->endpoint(stream).stream==stream,"standard endpoint identities collapsed");
    auto process=a->streams();first.close();closed([&]{a->acquire(Stream::input);});rejected([&]{first.console_context();});
    {auto op=b->acquire(Stream::input);check(!op.cancelled(),"closing one Runtime cancelled another");closed([&]{a->acquire(Stream::input);});}
    second.close();vd::Runtime later;check(later.console_context()->streams()==process,"process input context lost between Runtime lifetimes");
    auto fake=std::make_shared<Fake>();auto isolated=Streams::isolated(fake);vd::Runtime injected(0,isolated);auto c=injected.console_context();
    check(c->streams()!=process && !c->endpoint(Stream::input).redirected() && c->endpoint(Stream::output).redirected() && c->endpoint(Stream::error).redirected(),"isolated endpoint classification");
    {auto op=c->acquire(Stream::output);check(op.endpoint().stream==Stream::output && &op.adapter()==fake.get(),"adapter or endpoint binding changed");check(op.adapter().now().time_since_epoch()==7ns,"injected clock ignored");fake->tick=19;check(op.adapter().now().time_since_epoch()==19ns,"injected clock not live");auto moved=std::move(op);rejected([&]{op.endpoint();});check(moved.endpoint().stream==Stream::output,"move lost operation lease");}
    {auto op=c->acquire(Stream::input);auto wrong=std::async(std::launch::async,[&]{rejected([&]{op.endpoint();});});wrong.get();}
    auto invalid=std::make_shared<Fake>();invalid->endpoints[1].stream=Stream::input;rejected([&]{Streams::isolated(invalid);});
    rejected([&]{Streams::isolated({});});rejected([&]{Context bad({});});rejected([&]{c->acquire(static_cast<Stream>(9));});
}
void lazy_admission(){
    vd::Runtime runtime;std::barrier start(9);std::vector<std::future<std::shared_ptr<Context>>> calls;
    for(unsigned n=0;n<8;++n)calls.push_back(std::async(std::launch::async,[&]{start.arrive_and_wait();return runtime.console_context();}));
    start.arrive_and_wait();auto winner=calls[0].get();for(unsigned n=1;n<calls.size();++n)check(calls[n].get()==winner,"concurrent lazy initialization published multiple Runtime contexts");
    {auto op=winner->acquire(Stream::input);check(!op.cancelled(),"discarded lazy candidate cancelled the installed context");}
    vd::Runtime closing;std::barrier race(9);std::array<std::shared_ptr<Context>,8> observed;std::vector<std::future<void>> racers;
    for(unsigned n=0;n<8;++n)racers.push_back(std::async(std::launch::async,[&,n]{race.arrive_and_wait();try{observed[n]=closing.console_context();}catch(const std::logic_error&){};}));
    race.arrive_and_wait();closing.close();for(auto& call:racers)call.get();bool stopped=true;for(const auto& context:observed)if(context)stopped&=context->snapshot().closing;check(stopped,"lazy admission escaped a closing Runtime");rejected([&]{closing.console_context();});
}
void gates(){
    auto state=Streams::isolated(std::make_shared<Fake>());vd::Runtime first(0,state),second(0,state);auto a=first.console_context(),b=second.console_context();
    {auto op=a->acquire(Stream::output);rejected([&]{first.close();});check(first.console_context()==a,"self-drain rejection closed Runtime admission");}
    for(auto streams:{std::pair{Stream::input,Stream::input},std::pair{Stream::output,Stream::error},std::pair{Stream::error,Stream::output}}){
        std::atomic<unsigned> inside=0,peak=0;std::barrier start(3);
        auto writer=[&](std::shared_ptr<Context> c,Stream stream){start.arrive_and_wait();for(unsigned n=0;n<128;++n){auto op=c->acquire(stream);auto now=++inside;auto old=peak.load();while(old<now && !peak.compare_exchange_weak(old,now)){}std::this_thread::yield();--inside;}};
        auto one=std::async(std::launch::async,writer,a,streams.first),two=std::async(std::launch::async,writer,b,streams.second);start.arrive_and_wait();one.get();two.get();check(peak==1 && inside==0,"shared input or stdout/stderr gate split");
    }
    {auto in=a->acquire(Stream::input);auto out=a->acquire(Stream::output);rejected([&]{b->acquire(Stream::error);});rejected([&]{a->close();});}
    {auto out=a->acquire(Stream::error);rejected([&]{a->acquire(Stream::input);});}
    auto separate=std::make_shared<Context>(Streams::isolated(std::make_shared<Fake>()));
    {auto input=a->acquire(Stream::input);auto independent=separate->acquire(Stream::input);check(!independent.cancelled(),"isolated context inherited process gate");}
}
void closing_and_lifetime(){
    auto tracking=std::make_shared<Tracking>();auto adapter=std::make_shared<Fake>(tracking);std::weak_ptr<Fake> weak=adapter;
    auto state=Streams::isolated(adapter);auto runtime=std::make_unique<vd::Runtime>(0,state);auto context=runtime->console_context();
    state.reset();adapter.reset();std::binary_semaphore held(0),cancelled(0),release(0);
    auto operation=std::async(std::launch::async,[&]{auto op=context->acquire(Stream::input);std::stop_callback wake(op.cancellation(),[&]{cancelled.release();});held.release();release.acquire();check(op.cancelled() && static_cast<Fake&>(op.adapter()).native_buffer==std::vector<unsigned char>{1,2,3,4},"closing invalidated native operation/buffer");});
    held.acquire();std::atomic<bool> queued_rejected=false,done=false;
    auto queued=std::async(std::launch::async,[&]{closed([&]{context->acquire(Stream::input);});queued_rejected=true;});
    until([&]{return context->snapshot().admitted==2;});
    auto shutdown=std::async(std::launch::async,[&]{runtime.reset();done=true;});
    cancelled.acquire();check(context->snapshot().closing && !done && !weak.expired(),"Runtime teardown failed to cancel and drain");
    closed([&]{context->acquire(Stream::error);});queued.get();check(queued_rejected && context->snapshot().admitted==1,"queued admission not cancelled");
    release.release();operation.get();shutdown.get();check(done && context->snapshot().admitted==0,"shutdown did not drain");
    context->close();context->close();context.reset();check(weak.expired() && tracking->destroyed==1 && !tracking->managed_cleanup,"native adapter leaked or GC cleanup invoked");
    // A standalone Context can disappear before its lease. Native state survives.
    auto transient=std::make_unique<Context>(Streams::isolated(std::make_shared<Fake>()));auto op=transient->acquire(Stream::input);transient.reset();check(op.cancelled() && op.endpoint().stream==Stream::input,"lease lost state with Context destruction");
}
void concurrent_close(){
    static_assert(!std::is_copy_constructible_v<Operation> && !std::is_copy_constructible_v<Context>);
    vd::Runtime runtime(0,Streams::isolated(std::make_shared<Fake>()));auto context=runtime.console_context();
    std::optional<Operation> held(context->acquire(Stream::output));std::binary_semaphore cancelled(0);
    std::stop_callback wake(held->cancellation(),[&]{cancelled.release();});
    std::vector<std::future<void>> waiters;std::atomic<unsigned> refused=0;
    for(unsigned n=0;n<8;++n)waiters.push_back(std::async(std::launch::async,[&,n]{closed([&]{context->acquire(n%2?Stream::output:Stream::error);});++refused;}));
    until([&]{return context->snapshot().admitted==9;});
    std::atomic<unsigned> drained=0;auto one=std::async(std::launch::async,[&]{context->close();++drained;});auto two=std::async(std::launch::async,[&]{context->close();++drained;});
    cancelled.acquire();for(auto& waiter:waiters)waiter.get();check(refused==8 && drained==0 && context->snapshot().admitted==1,"concurrent close failed to reject queued writers and retain granted state");
    held.reset();one.get();two.get();check(drained==2 && context->snapshot().admitted==0,"concurrent close returned before drain or failed idempotence");
}
void independent_shutdown(){
    vd::Runtime first,second;auto a=first.console_context(),b=second.console_context();
    std::binary_semaphore held(0),release(0),cancelled(0);std::atomic<bool> b_cancelled=false;
    auto reader=std::async(std::launch::async,[&]{auto op=a->acquire(Stream::input);std::stop_callback wake(op.cancellation(),[&]{cancelled.release();});held.release();release.acquire();});held.acquire();
    auto waiter=std::async(std::launch::async,[&]{auto op=b->acquire(Stream::input);b_cancelled=op.cancelled();});until([&]{return b->snapshot().admitted==1;});
    auto close=std::async(std::launch::async,[&]{first.close();});cancelled.acquire();release.release();reader.get();close.get();waiter.get();check(!b_cancelled && !b->snapshot().closing,"Runtime shutdown cancelled another process reader");
}
void callback_shutdown(){
    struct Provider final:vd::CallbackProvider {
        std::future<bool> dispatch;bool called=false,result=false;
        void unregister_and_quiesce() noexcept override {called=true;try{result=dispatch.get();}catch(...){std::terminate();}}
    } provider;
    vd::Runtime runtime(0,Streams::isolated(std::make_shared<Fake>()));auto context=runtime.console_context();vd::CallbackRegistration registration(runtime,provider);
    std::binary_semaphore entered(0),wake(0);
    provider.dispatch=std::async(std::launch::async,[&]{return registration.run([&]{vd::Runtime::Native native(runtime);rejected([&]{runtime.close();});check(!context->snapshot().closing,"self-close rejection closed callback stream context");auto op=context->acquire(Stream::input);std::stop_callback cancel(op.cancellation(),[&]{wake.release();});entered.release();wake.acquire();check(op.cancelled(),"callback native wait not cancelled on Runtime close");});});
    entered.acquire();runtime.close();registration.close();check(provider.called && provider.result && runtime.snapshot().threads==0,"existing callback admission/drain failed with stream shutdown");check(!registration.run([]{}),"closing Runtime admitted a new callback");
}
void gc_and_cleanup(){
    auto tracking=std::make_shared<Tracking>();auto state=Streams::isolated(std::make_shared<Fake>(tracking));vd::Runtime runtime(0,state);auto context=runtime.console_context();
    std::binary_semaphore held(0),release(0);
    auto blocker=std::async(std::launch::async,[&]{vd::Runtime::Attachment a(runtime);vd::Runtime::Managed m(runtime);void* root=nullptr;std::size_t offset=0;vd::Runtime::Slot slot{&root,sizeof(root),&offset,1};vd::Runtime::Frame f(runtime,&slot,1);runtime.allocate_into(&root,{8,{}});{vd::Runtime::Native native(runtime);auto op=context->acquire(Stream::input);held.release();release.acquire();}check(runtime.contains(root),"NativeSafe operation lost frozen roots");});held.acquire();
    auto waiting=std::async(std::launch::async,[&]{vd::Runtime::Attachment a(runtime);vd::Runtime::Managed m(runtime);vd::Runtime::Native native(runtime);auto op=context->acquire(Stream::input);});until([&]{return context->snapshot().admitted==2;});
    {vd::Runtime::Attachment a(runtime);vd::Runtime::Managed m(runtime);check(runtime.console_context()==context,"Managed metadata access lost Runtime context");rejected([&]{context->acquire(Stream::output);});rejected([&]{context->snapshot();});rejected([&]{context->close();});check(runtime.collect(5s)==vd::Runtime::Collection::completed && runtime.snapshot().objects==1,"native stream lock obstructed GC");}
    release.release();blocker.get();waiting.get();{vd::Runtime::Attachment a(runtime);vd::Runtime::Managed m(runtime);check(runtime.collect()==vd::Runtime::Collection::completed && runtime.snapshot().objects==0,"console foundation retained managed state");}
    runtime.close();context.reset();state.reset();check(tracking->destroyed==0,"Runtime context released before Runtime member teardown");
    // Host shared native resources may be dropped during Managed code. Cleanup
    // itself must temporarily use NativeSafe, without a managed callback.
    {auto native=std::make_shared<Context>(Streams::isolated(std::make_shared<Fake>(tracking)));vd::Runtime other;vd::Runtime::Attachment a(other);vd::Runtime::Managed m(other);native.reset();check(!tracking->managed_cleanup,"native context cleanup ran Managed");}
}
#ifndef _WIN32
void probe(const char* path,bool payload){
    std::array<int,3> before{};for(int n=0;n<3;++n)before[n]=fcntl(n,F_GETFD);
    std::array<Endpoint,3> endpoints;errno=E2BIG;
    {vd::Runtime runtime;auto context=runtime.console_context();for(unsigned n=0;n<3;++n)endpoints[n]=context->endpoint(static_cast<Stream>(n));check(errno==E2BIG,"classification changed embedding errno");runtime.close();}
    for(int n=0;n<3;++n)check(fcntl(n,F_GETFD)==before[n],"borrowed standard descriptor closed or changed");
    if(payload){
        std::ofstream bytes(std::string(path)+".payload",std::ios::binary);char data[256];
        for(;;){auto count=read(STDIN_FILENO,data,sizeof(data));if(count<0 && errno==EINTR)continue;check(count>=0,"host payload read failed after Runtime destruction");if(!count)break;bytes.write(data,count);}check(bool(bytes),"host payload output");
    }
    std::ofstream output(path);for(const auto& endpoint:endpoints)output<<static_cast<int>(endpoint.stream)<<' '<<static_cast<int>(endpoint.kind)<<' '<<endpoint.redirected()<<' '<<endpoint.os_code<<'\n';check(bool(output),"endpoint probe output");
}
#endif
}
int main(int argc,char** argv){try{
#ifndef _WIN32
    if((argc==3 || argc==4) && std::string(argv[1])=="--probe"){probe(argv[2],argc==4);return 0;}
#endif
    shared_records();shared_modes_shutdown();identity();lazy_admission();gates();closing_and_lifetime();concurrent_close();independent_shutdown();callback_shutdown();gc_and_cleanup();std::cout<<checks<<" console foundation identity/lifecycle/GC checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
