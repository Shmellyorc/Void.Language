#include "void/http.hpp"
#include "void/http_file.hpp"
#include "void/detail/filesystem_hooks.hpp"
#include <chrono>
#include <iostream>
#include <thread>
#include <stdexcept>
#include <ctime>

namespace {
using namespace vd::http;
using Clock=std::chrono::steady_clock;
const char* phase(Phase p) {
    switch(p){case Phase::dns:return "dns";case Phase::connect:return "connect";case Phase::proxy:return "proxy";
    case Phase::tls:return "tls";case Phase::headers:return "headers";case Phase::upload:return "upload";
    case Phase::download:return "download";case Phase::file:return "file";default:return "other";}
}
void report(const char* name,const Result& result,Clock::time_point start) {
    std::cout<<"{\"case\":\""<<name<<"\",\"kind\":\""<<(result.error.kind==ErrorKind::success?"success":
        result.error.kind==ErrorKind::timeout?"timeout":"other")<<"\",\"phase\":\""<<phase(result.error.phase)
        <<"\",\"unknown\":"<<(result.error.remote_outcome_unknown?"true":"false")
        <<",\"kind_code\":"<<static_cast<int>(result.error.kind)<<",\"native_code\":"<<result.error.native_code<<",\"elapsed_ms\":"<<std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-start).count()<<"}\n";
}
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
class Borrow final : public vd::filesystem::FileBorrow {
    std::shared_ptr<vd::filesystem::File> file_;
public:
    explicit Borrow(std::shared_ptr<vd::filesystem::File> file):file_(std::move(file)){}
    std::shared_ptr<vd::filesystem::File> file() const noexcept override{return file_;}
};
void file_deadline(const std::string& base,const std::string& path) {
    vd::Runtime runtime;runtime.async_worker_limit(1);
    vd::Runtime::Attachment attachment(runtime);vd::Runtime::Managed managed(runtime);
    auto provider=runtime_driver(runtime);auto& driver=provider->driver();Error error;
    auto client=driver.open({},error);require(bool(client),"file timer client");
    struct Gate {std::mutex mutex;std::condition_variable changed;bool entered=false,released=false;};
    auto gate=std::make_shared<Gate>();auto hooks=std::make_shared<vd::filesystem::testing::Hooks>();
    hooks->before=[gate](vd::filesystem::Operation operation){
        if(operation!=vd::filesystem::Operation::read_bytes)return;
        std::unique_lock lock(gate->mutex);gate->entered=true;gate->changed.notify_all();
        gate->changed.wait(lock,[&]{return gate->released;});
    };
    std::shared_ptr<vd::filesystem::File> file;
    {vd::Runtime::Native safe(runtime);vd::filesystem::Result status;
     file=vd::filesystem::open(path,vd::filesystem::OpenMode::create,status,hooks);}
    require(bool(file),"file timer input");
    auto producer=FileUpload::prepare(runtime,provider,{{-1,1024,std::make_shared<Borrow>(file)}});
    Request request;request.target=base+"/early-stop";request.method="POST";
    request.body=Request::stream;request.stream_length=1024;request.source=producer->source();
    RequestOptions timed;timed.total_timeout_ms=100;timed.max_buffer_bytes=1024;
    auto start=Clock::now();auto transfer=driver.start_buffered_upload(client,request,timed,error);
    require(bool(transfer),"file deadline transfer");producer->start(transfer);
    {
        vd::Runtime::Native safe(runtime);std::unique_lock lock(gate->mutex);
        const bool entered=gate->changed.wait_for(lock,std::chrono::seconds(3),[&]{return gate->entered;});
        lock.unlock();
        Request release;release.target=base+"/early-release";
        auto peer=driver.send(client,release,{})->take();
        const auto cpu=std::clock();
        std::this_thread::sleep_for(std::chrono::milliseconds(160));
        std::cout<<"{\"case\":\"file-wait-cpu\",\"cpu_seconds\":"<<static_cast<double>(std::clock()-cpu)/CLOCKS_PER_SEC<<"}\n";
        auto result=driver.buffered_result(transfer);const bool waited=!result->ready();
        {std::lock_guard guard(gate->mutex);gate->released=true;gate->changed.notify_all();}
        auto value=result->take();report("file-drain",value,start);
        require(entered&&waited&&peer.error.kind==ErrorKind::success,"entered file IO must drain before publication");
        vd::filesystem::close(*file);driver.close(client)->wait();
    }
}
}
int main(int argc,char** argv) {
    try {
        require(argc==7,"timeout fixture endpoints and file required");
        Driver driver;Error error;ClientOptions options;options.max_connections=options.max_per_origin=options.max_idle=2;
        auto client=driver.open(options,error);require(bool(client),"timeout client");
        RequestOptions short_connect;short_connect.connect_timeout_ms=50;short_connect.total_timeout_ms=2000;
        short_connect.max_buffer_bytes=1024;
        {
            Request request;request.target=argv[2];auto start=Clock::now();
            report("tls",driver.send(client,request,short_connect)->take(),start);
        }
        {
            auto proxy_options=options;proxy_options.proxy.mode=ProxyOptions::explicit_proxy;proxy_options.proxy.endpoint=argv[3];
            auto proxy=driver.open(proxy_options,error);require(bool(proxy),"timeout proxy");
            Request request;request.target="https://127.0.0.1:9/";auto start=Clock::now();
            report("proxy",driver.send(proxy,request,short_connect)->take(),start);driver.close(proxy)->take();
        }
        auto get=[&](const char* path){Request request;request.target=std::string(argv[1])+path;
            RequestOptions bounded;bounded.max_buffer_bytes=1024;return driver.send(client,request,bounded)->take();};
        {
            Request request;request.target=std::string(argv[1])+"/producer-pause";request.method="POST";
            request.body=Request::stream;request.stream_length=2048;
            RequestOptions timed;timed.write_timeout_ms=20;timed.total_timeout_ms=3000;
            auto transfer=driver.start(client,request,timed,error);require(bool(transfer),"paused producer");
            require(driver.write(transfer,std::string(1024,'p'))->take().error.kind==ErrorKind::success,"first producer block");
            require(get("/producer-seen").error.kind==ErrorKind::success,"peer saw first producer bytes");
            std::this_thread::sleep_for(std::chrono::milliseconds(60));
            auto start=Clock::now();auto written=driver.write(transfer,std::string(1024,'p'))->take();
            if(written.error.kind==ErrorKind::success){driver.finish_upload(transfer);report("producer-resume",driver.response(transfer)->take(),start);}
            else report("producer-resume",written,start);
            driver.close(transfer)->take();
        }
        {
            Request request;request.target=std::string(argv[1])+"/write-stall";request.method="POST";
            request.body=Request::stream;request.stream_length=33554432;
            RequestOptions timed;timed.write_timeout_ms=30;timed.total_timeout_ms=2000;
            auto transfer=driver.start(client,request,timed,error);require(bool(transfer),"stalled upload");
            auto start=Clock::now();
            for(unsigned n=0;n<512;++n){auto result=driver.write(transfer,std::string(65536,'w'))->take();
                if(result.error.kind!=ErrorKind::success){report("write",result,start);break;}}
            driver.close(transfer)->take();
        }
        {
            auto trusted=options;trusted.tls.ca_bundle=argv[5];
            auto tls=driver.open(trusted,error);require(bool(tls),"digest timer client");
            Request request;request.target=std::string(argv[4])+"/digest-slow";request.method="POST";
            request.body=Request::bytes;request.data=std::string(8388608,'d');
            request.auth.kind=Authentication::digest;request.auth.user="user";request.auth.password="password";
            RequestOptions timed;timed.header_timeout_ms=20;timed.write_timeout_ms=0;timed.total_timeout_ms=5000;timed.max_buffer_bytes=1024;
            auto pending=driver.send(tls,request,timed);get("/digest-seen");
            std::this_thread::sleep_for(std::chrono::milliseconds(60));
            auto start=Clock::now();require(get("/digest-release").error.kind==ErrorKind::success,"release authenticated body");
            report("digest-upload",pending->take(),start);driver.close(tls)->take();
        }
        driver.close(client)->take();
        file_deadline(argv[1],argv[6]);
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
