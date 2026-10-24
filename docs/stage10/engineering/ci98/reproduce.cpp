// Deterministic scheduling evidence using the existing Runtime/Adapter contract.
#include "void/console.hpp"
#include "void/runtime.hpp"
#include <future>
#include <iostream>
#include <semaphore>
using namespace vd::console;
struct BeforeWrite:Adapter {
    std::binary_semaphore entered{0},release{0};
    std::array<Endpoint,3> inspect()const override{return {{{Stream::input,Kind::pipe},{Stream::output,Kind::pipe},{Stream::error,Kind::pipe}}};}
    std::chrono::steady_clock::time_point now()const noexcept override{return std::chrono::steady_clock::now();}
    Transfer read(std::span<std::uint8_t>,const Request&)override{return {0,false,{ErrorKind::unsupported}};}
    Transfer write(Stream,std::span<const std::uint8_t>,const Request& request)override{
        entered.release();release.acquire();return {0,false,{request.cancellation.stop_requested()?ErrorKind::closed:ErrorKind::io_failure}};
    }
};
int main(int argc,char**){
    if(argc>1){
        auto adapter=std::make_shared<BeforeWrite>();vd::Runtime runtime(0,Streams::isolated(adapter));auto c=runtime.console_context();
        auto work=std::async(std::launch::async,[&]{return c->write(Stream::output,Bytes(1024*1024,'W'));});adapter->entered.acquire();
        auto admitted=c->snapshot().admitted;runtime.console_context()->begin_close();adapter->release.release();auto result=work.get();runtime.close();
        std::cout<<"admitted="<<admitted<<" completed="<<result.completed<<" error="<<static_cast<unsigned>(result.error.kind)<<'\n';
        return admitted!=1 || result.completed!=0 || result.error.kind!=ErrorKind::closed;
    }
    vd::Runtime runtime;auto c=runtime.console_context();auto kind=c->endpoint(Stream::input).kind;
    auto rejected=c->present(Stream::output,Presentation::cursor_position);auto first=c->read_bytes(4096,0);
    std::cerr<<"READY\n"<<std::flush;auto later=c->read_bytes(4096,2000);
    std::cerr<<"input_kind="<<static_cast<unsigned>(kind)<<" query_error="<<static_cast<unsigned>(rejected.transfer.error.kind)
             <<" first_error="<<static_cast<unsigned>(first.transfer.error.kind)<<" first_bytes="<<first.bytes.size()<<" later=";
    for(auto b:later.bytes)std::cerr<<"0123456789abcdef"[b>>4]<<"0123456789abcdef"[b&15];std::cerr<<'\n';
    runtime.close();return first.transfer.error.kind!=ErrorKind::would_block || !first.bytes.empty() || later.transfer.error || later.bytes!=Bytes{'r','e','d','i','r','e','c','t','e','d',0,255};
}
