#include "../../../src/runtime/network.hpp"
#include "void/native_work.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#ifndef _WIN32
#include <fcntl.h>
#include <filesystem>
#endif

using namespace std::chrono_literals;
using namespace vd::net;
using namespace vd::host;
namespace {
void check(bool value,const char* what) {
    if(!value)throw std::runtime_error(what);
}
struct Pending {
    explicit Pending(unsigned operation):request(std::make_shared<Request>(operation)){}
    std::shared_ptr<Request> request;
    std::shared_ptr<vd::NativeWake> wake=std::make_shared<vd::NativeWake>();
    vd::NativeCompletion completion=vd::NativeCompletion::pending(wake);
    Request& operator*(){return *request;}
    Request* operator->(){return request.get();}
    void start(Service& service,std::size_t bytes=0) {
        check(service.prepare(*request,bytes),"native request admission");
        service.submit(request,completion);
    }
    const Reply& finish() {
        std::unique_lock lock(wake->mutex);
        check(wake->changed.wait_for(lock,5s,[&]{return completion.ready();}),"native request did not retire");
        return request->reply;
    }
};
void success(const Reply& reply,const char* operation) {
    if(reply.error.kind!=Kind::ok) {
        std::cerr<<operation<<" kind "<<static_cast<int>(reply.error.kind)<<" phase "<<static_cast<int>(reply.error.phase)
                 <<" native "<<reply.error.code<<" bytes "<<reply.number<<'\n';
        throw std::runtime_error(operation);
    }
}
std::size_t handles() {
#ifdef _WIN32
    DWORD count=0;
    check(GetProcessHandleCount(GetCurrentProcess(),&count)!=0,"Windows handle count");
    return count;
#else
    return static_cast<std::size_t>(std::distance(std::filesystem::directory_iterator("/proc/self/fd"),
                                                 std::filesystem::directory_iterator{}));
#endif
}
void no_inherit(const Socket& socket) {
#ifdef _WIN32
    DWORD flags=0;
    check(GetHandleInformation(reinterpret_cast<HANDLE>(socket.descriptor),&flags)!=0 &&
          !(flags&HANDLE_FLAG_INHERIT),"inheritable Winsock descriptor");
#else
    check((fcntl(socket.descriptor,F_GETFD)&FD_CLOEXEC)!=0,"inheritable socket");
#endif
}
std::shared_ptr<Socket> bind(Service& service,unsigned op,const char* ip) {
    Pending request(op);
    request->endpoint=parse_ip(ip);
    request->backlog=32;
    request.start(service);
    success(request.finish(),"native bind");
    check(request->created->local.port()!=0,"ephemeral port");
    no_inherit(*request->created);
    return request->created;
}
Pending lookup(Service& service,const char* name,int family=0,std::chrono::milliseconds timeout=2s) {
    Pending result(net_resolve);
    result->name=name;
    result->family=family;
    result->port=71;
    result->deadline=Clock::now()+timeout;
    result.start(service,32*65);
    return result;
}
void resolver(Service& service) {
    auto slow=lookup(service,"stall.test.",4);
    auto good=lookup(service,"good.test.",4);
    success(good.finish(),"controlled DNS A");
    check(good->reply.text=="127.0.0.1","DNS A answer");
    service.cancel(*slow);
    check(slow.finish().error.kind==Kind::closed,"DNS cancelled");
    auto v6=lookup(service,"good.test.",6);
    success(v6.finish(),"controlled DNS AAAA");
    check(v6->reply.text=="::1","DNS AAAA answer");
    auto missing=lookup(service,"nx.test.",4);
    check(missing.finish().error.kind==Kind::name_not_found,"DNS NXDOMAIN");
    auto nodata=lookup(service,"nodata.test.",4);
    check(nodata.finish().error.kind==Kind::name_not_found,"DNS no data");
    auto timeout=lookup(service,"stall.test.",4,60ms);
    check(timeout.finish().error.kind==Kind::timed_out,"DNS deadline");
    auto many=lookup(service,"fanout.test.");
    check(many.finish().error.kind==Kind::limit_exceeded,"DNS bounded distinct addresses");
}
struct Connection {
    std::shared_ptr<Socket> listener, client, peer;
};
Connection connect_pair(Service& service,const char* ip,bool by_name=false) {
    auto listener=bind(service,net_listen,ip);
    Pending accept(net_accept);
    accept->socket=listener;
    accept.start(service);
    Pending connect(net_connect);
    connect->deadline=Clock::now()+2s;
    if(by_name) {
        connect->hostname=true;connect->name="fallback.test.";connect->family=4;
        connect->port=listener->local.port();connect->attempt=200ms;
    } else connect->endpoint=listener->local;
    connect.start(service);
    success(connect.finish(),"native connect");
    success(accept.finish(),"native accept");
    if(by_name)check(connect->reply.number==2,"hostname candidate fallback");
    no_inherit(*connect->created);
    no_inherit(*accept->created);
    return {listener,connect->created,accept->created};
}
void tcp_controls(Service& service) {
    auto pair=connect_pair(service,"127.0.0.1",true);
    Pending awaiting(net_accept);
    awaiting->socket=pair.listener;awaiting.start(service);
    Pending same_direction(net_accept);
    same_direction->socket=pair.listener;
    check(!service.prepare(*same_direction,0) && same_direction->reply.error.kind==Kind::busy,"one accept reservation");
    check(service.close(pair.listener)==0,"close listener");
    check(awaiting.finish().error.kind==Kind::closed,"pending accept wakes on close");

    const int size=8192;
    check(setsockopt(pair.client->descriptor,SOL_SOCKET,SO_SNDBUF,reinterpret_cast<const char*>(&size),sizeof(size))==0 &&
          setsockopt(pair.peer->descriptor,SOL_SOCKET,SO_RCVBUF,reinterpret_cast<const char*>(&size),sizeof(size))==0,
          "deterministic backpressure socket buffers");
    Pending stalled(net_write);
    stalled->socket=pair.client;
    stalled->input.assign(max_tcp,'x');
    stalled->deadline=Clock::now()+180ms;
    stalled.start(service,max_tcp);
    const auto& partial=stalled.finish();
    check(partial.error.kind==Kind::timed_out && partial.number>0 && partial.number<static_cast<std::int64_t>(max_tcp),
          "write deadline retains exact positive prefix");
    Pending data(net_read);
    data->socket=pair.peer;data->maximum=1024;data->deadline=Clock::now()+2s;
    data.start(service,1024);
    success(data.finish(),"read remains usable after write timeout");
    check(data->reply.bytes==std::string(1024,'x'),"peer received accepted prefix");
    check(service.close(pair.client)==0 && service.close(pair.peer)==0,"close backpressure connection");

    auto former=bind(service,net_listen,"127.0.0.1");
    check(service.close(former)==0,"close listener before refusal control");
    Pending refused(net_connect);
    // Winsock on the qualified Windows 10 host reports the loopback RST
    // just after two seconds. Leave room to observe the native refusal.
    refused->endpoint=former->local;refused->deadline=Clock::now()+4s;
    refused.start(service);
    const auto& refusal=refused.finish();
    if(refusal.error.kind!=Kind::connection_refused) {
        std::cerr<<"refusal got kind "<<static_cast<int>(refusal.error.kind)<<" native "<<refusal.error.code
                 <<" endpoint "<<refusal.text<<'\n';
        throw std::runtime_error("numeric connection refusal");
    }

    pair=connect_pair(service,"127.0.0.1");
    linger abortive{1,0};
    check(setsockopt(pair.peer->descriptor,SOL_SOCKET,SO_LINGER,reinterpret_cast<const char*>(&abortive),sizeof(abortive))==0,
          "native reset fixture");
    Pending reset(net_read);
    reset->socket=pair.client;reset->maximum=8;reset->deadline=Clock::now()+2s;reset.start(service,8);
    check(service.close(pair.peer)==0,"abortive peer close");
    check(reset.finish().error.kind==Kind::connection_reset,"reset distinguished from EOF");
    service.close(pair.listener);

    pair=connect_pair(service,"::1");
    check(pair.client->remote.public_family()==6 && pair.peer->local.public_family()==6,"native IPv6 endpoints");
    service.close(pair.client);service.close(pair.peer);service.close(pair.listener);
}
void udp_controls(Service& service) {
    auto first=bind(service,net_bind,"127.0.0.1");
    auto second=bind(service,net_bind,"127.0.0.1");
    Pending send(net_send);
    send->socket=first;send->endpoint=second->local;send->input.assign(max_v4,'\0');
    send.start(service,max_v4);
    success(send.finish(),"maximum IPv4 datagram");
    Pending receive(net_receive);
    receive->socket=second;receive->maximum=max_v4;receive->deadline=Clock::now()+2s;
    receive.start(service,datagram_buffer);
    success(receive.finish(),"receive maximum IPv4 datagram");
    check(receive->reply.bytes==std::string(max_v4,'\0') && receive->reply.port==first->local.port(),"maximum UDP byte identity");
    service.close(first);service.close(second);
}
}
int main(int argc,char** argv) {
    try {
        check(argc==2,"local DNS oracle endpoint required");
        Service service(argv[1]);
        resolver(service);
        tcp_controls(service);
        udp_controls(service);
        const auto baseline=handles();
        for(int n=0;n<20;++n) {
            auto socket=bind(service,net_bind,"127.0.0.1");
            check(service.close(socket)==0,"repeated native UDP close");
        }
        check(handles()==baseline,"repeated socket handles returned to baseline");
        auto listener=bind(service,net_listen,"127.0.0.1");
        auto socket=bind(service,net_bind,"127.0.0.1");
        Pending accept(net_accept),receive(net_receive);
        accept->socket=listener;receive->socket=socket;receive->maximum=4;
        accept.start(service);receive.start(service,datagram_buffer);
        service.shutdown();
        check(accept.finish().error.kind==Kind::closed && receive.finish().error.kind==Kind::closed &&
              listener->closed && socket->closed,"shutdown drains pending native sockets");
        std::cout<<"Native TCP, UDP, DNS, descriptor and shutdown controls passed\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
