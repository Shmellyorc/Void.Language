#include "performance_metrics.hpp"
#include "void/async.hpp"
#include "void/runtime.hpp"
#include <algorithm>
#include <atomic>
#include <mutex>
#include <condition_variable>
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
    void append_frames(const vd::RuntimeFailure& failure) noexcept {
        bool truncated=value.count>32 || failure.count>32;
        try{for(unsigned n=0;n<std::min(failure.count,32u);++n){if(frames.size()<32)frames.emplace_back(failure.frames[n]);else truncated=true;}}catch(...){truncated=true;}
        value.count=truncated?33:static_cast<unsigned>(frames.size());for(unsigned n=0;n<frames.size();++n)value.frames[n]=frames[n].c_str();
    }
    void append(const vd::RuntimeFailure& failure) noexcept {
        bool truncated=value.count>32;
        try{auto add=[&](const char* text){if(frames.size()<32)frames.emplace_back(text);else truncated=true;};add("Additional unobserved task failure:\n");add(failure.message);for(unsigned n=0;n<std::min(failure.count,32u);++n)add(failure.frames[n]);truncated|=failure.count>32;}
        catch(...){truncated=true;}
        value.count=truncated?33:static_cast<unsigned>(frames.size());for(unsigned n=0;n<frames.size();++n)value.frames[n]=frames[n].c_str();
    }
};
struct Task;
struct Group {Task* owner=nullptr;Group* outer=nullptr;std::uint64_t id=0;bool closed=false;};
struct Task {
    enum State { prepared,ready,running,waiting,joining,working,finishing,finished } state=prepared;
    enum Outcome { success,cancelled,failed } outcome=success;
    vd::Runtime::Handle task,frame;
    void* address=nullptr;
    vd_async_resume_entry resume=nullptr;
    Task* dependency=nullptr;
    Task* next_ready=nullptr;
    Group children;
    Group* group=nullptr;
    Group* active_group=&children;
    Group* joining_group=nullptr;
    struct Work {
        std::atomic<bool> done{false};
        bool cancelled=false,draining=false,completed=false;
        unsigned cleanup_depth=0;
        Diagnostic failure;
        std::shared_ptr<vd::NativeJob> native;
        bool abandoning = false;
    };
    std::unique_ptr<Work> work;
    Task* next_worker=nullptr;
    std::atomic<bool> cancel_requested{false};
    bool observed=false,cancel_observed=false,draining=false,child_interrupt=false;
    unsigned cleanup_depth=0;
    Diagnostic interrupted;
    Diagnostic failure;
    explicit Task(vd::Runtime& runtime):task(runtime),frame(runtime){children.owner=this;}
};
thread_local Task* worker_task=nullptr;
constexpr char worker_cancellation[]="Async task cancelled";
struct Scope {
    vd::Runtime& runtime;
    std::thread::id owner=std::this_thread::get_id();
    std::vector<std::unique_ptr<Task>> tasks;
    std::vector<std::unique_ptr<Group>> groups;
    Task* current=nullptr;
    Task* ready_head=nullptr;
    Task* ready_tail=nullptr;
    std::mutex work_mutex;
    std::condition_variable work_changed;
    std::vector<std::thread> workers;
    Task* work_head=nullptr;
    Task* work_tail=nullptr;
    unsigned admitted=0;
    bool stopping=false,startup_failed=false;
    const void* generation;
    ~Scope(){ perf::Timer perf_timer(perf::async_cleanup);perf::add(perf::retained_tasks,tasks.size()); 
        for(auto& task:perf::visit(tasks,perf::async_scan))task->cancel_requested=true;
        {std::lock_guard lock(work_mutex);stopping=true;}work_changed.notify_all();
        if(!workers.empty()){vd::Runtime::Native native(runtime);for(auto& worker:workers)if(worker.joinable())worker.join();}
    }
    void worker_loop() noexcept {
        bool announced=false;
        try {
            vd::Runtime::Attachment attachment(runtime);
            vd::Runtime::Managed managed(runtime); // Admit once; Runtime close must drain this participant.
            {std::lock_guard lock(work_mutex);++admitted;announced=true;}work_changed.notify_all();
            for(;;){Task* task;
                {vd::Runtime::Native native(runtime);std::unique_lock lock(work_mutex);work_changed.wait(lock,[&]{return stopping || work_head;});if(!work_head){if(stopping)return;continue;}task=work_head;work_head=task->next_worker;if(!work_head)work_tail=nullptr;}
                // Retained dynamic diagnostics must survive every reached
                // cleanup on this worker, independently of the executor's TLS.
                vd::NativeCode::LeaseScope failure_scope({});
                worker_task=task;vd::RuntimeFailure failure;
                if (task->work->native) {
                    vd::Runtime::Native safe(runtime);
                    if (task->work->abandoning) {
                        task->work->native->abandon(&failure);
                    } else if (task->cancel_requested.load(std::memory_order_relaxed)) {
                        task->work->native->cancel_queued();
                        task->work->cancelled = true;
                    } else {
                        {perf::Timer native_execution(perf::native_execute);task->work->native->execute(task->cancel_requested, &failure);}
                    }
                } else {
                    task->resume(task->frame.get(),&failure);
                    if(!failure.message && !task->work->completed)failure.message="Worker returned without checked completion";
                }
                task->work->failure.assign(failure);worker_task=nullptr;
                // Only the executor reads the private payload after this release.
                {std::lock_guard lock(work_mutex);task->work->done.store(true,std::memory_order_release);}work_changed.notify_all();
            }
        }catch(...){worker_task=nullptr;std::lock_guard lock(work_mutex);startup_failed=true;if(!announced)++admitted;work_changed.notify_all();}
    }
    void ensure_workers(){
        if(!workers.empty())return;
        auto count=runtime.async_worker_limit();workers.reserve(count);
        for(unsigned n=0;n<count;++n)workers.emplace_back([this]{worker_loop();});
        {vd::Runtime::Native native(runtime);std::unique_lock lock(work_mutex);work_changed.wait(lock,[&]{return admitted==count;});if(startup_failed)throw std::logic_error("Worker Runtime admission failed");}
    }
    void publish(Task& task){
        ensure_workers();task.state=Task::working;task.next_worker=nullptr;
        {std::lock_guard lock(work_mutex);if(startup_failed)throw std::logic_error("Worker unavailable");if(work_tail)work_tail->next_worker=&task;else work_head=&task;work_tail=&task;}work_changed.notify_one();
    }
    bool work_pending() const noexcept {for(const auto& task:perf::visit(tasks,perf::async_scan))if(task->state==Task::working)return true;return false;}
    void receive() noexcept { perf::Timer perf_timer(perf::async_receive_time); 
        for(auto& task:perf::visit(tasks,perf::async_receive))if(task->state==Task::working && task->work->done.load(std::memory_order_acquire)){
            if (task->work->native) {
                receive_native(*task);
                continue;
            }
            task->state=Task::finished;const auto& failure=task->work->failure.value;
            if(failure.message){task->failure.assign(failure);task->outcome=task->work->cancelled?Task::cancelled:Task::failed;if(task->outcome==Task::cancelled)task->failure.value={};}
            else{std::int32_t complete=1;std::memcpy(task->address,&complete,sizeof(complete));}
            failed_child(*task);
            // The acquire handoff proves the worker has returned from resume,
            // removed its scratch roots and stopped accessing this frame.
            task->frame.reset(nullptr);
        }
    }
    void abandon(Task& task) noexcept {
        task.work->abandoning = true;
        task.work->done.store(false, std::memory_order_relaxed);
        // Workers were admitted before this resource could be acquired. Reuse
        // the completed job and intrusive queue; cleanup cannot allocate.
        task.state = Task::working;
        task.next_worker = nullptr;
        {
            std::lock_guard lock(work_mutex);
            if (work_tail) work_tail->next_worker = &task;
            else work_head = &task;
            work_tail = &task;
        }
        work_changed.notify_one();
    }
    void receive_native(Task& task) noexcept {
        auto& work = *task.work;
        if (work.failure.value.message) {
            if (task.failure.value.message) task.failure.append(work.failure.value);
            else task.failure.assign(work.failure.value);
            task.outcome = Task::failed;
        }
        if (!work.abandoning && task.outcome != Task::failed) {
            if (work.cancelled || task.cancel_requested.load(std::memory_order_relaxed)) {
                task.outcome = Task::cancelled;
            } else {
                try {
                    work.native->publish(runtime, task.address);
                } catch (const std::bad_alloc&) {
                    vd::RuntimeFailure failure;
                    failure.message = "Native job result allocation failed";
                    task.failure.assign(failure);
                    task.outcome = Task::failed;
                } catch (...) {
                    vd::RuntimeFailure failure;
                    failure.message = "Native job result publication failed";
                    task.failure.assign(failure);
                    task.outcome = Task::failed;
                }
            }
        }
        if (!work.abandoning && task.outcome != Task::success && work.native->acquired()) {
            abandon(task);
            return;
        }
        task.state = Task::finished;
        if (task.outcome == Task::success) {
            std::int32_t complete = 1;
            std::memcpy(task.address, &complete, sizeof(complete));
        }
        failed_child(task);
    }
    void finish_native_children(Task& owner) noexcept { perf::Timer perf_timer(perf::async_children_time); 
        bool pending_cleanup = false;
        for (auto& child :perf::visit(tasks,perf::async_children)) {
            if (!owned(*child, owner) || !child->work || !child->work->native) continue;
            auto& work = *child->work;
            if (child->state == Task::working) {
                pending_cleanup = true;
            } else if (work.native->acquired()) {
                if (owner.outcome == Task::success) work.native->commit();
                else {
                    abandon(*child);
                    pending_cleanup = true;
                }
            }
        }
        if (pending_cleanup) owner.state = Task::finishing;
        else {
            collect(owner);
            owner.state = Task::finished;
            failed_child(owner);
            owner.frame.reset(nullptr);
        }
    }
    void wait_work(){ perf::Timer perf_timer(perf::async_wait); 
        vd::Runtime::Native native(runtime);std::unique_lock lock(work_mutex);
        work_changed.wait(lock,[&]{if(startup_failed)return true;for(const auto& task:perf::visit(tasks,perf::async_work))if(task->state==Task::working && task->work->done.load(std::memory_order_acquire))return true;return false;});
        if(startup_failed)throw std::logic_error("Worker execution failed");
    }

