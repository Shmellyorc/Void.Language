/* Independent C translation unit. The thunk's export declarations are absent. */
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

const char* g1_fixture_identity(void) {
#ifdef G1_MISMATCHED_FIXTURE
    return "deliberately different independently compiled fixture";
#else
    return G1_FIXTURE_IDENTITY;
#endif
}
int8_t g1_signed8(int8_t value) { return value; }
int16_t g1_signed16(int16_t value) { return value; }
int32_t g1_signed32(int32_t value) { return value; }
int64_t g1_signed64(int64_t value) { return value; }
uint8_t g1_unsigned8(uint8_t value) { return value; }
uint16_t g1_unsigned16(uint16_t value) { return value; }
uint32_t g1_unsigned32(uint32_t value) { return value; }
uint64_t g1_unsigned64(uint64_t value) { return value; }
int g1_cint(int value) { return value; }
long g1_clong(long value) { return value; }
size_t g1_csize(size_t value) { return value; }
bool g1_invert(bool value) { return !value; }
float g1_single(float a, float b) { return 2.0f*a + b; }
double g1_double(double a, double b) { return a + 2.0*b; }
bool g1_sign_zero(float a, double b) { return a == 0.0f && b == 0.0 && signbit(a) && !signbit(b); }
uint64_t g1_mixed(int8_t a, uint16_t b, int32_t c, float d, uint64_t e, double f, int64_t g,
                  float h, uint32_t i, double j, uint8_t k, int16_t l, float m, double n) {
    if (a == -111 && b == 60001 && c == -200000001 && d == 1.25f && e == UINT64_C(18446744073709551615) &&
        f == -2.5 && g == -INT64_C(800000000000000001) && h == 5.5f && i == UINT32_C(4000000001) &&
        j == -7.75 && k == 251 && l == -30123 && m == -0.5f && n == 17.5)
        return UINT64_C(17293822569102704641);
    return 0;
}
uint32_t g1_sentinel(void) { return UINT32_C(4000000007); }
void g1_no_result(uint32_t value) {
    if (value != UINT32_C(4000000007)) abort();
}
