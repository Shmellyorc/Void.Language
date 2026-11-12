#include "network.hpp"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <limits>
#include <utility>
#ifndef _WIN32
#include <fcntl.h>
#include <netinet/tcp.h>
#include <unistd.h>
#endif

namespace vd::net {
namespace {
#ifdef _WIN32
constexpr Domain os_domain=Domain::winsock;
constexpr int bad_family=WSAEAFNOSUPPORT, oversized=WSAEMSGSIZE;
constexpr int send_flags=0, datagram_flags=0, send_shutdown=SD_SEND;
int last_error(){return WSAGetLastError();}
bool waiting(int error){return error==WSAEWOULDBLOCK || error==WSAEINPROGRESS || error==WSAEALREADY || error==WSAEINTR;}
int release(Descriptor fd){return closesocket(fd)==0?0:last_error();}
int poll_sockets(std::vector<Poll>& sockets,int timeout){return WSAPoll(sockets.data(),static_cast<ULONG>(sockets.size()),timeout);}
#else
constexpr Domain os_domain=Domain::posix;
constexpr int bad_family=EAFNOSUPPORT, oversized=EMSGSIZE;
#ifdef MSG_NOSIGNAL
constexpr int send_flags=MSG_NOSIGNAL;
#else
constexpr int send_flags=0;
#endif
constexpr int send_shutdown=SHUT_WR;
#ifdef __linux__
constexpr int datagram_flags=MSG_TRUNC;
#else
constexpr int datagram_flags=0;
#endif
int last_error(){return errno;}
bool waiting(int error){return error==EWOULDBLOCK || error==EAGAIN || error==EINPROGRESS || error==EALREADY || error==EINTR;}
int release(Descriptor fd){return ::close(fd)==0?0:last_error();}
int poll_sockets(std::vector<Poll>& sockets,int timeout){return ::poll(sockets.data(),sockets.size(),timeout);}
#endif
constexpr unsigned read_slot=1, write_slot=2, close_slot=4;
constexpr std::size_t request_capacity=max_operations+max_sockets;

Phase phase(unsigned op) {
    using namespace host;
    switch(op) {
    case net_resolve:return Phase::resolve;
    case net_connect:return Phase::connect;
    case net_listen:return Phase::listen;
    case net_accept:return Phase::accept;
    case net_bind:return Phase::bind;
    case net_receive:case net_read:return Phase::read;
    case net_send:case net_write:return Phase::write;
    case net_shutdown:return Phase::shutdown;
    case net_close_udp:case net_close_listener:case net_close_stream:return Phase::close;
    default:return Phase::prepare;
    }
}
unsigned direction(unsigned op) {
    using namespace host;
    switch(op) {
    case net_accept:case net_read:case net_receive:return read_slot;
    case net_write:case net_send:case net_shutdown:return write_slot;
    case net_close_udp:case net_close_stream:case net_close_listener:return close_slot;
    default:return 0;
    }
}
bool reserve_direction(Socket& socket,unsigned direction) {
    auto previous=socket.directions.load(std::memory_order_relaxed);
    do {
        if(previous&direction)return false;
    } while(!socket.directions.compare_exchange_weak(previous,previous|direction,std::memory_order_acq_rel));
    return true;
}
int option(Descriptor fd,int level,int name,int value) {
    return setsockopt(fd,level,name,reinterpret_cast<const char*>(&value),sizeof(value))==0?0:last_error();
}
int nonblocking(Descriptor fd) {
#ifdef _WIN32
    u_long enabled=1;
    if(ioctlsocket(fd,FIONBIO,&enabled)!=0)return last_error();
    if(!SetHandleInformation(reinterpret_cast<HANDLE>(fd),HANDLE_FLAG_INHERIT,0))
        return static_cast<int>(GetLastError());
    return 0;
#elif defined(__linux__)
    (void)fd;
    return 0;
#else
    return fcntl(fd,F_SETFD,FD_CLOEXEC)<0 || fcntl(fd,F_SETFL,O_NONBLOCK)<0?last_error():0;
#endif
}
Descriptor open_socket(int family,int type,int& error) {
#ifdef _WIN32
    auto fd=WSASocketW(family,type,0,nullptr,0,WSA_FLAG_NO_HANDLE_INHERIT);
#elif defined(__linux__)
    auto fd=::socket(family,type|SOCK_CLOEXEC|SOCK_NONBLOCK,0);
#else
    auto fd=::socket(family,type,0);
#endif
    if(fd==invalid_descriptor){error=last_error();return fd;}
    error=nonblocking(fd);
    if(!error && family==AF_INET6)error=option(fd,IPPROTO_IPV6,IPV6_V6ONLY,1);
#ifdef SO_NOSIGPIPE
    if(!error && type==SOCK_STREAM)error=option(fd,SOL_SOCKET,SO_NOSIGPIPE,1);
#endif
    if(error){(void)release(fd);return invalid_descriptor;}
    return fd;
}
bool valid_endpoint(const Endpoint& value) {
    return (value.family()==AF_INET && value.size==sizeof(sockaddr_in)) ||
        (value.family()==AF_INET6 && value.size==sizeof(sockaddr_in6) &&
         !IN6_IS_ADDR_V4MAPPED(&reinterpret_cast<const sockaddr_in6*>(&value.storage)->sin6_addr));
}
int name_of(Descriptor fd,Endpoint& endpoint,bool peer=false) {
    endpoint.size=sizeof(endpoint.storage);
    int result=peer?getpeername(fd,endpoint.data(),&endpoint.size):getsockname(fd,endpoint.data(),&endpoint.size);
    return result!=0?last_error():valid_endpoint(endpoint)?0:bad_family;
}
int stream_options(Socket& socket) {
    if(auto error=option(socket.descriptor,IPPROTO_TCP,TCP_NODELAY,socket.no_delay?1:0))return error;
#ifdef SO_NOSIGPIPE
    if(auto error=option(socket.descriptor,SOL_SOCKET,SO_NOSIGPIPE,1))return error;
#endif
    return name_of(socket.descriptor,socket.local);
}
int socket_error(Descriptor fd) {
    int error=0;
    AddressSize length=sizeof(error);
    return getsockopt(fd,SOL_SOCKET,SO_ERROR,reinterpret_cast<char*>(&error),&length)==0?error:last_error();
}
bool same_address(const Endpoint& left,const Endpoint& right) {
    if(left.family()!=right.family() || left.port()!=right.port() || left.scope()!=right.scope())return false;
    if(left.family()==AF_INET)
        return reinterpret_cast<const sockaddr_in*>(&left.storage)->sin_addr.s_addr==
               reinterpret_cast<const sockaddr_in*>(&right.storage)->sin_addr.s_addr;
    return std::memcmp(&reinterpret_cast<const sockaddr_in6*>(&left.storage)->sin6_addr,
                       &reinterpret_cast<const sockaddr_in6*>(&right.storage)->sin6_addr,sizeof(in6_addr))==0;
}
void watch_socket(void* owner,ares_socket_t fd,int readable,int writable) {
    auto& lookup=*static_cast<Lookup*>(owner);
    const auto events=static_cast<short>((readable?POLLIN:0)|(writable?POLLOUT:0));
    Lookup::Watch* free=nullptr;
    for(auto& watch:lookup.watches) {
        if(watch.fd==fd){watch=events?Lookup::Watch{fd,events}:Lookup::Watch{};return;}
        if(!free && watch.fd==invalid_descriptor)free=&watch;
    }
    if(events && free)*free={fd,events};
    else if(events)lookup.overflow=true;
}
void dns_complete(void* owner,int status,int,ares_addrinfo* addresses) {
    auto& lookup=*static_cast<Lookup*>(owner);
    lookup.finished=true;
    lookup.status=status;
    if(status==ARES_SUCCESS && addresses) {
        for(auto* node=addresses->nodes;node;node=node->ai_next) {
            if(node->ai_addrlen>sizeof(sockaddr_storage))continue;
            Endpoint candidate;
            candidate.size=static_cast<AddressSize>(node->ai_addrlen);
            std::memcpy(&candidate.storage,node->ai_addr,node->ai_addrlen);
            if(!valid_endpoint(candidate))continue;
            const auto seen=std::span(lookup.results).first(lookup.count);
            if(std::ranges::any_of(seen,[&](const Endpoint& value){return same_address(value,candidate);}))continue;
            if(lookup.count==lookup.results.size()){lookup.overflow=true;break;}
            lookup.results[lookup.count++]=candidate;
        }
    }
    if(addresses)ares_freeaddrinfo(addresses);
}
}
bool supported() {
#if (defined(__linux__) && defined(__x86_64__)) || (defined(_WIN32) && defined(_M_X64) && defined(_MSC_VER))
    return true;
#else
    return false;
#endif
}
Error os_error(Phase phase,int code) {
    Kind kind=Kind::io_failure;
    switch(code) {
#ifdef _WIN32
    case WSAEADDRINUSE:kind=Kind::address_in_use;break;
    case WSAEADDRNOTAVAIL:kind=Kind::address_not_available;break;
    case WSAEACCES:kind=Kind::permission_denied;break;
    case WSAECONNREFUSED:kind=Kind::connection_refused;break;
    case WSAECONNRESET:case WSAECONNABORTED:case WSAENETRESET:kind=Kind::connection_reset;break;
    case WSAENETUNREACH:kind=Kind::network_unreachable;break;
    case WSAEHOSTUNREACH:kind=Kind::host_unreachable;break;
    case WSAETIMEDOUT:kind=Kind::timed_out;break;
    case WSAENOTSOCK:kind=Kind::closed;break;
    case WSAESHUTDOWN:kind=Kind::write_closed;break;
    case WSAEMSGSIZE:kind=Kind::message_too_large;break;
    case WSAEAFNOSUPPORT:case WSAEPROTONOSUPPORT:case WSAEOPNOTSUPP:kind=Kind::unsupported;break;
#else
    case EADDRINUSE:kind=Kind::address_in_use;break;
    case EADDRNOTAVAIL:kind=Kind::address_not_available;break;
    case EACCES:case EPERM:kind=Kind::permission_denied;break;
    case ECONNREFUSED:kind=Kind::connection_refused;break;
    case ECONNRESET:case ECONNABORTED:case EPIPE:kind=Kind::connection_reset;break;
    case ENETUNREACH:kind=Kind::network_unreachable;break;
    case EHOSTUNREACH:kind=Kind::host_unreachable;break;
    case ETIMEDOUT:kind=Kind::timed_out;break;
    case EBADF:case ENOTSOCK:kind=Kind::closed;break;
    case ESHUTDOWN:kind=Kind::write_closed;break;
    case EMSGSIZE:kind=Kind::message_too_large;break;
    case EAFNOSUPPORT:case EPROTONOSUPPORT:case EOPNOTSUPP:kind=Kind::unsupported;break;
#endif
    default:break;
    }
    return {kind,phase,os_domain,code};
}
Error resolver_error(int code) {
    auto kind=Kind::resolve_failed;
    if(code==ARES_ENOTFOUND || code==ARES_ENODATA || code==ARES_ENONAME)kind=Kind::name_not_found;
    else if(code==ARES_ETIMEOUT)kind=Kind::timed_out;
    return {kind,Phase::resolve,Domain::cares,code};
}
bool close_operation(unsigned op) {return direction(op)==close_slot;}
bool data_operation(unsigned op) {return op==host::net_read || op==host::net_write;}
bool factory_operation(unsigned op) {
    return op==host::net_connect || op==host::net_listen || op==host::net_bind || op==host::net_accept;
}
Credit::~Credit(){release();}
bool Credit::take(std::atomic<std::size_t>& account,std::size_t count,std::size_t limit) {
    auto previous=account.load(std::memory_order_relaxed);
    do {
        if(count>limit-previous)return false;
    } while(!account.compare_exchange_weak(previous,previous+count,std::memory_order_relaxed));
    account_=&account;count_=count;
    return true;
}
void Credit::release() noexcept {
    if(auto* account=std::exchange(account_,nullptr))account->fetch_sub(count_,std::memory_order_relaxed);
}
void Lookup::stop() noexcept {
    if(auto* active=std::exchange(channel,nullptr))ares_destroy(active);
}
Request::~Request(){retire();}
void Request::retire() noexcept {
    if(reservation)socket->directions.fetch_and(~std::exchange(reservation,0),std::memory_order_release);
    if(auto* runtime=std::exchange(admission,nullptr))runtime->end_native_job();
}
Service::Service(std::string test_resolver)
    :lease_(http::ProviderLease::acquire()),test_resolver_(std::move(test_resolver)) {
    incoming_.reserve(request_capacity);
    active_.reserve(request_capacity);
    sockets_.reserve(max_sockets);
    polls_.reserve(request_capacity+max_lookups*16);
    targets_.reserve(polls_.capacity());
    worker_=std::thread([this]{run();});
}
Service::~Service(){shutdown();}
bool Service::prepare(Request& request,std::size_t bytes) {
    const auto op=request.operation;
    const auto direction_mask=direction(op);
    auto reject=[&](Kind kind){request.reply.error={kind,Phase::prepare};return false;};
    if(request.socket) {
        const bool closed=request.socket->closed.load(std::memory_order_acquire);
        if(closed && close_operation(op))return false;
        if(!close_operation(op) && (closed || request.socket->closing.load(std::memory_order_acquire)))
            return reject(Kind::closed);
        if(direction_mask && !reserve_direction(*request.socket,direction_mask))return reject(Kind::busy);
        request.reservation=direction_mask;
    }
    request.quota=quota_;
    if(close_operation(op)) {
        if(!request.operations.take(quota_->closes,1,max_sockets))return reject(Kind::limit_exceeded);
    } else if(!request.operations.take(quota_->operations,1,max_operations))return reject(Kind::limit_exceeded);
    if(!request.payload.take(quota_->bytes,bytes,max_payload))return reject(Kind::limit_exceeded);
    if((op==host::net_resolve || op==host::net_connect) && !request.endpoint) {
        if(!request.lookup_slot.take(quota_->lookups,1,max_lookups))return reject(Kind::limit_exceeded);
    }
    if(factory_operation(op)) {
        request.created=std::make_shared<Socket>(quota_);
        if(!request.created->slot.take(quota_->sockets,1,max_sockets))return reject(Kind::limit_exceeded);
    }
    return true;
}
void Service::submit(const std::shared_ptr<Request>& request,NativeCompletion completion) noexcept {
    {
        std::lock_guard lock(mutex_);
        if(!stopped_) {
            request->completion.emplace(std::move(completion));
            incoming_.push_back(request); // Reserved by ordinary or per-socket close admission.
            changed_.notify_one();
            return;
        }
    }
    if(!request->abandon && !close_operation(request->operation))request->reply.error={Kind::closed,phase(request->operation)};
    request->retire();
    completion.complete();
}
void Service::cancel(Request& request) noexcept {
    request.cancelled.store(true,std::memory_order_release);
    if(data_operation(request.operation) && request.socket)request.socket->closing.store(true,std::memory_order_release);
    std::lock_guard lock(mutex_);
    changed_.notify_one();
}
int Service::close(const std::shared_ptr<Socket>& socket) noexcept {
    socket->closing.store(true,std::memory_order_release);
    // The condition-variable predicate also covers an idle provider. Pair the
    // wake with its mutex so a close cannot fall between the check and wait.
    {std::lock_guard lock(mutex_);changed_.notify_one();}
    socket->closed.wait(false,std::memory_order_acquire);
    return socket->close_error;
}
void Service::request_stop() noexcept {
    std::lock_guard lock(mutex_);
    stopping_=true;
    changed_.notify_one();
}
void Service::shutdown() {
    std::lock_guard lock(join_);
    request_stop();
    if(worker_.joinable())worker_.join();
}
void Service::close_socket(Socket& socket) noexcept {
    if(socket.closed.load(std::memory_order_relaxed))return;
    const auto fd=std::exchange(socket.descriptor,invalid_descriptor);
    if(fd!=invalid_descriptor)socket.close_error=release(fd);
    // Keep the quota through owner-side publication or abandonment. A
    // terminal descriptor can still have a rooted wrapper or pending reply.
    socket.closed.store(true,std::memory_order_release);
    socket.closed.notify_all();
}
void Service::complete(Request& request) noexcept {
    if(request.lookup)request.lookup->stop();
    if(!request.abandon && request.created && request.reply.error.kind!=Kind::ok) {
        close_socket(*request.created);
        if(request.created->close_error)request.reply.error.cleanup=request.created->close_error;
    }
    auto ticket=std::move(*request.completion);
    request.completion.reset();
    request.retire();
    RuntimeFailure failure;
    if(request.abandon && request.reply.error.kind!=Kind::ok)failure.message="Native socket abandonment failed";
    ticket.complete(failure);
}
bool Service::resolve(Request& request) {
    if(!request.lookup) {
        request.lookup=std::make_unique<Lookup>();
        auto& query=*request.lookup;
        ares_options options{};
        options.sock_state_cb=watch_socket;options.sock_state_cb_data=&query;
        auto mask=ARES_OPT_SOCK_STATE_CB;
        char dns_only[]="b";
        if(!test_resolver_.empty()){options.lookups=dns_only;mask|=ARES_OPT_LOOKUPS;}
        auto status=ares_init_options(&query.channel,&options,mask);
        if(status==ARES_SUCCESS && !test_resolver_.empty())status=ares_set_servers_ports_csv(query.channel,test_resolver_.c_str());
        if(status!=ARES_SUCCESS){request.reply.error=resolver_error(status);return true;}
        ares_addrinfo_hints hints{};
        hints.ai_family=request.family?request.family==4?AF_INET:AF_INET6:AF_UNSPEC;
        hints.ai_socktype=SOCK_STREAM;
        hints.ai_flags=ARES_AI_NOSORT | ARES_AI_NUMERICSERV;
        const auto service=std::to_string(request.port);
        ares_getaddrinfo(query.channel,request.name.c_str(),service.c_str(),&hints,dns_complete,&query);
    }
    auto& query=*request.lookup;
    if(!query.finished && !query.overflow) {
        auto status=ares_process_fds(query.channel,query.ready.data(),query.ready_count,ARES_PROCESS_FLAG_NONE);
        query.ready_count=0;
        if(status!=ARES_SUCCESS){request.reply.error=resolver_error(status);return true;}
    }
    if(query.overflow){request.reply.error={Kind::limit_exceeded,Phase::resolve};return true;}
    if(!query.finished)return false;
    query.stop();
    if(query.status!=ARES_SUCCESS || !query.count)
        request.reply.error=resolver_error(query.status==ARES_SUCCESS?ARES_ENODATA:query.status);
    return true;
}
bool Service::connect(Request& request) {
    auto& socket=*request.created;
    if(!request.endpoint && !request.lookup) {
        if(!resolve(request))return false;
        if(request.reply.error.kind!=Kind::ok)return true;
    } else if(request.lookup && request.lookup->channel) {
        if(!resolve(request))return false;
        if(request.reply.error.kind!=Kind::ok)return true;
    }
    const auto count=request.endpoint?1u:request.lookup->count;
    if(request.connecting) {
        if(!request.ready && Clock::now()<request.attempt_deadline)return false;
        auto error=request.ready?socket_error(socket.descriptor):0;
        if(!request.ready)request.last_attempt={Kind::timed_out,Phase::connect};
        else {
            if(!error)error=name_of(socket.descriptor,socket.remote,true);
            if(!error)error=stream_options(socket);
            if(!error)return true;
            request.last_attempt=os_error(Phase::connect,error);
        }
        request.last_attempt.cleanup=release(std::exchange(socket.descriptor,invalid_descriptor));
        request.connecting=false;
        request.events=0;
        if(request.last_attempt.cleanup){request.reply.error=request.last_attempt;return true;}
    }
    while(request.candidate<count && Clock::now()<request.deadline) {
        const auto& target=request.endpoint?*request.endpoint:request.lookup->results[request.candidate];
        ++request.candidate;
        request.reply.number=static_cast<std::int64_t>(request.candidate);
        request.reply.text=target.format();
        if(target.multicast() || target.unspecified() || target.link_without_scope()) {
            request.last_attempt={Kind::invalid_address,Phase::connect};
            continue;
        }
        int error=0;
        socket.no_delay=request.no_delay;
        socket.descriptor=open_socket(target.family(),SOCK_STREAM,error);
        if(socket.descriptor==invalid_descriptor){request.last_attempt=os_error(Phase::open,error);continue;}
        request.attempt_deadline=request.endpoint?request.deadline:std::min(request.deadline,Clock::now()+request.attempt);
        if(::connect(socket.descriptor,target.data(),target.size)==0) {
            socket.remote=target;
            error=stream_options(socket);
            if(!error)return true;
        } else {
            error=last_error();
            if(waiting(error)) {
                request.connecting=true;
                request.events=POLLOUT;
                return false;
            }
        }
        request.last_attempt=os_error(Phase::connect,error);
        request.last_attempt.cleanup=release(std::exchange(socket.descriptor,invalid_descriptor));
        if(request.last_attempt.cleanup)break;
    }
    request.reply.error=Clock::now()>=request.deadline?Error{Kind::timed_out,Phase::connect}:request.last_attempt;
    return true;
}

bool Service::transfer(Request& request) {
    using namespace host;
    auto& socket=*request.socket;
    const auto op=request.operation;
    auto fail=[&](int code){
        request.reply.error=os_error(phase(op),code);
        if(data_operation(op) && (request.reply.error.kind==Kind::connection_reset ||
            request.reply.error.kind==Kind::connection_refused ||
            request.reply.error.kind==Kind::closed || request.reply.error.kind==Kind::io_failure)) {
            close_socket(socket);
            request.reply.error.cleanup=socket.close_error;
        }
        return true;
    };
    if(op==net_write && socket.write_closed){request.reply.error={Kind::write_closed,Phase::write};return true;}
    if(op==net_read && socket.eof){request.reply.flag=true;return true;}
    if(op==net_write && request.input.empty())return true;
    if(op==net_read || op==net_receive) {
        const bool udp=op==net_receive;
        const auto maximum=udp?datagram_buffer:request.maximum;
        if(request.reply.bytes.empty())request.reply.bytes.resize(maximum);
        Endpoint sender;
        sender.size=sizeof(sender.storage);
        auto size=udp?recvfrom(socket.descriptor,request.reply.bytes.data(),static_cast<int>(maximum),datagram_flags,sender.data(),&sender.size):
                      recv(socket.descriptor,request.reply.bytes.data(),static_cast<int>(maximum),0);
        if(size<0) {
            const auto code=last_error();
            if(waiting(code)){request.events=POLLIN;return false;}
            request.reply.bytes.clear();
            if(udp && code==oversized) {
                request.reply.flag=true;
                if(valid_endpoint(sender))request.reply.text=sender.format();
            }
            return fail(code);
        }
        request.reply.number=size;
        if(udp) {
            if(!valid_endpoint(sender)) {
                request.reply.bytes.clear();
                request.reply.flag=true;
                return fail(bad_family);
            }
            request.reply.text=sender.ip();
            request.reply.port=sender.port();
            request.reply.family=sender.public_family();
            request.reply.detail=size;
            if(static_cast<std::size_t>(size)>request.maximum) {
                request.reply.bytes.clear();
                request.reply.text=sender.format();
                request.reply.flag=true;
                request.reply.error={Kind::message_too_large,Phase::read};
                return true;
            }
        } else if(!size)socket.eof=request.reply.flag=true;
        request.reply.bytes.resize(static_cast<std::size_t>(size));
        return true;
    }
    const bool udp=op==net_send;
    if(udp && socket.local.family()!=request.endpoint->family()){
        request.reply.error={Kind::invalid_address,Phase::write};return true;
    }
    const auto chunk=std::min<std::size_t>(65536,request.input.size()-request.progress);
    const auto size=udp?sendto(socket.descriptor,request.input.data(),static_cast<int>(chunk),send_flags,request.endpoint->data(),request.endpoint->size):
                        send(socket.descriptor,request.input.data()+request.progress,static_cast<int>(chunk),send_flags);
    if(size<0) {
        const auto code=last_error();
        if(waiting(code)){request.events=POLLOUT;return false;}
        return fail(code);
    }
    request.progress+=static_cast<std::size_t>(size);
    request.reply.number=static_cast<std::int64_t>(request.progress);
    if(request.progress==request.input.size())return true;
    if(udp || !size){request.reply.error={Kind::io_failure,Phase::write};return true;}
    request.events=POLLOUT;
    return false;
}
bool Service::step(Request& request) {
    using namespace host;
    const auto op=request.operation;
    if(request.abandon || close_operation(op)) {
        const auto& socket=request.abandon && request.created?request.created:request.socket;
        if(socket) {
            close_socket(*socket);
            if(socket->close_error)request.reply.error=os_error(Phase::close,socket->close_error);
        }
        return true;
    }
    if(request.cancelled.load(std::memory_order_acquire) ||
       (request.socket && (request.socket->closing.load(std::memory_order_acquire) ||
                           request.socket->closed.load(std::memory_order_acquire)))) {
        request.reply.error={Kind::closed,phase(op)};
        return true;
    }
    if(Clock::now()>=request.deadline) {
        const auto stage=op==net_connect && !request.endpoint && !request.candidate?Phase::resolve:phase(op);
        request.reply.error={Kind::timed_out,stage};
        return true;
    }
    if(request.created && !request.registered) {
        sockets_.push_back(request.created);
        request.registered=true;
    }
    if(op==net_connect)return connect(request);
    if(op==net_resolve) {
        if(request.endpoint){request.reply=address_reply(*request.endpoint);return true;}
        if(!resolve(request))return false;
        if(request.reply.error.kind==Kind::ok) {
            for(const auto& result:std::span(request.lookup->results).first(request.lookup->count)) {
                if(!request.reply.text.empty())request.reply.text+='\n';
                request.reply.text+=result.ip();
            }
        }
        return true;
    }
    if(request.events && !request.ready)return false;
    request.events=0;
    switch(op) {
    case net_listen:case net_bind: {
        auto& socket=*request.created;
        socket.no_delay=request.no_delay;
        int error=0;
        socket.descriptor=open_socket(request.endpoint->family(),op==net_listen?SOCK_STREAM:SOCK_DGRAM,error);
        if(error){request.reply.error=os_error(Phase::open,error);return true;}
#ifdef _WIN32
        error=option(socket.descriptor,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,1);
#endif
        if(!error && ::bind(socket.descriptor,request.endpoint->data(),request.endpoint->size)!=0)error=last_error();
        if(!error)error=name_of(socket.descriptor,socket.local);
        if(error){request.reply.error=os_error(Phase::bind,error);return true;}
        if(op==net_listen && ::listen(socket.descriptor,request.backlog)!=0)request.reply.error=os_error(Phase::listen,last_error());
        return true;
    }
    case net_accept: {
        auto& stream=*request.created;
        stream.remote.size=sizeof(stream.remote.storage);
#ifdef __linux__
        auto fd=accept4(request.socket->descriptor,stream.remote.data(),&stream.remote.size,SOCK_NONBLOCK|SOCK_CLOEXEC);
#else
        auto fd=::accept(request.socket->descriptor,stream.remote.data(),&stream.remote.size);
#endif
        if(fd==invalid_descriptor) {
            const auto error=last_error();
            if(waiting(error)){request.events=POLLIN;return false;}
            request.reply.error=os_error(Phase::accept,error);
            return true;
        }
        stream.descriptor=fd;
        stream.no_delay=request.socket->no_delay;
        auto error=nonblocking(fd);
        if(!error)error=valid_endpoint(stream.remote)?stream_options(stream):bad_family;
        if(error)request.reply.error=os_error(Phase::accept,error);
        return true;
    }
    case net_shutdown: {
        auto& stream=*request.socket;
        if(!stream.write_closed) {
            if(::shutdown(stream.descriptor,send_shutdown)!=0)
                request.reply.error=os_error(Phase::shutdown,last_error());
            else stream.write_closed=true;
        }
        return true;
    }
    case net_stream_local:case net_listener_local:case net_udp_local:
        request.reply=address_reply(request.socket->local);return true;
    case net_stream_remote:request.reply=address_reply(request.socket->remote);return true;
    default:return transfer(request);
    }
}
void Service::collect_ready() {
    polls_.clear();targets_.clear();
    auto wake=Clock::now()+std::chrono::milliseconds(10);
    for(auto& request:active_) {
        wake=std::min(wake,request->deadline);
        if(request->connecting)wake=std::min(wake,request->attempt_deadline);
        request->ready=false;
        if(auto* lookup=request->lookup.get();lookup && lookup->channel) {
            for(const auto& watch:lookup->watches)if(watch.fd!=invalid_descriptor) {
                polls_.push_back({watch.fd,watch.events,0});targets_.push_back({request.get(),true});
            }
        } else if(request->events) {
            const auto& socket=request->operation==host::net_connect?request->created:request->socket;
            polls_.push_back({socket->descriptor,request->events,0});targets_.push_back({request.get(),false});
        }
    }
    if(polls_.empty()) {
        std::unique_lock lock(mutex_);
        if(active_.empty()) {
            changed_.wait(lock,[&]{
                return stopping_ || !incoming_.empty() || std::ranges::any_of(sockets_,[](const auto& socket){
                    return socket->closing.load(std::memory_order_acquire);
                });
            });
        } else if(!stopping_ && incoming_.empty())changed_.wait_until(lock,wake);
        return;
    }
    const auto remaining=std::chrono::ceil<std::chrono::milliseconds>(wake-Clock::now()).count();
    if(poll_sockets(polls_,static_cast<int>(std::max<std::int64_t>(0,remaining)))<0)return;
    for(std::size_t index=0;index<polls_.size();++index) {
        const auto& event=polls_[index];
        if(!event.revents)continue;
        auto& target=*targets_[index].request;
        if(!targets_[index].dns)target.ready=true;
        else {
            auto& lookup=*target.lookup;
            unsigned events=0;
            if(event.revents&(POLLIN|POLLERR|POLLHUP|POLLNVAL))events|=ARES_FD_EVENT_READ;
            if(event.revents&POLLOUT)events|=ARES_FD_EVENT_WRITE;
            if(events)lookup.ready[lookup.ready_count++]={event.fd,events};
        }
    }
}
void Service::run() noexcept {
    for(;;) {
        bool stopping=false;
        {
            std::lock_guard lock(mutex_);
            for(auto& request:incoming_)active_.push_back(std::move(request));
            incoming_.clear();
            stopping=stopping_;
            if(stopping && active_.empty() && sockets_.empty()) {
                stopped_=true;
                return;
            }
        }
        // Observe close commands before visiting any pending IO on the socket.
        for(auto& request:active_)if(close_operation(request->operation) || request->abandon) {
            const auto& socket=request->abandon && request->created?request->created:request->socket;
            if(socket)socket->closing.store(true,std::memory_order_release);
        }
        for(auto& socket:sockets_)if(stopping || socket->closing.load(std::memory_order_acquire))close_socket(*socket);
        std::erase_if(sockets_,[](const auto& socket){return socket->closed.load(std::memory_order_relaxed);});
        for(std::size_t i=0;i<active_.size();) {
            auto& request=*active_[i];
            bool finished=true;
            try {
                if(stopping && !request.abandon && !close_operation(request.operation))
                    request.reply.error={Kind::closed,phase(request.operation)};
                else finished=step(request);
            } catch(...) {request.reply.error={Kind::io_failure,phase(request.operation)};}
            if(!finished){++i;continue;}
            if(request.reply.error.kind!=Kind::ok && (request.operation==host::net_read || request.operation==host::net_receive))
                request.reply.bytes.clear();
            auto retired=std::move(active_[i]);
            if(i+1!=active_.size())active_[i]=std::move(active_.back());
            active_.pop_back();
            complete(*retired);
        }
        if(!stopping)collect_ready();
    }
}
}
