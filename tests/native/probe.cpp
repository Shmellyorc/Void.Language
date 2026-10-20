#include "probe.hpp"
#include "../managed/probe.hpp"
#include "../../src/native/fixture.h"
namespace native_test {std::atomic<Gate*> gate{nullptr};std::atomic<void(*)(std::int32_t)> scalar_poll{nullptr};}
extern "C" std::int32_t vd_fixture_real_i32(std::int32_t);
extern "C" std::int32_t vd_fixture_i32(std::int32_t input){if(auto hook=native_test::scalar_poll.load())hook(input);return vd_fixture_real_i32(input);}
extern "C" std::int32_t vd_fixture_real_dispatch(vd_fixture_callback,void*,std::int32_t);
extern "C" void vd_fixture_real_unregister(vd_fixture_resource*);
extern "C" std::int32_t vd_fixture_dispatch(vd_fixture_callback callback,void* context,std::int32_t input){
    if(auto* g=native_test::gate.load();g && std::this_thread::get_id()!=g->owner && g->armed.exchange(false)){
        if(g->fail_callback)managed_test::allocation_budget(0);
        else{g->reached.release();g->release.acquire();}
    }
    return vd_fixture_real_dispatch(callback,context,input);
}
extern "C" void vd_fixture_unregister(vd_fixture_resource* resource){vd_fixture_real_unregister(resource);if(auto* g=native_test::gate.load();g && !g->fail_callback)g->unregistered.release();}
