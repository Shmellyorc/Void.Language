#include <stddef.h>
#include <stdint.h>

typedef struct G2Color { uint8_t r, g, b, a; } G2Color;
typedef struct G2Sample { int32_t tag; double reading; } G2Sample;
typedef struct G2Packet {
    G2Color color;
    uint16_t channels[3];
    uint8_t mode;
    G2Sample datum;
} G2Packet;

_Static_assert(sizeof(G2Color) == 4 && _Alignof(G2Color) == 1, "small aggregate ABI");
_Static_assert(sizeof(G2Sample) == 16 && _Alignof(G2Sample) == 8 && offsetof(G2Sample, reading) == 8, "mixed ABI");
_Static_assert(sizeof(G2Packet) == 32 && _Alignof(G2Packet) == 8 && offsetof(G2Packet, datum) == 16 &&
    offsetof(G2Packet, channels[2]) == 8, "nested and inline stride ABI");

const char* g2_fixture_identity(void) { return G2_FIXTURE_IDENTITY; }
G2Color g2_color(G2Color input) {
    return (G2Color){input.b, input.g, input.r, input.a};
}
static int32_t clamped_tag(int64_t value) {
    if (value < INT32_MIN) return INT32_MIN;
    if (value > INT32_MAX) return INT32_MAX;
    return (int32_t)value;
}
G2Sample g2_calibrate(G2Sample input, float factor) {
    return (G2Sample){clamped_tag((int64_t)input.tag - 7), input.reading * factor};
}
G2Packet g2_packet(G2Sample base, G2Packet value, uint32_t delta) {
    G2Packet output = {0};
    output.color = g2_color(value.color);
    output.channels[0] = (uint16_t)(value.channels[2] + delta);
    output.channels[1] = value.channels[1];
    output.channels[2] = value.channels[0];
    output.mode = (uint8_t)(value.mode + delta);
    output.datum.tag = clamped_tag((int64_t)base.tag + value.datum.tag);
    output.datum.reading = base.reading + value.datum.reading;
    return output;
}
static int checked_address(const uint8_t* data, size_t count) {
    return count ? data != NULL && (uintptr_t)data % 8 == 0 : data == NULL;
}
int64_t g2_sum(const uint8_t* input, size_t count) {
    if (!checked_address(input, count)) return -8;
    int64_t sum = 0;
    for (size_t i = 0; i < count; ++i) sum += input[i];
    return sum;
}
int64_t g2_fill(uint8_t* output, size_t count, uint8_t seed) {
    if (!checked_address(output, count)) return -8;
    if (count == 0) return 0;
    if (seed == 255) { output[0] = 99; return -7; }
    if (seed == 254) { output[0] = 99; return (int64_t)count + 1; }
    if (seed == 253) { output[0] = 42; return 1; }
    for (size_t i = 0; i < count; ++i) output[i] = (uint8_t)(seed + i);
    return (int64_t)count;
}
int64_t g2_adjust(uint8_t* value, size_t count) {
    if (!checked_address(value, count)) return -8;
    for (size_t i = 0; i < count; ++i) value[i] ^= 0x55;
    return (int64_t)count;
}
int64_t g2_move(const uint8_t* input, size_t count, uint8_t* output, size_t capacity) {
    if (!checked_address(input, count) || !checked_address(output, capacity) || count > capacity) return -8;
    for (size_t i = 0; i < count; ++i) output[i] = input[i];
    return (int64_t)count;
}
