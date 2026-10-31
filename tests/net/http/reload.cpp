#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/http.hpp"
#include "void/runtime.hpp"
#include <fstream>
#include <future>
#include <iostream>
#include <semaphore>
#include <stdexcept>

namespace {
void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
void write(const std::filesystem::path& path,const std::string& text){std::ofstream out(path);out<<text;require(bool(out),"reload source write");}
auto analyze(const std::filesystem::path& path){auto result=vd::analyze_project(path);if(!result.succeeded())throw vd::CompilationError(result.diagnostics);return result.program;}
}
int main(int argc,char** argv){
    try{
        require(argc==4,"reload source, peer and cache required");
        const std::filesystem::path path=argv[1];const auto original=vd::read_source(path).text();
        vd::Jit jit({argv[3],std::string(vd::compiler_build_identity())});
        auto program=analyze(path);auto old=jit.compile_async(*program,true);
        const bool cache=jit.cache_status()=="miss";
        {auto hit=jit.compile_async(*program,true);require(jit.cache_status()==(cache?"hit":"unavailable"),"HTTP object cache hit");old=std::move(hit);}
        auto runtime=old->runtime_owner();runtime->async_worker_limit(1);vd::ReloadSlot slot(old);
        vd::http::Driver control;vd::http::Error error;auto client=control.open({},error);require(bool(client),"reload control client");
        auto get=[&](const char* suffix){vd::http::Request request;request.target=std::string(argv[2])+suffix;
            vd::http::RequestOptions options;options.max_buffer_bytes=1024;return control.send(client,request,options)->take();};
        std::binary_semaphore next{0};std::promise<int> first;auto first_result=first.get_future();
        auto active=std::async(std::launch::async,[&]{
            try { first.set_value(slot.invoke(7)); } catch(...) { first.set_exception(std::current_exception()); throw; }
            next.acquire();return slot.invoke(7);
        });
        struct Release {decltype(get)& request;std::binary_semaphore& next;bool resumed=false;
            ~Release(){try{request("/release");}catch(...){}if(!resumed)next.release();}} release{get,next};
        require(get("/seen").error.kind==vd::http::ErrorKind::success,"old HTTP request reached held peer");
        {vd::Runtime::Attachment attached(*runtime);vd::Runtime::Managed managed(*runtime);
            require(runtime->collect()==vd::Runtime::Collection::completed&&runtime->snapshot().objects,"held HTTP roots survive precise GC");}
        auto replacement=original;const auto offset=replacement.find("value + 1");require(offset!=std::string::npos,"reload body marker");
        replacement.replace(offset,9,"value + 2");write(path,replacement);
        require(jit.replace(slot,*analyze(path),true),"compatible HTTP body replacement");old.reset();
        require(jit.live_generations()==2,"held HTTP activation lost its original generation");
        write(path,replacement+"\nstruct incompatible:\n    field: i32 = 0\n");
        require(!jit.replace(slot,*analyze(path),true),"incompatible HTTP module layout needs restart");write(path,replacement);
        require(get("/release").error.kind==vd::http::ErrorKind::success,"release old HTTP response");
        require(first_result.get()==8&&jit.live_generations()==1,"old HTTP task migrated or retained its generation after drain");
        // Resource acquisition remains on the original application owner. The
        // development host replaces code concurrently; it does not migrate IO.
        release.resumed=true;next.release();
        require(active.get()==9,"replacement HTTP activation did not use new body");
        {vd::Runtime::Attachment attached(*runtime);vd::Runtime::Managed managed(*runtime);
            require(runtime->collect()==vd::Runtime::Collection::completed&&runtime->snapshot().objects==0,"HTTP reload retained managed roots");}
        require(!runtime->close()&&runtime->snapshot().threads==0,"HTTP reload Runtime did not quiesce");control.close(client)->take();
        std::cout<<"HTTP cache, held streaming request, precise GC, compatible reload, incompatible restart and generation retirement passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