    Scope(vd::Runtime& owner,const void* identity):runtime(owner),generation(identity){}
    Task& find(void* address) { perf::Timer perf_timer(perf::async_find_time); 
        for(auto& task:perf::visit(tasks,perf::async_find))if(task->address==address)return *task;
        throw std::logic_error("Task does not belong to the active root adapter");
    }
    void enqueue(Task& task) noexcept {
        task.state=Task::ready;task.next_ready=nullptr;
        if(ready_tail)ready_tail->next_ready=&task;else ready_head=&task;
        ready_tail=&task;
    }
    Task* pop() noexcept {auto* task=ready_head;if(task){ready_head=task->next_ready;if(!ready_head)ready_tail=nullptr;task->next_ready=nullptr;}return task;}
    bool member(const Task& task,const Group* group) const noexcept {return task.group==group;}
    bool owned(const Task& task,const Task& owner) const noexcept {return task.group && task.group->owner==&owner;}
    bool pending(const Task& owner,const Group* group=nullptr) const noexcept { perf::Timer perf_timer(perf::async_pending_time); 
        for(auto& task:perf::visit(tasks,perf::async_pending))if((group?member(*task,group):owned(*task,owner)) && task->state!=Task::prepared && task->state!=Task::finished)return true;
        return false;
    }
    void cancel_children(Task& owner,Group* group=nullptr) noexcept { perf::Timer perf_timer(perf::async_children_time); 
        for(auto& task:perf::visit(tasks,perf::async_children))if((group?member(*task,group):owned(*task,owner)) && task->state!=Task::finished)task->cancel_requested=true;
    }
    Task* child_failure(Task& owner,Group* group=nullptr) noexcept { perf::Timer perf_timer(perf::async_failure_time); 
        for(auto& task:perf::visit(tasks,perf::async_failure))if((group?member(*task,group):owned(*task,owner)) && task->state!=Task::prepared && task->outcome==Task::failed && !task->observed)return task.get();
        return nullptr;
    }
    void collect(Task& owner,Group* group=nullptr) noexcept { perf::Timer perf_timer(perf::async_collect_time); 
        for(auto& task:perf::visit(tasks,perf::async_collect))if((group?member(*task,group):owned(*task,owner)) && task->outcome==Task::failed && !task->observed){
            if(owner.outcome!=Task::failed){owner.failure.assign(task->failure.value);owner.outcome=Task::failed;}
            else owner.failure.append(task->failure.value);
            task->observed=true;
        }
    }
    void failed_child(Task& child) noexcept {
        if(child.group && child.outcome==Task::failed){auto& owner=*child.group->owner;cancel_children(owner,child.group);}
    }
};
thread_local Scope* active=nullptr;
// NativeCode::invoke/AOT copies or prints this owned export immediately after
// the adapter returns. Per-activation failures themselves are owned by Task.
thread_local Diagnostic exported_failure;
Scope& scope(){if(!active || active->owner!=std::this_thread::get_id())throw std::logic_error("Async calls require an explicit owning root adapter");return *active;}
void exception(vd::RuntimeFailure* failure,const char* message) noexcept {failure->message=message;}
}
extern "C" {
void vd_async_new(void** task,void** frame,std::size_t frame_size,const std::size_t* frame_offsets,std::size_t frame_count,std::size_t task_size,const std::size_t* task_offsets,std::size_t task_count,const char* task_key,const void* generation,vd_async_resume_entry resume,vd::RuntimeFailure* failure,std::int32_t worker) noexcept { perf::Timer perf_timer(perf::async_new); 
    try {
        auto& s=scope();if(s.current && (s.current->draining || s.current->cleanup_depth)){exception(failure,"Async calls cannot enqueue during structured cleanup");return;}if(s.generation!=generation)throw std::logic_error("async generation mismatch");auto item=std::make_unique<Task>(s.runtime);item->resume=resume;if(worker)item->work=std::make_unique<Task::Work>();item->group=s.current?s.current->active_group:nullptr;
        vd::TypeLayout task_layout{task_size,{}},frame_layout{frame_size,{}};
        if(task_count)task_layout.references.assign(task_offsets,task_offsets+task_count);
        if(frame_count)frame_layout.references.assign(frame_offsets,frame_offsets+frame_count);
        task_layout.descriptor=s.runtime.descriptor(task_key);
        s.runtime.allocate_into(task,task_layout);item->task.reset(*task);item->address=*task;
        s.runtime.allocate_into(frame,frame_layout);item->frame.reset(*frame);
        s.tasks.push_back(std::move(item));perf::add(perf::async_tasks);
    }catch(const std::bad_alloc&){exception(failure,"Async activation allocation failed");}
     catch(const std::exception&){exception(failure,"Async activation requires a valid owning root adapter");}
}
void vd_async_start(void* address,vd::RuntimeFailure* failure) noexcept {
    try{auto& s=scope();auto& task=s.find(address);if(task.state!=Task::prepared)throw std::logic_error("duplicate async start");if(task.work)s.publish(task);else s.enqueue(task);}
    catch(...){exception(failure,"Async activation publication failed");}
}
bool vd_async_ready(void* address,vd::RuntimeFailure* failure) noexcept {
    try {
        auto& s=scope();if(!s.current)throw std::logic_error("await outside activation");
        vd_async_checkpoint(failure);if(failure->message)return true;
        auto& task=s.find(address);
        if(&task==s.current){exception(failure,"Task cannot await itself");return true;}
        for(auto* owner=s.current->group? s.current->group->owner:nullptr;owner;owner=owner->group?owner->group->owner:nullptr)if(owner==&task && task.state!=Task::finished){exception(failure,"Async await dependency cycle");return true;}
        if(task.state!=Task::finished)return false;
        task.observed=true;if(task.outcome==Task::cancelled){s.current->cancel_observed=true;exception(failure,"Async task cancelled");}else if(task.failure.value.message)*failure=task.failure.value;
        return true;
    }catch(...){exception(failure,"Invalid task await");return true;}
}
void vd_async_suspend(void* frame,void* address,vd::RuntimeFailure* failure) noexcept {
    try{auto& s=scope();if(!s.current || s.current->state!=Task::running || s.current->frame.get()!=frame)throw std::logic_error("invalid suspension");auto* dependency=&s.find(address);
        for(auto* next=dependency;next;next=next->state==Task::waiting?next->dependency:nullptr)if(next==s.current)throw std::logic_error("cyclic suspension");
        s.current->dependency=dependency;s.current->state=Task::waiting;}
    catch(...){exception(failure,"Invalid async suspension");}
}
void vd_async_complete(void* address) noexcept {
    if(worker_task){if(worker_task->address!=address || worker_task->work->completed)std::terminate();worker_task->work->completed=true;return;}
    try{auto& s=scope();if(!s.current || s.current->address!=address || s.current->state!=Task::running)std::terminate();std::int32_t complete=1;std::memcpy(address,&complete,sizeof(complete));s.current->state=Task::finished;}
    catch(...){std::terminate();}
}
void vd_async_cancel(void* address,vd::RuntimeFailure* failure) noexcept {
    try{auto& s=scope();auto& task=s.find(address);if(task.state!=Task::finished)task.cancel_requested=true;}
    catch(...){exception(failure,"Invalid task cancellation");}
}
void vd_async_cleanup_enter() noexcept {if(worker_task){++worker_task->work->cleanup_depth;return;}if(active && active->current)++active->current->cleanup_depth;}
void vd_async_cleanup_leave() noexcept {if(worker_task){if(!worker_task->work->cleanup_depth)std::terminate();--worker_task->work->cleanup_depth;return;}if(active && active->current){if(!active->current->cleanup_depth)std::terminate();--active->current->cleanup_depth;}}
void vd_async_cancel_children() noexcept {try{auto& s=scope();if(!s.current)std::terminate();s.cancel_children(*s.current);}catch(...){std::terminate();}}
void vd_async_finish(vd::RuntimeFailure* failure) noexcept {
    if(worker_task)return;
    try{auto& task=*scope().current;
        if(task.outcome==Task::cancelled){if(failure->message){task.failure.assign(*failure);task.outcome=Task::failed;}else exception(failure,"Async task cancelled");}
    }catch(...){exception(failure,"Invalid async completion outcome");}
}
bool vd_worker_cleanup_failure(vd::RuntimeFailure* primary,const vd::RuntimeFailure* cleanup) noexcept {
    if(!worker_task || !worker_task->work->cancelled || primary->message!=worker_cancellation || !cleanup->message)return false;
    worker_task->work->cancelled=false;*primary=*cleanup;return true;
}
bool vd_worker_checkpoint(vd::RuntimeFailure* failure) noexcept {
    if(!worker_task || failure->message || worker_task->work->draining || worker_task->work->cleanup_depth)return false;
    if(worker_task->cancel_requested.load(std::memory_order_relaxed)){worker_task->work->cancelled=true;exception(failure,worker_cancellation);return true;}
    return false;
}
void vd_async_checkpoint(vd::RuntimeFailure* failure) noexcept {
    if(worker_task){vd_worker_checkpoint(failure);return;}
    try{auto& s=scope();s.receive();auto& task=*s.current;if(task.draining)return;
        if(task.failure.value.message){*failure=task.failure.value;return;}
        // Child failure is selected only after the shielded join, in creation order.
        if(s.child_failure(task)){task.child_interrupt=true;exception(failure,"Async child failure requires structured cleanup");return;}
        if(task.cancel_requested){task.cancel_observed=true;exception(failure,"Async task cancelled");}
    }catch(...){exception(failure,"Invalid async checkpoint");}
}
std::uint64_t vd_async_group_enter(vd::RuntimeFailure* failure) noexcept {
    try{auto& s=scope();if(!s.current || s.current->draining)throw std::logic_error("invalid group entry");
        auto group=std::make_unique<Group>();group->owner=s.current;group->outer=s.current->active_group;group->id=s.groups.size()+1;
        auto* address=group.get();s.groups.push_back(std::move(group));s.current->active_group=address;return address->id;
    }catch(...){exception(failure,"Async group allocation or entry failed");return 0;}
}
bool vd_async_group_join(std::uint64_t id,std::int32_t cancel,void* frame,vd::RuntimeFailure* failure) noexcept {
    try{auto& s=scope();if(!s.current || s.current->frame.get()!=frame || !id || id>s.groups.size())throw std::logic_error("invalid group join");
        auto& task=*s.current;auto* group=s.groups[id-1].get();if(group->owner!=&task || task.active_group!=group || group->closed)throw std::logic_error("invalid group ownership");
        if(cancel || task.cancel_requested || s.child_failure(task,group))s.cancel_children(task,group);
        if(s.pending(task,group)){task.joining_group=group;task.state=Task::joining;return false;}
        group->closed=true;task.active_group=group->outer;task.joining_group=nullptr;
        // Select failures across all owned groups at the activation drain, so a
        // later lexical group cannot outrank an earlier-created outer child.
        if(s.child_failure(task,group)){task.child_interrupt=true;exception(failure,"Async child failure requires structured cleanup");}
        else if(task.cancel_requested){task.cancel_observed=true;exception(failure,"Async task cancelled");}
        return true;
    }catch(...){exception(failure,"Invalid async group join");return true;}
}
bool vd_async_drain(void* frame,vd::RuntimeFailure* failure) noexcept {
    if(worker_task){worker_task->work->draining=true;return true;}
    try{auto& s=scope();auto& task=*s.current;if(task.frame.get()!=frame)throw std::logic_error("invalid drain frame");
        if(!task.draining){
            task.draining=true;
            if(failure->message && task.failure.value.message)task.failure.assign(*failure);
            if(failure->message && !task.failure.value.message){
                if(task.child_interrupt)task.interrupted.assign(*failure);
                else if(task.cancel_observed)task.outcome=Task::cancelled;
                else {task.failure.assign(*failure);task.outcome=Task::failed;}
            }
        }
        if(task.outcome==Task::failed)s.failed_child(task);
        *failure={};
        if(task.outcome!=Task::success || s.child_failure(task))s.cancel_children(task);
        if(s.pending(task)){task.joining_group=nullptr;task.state=Task::joining;return false;}
        s.collect(task);if(task.child_interrupt){task.failure.append_frames(task.interrupted.value);task.child_interrupt=false;}task.active_group=&task.children;
        if(task.outcome==Task::failed)*failure=task.failure.value;
        return true;
    }catch(...){exception(failure,"Invalid async structured drain");return true;}
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
            s.receive();
            bool retired=false;
            for(auto& task:perf::visit(s.tasks,perf::async_scan)){
                if(task->state==Task::waiting && (task->dependency->state==Task::finished || task->cancel_requested || s.child_failure(*task)))s.enqueue(*task);
                else if(task->state==Task::joining && !s.pending(*task,task->joining_group))s.enqueue(*task);
                else if (task->state == Task::finishing && !s.pending(*task)) {
                    s.finish_native_children(*task);
                    retired |= task->state == Task::finished;
                }
            }
            Task* next=s.pop();bool pending=false;
            for(auto& task:perf::visit(s.tasks,perf::async_scan)){pending|=task->state!=Task::finished && task->state!=Task::prepared;}
            if(!next){
                if(!pending)break;
                // A later child may have just retired after an earlier parent's
                // readiness check. Rescan before diagnosing a dependency cycle.
                if(retired)continue;
                if(s.work_pending()){s.wait_work();continue;}
                // Resume through the checked await failure edge so reached
                // cleanup runs even for an invalid cyclic dependency graph.
                vd::RuntimeFailure cycle;cycle.message="Async await dependency cycle";
                for(auto& task:perf::visit(s.tasks,perf::async_scan))if(task->state==Task::waiting){task->failure.assign(cycle);task->outcome=Task::failed;s.enqueue(*task);}
                continue;
            }
            s.current=next;next->state=Task::running;vd::RuntimeFailure result;
            {perf::Timer timed_resume(perf::async_resume);next->resume(next->frame.get(),&result);}
            if(result.message){if(next->outcome==Task::failed)next->failure.assign(result);else {if(next->cancel_observed)next->outcome=Task::cancelled;else{next->failure.assign(result);next->outcome=Task::failed;}}next->state=Task::finished;}
            else if(next->state==Task::running){result.message="Async activation returned without completion or suspension";next->failure.assign(result);next->outcome=Task::failed;next->state=Task::finished;}
            s.failed_child(*next);s.current=nullptr;
            // Completion alone is insufficient: generated return/cleanup still
            // uses the activation. Retire its root only after resume returned.
            // Result handles, diagnostics, ordering records and the enclosing
            // generation lease remain intact for repeated awaits and drains.
            if(next->state==Task::finished)s.finish_native_children(*next);
        }
        const vd::RuntimeFailure* outcome=initial.message?&root_failure.value:root && root->failure.value.message?&root->failure.value:nullptr;
        if(!outcome)for(auto& task:perf::visit(s.tasks,perf::async_scan))if(task->failure.value.message && !task->observed){outcome=&task->failure.value;break;}
        std::int32_t value=0;
        if(!outcome && root && root->outcome==Task::cancelled){exception(failure,"Async task cancelled");return 0;}
        if(outcome){exported_failure.assign(*outcome);for(auto& task:perf::visit(s.tasks,perf::async_scan))if(task.get()!=root && &task->failure.value!=outcome && task->failure.value.message && !task->observed)exported_failure.append(task->failure.value);*failure=exported_failure.value;}
        else if(root)std::memcpy(&value,static_cast<const char*>(root->task.get())+result_offset,sizeof(value));
        return value;
    }catch(const std::bad_alloc&){exception(failure,"Async scheduler allocation failed");}
     catch(...){exception(failure,"Async scheduler invariant failed");}
    active=nullptr;return 0;
}
}

