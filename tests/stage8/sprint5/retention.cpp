// Compile the identical observer with either the recorded baseline or current
// scheduler. Static archive extraction leaves this TU's scheduler as the only one.
#include "void/async.hpp"
#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/native_runtime.hpp"
#include "../../native/probe.hpp"
#include "../../managed/probe.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <filesystem>
#if defined(__linux__)
#include <unistd.h>
#endif
#define vd_async_new measured_async_new
#define vd_async_root measured_async_root
#include VOID_ASYNC_IMPLEMENTATION
#undef vd_async_new
#undef vd_async_root
namespace {
struct Shape {std::size_t frame,task;};
std::map<vd_async_resume_entry,Shape> shapes;
std::size_t frame_allocations=0,frame_allocated_bytes=0;
std::string label,mode;
vd::Jit* jit_owner=nullptr;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
std::size_t rss(){
#if defined(__linux__)
    std::ifstream in("/proc/self/statm");std::size_t total=0,resident=0;in>>total>>resident;return resident*static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
#else
    return 0;
#endif
}
void sample(const char* phase,int batch,bool collect){
    auto& runtime=vd::Runtime::current();if(collect)require(runtime.collect()==vd::Runtime::Collection::completed,"retention collection failed");
    std::size_t tasks=0,finished=0,frames=0,frame_bytes=0,work=0,task_bytes=0;
    if(active)for(const auto& task:active->tasks){++tasks;task_bytes+=shapes.at(task->resume).task;finished+=task->state==Task::finished;if(task->frame.get()){++frames;frame_bytes+=shapes.at(task->resume).frame;}work+=bool(task->work);}
    const auto memory=runtime.memory_snapshot();auto lease=vd::NativeCode::active_lease();
    std::cout<<"{\"variant\":\""<<VOID_RETENTION_VARIANT<<"\",\"mode\":\""<<mode<<"\",\"workload\":\""<<label<<"\",\"phase\":\""<<phase<<"\",\"batch\":"<<batch
     <<",\"tasks\":"<<tasks<<",\"completed_records\":"<<finished<<",\"record_bytes\":"<<tasks*sizeof(Task)<<",\"task_capacity\":"<<(active?active->tasks.capacity():0)
     <<",\"groups\":"<<(active?active->groups.size():0)<<",\"group_capacity\":"<<(active?active->groups.capacity():0)<<",\"worker_records\":"<<work
     <<",\"rooted_frames\":"<<frames<<",\"rooted_frame_bytes\":"<<frame_bytes<<",\"frame_allocations\":"<<frame_allocations<<",\"frame_allocated_bytes\":"<<frame_allocated_bytes
     <<",\"task_payload_bytes\":"<<task_bytes<<",\"worker_record_bytes\":"<<work*sizeof(Task::Work)<<",\"group_record_bytes\":"<<(active?active->groups.size()*sizeof(Group):0)<<",\"native_heap_execution_frames\":0,\"objects\":"<<memory.heap.objects<<",\"payload_bytes\":"<<memory.heap.payload_bytes<<",\"heap_buckets\":"<<memory.heap.buckets
     <<",\"nominal_types\":"<<memory.types<<",\"schemas\":"<<memory.schemas<<",\"witnesses\":"<<memory.witnesses<<",\"persistent_handles\":"<<memory.handles
     <<",\"handle_capacity\":"<<memory.handle_capacity<<",\"participants\":"<<memory.participants<<",\"callbacks\":"<<memory.callbacks
     <<",\"generations\":"<<(jit_owner?jit_owner->live_generations():0)<<",\"active_lease_refs_including_observer\":"<<lease.use_count()<<",\"rss\":"<<rss()<<"}\n";
}
void marker(std::int32_t value){if(value>=0){sample("awaited",value,false);sample("awaited_gc",value,true);}else if(value==-1)sample("group_exit_gc",value,true);else if(value==-2)sample("child_running_gc",value,true);}
}
extern "C" void vd_async_new(void** task,void** frame,std::size_t size,const std::size_t* offsets,std::size_t count,std::size_t task_size,const std::size_t* task_offsets,std::size_t task_count,const char* key,const void* generation,vd_async_resume_entry resume,vd::RuntimeFailure* failure,std::int32_t worker) noexcept {
    measured_async_new(task,frame,size,offsets,count,task_size,task_offsets,task_count,key,generation,resume,failure,worker);
    if(!failure->message){try{shapes[resume]={size,task_size};++frame_allocations;frame_allocated_bytes+=size;}catch(...){std::terminate();}}
}
extern "C" std::int32_t vd_async_root(vd_async_start_entry start,std::int32_t input,std::size_t offset,const void* generation,vd::RuntimeFailure* failure) noexcept {
    auto result=measured_async_root(start,input,offset,generation,failure);
    sample("root_drained",input,false);sample("root_drained_gc",input,true);
    if(vd::Runtime::current().snapshot().objects)std::terminate();
    return result;
}
#ifdef VOID_RETENTION_AOT
namespace {struct Observe {Observe(){mode=VOID_RETENTION_MODE;label=VOID_RETENTION_WORKLOAD;native_test::scalar_poll=marker;}} observe;}
#else
int main(int argc,char** argv){try{
    if(argc==5 && std::string(argv[1])=="--emit") {auto a=vd::analyze_project(argv[2],vd::native_fixture_project());if(!a.succeeded())throw vd::CompilationError(a.diagnostics);vd::emit_async_object(a.program->lower(),argv[3],std::string(argv[4])=="optimized");return 0;}
    require(argc==4,"source, normal/optimized, and batch size required");mode=argv[2];auto descriptor=vd::native_fixture_project();auto analysis=vd::analyze_project(argv[1],descriptor);if(!analysis.succeeded())throw vd::CompilationError(analysis.diagnostics);
    label=std::filesystem::path(argv[1]).stem().string();vd::Jit jit;jit_owner=&jit;auto code=jit.compile_async(*analysis.program,mode=="optimized");auto runtime=code->runtime_owner();runtime->async_worker_limit(2);
    native_test::scalar_poll=marker;auto result=code->invoke(std::stoi(argv[3]));native_test::scalar_poll=nullptr;require(result==7,"retention result/repeated await changed");
    require(!vd::native_fixture_live(),"retention native resource leaked");return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}

#endif
