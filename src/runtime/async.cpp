#include "void/async.hpp"
#include "void/runtime.hpp"
#include "void/native_work.hpp"
#include <algorithm>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <limits>
#include <memory>
#include <list>
#include <optional>
#include <string>
#include <stdexcept>
#include <thread>
#include <unordered_map>
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
using NativeWake = vd::NativeWake;
}
namespace vd {
struct NativeCompletion::State {
    std::atomic<unsigned> phase{0}; // pending, completing, quiescent
    std::atomic<bool> cancel_requested{false};
    ::Diagnostic failure;
    std::shared_ptr<NativeWake> wake;
    explicit State(std::shared_ptr<NativeWake> notification) : wake(std::move(notification)) {}
};

NativeCompletion NativeCompletion::pending(std::shared_ptr<NativeWake> wake) {
    if (!wake) throw std::invalid_argument("Native completion requires a wake state");
    return NativeCompletion(std::make_shared<State>(std::move(wake)));
}
bool NativeCompletion::ready() const noexcept { return state_ && state_->phase.load(std::memory_order_acquire) == 2; }

bool NativeCompletion::complete(const RuntimeFailure& failure) const noexcept {
    if (!state_) return false;
    unsigned pending = 0;
    if (!state_->phase.compare_exchange_strong(pending, 1, std::memory_order_acq_rel)) return false;
    state_->failure.assign(failure);
    {
        // Serialize with the waiter's predicate-to-sleep transition. This
        // notification state can outlive Scope; no raw executor is retained.
        std::lock_guard lock(state_->wake->mutex);
        state_->phase.store(2, std::memory_order_release);
    }
    state_->wake->changed.notify_all();
    return true;
}

bool NativeCompletion::cancellation_requested() const noexcept {
    return state_ && state_->cancel_requested.load(std::memory_order_acquire);
}

void ExternalNativeJob::execute(const std::atomic<bool>&, RuntimeFailure*) noexcept {
    std::terminate(); // External jobs must never enter the source-worker queue.
}
}
namespace {
struct Task;
struct Group {Task* owner=nullptr;Group* outer=nullptr;std::uint64_t id=0;bool closed=false;std::size_t pending=0,failures=0;};
struct Task {
    enum State { prepared,ready,running,waiting,joining,working,finishing,finished } state=prepared;
    enum Outcome { success,cancelled,failed } outcome=success;
    // Only scheduler-owned roots live here. A completed result is thereafter
    // owned by ordinary precise source roots; retained handles still keep the
    // same object and repeated-await semantics. Empty scheduler roots unregister
    // their existing Handle rather than retaining one null slot per past task.
    struct Root {
        vd::Runtime& runtime;
        std::optional<vd::Runtime::Handle> handle;
        explicit Root(vd::Runtime& owner) : runtime(owner), handle(std::in_place, owner) {}
        void* get() const { return handle ? handle->get() : nullptr; }
        void reset(void* value) {
            if (!value) handle.reset();
            else if (handle) handle->reset(value);
            else handle.emplace(runtime, value);
        }
    } task,frame;
    void* address=nullptr;
    vd_async_resume_entry resume=nullptr;
    Task* dependency=nullptr;
    Task* next_ready=nullptr;
    Task* next_live=nullptr;
    Task* previous_live=nullptr;
    Task* first_child=nullptr;
    Task* last_child=nullptr;
    Task* next_sibling=nullptr;
    std::size_t sequence=0,pending_children=0,failed_children=0;
    bool live=false,listed_failure=false;
    Group children;
    Group* group=nullptr;
    Group* active_group=&children;
    Group* joining_group=nullptr;
    struct Work : vd::NativeWorkerItem {
        Task* owner = nullptr;
        std::shared_ptr<NativeWake> wake;
        void run(vd::Runtime&) noexcept override;
        void request_stop() noexcept override;
        void complete() noexcept override;
        bool runtime_owned() const noexcept override { return false; }
        std::atomic<bool> done{false};
        bool cancelled=false,draining=false,completed=false;
        unsigned cleanup_depth=0;
        Diagnostic failure;
        std::shared_ptr<vd::NativeJob> native;
        std::shared_ptr<vd::NativeCompletion::State> external, external_cleanup;
        bool abandoning = false;
        bool cancel_sent = false;
        bool ready() const noexcept {
            return external ? external->phase.load(std::memory_order_acquire) == 2
                            : done.load(std::memory_order_acquire);
        }
        const vd::RuntimeFailure& diagnostic() const noexcept {
            return external ? external->failure.value : failure.value;
        }
    };
    std::unique_ptr<Work> work;
    std::atomic<bool> cancel_requested{false};
    bool observed=false,cancel_observed=false,draining=false,child_interrupt=false;
    bool activation_lifetime=false;
    unsigned cleanup_depth=0;
    Diagnostic interrupted;
    Diagnostic failure;
    explicit Task(vd::Runtime& runtime):task(runtime),frame(runtime){children.owner=this;}
};
thread_local Task* worker_task=nullptr;
constexpr char worker_cancellation[]="Async task cancelled";
void Task::Work::run(vd::Runtime& runtime) noexcept {
    vd::NativeCode::LeaseScope failure_scope({});
    worker_task = owner;
    vd::RuntimeFailure detail;
    try {
        if (native) {
            vd::Runtime::Native safe(runtime);
            if (abandoning) native->abandon(&detail);
            else if (owner->cancel_requested.load(std::memory_order_relaxed)) {
                native->cancel_queued(); cancelled = true;
            } else native->execute(owner->cancel_requested, &detail);
        } else {
            owner->resume(owner->frame.get(), &detail);
            if (!detail.message && !completed) detail.message = "Worker returned without checked completion";
        }
    } catch (...) { detail.message = "Worker execution failed"; }
    failure.assign(detail); worker_task = nullptr;
}
void Task::Work::request_stop() noexcept { owner->cancel_requested.store(true, std::memory_order_relaxed); }
void Task::Work::complete() noexcept {
    auto notification = wake;
    { std::lock_guard lock(notification->mutex); done.store(true, std::memory_order_release); }
    // No Work/Task access after the release handoff; Scope can retire it now.
    notification->changed.notify_all();
}
struct Scope {
    vd::Runtime& runtime;
    std::thread::id owner=std::this_thread::get_id();
    std::list<std::unique_ptr<Task>> tasks;
    std::size_t next_sequence = 0;
    std::unordered_map<void*,Task*> task_index;
    Task* live_head=nullptr;
    Task* live_tail=nullptr;
    std::list<std::unique_ptr<Group>> groups;
    std::uint64_t next_group = 0, swept_epoch = 0;
    bool sweep_needed = false;
    Task* current=nullptr;
    Task* ready_head=nullptr;
    Task* ready_tail=nullptr;
    std::shared_ptr<NativeWake> wake = std::make_shared<NativeWake>();
    std::shared_ptr<vd::NativeWorkers> workers;
    const void* generation;
    ~Scope(){
        for(auto& task:tasks)request_cancel(*task);
        vd::Runtime::Native native(runtime);
        for (auto& task : tasks) {
            if (!task->work || task->state == Task::prepared) continue;
            auto& work = *task->work;
            {
                std::unique_lock lock(wake->mutex);
                wake->changed.wait(lock, [&] { return work.ready(); });
            }
            if (work.native && work.native->acquired()) {
                // Emergency adapter unwind uses the preallocated cleanup
                // ticket too. No executor publication or allocation is needed.
                if (work.external) {
                    work.external = work.external_cleanup;
                    static_cast<vd::ExternalNativeJob&>(*work.native).abandon_external(vd::NativeCompletion(work.external));
                } else {
                    work.abandoning = true; work.done.store(false, std::memory_order_relaxed);
                    workers->submit_cleanup(work);
                }
                std::unique_lock lock(wake->mutex);
                wake->changed.wait(lock, [&] { return work.ready(); });
            }
        }
    }
    void ensure_workers(){
        if(!workers) workers = vd::NativeWorkers::get(runtime);
    }
    void publish(Task& task){
        ensure_workers();activate(task);task.state=Task::working;
        task.work->owner = &task; task.work->wake = wake;
        workers->submit_scoped(*task.work);
    }
    void publish_external(Task& task) noexcept {
        activate(task);
        task.state = Task::working;
        static_cast<vd::ExternalNativeJob&>(*task.work->native).start(
            vd::NativeCompletion(task.work->external));
    }
    void request_cancel(Task& task) noexcept {
        if (task.state == Task::finished) return;
        task.cancel_requested.store(true, std::memory_order_relaxed);
        if (task.work && task.work->external && !task.work->abandoning && !task.work->cancel_sent) {
            task.work->cancel_sent = true;
            task.work->external->cancel_requested.store(true, std::memory_order_release);
            static_cast<vd::ExternalNativeJob&>(*task.work->native).cancel();
        }
    }
    bool work_pending() const noexcept {for(auto* task=live_head;task;task=task->next_live)if(task->state==Task::working)return true;return false;}
    void receive() noexcept {
        for(auto* task=live_head;task;){
            auto* following=task->next_live;
            if(task->state==Task::working && task->work->ready()){
            if (task->work->native) {
                receive_native(*task);
            } else {
            task->state=Task::finished;const auto& failure=task->work->failure.value;
            if(failure.message){task->failure.assign(failure);task->outcome=task->work->cancelled?Task::cancelled:Task::failed;if(task->outcome==Task::cancelled)task->failure.value={};}
            else{std::int32_t complete=1;std::memcpy(task->address,&complete,sizeof(complete));}
            failed_child(*task);
            // The acquire handoff proves the worker has returned from resume,
            // removed its scratch roots and stopped accessing this frame.
            task->frame.reset(nullptr);
            retire(*task);
            }
            }
            task=following;
        }
    }
    void abandon(Task& task) noexcept {
        activate(task);
        task.work->abandoning = true;
        if (task.work->external) {
            task.work->external = task.work->external_cleanup;
            task.state = Task::working;
            static_cast<vd::ExternalNativeJob&>(*task.work->native).abandon_external(
                vd::NativeCompletion(task.work->external));
            return;
        }
        task.work->done.store(false, std::memory_order_relaxed);
        // Workers were admitted before this resource could be acquired. Reuse
        // the completed job and intrusive queue; cleanup cannot allocate.
        task.state = Task::working;
        workers->submit_cleanup(*task.work);
    }
    void receive_native(Task& task) noexcept {
        auto& work = *task.work;
        if (work.diagnostic().message) {
            if (task.failure.value.message) task.failure.append(work.diagnostic());
            else task.failure.assign(work.diagnostic());
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
        retire(task);
    }
    void finish_native_children(Task& owner) noexcept {
        bool pending_cleanup = false;
        for (auto* child=owner.first_child;child;child=child->next_sibling) {
            if (!child->work || !child->work->native) continue;
            auto& work = *child->work;
            if (child->state == Task::working) {
                pending_cleanup = true;
            } else if (work.native->acquired()) {
                if (owner.outcome == Task::success) {
                    work.native->commit();
                    release_result_root(*child);
                }
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
            retire(owner);
        }
    }
    void wait_work(){
        vd::Runtime::Native native(runtime);std::unique_lock lock(wake->mutex);
        wake->changed.wait(lock,[&]{for(auto* task=live_head;task;task=task->next_live)if(task->state==Task::working && task->work->ready())return true;return false;});
    }

    Scope(vd::Runtime& owner,const void* identity):runtime(owner),generation(identity){}
    void register_task(std::unique_ptr<Task> item) {
        auto* task=item.get();
        if (next_sequence == std::numeric_limits<std::size_t>::max())
            throw std::length_error("Async creation sequence exhausted");
        task->sequence=next_sequence++;
        tasks.push_back(std::move(item));
        try {
            auto [entry, inserted] = task_index.emplace(task->address, task);
            if (!inserted) {
                // An address can be recycled only after the existing precise
                // collector reclaimed an unreachable result. An old record can
                // still be needed as the ancestor of a retained child. Mark its
                // reclaimed payload explicitly before the address is reused.
                auto& old = *entry->second;
                if (old.state != Task::finished || old.task.get())
                    throw std::logic_error("duplicate async task address");
                entry->second = task;
                old.address = nullptr;
                sweep_needed = true;
            }
        }catch(...){tasks.pop_back();throw;}
        if(task->group) {
            auto& owner=*task->group->owner;
            if(owner.last_child)owner.last_child->next_sibling=task;else owner.first_child=task;
            owner.last_child=task;
        }
    }
    // Publication can fail before the job reaches a worker. Remove every scan
    // index before destroying the just-registered task and its rooted handles.
    void discard_last_task() noexcept {
        auto& task=*tasks.back();
        retire(task);
        if(task.group) {
            auto& owner=*task.group->owner;
            Task* previous=nullptr;
            for(auto* child=owner.first_child;child!=&task;child=child->next_sibling)previous=child;
            if(previous)previous->next_sibling=nullptr;else owner.first_child=nullptr;
            owner.last_child=previous;
        }
        task_index.erase(task.address);
        tasks.pop_back();
    }
    // A scan index, not another ready queue. Retain creation order exactly as
    // the original task-vector scan, including out-of-order starts/abandonment.
    // Intrusive links permit reactivation during cleanup without allocation.
    void activate(Task& task) noexcept {
        if(task.live)return;
        auto* previous=live_tail;
        while(previous && previous->sequence>task.sequence)previous=previous->previous_live;
        task.previous_live=previous;task.next_live=previous?previous->next_live:live_head;
        if(task.next_live)task.next_live->previous_live=&task;else live_tail=&task;
        if(previous)previous->next_live=&task;else live_head=&task;
        task.live=true;
        if(task.group){++task.group->pending;++task.group->owner->pending_children;}
    }
    void retire(Task& task) noexcept {
        if(!task.live)return;
        if(task.previous_live)task.previous_live->next_live=task.next_live;else live_head=task.next_live;
        if(task.next_live)task.next_live->previous_live=task.previous_live;else live_tail=task.previous_live;
        task.next_live=task.previous_live=nullptr;task.live=false;
        if(task.group){if(!task.group->pending || !task.group->owner->pending_children)std::terminate();--task.group->pending;--task.group->owner->pending_children;}
        release_result_root(task);
    }
    void release_result_root(Task& task) noexcept {
        if (task.state != Task::finished) return;
        if (task.work && task.work->native && task.work->native->acquired()) return;
        // Quiescence and rooted owner publication precede every finished
        // retirement. Native payloads may duplicate large result bytes too.
        task.work.reset();
        task.frame.reset(nullptr);
        if (task.group) task.task.reset(nullptr);
        // The root adapter's result remains rooted until its final host copy.
    }
    void sweep_completed() {
        const auto epoch = runtime.snapshot().completed_epoch;
        if (!sweep_needed && swept_epoch == epoch) return;
        swept_epoch = epoch;
        sweep_needed = false;
        // This is native bookkeeping after the existing precise collector. It
        // neither traces nor frees managed objects and creates no weak roots.
        // Address reuse may conservatively retain a record until a later GC;
        // register_task invalidates the old identity before replacing its index.
        bool changed;
        do {
            changed = false;
            for (auto cursor = groups.end(); cursor != groups.begin();) {
                auto it = std::prev(cursor);
                auto* group = it->get();
                bool used = (!group->closed && group->owner->state != Task::finished) ||
                            group->pending || group->failures;
                if (!used) for (const auto& task : tasks)
                    if (task->group == group || task->active_group == group || task->joining_group == group) { used = true; break; }
                if (!used) for (const auto& other : groups)
                    if (other->outer == group) { used = true; break; }
                if (used) cursor = it;
                else { cursor = groups.erase(it); changed = true; }
            }
            for (auto cursor = tasks.end(); cursor != tasks.begin();) {
                auto it = std::prev(cursor);
                auto& task = **it;
                bool keep = task.state != Task::finished || task.task.get() || task.frame.get() || task.work ||
                            !task.group || task.first_child || task.listed_failure || task.failure.value.message;
                if (!keep && task.address) keep = runtime.contains(task.address);
                if (!keep) for (const auto& group : groups)
                    if (group->owner == &task) { keep = true; break; }
                if (!keep) for (const auto& other : tasks)
                    if (other->state == Task::waiting && other->dependency == &task) { keep = true; break; }
                if (keep) { cursor = it; continue; }
                auto& parent = *task.group->owner;
                Task* previous = nullptr;
                for (auto* child = parent.first_child; child != &task; child = child->next_sibling) {
                    if (!child) std::terminate();
                    previous = child;
                }
                if (previous) previous->next_sibling = task.next_sibling;
                else parent.first_child = task.next_sibling;
                if (parent.last_child == &task) parent.last_child = previous;
                if (task.address) {
                    auto entry = task_index.find(task.address);
                    if (entry != task_index.end() && entry->second == &task) task_index.erase(entry);
                }
                cursor = tasks.erase(it);
                changed = true;
            }
        } while (changed);
    }
    void observe(Task& task) noexcept {
        task.observed=true;
        if(task.listed_failure) {
            if(!task.group || !task.group->failures || !task.group->owner->failed_children)std::terminate();
            --task.group->failures;--task.group->owner->failed_children;task.listed_failure=false;
        }
    }
    Task& find(void* address) {
        auto found=task_index.find(address);if(found!=task_index.end())return *found->second;
        throw std::logic_error("Task does not belong to the active root adapter");
    }
    void enqueue(Task& task) noexcept {
        activate(task);
        task.state=Task::ready;task.next_ready=nullptr;task.dependency=nullptr;
        if(ready_tail)ready_tail->next_ready=&task;else ready_head=&task;
        ready_tail=&task;
    }
    Task* pop() noexcept {auto* task=ready_head;if(task){ready_head=task->next_ready;if(!ready_head)ready_tail=nullptr;task->next_ready=nullptr;}return task;}
    bool member(const Task& task,const Group* group) const noexcept {return task.group==group;}
    bool owned(const Task& task,const Task& owner) const noexcept {return task.group && task.group->owner==&owner;}
    bool pending(const Task& owner,const Group* group=nullptr) const noexcept {
        return group?group->pending!=0:owner.pending_children!=0;
    }
    void cancel_children(Task& owner,Group* group=nullptr) noexcept {
        for(auto* task=owner.first_child;task;task=task->next_sibling)if(!group || member(*task,group))request_cancel(*task);
    }
    void cancel_lifetimes(Task& owner,Group* group=nullptr) noexcept {
        for(auto* task=owner.first_child;task;task=task->next_sibling)
            if(task->activation_lifetime && (!group || member(*task,group))) request_cancel(*task);
    }
    Task* child_failure(Task& owner,Group* group=nullptr) noexcept {
        if(!(group?group->failures:owner.failed_children))return nullptr;
        for(auto* task=owner.first_child;task;task=task->next_sibling)if((!group || member(*task,group)) && task->state!=Task::prepared && task->outcome==Task::failed && !task->observed)return task;
        return nullptr;
    }
    void collect(Task& owner,Group* group=nullptr) noexcept {
        if(!(group?group->failures:owner.failed_children))return;
        for(auto* task=owner.first_child;task;task=task->next_sibling)if((!group || member(*task,group)) && task->outcome==Task::failed && !task->observed){
            if(owner.outcome!=Task::failed){owner.failure.assign(task->failure.value);owner.outcome=Task::failed;}
            else owner.failure.append(task->failure.value);
            observe(*task);
        }
    }
    void failed_child(Task& child) noexcept {
        if(child.group && child.outcome==Task::failed){
            auto& owner=*child.group->owner;
            if(!child.observed && !child.listed_failure){++owner.failed_children;++child.group->failures;child.listed_failure=true;}
            cancel_children(owner,child.group);
        }
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
void vd_async_new(void** task,void** frame,std::size_t frame_size,const std::size_t* frame_offsets,std::size_t frame_count,std::size_t task_size,const std::size_t* task_offsets,std::size_t task_count,const char* task_key,const void* generation,vd_async_resume_entry resume,vd::RuntimeFailure* failure,std::int32_t worker) noexcept {
    try {
        auto& s=scope();if(s.current && (s.current->draining || s.current->cleanup_depth)){exception(failure,"Async calls cannot enqueue during structured cleanup");return;}if(s.generation!=generation)throw std::logic_error("async generation mismatch");auto item=std::make_unique<Task>(s.runtime);item->resume=resume;if(worker)item->work=std::make_unique<Task::Work>();item->group=s.current?s.current->active_group:nullptr;
        vd::TypeLayout task_layout{task_size,{}},frame_layout{frame_size,{}};
        if(task_count)task_layout.references.assign(task_offsets,task_offsets+task_count);
        if(frame_count)frame_layout.references.assign(frame_offsets,frame_offsets+frame_count);
        task_layout.descriptor=s.runtime.descriptor(task_key);
        s.runtime.allocate_into(task,task_layout);item->task.reset(*task);item->address=*task;
        s.runtime.allocate_into(frame,frame_layout);item->frame.reset(*frame);
        s.register_task(std::move(item));
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
        s.observe(task);if(task.outcome==Task::cancelled){s.current->cancel_observed=true;exception(failure,"Async task cancelled");}else if(task.failure.value.message)*failure=task.failure.value;
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
    try{auto& s=scope();auto& task=s.find(address);s.request_cancel(task);}
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
    try{auto& s=scope();s.receive();s.sweep_completed();auto& task=*s.current;if(task.draining)return;
        if(task.failure.value.message){*failure=task.failure.value;return;}
        // Child failure is selected only after the shielded join, in creation order.
        if(s.child_failure(task)){task.child_interrupt=true;exception(failure,"Async child failure requires structured cleanup");return;}
        if(task.cancel_requested){task.cancel_observed=true;exception(failure,"Async task cancelled");}
    }catch(...){exception(failure,"Invalid async checkpoint");}
}
std::uint64_t vd_async_group_enter(vd::RuntimeFailure* failure) noexcept {
    try{auto& s=scope();if(!s.current || s.current->draining)throw std::logic_error("invalid group entry");
        s.sweep_completed();
        if (s.next_group == std::numeric_limits<std::uint64_t>::max()) throw std::length_error("Async group sequence exhausted");
        auto group=std::make_unique<Group>();group->owner=s.current;group->outer=s.current->active_group;group->id=++s.next_group;
        auto* address=group.get();s.groups.push_back(std::move(group));s.current->active_group=address;return address->id;
    }catch(...){exception(failure,"Async group allocation or entry failed");return 0;}
}
bool vd_async_group_join(std::uint64_t id,std::int32_t cancel,void* frame,vd::RuntimeFailure* failure) noexcept {
    try{auto& s=scope();if(!s.current || s.current->frame.get()!=frame || !id)throw std::logic_error("invalid group join");
        auto entry=std::find_if(s.groups.begin(),s.groups.end(),[&](const auto& group){return group->id==id;});
        if(entry==s.groups.end())throw std::logic_error("invalid group join");
        auto& task=*s.current;auto* group=entry->get();if(group->owner!=&task || task.active_group!=group || group->closed)throw std::logic_error("invalid group ownership");
        s.cancel_lifetimes(task,group);
        if(cancel || task.cancel_requested || s.child_failure(task,group))s.cancel_children(task,group);
        if(s.pending(task,group)){task.joining_group=group;task.state=Task::joining;return false;}
        group->closed=true;task.active_group=group->outer;task.joining_group=nullptr;
        s.sweep_needed=true;
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
            s.cancel_lifetimes(task);
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
            s.sweep_completed();
            bool retired=false;
            for(auto* task=s.live_head;task;){
                auto* following=task->next_live;
                if(task->state==Task::waiting && (task->dependency->state==Task::finished || task->cancel_requested || s.child_failure(*task)))s.enqueue(*task);
                else if(task->state==Task::joining && !s.pending(*task,task->joining_group))s.enqueue(*task);
                else if (task->state == Task::finishing && !s.pending(*task)) {
                    s.finish_native_children(*task);
                    retired |= task->state == Task::finished;
                }
                task=following;
            }
            Task* next=s.pop();bool pending=s.live_head!=nullptr;
            if(!next){
                if(!pending)break;
                // A later child may have just retired after an earlier parent's
                // readiness check. Rescan before diagnosing a dependency cycle.
                if(retired)continue;
                if(s.work_pending()){s.wait_work();continue;}
                // Resume through the checked await failure edge so reached
                // cleanup runs even for an invalid cyclic dependency graph.
                vd::RuntimeFailure cycle;cycle.message="Async await dependency cycle";
                for(auto* task=s.live_head;task;task=task->next_live)if(task->state==Task::waiting){task->failure.assign(cycle);task->outcome=Task::failed;s.enqueue(*task);}
                continue;
            }
            s.current=next;next->state=Task::running;vd::RuntimeFailure result;
            next->resume(next->frame.get(),&result);
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
        if(!outcome)for(auto& task:s.tasks)if(task->failure.value.message && !task->observed){outcome=&task->failure.value;break;}
        std::int32_t value=0;
        if(!outcome && root && root->outcome==Task::cancelled){exception(failure,"Async task cancelled");return 0;}
        if(outcome){exported_failure.assign(*outcome);for(auto& task:s.tasks)if(task.get()!=root && &task->failure.value!=outcome && task->failure.value.message && !task->observed)exported_failure.append(task->failure.value);*failure=exported_failure.value;}
        else if(root)std::memcpy(&value,static_cast<const char*>(root->task.get())+result_offset,sizeof(value));
        return value;
    }catch(const std::bad_alloc&){exception(failure,"Async scheduler allocation failed");}
     catch(...){exception(failure,"Async scheduler invariant failed");}
    active=nullptr;return 0;
}
}

namespace vd {
const void* async_execution_context() noexcept {return active?active->current:nullptr;}
AsyncSnapshot async_snapshot() {
    auto& executor = scope();
    if (worker_task || !executor.runtime.is_managed()) throw std::logic_error("Async observation requires the owning executor");
    AsyncSnapshot result{executor.tasks.size(), executor.groups.size(), 0};
    for (const auto& task : executor.tasks) result.work += bool(task->work);
    return result;
}

bool async_context_descends_from(const void* ancestor) noexcept {
    for (auto* task = active ? active->current : nullptr; task;
         task = task->group ? task->group->owner : nullptr) {
        if (task == ancestor) return true;
    }
    return false;
}

void async_submit_native(void** output, const TypeLayout& layout,
                         const void* generation, std::shared_ptr<NativeJob> job) {
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
    executor.register_task(std::move(item));
    try {
        executor.publish(*executor.tasks.back());
    } catch (...) {
        // No native operation has been queued if admission fails. Retire the
        // reserved file-operation count before source cleanup can run.
        executor.tasks.back()->work->native->cancel_queued();
        executor.discard_last_task();
        *output = nullptr;
        throw;
    }
}

void async_submit_external(void** output, const TypeLayout& layout,
                           const void* generation,
                           std::shared_ptr<ExternalNativeJob> job,
                           bool activation_lifetime) {
    auto& executor = scope();
    if (!executor.current || executor.current->draining || executor.current->cleanup_depth) {
        throw std::logic_error("External jobs require an active async body");
    }
    if (executor.generation != generation || !job) {
        throw std::logic_error("Invalid external job generation or payload");
    }
    try {
        auto item = std::make_unique<Task>(executor.runtime);
        item->work = std::make_unique<Task::Work>();
        item->work->native = job;
        item->work->external = std::make_shared<NativeCompletion::State>(executor.wake);
        item->work->external_cleanup = std::make_shared<NativeCompletion::State>(executor.wake);
        item->activation_lifetime = activation_lifetime;
        item->group = executor.current->active_group;
        executor.runtime.allocate_into(output, layout);
        item->task.reset(*output);
        item->address = *output;
        executor.register_task(std::move(item));
    } catch (...) {
        job->cancel_queued();
        *output = nullptr;
        throw;
    }
    executor.publish_external(*executor.tasks.back());
}
}