namespace vd {
const void* async_execution_context() noexcept {return active?active->current:nullptr;}

bool async_context_descends_from(const void* ancestor) noexcept {
    for (auto* task = active ? active->current : nullptr; task;
         task = task->group ? task->group->owner : nullptr) {
        if (task == ancestor) return true;
    }
    return false;
}

void async_submit_native(void** output, const TypeLayout& layout,
                         const void* generation, std::shared_ptr<NativeJob> job) { perf::Timer perf_timer(perf::async_new); 
    auto& executor = scope();
    if (!executor.current || executor.current->draining || executor.current->cleanup_depth) {
        throw std::logic_error("Native jobs cannot enqueue outside an active async body");
    }
    if (executor.generation != generation || !job) {
        throw std::logic_error("Invalid native job generation or payload");
    }
    auto item = std::make_unique<Task>(executor.runtime);
    item->work = std::make_unique<Task::Work>();
    item->work->native = std::move(job);
    item->group = executor.current->active_group;
    executor.runtime.allocate_into(output, layout);
    item->task.reset(*output);
    item->address = *output;
    executor.tasks.push_back(std::move(item));perf::add(perf::async_tasks);
    try {
        executor.publish(*executor.tasks.back());
    } catch (...) {
        // No native operation has been queued if admission fails. Retire the
        // reserved file-operation count before source cleanup can run.
        executor.tasks.back()->work->native->cancel_queued();
        executor.tasks.pop_back();
        *output = nullptr;
        throw;
    }
}
}
