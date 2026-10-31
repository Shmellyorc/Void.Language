#include "void/http.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <ctime>
#include <filesystem>
#ifndef _WIN32
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/evp.h>
#endif
#if __has_feature(address_sanitizer)
#include <sanitizer/allocator_interface.h>
#endif

namespace {
void require(bool condition, const char* message) { if(!condition)throw std::runtime_error(message); }
#ifndef _WIN32
int make_roots(const char* certificate,const char* key,const char* destination) {
    std::unique_ptr<BIO,decltype(&BIO_free)> cert_input(BIO_new_file(certificate,"rb"),BIO_free);
    std::unique_ptr<BIO,decltype(&BIO_free)> key_input(BIO_new_file(key,"rb"),BIO_free);
    require(bool(cert_input)&&bool(key_input),"corpus input files");
    std::unique_ptr<X509,decltype(&X509_free)> original(PEM_read_bio_X509(cert_input.get(),nullptr,nullptr,nullptr),X509_free);
    std::unique_ptr<EVP_PKEY,decltype(&EVP_PKEY_free)> signing(PEM_read_bio_PrivateKey(key_input.get(),nullptr,nullptr,nullptr),EVP_PKEY_free);
    require(bool(original)&&bool(signing),"corpus fixture certificate/key");
    const auto directory=std::filesystem::path(destination).parent_path();
    auto leaf=[&](const char* name,long before,long after) {
        std::unique_ptr<X509,decltype(&X509_free)> copy(X509_dup(original.get()),X509_free);
        std::unique_ptr<X509_NAME,decltype(&X509_NAME_free)> subject(X509_NAME_new(),X509_NAME_free);
        const auto now=std::time(nullptr);
        require(copy && subject && X509_NAME_add_entry_by_txt(subject.get(),"CN",MBSTRING_ASC,
            reinterpret_cast<const unsigned char*>("fixture-only.invalid"),-1,-1,0)==1 &&
            X509_set_subject_name(copy.get(),subject.get())==1 &&
            X509_set_issuer_name(copy.get(),X509_get_subject_name(original.get()))==1 &&
            ASN1_TIME_set(X509_getm_notBefore(copy.get()),now+before) &&
            ASN1_TIME_set(X509_getm_notAfter(copy.get()),now+after) &&
            X509_sign(copy.get(),signing.get(),EVP_sha256())>0,"fixture leaf signing");
        std::unique_ptr<BIO,decltype(&BIO_free)> output(BIO_new_file((directory/name).c_str(),"wb"),BIO_free);
        require(output && PEM_write_bio_X509(output.get(),copy.get())==1,"fixture leaf output");
    };
    leaf("leaf.pem",-3600,86400);
    leaf("expired.pem",-172800,-86400);
    leaf("future.pem",86400,172800);
    std::ofstream output(destination,std::ios::binary);
    std::size_t total=0,count=0;
    for(long serial=1;;++serial) {
        std::unique_ptr<X509,decltype(&X509_free)> copy(X509_dup(original.get()),X509_free);
        std::unique_ptr<BIO,decltype(&BIO_free)> encoded(BIO_new(BIO_s_mem()),BIO_free);
        require(copy && encoded && ASN1_INTEGER_set(X509_get_serialNumber(copy.get()),serial)==1 &&
            X509_sign(copy.get(),signing.get(),EVP_sha256())>0 && PEM_write_bio_X509(encoded.get(),copy.get())==1,"corpus signing");
        char* bytes=nullptr;const auto size=BIO_get_mem_data(encoded.get(),&bytes);
        require(size>0,"corpus encoding");
        if(static_cast<std::size_t>(size)>1048576-total)break;
        output.write(bytes,size);total+=static_cast<std::size_t>(size);++count;
    }
    require(bool(output),"corpus output");
    std::cout<<"{\"roots\":"<<count<<",\"bytes\":"<<total<<"}\n";
    return 0;
}
#endif
std::size_t allocated() {
#if __has_feature(address_sanitizer)
    return __sanitizer_get_current_allocated_bytes();
#elif defined(__linux__)
    std::ifstream status("/proc/self/status");std::string line;
    while(std::getline(status,line))if(line.starts_with("VmRSS:"))return std::stoull(line.substr(6))*1024;
    return 0;
#else
    return 0;
#endif
}
}
int main(int argc,char** argv) {
    using namespace vd::http;
    try {
#ifndef _WIN32
        if(argc==5 && std::string_view(argv[1])=="--make-roots")return make_roots(argv[2],argv[3],argv[4]);
#endif
        require(argc==7,"TLS memory fixture arguments");
        Driver driver; Error error;
        ClientOptions options;
        options.max_connections=options.max_active=options.max_per_origin=options.max_idle=options.max_http2_streams=1;
        options.tls.ca_bundle=argv[3];
        const auto baseline=allocated();
        std::vector<std::shared_ptr<Client>> clients;
        for(unsigned i=0;i<4;++i){auto client=driver.open(options,error);require(bool(client),"TLS corpus client admission");clients.push_back(client);}
        const auto admission=driver.snapshot().reserved_bytes;
        std::atomic<bool> stop=false;
        std::atomic<std::size_t> peak=allocated();
        std::thread sampler([&]{while(!stop.load()){const auto value=allocated();auto prior=peak.load();while(value>prior&&!peak.compare_exchange_weak(prior,value)){};std::this_thread::sleep_for(std::chrono::milliseconds(1));}});
        try {
            std::size_t minimum=static_cast<std::size_t>(-1), maximum=0;
            RequestOptions request_options;request_options.max_buffer_bytes=1024;
            // Four near-1-MiB trust stores are parsed on one instrumented driver.
            // This test measures retention, with an explicit workload deadline.
            request_options.connect_timeout_ms=30000;
            for(unsigned round=0;round<64;++round) {
                std::vector<std::shared_ptr<Operation>> pending;
                for(auto& client:clients){Request request;request.target=argv[1];pending.push_back(driver.send(client,request,request_options));}
                for(auto& operation:pending){auto result=operation->take();
                    if(result.error.kind!=ErrorKind::success)std::cerr<<"TLS failure kind="<<static_cast<int>(result.error.kind)<<" code="<<result.error.native_code<<'\n';
                    require(result.error.kind==ErrorKind::success && result.bytes=="ok","TLS corpus verified handshake");}
                pending.clear();
                if(round>=16){auto value=allocated();minimum=std::min(minimum,value);maximum=std::max(maximum,value);}
                if((round+1)%16==0)std::cerr<<"TLS rounds "<<round+1<<"/64; current allocation measure "<<allocated()<<std::endl;
            }
            std::cout<<"{\"tls_memory\":{\"baseline\":"<<baseline<<",\"peak_sample\":"<<peak.load()
                <<",\"steady_min\":"<<minimum<<",\"steady_max\":"<<maximum<<",\"client_reservation\":"<<admission<<"}}\n";
#if __has_feature(address_sanitizer)
            require(maximum-minimum<2*1024*1024,"TLS retained live allocations must plateau across fresh handshakes");
#else
            // RSS includes allocator arenas and thread caches after objects die.
            require(maximum-minimum<8*1024*1024,"TLS retained RSS must plateau across fresh handshakes");
#endif
            require(peak.load()<baseline+admission,"TLS observed allocations fit charged client admission");
            {
                Request oversized;oversized.target=argv[2];
                auto denied=driver.send(clients.front(),oversized,request_options)->take();
                require(denied.error.kind==ErrorKind::tls_failure && !denied.error.remote_outcome_unknown,"oversized TLS certificate flight rejected before HTTP");
            }
            for(int index:{4,5}) {
                Request invalid;invalid.target=argv[index];
                auto rejected=driver.send(clients.front(),invalid,request_options)->take();
                require(rejected.error.kind==ErrorKind::tls_verification && !rejected.error.remote_outcome_unknown,
                    "expired/future leaf rejected before HTTP");
            }
            {
                Request invalid;invalid.target=argv[1];
                invalid.target.replace(invalid.target.find("127.0.0.1"),9,"localhost");
                auto rejected=driver.send(clients.front(),invalid,request_options)->take();
                require(rejected.error.kind==ErrorKind::tls_verification && !rejected.error.remote_outcome_unknown,
                    "wrong hostname rejected before HTTP");
            }
            {
                Request legacy;legacy.target=argv[6];
                auto rejected=driver.send(clients.front(),legacy,request_options)->take();
                require(rejected.error.kind==ErrorKind::tls_failure && !rejected.error.remote_outcome_unknown,
                    "TLS below 1.2 rejected before HTTP");
            }
            auto system_options=options;system_options.tls.ca_bundle.clear();
            auto system=driver.open(system_options,error);require(bool(system),"system trust test client");
            {
                Request untrusted;untrusted.target=argv[1];
                auto rejected=driver.send(system,untrusted,request_options)->take();
                require(rejected.error.kind==ErrorKind::tls_verification,"distribution roots reject local untrusted certificate");
            }
            driver.close(system)->take();
            for(auto& client:clients)driver.close(client)->take();
            require(driver.snapshot().reserved_bytes==0,"TLS snapshots and client reservations released");
        } catch(...) {stop=true;sampler.join();throw;}
        stop=true;sampler.join();
        std::cout<<"TLS corpus, certificate-flight limit, distribution roots and retained plateau passed\n";
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
