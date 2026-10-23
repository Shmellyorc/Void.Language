#include "void/execution.hpp"
#include "void/runtime.hpp"
#include "void/runtime_abi.hpp"
#include <stdexcept>
#include <algorithm>
namespace vd {
namespace {thread_local std::shared_ptr<void> active_code_lease;thread_local NativeCode::LeaseScope* active_failures=nullptr;thread_local OwnedRuntimeFailure exported_native_failure;}
void OwnedRuntimeFailure::assign(const RuntimeFailure& failure) noexcept {
    try{std::string message=failure.message?failure.message:"";std::vector<std::string> frames;for(unsigned n=0;n<std::min(failure.count,32u);++n)frames.emplace_back(failure.frames[n]);message_.swap(message);frames_.swap(frames);value_={};value_.message=failure.message?message_.c_str():nullptr;value_.count=failure.count;for(unsigned n=0;n<frames_.size();++n)value_.frames[n]=frames_[n].c_str();}
    catch(...){value_={};value_.message="Native failure diagnostic allocation failed";}
}
struct NativeCode::LeaseScope::FailureRecord {OwnedRuntimeFailure failure;std::unique_ptr<FailureRecord> next;};
std::shared_ptr<void> NativeCode::active_lease(){return active_code_lease;}
NativeCode::LeaseScope::LeaseScope(std::shared_ptr<void> lease):previous_failures_(active_failures),previous_(std::move(active_code_lease)){active_code_lease=std::move(lease);active_failures=this;}
NativeCode::LeaseScope::~LeaseScope(){active_failures=previous_failures_;active_code_lease=std::move(previous_);while(failures_){auto retired=std::move(failures_);failures_=std::move(retired->next);}}
void retain_native_failure(const RuntimeFailure& value,RuntimeFailure* output) noexcept {
    if(!active_failures){exported_native_failure.assign(value);*output=exported_native_failure.value();return;}
    try{auto record=std::make_unique<NativeCode::LeaseScope::FailureRecord>();record->failure.assign(value);*output=record->failure.value();record->next=std::move(active_failures->failures_);active_failures->failures_=std::move(record);}
    catch(...){*output={};output->message="Native failure retention allocation failed";}
}
void export_native_failure(RuntimeFailure* failure) noexcept {exported_native_failure.assign(*failure);*failure=exported_native_failure.value();}
NativeCode::NativeCode(Entry entry, std::shared_ptr<void> owner, Compatibility compatibility)
    : entry_(entry), owner_(std::move(owner)), compatibility_(std::move(compatibility)) {
    if (!entry_ || !owner_) throw std::invalid_argument("native code requires an entry and lifetime owner");
}
NativeCode::NativeCode(CheckedEntry entry, std::shared_ptr<void> owner, Compatibility compatibility, std::shared_ptr<Runtime> runtime)
    : checked_(entry), owner_(std::move(owner)), compatibility_(std::move(compatibility)), runtime_(std::move(runtime)) {
    if (!checked_ || !owner_) throw std::invalid_argument("native code requires an entry and lifetime owner");
}
std::int32_t NativeCode::invoke(std::int32_t value) const {
    // Host dispatch/diagnostic copying/lease retirement must run NativeSafe,
    // including reentrant dispatch from a Managed callback. Generated direct
    // calls stay Managed and use their ordinary argument/return root handoff.
    if (auto* attached = Runtime::attached(); attached && attached->is_managed()) {
        Runtime::Native native(*attached);
        return invoke(value);
    }
    if (!checked_) return entry_(value);
    LeaseScope lease(owner_);
    RuntimeFailure failure;
    std::int32_t result;
    if (runtime_) {
        Runtime::Attachment attachment(*runtime_);
        auto checkpoint=runtime_->console_checkpoint();
        {Runtime::Managed managed(*runtime_);result = checked_(value, &failure);}
        finish_console(*runtime_,checkpoint,&failure);
    } else result = checked_(value, &failure);
    if (failure.message) {
        // Copy borrowed generation strings before leaving the owning code lease.
        std::string message = failure.message;
        for (std::uint32_t i = 0; i < failure.count && i < 32; ++i) message += failure.frames[i];
        if (failure.count > 32) message += "\nSource call trace limited to 32 callers.\n";
        throw std::runtime_error(message); // Outside generated code; no native unwind.
    }
    return result;
}
ReloadSlot::ReloadSlot(std::shared_ptr<const NativeCode> initial) : current_(std::move(initial)) {
    if (!current_) throw std::invalid_argument("reload slot requires initial code");
}
std::shared_ptr<const NativeCode> ReloadSlot::acquire() const {
    std::lock_guard lock(mutex_);
    return current_;
}
bool ReloadSlot::replace(std::shared_ptr<const NativeCode> replacement) {
    if (!replacement) return false;
    std::shared_ptr<const NativeCode> previous;
    {
        std::lock_guard lock(mutex_);
        if (!(current_->compatibility() == replacement->compatibility())) return false;
        previous = std::move(current_);
        current_ = std::move(replacement);
    }
    return true; // Destruction and ORC retirement happen outside the publication lock.
}
std::int32_t ReloadSlot::invoke(std::int32_t value) const { return acquire()->invoke(value); }
}
