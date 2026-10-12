#include "void/callback.hpp"
#include "void/backend.hpp"
#include "proof.hpp"
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <latch>
#include <stdexcept>
using namespace std::chrono_literals;
namespace {
void check(bool b, const char* text) { if (!b) throw std::runtime_error(text); }
// A real provider fixture owns every dispatched native thread. Joining these
// threads is its quiescence guarantee, including dispatch before admission.
struct Provider final : vd::CallbackProvider {
    std::future<void> dispatch;
    std::latch quiescing{1};
    void unregister_and_quiesce() noexcept override {
        quiescing.count_down();
        try { if (dispatch.valid()) dispatch.get(); } catch (...) { std::terminate(); }
    }
};
std::latch* native_entered;
std::latch* native_release;
vd::CallbackRegistration* native_registration;
vd::ReloadSlot* native_reentry;
std::atomic<bool> block_once{false}, native_callback_ok{false};
extern "C" std::int32_t hold_native(std::int32_t value) noexcept {
    if (block_once.exchange(false)) {
        native_callback_ok = native_registration->run([&] {
            check(native_reentry->invoke(value) == value*2+7,"generated native callback result");
        });
        native_entered->count_down();
        native_release->wait();
    }
    return value;
}
void generated_native_boundary(bool optimized) {
    auto runtime = std::make_shared<vd::Runtime>();
    vd::Jit jit; jit.bind("hold_native",hold_native);
    auto normal = runtime_test::proof();
    vd::ReloadSlot reentry(jit.compile(normal,"entry",optimized,{},runtime));
    auto module = normal;
    module.imports = {"hold_native"};
    module.functions[2].blocks[0].instructions.push_back({vd::ir::Op::call,0,0,0,"hold_native"});
    auto code = jit.compile(module,"entry",optimized,{},runtime);
    vd::ReloadSlot slot(code);
    Provider provider;
    vd::CallbackRegistration registration(*runtime,provider);
    std::latch entered(1), release(1);
    native_entered = &entered; native_release = &release;
    native_registration = &registration; native_reentry = &reentry;
    block_once = true; native_callback_ok = false;
    std::atomic<int> result = -1;
    provider.dispatch = std::async(std::launch::async,[&,lease=code] { result = lease->invoke(10); });
    code.reset(); entered.wait();
    auto replacement = module;
    replacement.functions[2].blocks[3].instructions[7].immediate = 11;
    check(slot.replace(jit.compile(replacement,"entry",optimized,{},runtime)),"native-active replacement failed");
    check(jit.live_generations() == 3,"native-active generation retired early");
    {
        vd::Runtime::Attachment a(*runtime); vd::Runtime::Managed m(*runtime);
        for (int i = 0; i < 100; ++i) {
            check(runtime->collect() == vd::Runtime::Collection::completed,"generated NativeSafe collection failed");
            check(runtime->snapshot().objects >= 3,"optimized frozen generated frame lost roots");
        }
    }
    check(jit.live_generations() == 3,"GC retired native-active code");
    release.count_down();
    registration.close(); // Provider joins the actual native/generated invocation.
    check(result == 27 && native_callback_ok && slot.invoke(10) == 31,"native reentry/reload result");
    check(jit.live_generations() == 2,"finished native-active generation retained");
}
void lifecycle() {
    vd::Runtime runtime;
    Provider provider;
    vd::CallbackRegistration registration(runtime,provider);
    std::latch dispatched(1), enter_trampoline(1);
    std::atomic<bool> accepted = true, closed = false;
    provider.dispatch = std::async(std::launch::async,[&] {
        dispatched.count_down();
        enter_trampoline.wait();
        accepted = registration.run([] {});
    });
    dispatched.wait();
    auto closing = std::async(std::launch::async,[&] { registration.close(); closed = true; });
    provider.quiescing.wait();
    check(!closed, "context closed before provider quiescence");
    enter_trampoline.count_down(); closing.get();
    check(closed && !accepted, "dispatched callback admitted after close");
    registration.close(); // Idempotent, no second provider teardown.
}
void admitted_shutdown_and_self_close() {
    vd::Runtime runtime;
    Provider provider;
    vd::CallbackRegistration registration(runtime,provider);
    std::latch inside(1), finish(1);
    std::atomic<bool> accepted = false;
    provider.dispatch = std::async(std::launch::async,[&] {
        accepted = registration.run([&] {
            bool rejected = false;
            try { vd::Runtime::Native native(runtime); registration.close(); }
            catch (const std::logic_error&) { rejected = true; }
            check(rejected,"self drain accepted");
            vd::Runtime::Native blocking(runtime);
            inside.count_down(); finish.wait();
        });
    });
    inside.wait(); runtime.close();
    auto closing = std::async(std::launch::async,[&] { registration.close(); });
    provider.quiescing.wait(); finish.count_down(); closing.get();
    check(accepted && runtime.snapshot().threads == 0,"admitted callback failed to drain during shutdown");
}
void callback_reload_and_gc() {
    auto runtime = std::make_shared<vd::Runtime>();
    std::unique_ptr<vd::Runtime::Handle> capture;
    {
        vd::Runtime::Attachment a(*runtime); vd::Runtime::Managed m(*runtime);
        void* value = nullptr; constexpr std::size_t zero = 0;
        vd::Runtime::Slot root{&value,sizeof(value),&zero,1}; vd::Runtime::Frame frame(*runtime,&root,1);
        runtime->allocate_into(&value,{8,{}});
        capture = std::make_unique<vd::Runtime::Handle>(*runtime,value);
    }
    vd::Jit jit;
    auto module = runtime_test::proof();
    auto original = jit.compile(module,"entry",true,{},runtime);
    vd::ReloadSlot slot(original);
    original.reset();
    Provider provider;
    vd::CallbackRegistration registration(*runtime,provider);
    std::latch acquired(1), resume(1);
    std::atomic<int> result = -1;
    provider.dispatch = std::async(std::launch::async,[&] {
        check(registration.run([&] {
            check(runtime->contains(capture->get()),"persistent callback capture reclaimed");
            auto lease = slot.acquire();
            {
                vd::Runtime::Native native(*runtime);
                acquired.count_down(); resume.wait();
            }
            result = lease->invoke(10);
        }),"foreign callback failed");
    });
    acquired.wait();
    auto replacement_module = module;
    replacement_module.functions[2].blocks[3].instructions[7].immediate = 9;
    check(slot.replace(jit.compile(replacement_module,"entry",true,{},runtime)),"body replacement rejected");
    check(jit.live_generations() == 2,"parked/native code lease retired");
    {
        vd::Runtime::Attachment a(*runtime); vd::Runtime::Managed m(*runtime);
        check(runtime->collect() == vd::Runtime::Collection::completed,"collection during callback failed");
    }
    check(jit.live_generations() == 2,"GC reclaimed executable generation");
    resume.count_down();
    // The provider owns/joins the foreign thread, then closes the context.
    provider.dispatch.get();
    check(result == 27 && slot.invoke(10) == 29,"leased/reloaded callback result");
    check(jit.live_generations() == 1,"completed old lease retained");
    // Same-thread native reentry and exception cleanup use the same attachment.
    {
        vd::Runtime::Attachment a(*runtime); vd::Runtime::Managed m(*runtime);
        vd::Runtime::Native n(*runtime);
        check(registration.run([&] { check(slot.invoke(2) == 13,"same-thread callback result"); }),"same-thread callback failed");
        check(!registration.run([] { throw std::runtime_error("binding failure"); }),"callback exception escaped failure protocol");
    }
    registration.close();
    capture.reset();
    {
        vd::Runtime::Attachment a(*runtime); vd::Runtime::Managed m(*runtime);
        check(runtime->collect() == vd::Runtime::Collection::completed && runtime->snapshot().objects == 0,"callback capture retained after quiescence");
    }
}
}
int main() {
    try { lifecycle(); admitted_shutdown_and_self_close(); callback_reload_and_gc(); generated_native_boundary(false); generated_native_boundary(true); std::cout << "callback provider quiescence, shutdown, self-drain, generated native roots/reentry, exceptions and code leases passed\n"; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
