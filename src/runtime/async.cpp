#include "void/async.hpp"
#include "void/runtime.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <stdexcept>
#include <thread>
#include <vector>
namespace {
struct Diagnostic {
    std::string message;
    std::vector<std::string> frames;
    vd::RuntimeFailure value;
    void assign(const vd::RuntimeFailure& failure) noexcept {
        try {
            std::string next_message=failure.message?failure.message:"";std::vector<std::string> next_frames;
            for(unsigned n=0;n<std::min(failure.count,32u);++n)next_frames.emplace_back(failure.frames[n]);
            message.swap(next_message);frames.swap(next_frames);
            value={};value.message=failure.message?message.c_str():nullptr;value.count=failure.count;
            for(unsigned n=0;n<frames.size();++n)value.frames[n]=frames[n].c_str();
        }catch(...){value={};value.message="Async failure diagnostic allocation failed";}
    }
    void append(const vd::RuntimeFailure& failure) noexcept {
        bool truncated=value.count>32;
        try{auto add=[&](const char* text){if(frames.size()<32)frames.emplace_back(text);else truncated=true;};add("Additional unobserved task failure:\n");add(failure.message);for(unsigned n=0;n<std::min(failure.count,32u);++n)add(failure.frames[n]);truncated|=failure.count>32;}
        catch(...){truncated=true;}
        value.count=truncated?33:static_cast<unsigned>(frames.size());for(unsigned n=0;n<frames.size();++n)value.frames[n]=frames[n].c_str();
    }
};
struct Task {
    enum State { prepared,ready,running,waiting,finished } state=prepared;
    vd::Runtime::Handle task,frame;
    void* address=nullptr;
    vd_async_resume_entry resume=nullptr;
    Task* dependency=nullptr;
    Task* next_ready=nullptr;
    bool observed=false;
    Diagnostic failure;
    explicit Task(vd::Runtime& runtime):task(runtime),frame(runtime){}
};
struct Scope {
    vd::Runtime& runtime;
    std::thread::id owner=std::this_thread::get_id();
    std::vector<std::unique_ptr<Task>> tasks;
    Task* current=nullptr;
    Task* ready_head=nullptr;
    Task* ready_tail=nullptr;
    const void* generation;
    Scope(vd::Runtime& owner,const void* identity):runtime(owner),generation(identity){}
    Task& find(void* address) {
        for(auto& task:tasks)if(task->address==address)return *task;
        throw std::logic_error("Task does not belong to the active root adapter");
    }
    void enqueue(Task& task) noexcept {
        task.state=Task::ready;task.next_ready=nullptr;
        if(ready_tail)ready_tail->next_ready=&task;else ready_head=&task;
        ready_tail=&task;
    }
    Task* pop() noexcept {auto* task=ready_head;if(task){ready_head=task->next_ready;if(!ready_head)ready_tail=nullptr;task->next_ready=nullptr;}return task;}

};
thread_local Scope* active=nullptr;
// NativeCode::invoke/AOT copies or prints this owned export immediately after
// the adapter returns. Per-activation failures themselves are owned by Task.
thread_local Diagnostic exported_failure;
Scope& scope(){if(!active || active->owner!=std::this_thread::get_id())throw std::logic_error("Async calls require an explicit owning root adapter");return *active;}
void exception(vd::RuntimeFailure* failure,const char* message) noexcept {failure->message=message;}
}
extern "C" {
void vd_async_new(void** task,void** frame,std::size_t frame_size,const std::size_t* frame_offsets,std::size_t frame_count,std::size_t task_size,const std::size_t* task_offsets,std::size_t task_count,const char* task_key,const void* generation,vd_async_resume_entry resume,vd::RuntimeFailure* failure) noexcept {
    try {
        auto& s=scope();if(s.generation!=generation)throw std::logic_error("async generation mismatch");auto item=std::make_unique<Task>(s.runtime);item->resume=resume;
        vd::TypeLayout task_layout{task_size,{}},frame_layout{frame_size,{}};
        if(task_count)task_layout.references.assign(task_offsets,task_offsets+task_count);
        if(frame_count)frame_layout.references.assign(frame_offsets,frame_offsets+frame_count);
        task_layout.descriptor=s.runtime.descriptor(task_key);
        s.runtime.allocate_into(task,task_layout);item->task.reset(*task);item->address=*task;
        s.runtime.allocate_into(frame,frame_layout);item->frame.reset(*frame);
        s.tasks.push_back(std::move(item));
    }catch(const std::bad_alloc&){exception(failure,"Async activation allocation failed");}
     catch(const std::exception&){exception(failure,"Async activation requires a valid owning root adapter");}
}
void vd_async_start(void* address,vd::RuntimeFailure* failure) noexcept {
    try{auto& s=scope();auto& task=s.find(address);if(task.state!=Task::prepared)throw std::logic_error("duplicate async start");s.enqueue(task);}
    catch(...){exception(failure,"Async activation publication failed");}
}
bool vd_async_ready(void* address,vd::RuntimeFailure* failure) noexcept {
    try {
        auto& s=scope();if(!s.current)throw std::logic_error("await outside activation");
        if(s.current->failure.value.message){*failure=s.current->failure.value;return true;}
        auto& task=s.find(address);
        if(&task==s.current){exception(failure,"Task cannot await itself");return true;}
        if(task.state!=Task::finished)return false;
        task.observed=true;if(task.failure.value.message)*failure=task.failure.value;
        return true;
    }catch(...){exception(failure,"Invalid task await");return true;}
}
void vd_async_suspend(void* frame,void* address,vd::RuntimeFailure* failure) noexcept {
    try{auto& s=scope();if(!s.current || s.current->state!=Task::running || s.current->frame.get()!=frame)throw std::logic_error("invalid suspension");s.current->dependency=&s.find(address);s.current->state=Task::waiting;}
    catch(...){exception(failure,"Invalid async suspension");}
}
void vd_async_complete(void* address) noexcept {
    try{auto& s=scope();if(!s.current || s.current->address!=address || s.current->state!=Task::running)std::terminate();std::int32_t complete=1;std::memcpy(address,&complete,sizeof(complete));s.current->state=Task::finished;}
    catch(...){std::terminate();}
}
std::int32_t vd_async_root(vd_async_start_entry start,std::int32_t input,std::size_t result_offset,const void* generation,vd::RuntimeFailure* failure) noexcept {
    if(active){exception(failure,"Nested async root adapters are unsupported");return 0;}
    try {
        auto& runtime=vd::Runtime::current();if(!runtime.enter_async()){exception(failure,"Runtime already has an owning async executor or is closing");return 0;}
        struct Ownership {vd::Runtime& runtime;~Ownership(){runtime.leave_async();}} ownership{runtime};
        Scope s(runtime,generation);active=&s;
        struct Reset {~Reset(){active=nullptr;}} reset;
        vd::RuntimeFailure initial;void* root_address=start(input,&initial);
        Diagnostic root_failure;if(initial.message)root_failure.assign(initial);
        Task* root=nullptr;if(root_address)root=&s.find(root_address);
        for(;;) {
            for(auto& task:s.tasks)if(task->state==Task::waiting && task->dependency->state==Task::finished)s.enqueue(*task);
            Task* next=s.pop();bool pending=false;
            for(auto& task:s.tasks){pending|=task->state!=Task::finished && task->state!=Task::prepared;}
            if(!next){
                if(!pending)break;
                // Resume through the checked await failure edge so reached
                // cleanup runs even for an invalid cyclic dependency graph.
                vd::RuntimeFailure cycle;cycle.message="Async await dependency cycle";
                for(auto& task:s.tasks)if(task->state==Task::waiting){task->failure.assign(cycle);s.enqueue(*task);}
                continue;
            }
            s.current=next;next->state=Task::running;vd::RuntimeFailure result;
            next->resume(next->frame.get(),&result);
            if(result.message){next->failure.assign(result);next->state=Task::finished;}
            else if(next->state==Task::running){result.message="Async activation returned without completion or suspension";next->failure.assign(result);next->state=Task::finished;}
            s.current=nullptr;
        }
        const vd::RuntimeFailure* outcome=initial.message?&root_failure.value:root && root->failure.value.message?&root->failure.value:nullptr;
        if(!outcome)for(auto& task:s.tasks)if(task->failure.value.message && !task->observed){outcome=&task->failure.value;break;}
        std::int32_t value=0;
        if(outcome){exported_failure.assign(*outcome);for(auto& task:s.tasks)if(task.get()!=root && &task->failure.value!=outcome && task->failure.value.message && !task->observed)exported_failure.append(task->failure.value);*failure=exported_failure.value;}
        else if(root)std::memcpy(&value,static_cast<const char*>(root->task.get())+result_offset,sizeof(value));
        return value;
    }catch(const std::bad_alloc&){exception(failure,"Async scheduler allocation failed");}
     catch(...){exception(failure,"Async scheduler invariant failed");}
    active=nullptr;return 0;
}
}
