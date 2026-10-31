#include "void/http.hpp"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iostream>
#include <stdexcept>

namespace {
using namespace vd::http;
using Clock=std::chrono::steady_clock;
double milliseconds(Clock::time_point start){return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
void require(bool value){if(!value)throw std::runtime_error("HTTP measurement assertion");}
void requests(const char* name,const std::string& base,const char* ca,Protocol protocol) {
    Driver driver;Error error;ClientOptions settings;settings.tls.ca_bundle=ca;
    settings.max_connections=settings.max_per_origin=settings.max_idle=1;
    auto client=driver.open(settings,error);require(bool(client));
    Request request;request.method="POST";request.target=base+"/echo";
    request.body=Request::bytes;request.data="{\"value\":7}";
    request.headers={{"Content-Type","application/json"}};
    RequestOptions options;options.protocol=protocol;options.max_buffer_bytes=1024;
    auto send=[&]{return driver.send(client,request,options);};
    auto receive=[&](const std::shared_ptr<Operation>& pending){auto result=pending->take();
        require(result.error.kind==ErrorKind::success&&result.bytes==request.data&&result.info&&
            result.info->version==(protocol==Protocol::http2_required?Version::http2:Version::http1_1));};
    auto start=Clock::now();receive(send());const auto cold=milliseconds(start);
    for(unsigned n=0;n<8;++n)receive(send());
    std::vector<double> sequential,batches;
    const auto cpu=std::clock();
    for(unsigned n=0;n<64;++n){start=Clock::now();receive(send());sequential.push_back(milliseconds(start));}
    for(unsigned n=0;n<16;++n){
        start=Clock::now();std::vector<std::shared_ptr<Operation>> pending;
        for(unsigned index=0;index<4;++index)pending.push_back(send());
        for(auto& operation:pending)receive(operation);
        batches.push_back(milliseconds(start));
    }
    std::sort(sequential.begin(),sequential.end());std::sort(batches.begin(),batches.end());
    const auto cpu_seconds=static_cast<double>(std::clock()-cpu)/CLOCKS_PER_SEC;
    std::vector<double> closes;
    for(unsigned n=0;n<16;++n){
        Request paused;paused.target=base+"/paused";
        auto transfer=driver.start(client,paused,options,error);require(bool(transfer));
        require(driver.response(transfer)->take().error.kind==ErrorKind::success);
        start=Clock::now();driver.close(transfer)->take();closes.push_back(milliseconds(start));
    }
    std::sort(closes.begin(),closes.end());
    std::cout<<"{\"mode\":\""<<name<<"\",\"cold_ms\":"<<cold<<",\"warm_samples\":64,\"warm_median_ms\":"<<sequential[31]
        <<",\"warm_p95_ms\":"<<sequential[60]<<",\"four_concurrent_batches\":16,\"batch_median_ms\":"<<batches[7]
        <<",\"client_cpu_seconds\":"<<cpu_seconds<<",\"close_median_ms\":"<<closes[7]<<",\"close_max_ms\":"<<closes.back()<<"}\n";
    driver.close(client)->take();require(driver.snapshot().reserved_bytes==0);
}
}
int main(int argc,char** argv){
    try{require(argc==5);requests("h1",argv[1],"",Protocol::http1);
        requests("h1_tls",argv[2],argv[4],Protocol::http1);
        requests("h2_tls",argv[3],argv[4],Protocol::http2_required);
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
