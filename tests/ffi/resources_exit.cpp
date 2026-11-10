// Test-only process check. Runtime shutdown must release every linked fixture owner.
#include <cstdint>
#include <cstdlib>
extern "C" std::int32_t g3_live();
namespace {
void verify_release() {
    if (g3_live()) std::abort();
}
const int registered = std::atexit(verify_release);
}
