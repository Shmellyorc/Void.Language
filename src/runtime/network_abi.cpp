#include "network.hpp"
#include "void/host_abi.hpp"
#include "void/native_resource.hpp"
#include "void/native_work.hpp"
#include <cstring>
#include <stdexcept>
#include <utility>

namespace vd::net {
namespace {
char provider_kind, stream_kind, listener_kind, udp_kind;

class Provider final : public NativeResource {
public:
    explicit Provider(Runtime& runtime) : NativeResource(runtime,nullptr) {
        Runtime::Native safe(runtime);
        service=std::make_unique<Service>();
    }
    ~Provider() override {
        if(runtime.is_managed()){Runtime::Native safe(runtime);service.reset();}
        else service.reset();
    }
    const void* kind() const noexcept override {return &provider_kind;}
    void request_stop() noexcept override {service->request_stop();}
    std::error_code close() override {
        if(runtime.is_managed()){Runtime::Native safe(runtime);return close();}
        service->shutdown();
        return {};
    }
    std::unique_ptr<Service> service;
};
class Resource final : public NativeResource {
public:
    Resource(Runtime& runtime,void* wrapper,const void* kind,std::shared_ptr<Provider> provider,std::shared_ptr<Socket> socket)
        :NativeResource(runtime,wrapper),provider(std::move(provider)),socket(std::move(socket)),identity_(wrapper),kind_(kind){}
    const void* kind() const noexcept override {return kind_;}
    void release() {
        if(auto ledger=NativeLibrary::existing(runtime))ledger->erase(identity_,this);
        wrapper.reset(nullptr);
    }
    std::error_code close() override {
        int error;
        if(runtime.is_managed()){
            Runtime::Native safe(runtime);
            error=provider->service->close(socket);
        } else error=provider->service->close(socket);
        release();
        return {error,std::system_category()};
    }
    std::shared_ptr<Provider> provider;
    std::shared_ptr<Socket> socket;
private:
    void* identity_;
    const void* kind_;
};
std::shared_ptr<Resource> resource(Runtime& runtime,void* key,const void* kind) {
    const auto ledger=NativeLibrary::existing(runtime);
    if(!ledger)return {};
    auto value=ledger->find(key,true);
    if(!value)return {};
    if(value->kind()!=kind)throw std::logic_error("Foreign networking resource");
    value->own_thread();
    return std::static_pointer_cast<Resource>(value);
}
std::shared_ptr<Provider> provider(Runtime& runtime) {
    auto ledger=NativeLibrary::get(runtime);
    if(auto value=ledger->find(&provider_kind,true)) {
        value->own_thread();
        return std::static_pointer_cast<Provider>(value);
    }
    auto value=std::make_shared<Provider>(runtime);
    ledger->publish(&provider_kind,value);
    return value;
}
const void* kind_of(char resource) {
    switch(resource) {
    case 'S':return &stream_kind;
    case 'N':return &listener_kind;
    case 'U':return &udp_kind;
    default:return nullptr;
    }
}
struct NativeReply {
    std::int64_t kind,phase,domain,code,cleanup,number,detail,port,family;
    bool flag;
    void* text=nullptr;
    void* bytes=nullptr;
};
constexpr std::array<std::size_t,12> fields{
    offsetof(NativeReply,kind),offsetof(NativeReply,phase),offsetof(NativeReply,domain),
    offsetof(NativeReply,code),offsetof(NativeReply,cleanup),offsetof(NativeReply,number),
    offsetof(NativeReply,detail),offsetof(NativeReply,port),offsetof(NativeReply,family),
    offsetof(NativeReply,flag),offsetof(NativeReply,text),offsetof(NativeReply,bytes)};
constexpr std::array references{fields[10],fields[11]};
void publish_reply(Runtime& runtime,void* output,const Reply& value,std::optional<std::size_t> offset={}) {
    NativeReply reply{static_cast<int>(value.error.kind),static_cast<int>(value.error.phase),
        static_cast<int>(value.error.domain),value.error.code,value.error.cleanup,value.number,
        value.detail,value.port,value.family,value.flag};
    Runtime::Slot slot{&reply,sizeof(reply),references.data(),references.size()};
    Runtime::Frame roots(runtime,&slot,1);
    runtime.string_into(&reply.text,value.text);
    runtime.bytes_into(&reply.bytes,value.bytes);
    if(!offset){std::memcpy(output,&reply,sizeof(reply));return;}
    std::memcpy(static_cast<char*>(output)+*offset,&reply,references[0]);
    runtime.store(output,*offset+references[0],reply.text);
    runtime.store(output,*offset+references[1],reply.bytes);
}
struct Inputs {
    void* acquisition=nullptr;
    const void* acquired_kind=nullptr;
    void* wrapper=nullptr;
    const void* wrapper_kind=nullptr;
    void* payload=nullptr;
    std::size_t byte_reservation=0;
};
bool options(Runtime& runtime,Request& request,Inputs& input,void* const* refs,const std::int64_t* nums) {
    using namespace host;
    const auto op=request.operation;
    auto invalid=[&](Kind kind=Kind::invalid_argument){request.reply.error={kind,Phase::prepare};return false;};
    const auto kind=kind_of(operations[op].arguments.front());
    if(factory_operation(op) && op!=net_accept) {
        input.acquisition=refs[0];input.acquired_kind=kind;
    } else if(kind) {
        input.wrapper=refs[0];input.wrapper_kind=kind;
        if(op==net_accept){input.acquisition=refs[1];input.acquired_kind=&stream_kind;}
    }
    auto timeout=[&](std::int64_t value,bool positive=false) {
        if(value<(positive?1:0) || value>max_timeout)return false;
        if(value)request.deadline=Clock::now()+std::chrono::milliseconds(value);
        return true;
    };
    auto endpoint=[&](void* value,std::int64_t port,bool remote,bool allow_hostname=false) {
        if(port<0 || port>65535 || (remote && !port))return invalid();
        request.port=static_cast<std::uint16_t>(port);
        const auto text=runtime.string_bytes(value);
        request.endpoint=parse_ip(text,request.port);
        if(!request.endpoint && allow_hostname && dns_name(text)){
            request.name=text;
            return true;
        }
        if(!request.endpoint || (remote && (request.endpoint->multicast() || request.endpoint->unspecified() ||
            request.endpoint->link_without_scope())) ||
           (request.family && request.endpoint->public_family()!=request.family))
            return invalid(Kind::invalid_address);
        return true;
    };
    auto family=[&](std::int64_t value){
        if(value!=0 && value!=4 && value!=6)return false;
        request.family=static_cast<int>(value);return true;
    };
    switch(op) {
    case net_resolve:
        if(!family(nums[1]) || !timeout(nums[2],true))return invalid();
        input.byte_reservation=32*65;
        return endpoint(refs[0],nums[0],false,true);
    case net_connect:
        if(!family(nums[1]) || !timeout(nums[2],true) || nums[3]<1 || nums[3]>max_timeout ||
           (nums[4]!=0 && nums[4]!=1) || (nums[5]!=0 && nums[5]!=1))return invalid();
        request.attempt=std::chrono::milliseconds(nums[3]);
        request.no_delay=nums[4]!=0;
        request.hostname=nums[5]!=0;
        return endpoint(refs[1],nums[0],true,request.hostname);
    case net_listen:
        if(nums[1]<1 || nums[1]>4096 || (nums[2]!=0 && nums[2]!=1))return invalid();
        request.backlog=static_cast<int>(nums[1]);request.no_delay=nums[2]!=0;
        return endpoint(refs[1],nums[0],false) &&
            (!request.endpoint->link_without_scope() || invalid(Kind::invalid_address));
    case net_bind:
        return endpoint(refs[1],nums[0],false) &&
            (!request.endpoint->link_without_scope() || invalid(Kind::invalid_address));
    case net_accept:return timeout(nums[0]) || invalid();
    case net_read:case net_receive:
        if(nums[0]<(op==net_read?1:0) || nums[0]>static_cast<std::int64_t>(op==net_read?max_tcp:max_v6) ||
           !timeout(nums[1]))return invalid();
        request.maximum=static_cast<std::size_t>(nums[0]);
        input.byte_reservation=op==net_read?request.maximum:datagram_buffer;
        return true;
    case net_send:case net_write:
        if(op==net_send && !endpoint(refs[2],nums[0],true))return false;
        if(!timeout(nums[op==net_send?1:0]))return invalid();
        input.payload=refs[1];
        return true;
    default:return true;
    }
}
bool reserve_payload(Runtime& runtime,Request& request,Inputs& input) {
    if(!input.payload)return true;
    constexpr std::size_t zero=0;
    Runtime::Slot slot{&input.payload,sizeof(void*),&zero,1};
    Runtime::Frame roots(runtime,&slot,1);
    const auto count=runtime.byte_array_size(runtime.root_word(&input.payload));
    const bool datagram=request.operation==host::net_send;
    const auto maximum=datagram?(request.endpoint->public_family()==4?max_v4:max_v6):max_tcp;
    if(count>maximum) {
        request.reply.error={datagram?Kind::message_too_large:Kind::limit_exceeded,Phase::prepare};
        return false;
    }
    input.byte_reservation=static_cast<std::size_t>(count);
    return true;
}
void acquire_wrapper(Runtime& runtime,void* wrapper,const void* kind,const std::shared_ptr<Provider>& provider,
                     const Request& request,std::shared_ptr<Resource>& published) {
    if(!wrapper || request.reply.error.kind!=Kind::ok)return;
    published=std::make_shared<Resource>(runtime,wrapper,kind,provider,request.created);
    NativeLibrary::get(runtime)->publish(wrapper,published);
}
bool cleanup_required(const Request& request) {
    if(request.created) {
        if(request.created->closed.load(std::memory_order_acquire))return request.created->close_error!=0;
        return request.reply.error.kind==Kind::ok;
    }
    return data_operation(request.operation) && request.socket &&
        (!request.socket->closed.load(std::memory_order_acquire) || request.socket->close_error!=0);
}
void release_terminal(const std::shared_ptr<Resource>& receiver) {
    if(receiver && receiver->socket->closed.load(std::memory_order_acquire))receiver->release();
}
void wait_request(Runtime& runtime,Service& service,const std::shared_ptr<Request>& request) {
    auto wake=std::make_shared<NativeWake>();
    auto ticket=NativeCompletion::pending(wake);
    Runtime::Native safe(runtime);
    service.submit(request,ticket);
    std::unique_lock lock(wake->mutex);
    wake->changed.wait(lock,[&]{return ticket.ready();});
}
class Job final : public ExternalNativeJob {
public:
    Job(Runtime& runtime,std::shared_ptr<Provider> provider,std::shared_ptr<Request> request,
        const Inputs& input,std::shared_ptr<Resource> receiver,std::size_t offset,bool prepared)
        :provider_(std::move(provider)),request_(std::move(request)),receiver_(std::move(receiver)),
         acquisition_(runtime,input.acquisition),kind_(input.acquired_kind),offset_(offset),prepared_(prepared){}
    ~Job() override {release_terminal(receiver_);}
    void start(NativeCompletion ticket) noexcept override {
        if(prepared_){provider_->service->submit(request_,std::move(ticket));return;}
        request_->retire();
        ticket.complete();
    }
    void cancel_queued() noexcept override {request_->retire();}
    void cancel() noexcept override {if(prepared_)provider_->service->cancel(*request_);}
    bool acquired() const noexcept override {return prepared_ && !committed_ && cleanup_required(*request_);}
    void commit() noexcept override {committed_=true;}
    void abandon_external(NativeCompletion ticket) noexcept override {
        committed_=true; // The cleanup ticket, rather than a second abandonment, retires this job.
        if(published_)published_->release();
        request_->abandon=true;
        request_->reply.error={};
        provider_->service->submit(request_,std::move(ticket));
    }
    void publish(Runtime& runtime,void* task) override {
        release_terminal(receiver_);
        acquire_wrapper(runtime,acquisition_.get(),kind_,provider_,*request_,published_);
        publish_reply(runtime,task,request_->reply,offset_);
    }
private:
    std::shared_ptr<Provider> provider_;
    std::shared_ptr<Request> request_;
    std::shared_ptr<Resource> receiver_,published_;
    Runtime::Handle acquisition_;
    const void* kind_;
    std::size_t offset_;
    bool prepared_, committed_=false;
};
}
}
extern "C" void vd_net_call(void* output,const vd::host::NetworkLayout* layout,unsigned operation,
    void* const* refs,const std::int64_t* nums,const void* generation,vd::RuntimeFailure* failure) noexcept {
    using namespace vd;using namespace vd::host;using namespace vd::net;
    try {
        if(!output || !layout || !refs || !nums || !failure || !network_operation(operation) ||
           operations[operation].cleanup || layout->size!=sizeof(NativeReply) || layout->fields!=fields)
            throw std::logic_error("Invalid networking ABI");
        auto& runtime=Runtime::current();
        if(operation==net_parse) {
            const auto result=parse_ip(runtime.string_bytes(refs[0]));
            Reply reply;
            if(result)reply=address_reply(*result);
            else reply.error={Kind::invalid_address,Phase::prepare};
            publish_reply(runtime,output,reply);
            return;
        }
        const auto op=network_sync(operation);
        auto request=std::make_shared<Request>(op);
        Inputs input;
        std::shared_ptr<Provider> owner;
        std::shared_ptr<Resource> receiver;
        bool prepared=false;
        if(!supported())request->reply.error={Kind::unsupported,Phase::prepare};
        else if(options(runtime,*request,input,refs,nums)) {
            if(input.wrapper) {
                receiver=resource(runtime,input.wrapper,input.wrapper_kind);
                if(receiver)request->socket=receiver->socket;
                else if(!close_operation(op))request->reply.error={Kind::closed,Phase::prepare};
            }
            if(op==net_send && request->socket && request->socket->local.family()!=request->endpoint->family())
                request->reply.error={Kind::invalid_address,Phase::prepare};
            if(request->reply.error.kind==Kind::ok && (!input.wrapper || receiver) && reserve_payload(runtime,*request,input)) {
                runtime.begin_native_job();
                request->admission=&runtime;
                owner=receiver?receiver->provider:provider(runtime);
                prepared=owner->service->prepare(*request,input.byte_reservation);
                if(prepared && input.payload) {
                    constexpr std::size_t zero=0;
                    Runtime::Slot slot{&input.payload,sizeof(void*),&zero,1};
                    Runtime::Frame roots(runtime,&slot,1);
                    auto bytes=runtime.bounded_byte_array_bytes(runtime.root_word(&input.payload),input.byte_reservation);
                    if(bytes)request->input=std::move(*bytes);
                    else {request->reply.error={Kind::limit_exceeded,Phase::prepare};prepared=false;}
                }
            }
        }
        if(asynchronous(operation)) {
            if(!layout->task_key || !layout->task_references || layout->task_reference_count!=references.size() ||
               layout->task_result>layout->task_size || sizeof(NativeReply)>layout->task_size-layout->task_result ||
               layout->task_references[0]!=layout->task_result+references[0] ||
               layout->task_references[1]!=layout->task_result+references[1])
                throw std::logic_error("Invalid networking task ABI");
            TypeLayout task{layout->task_size,{layout->task_references[0],layout->task_references[1]}};
            task.descriptor=runtime.descriptor(layout->task_key);
            auto job=std::make_shared<Job>(runtime,std::move(owner),std::move(request),input,std::move(receiver),layout->task_result,prepared);
            async_submit_external(static_cast<void**>(output),task,generation,std::move(job));
            return;
        }
        if(prepared)wait_request(runtime,*owner->service,request);
        else request->retire();
        std::shared_ptr<Resource> published;
        try {
            release_terminal(receiver);
            acquire_wrapper(runtime,input.acquisition,input.acquired_kind,owner,*request,published);
            publish_reply(runtime,output,request->reply);
        } catch(...) {
            if(prepared && cleanup_required(*request)) {
                Runtime::Native safe(runtime);
                owner->service->close(request->created?request->created:request->socket);
            }
            if(published)published->release();
            release_terminal(receiver);
            throw;
        }
    } catch(const std::bad_alloc&) {if(failure)failure->message="Networking allocation failed";}
      catch(...) {if(failure)failure->message="Networking boundary failed";}
}
extern "C" void vd_net_cleanup(unsigned operation,void* wrapper,vd::RuntimeFailure* failure) noexcept {
    using namespace vd;using namespace vd::net;using namespace vd::host;
    try {
        if(operation<net_cleanup_stream || !network_operation(operation) || !operations[operation].cleanup)
            throw std::logic_error("Invalid networking cleanup ABI");
        auto found=resource(Runtime::current(),wrapper,kind_of(operations[operation].arguments.front()));
        if(found && found->close() && failure)failure->message="Native socket close failed";
    } catch(...) {if(failure)failure->message="Networking cleanup failed";}
}
