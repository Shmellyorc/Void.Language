/* Exercise the emitted Windows input helpers, including their shared cache. */
#define main void_test_original_main
#include VOID_GENERATED_SOURCE
#undef main
#include <assert.h>

int main(void)
{
    assert(vc_native_environment_init());
    assert(vc_terminal_key_available() == 0);
    fputs("True\r\n", stdout);
    assert(fflush(stdout) == 0);
    while (vc_terminal_key_available() == 0) Sleep(1);
    int32_t values[3] = {0};
    VcArray data = {.length = 3, .rank = 1, .element_size = sizeof(int32_t), .data = values};
    assert(vc_terminal_read_event(&data));
    assert(values[0] == 0x1f642 && values[1] == VC_TERMINAL_EVENT_CHARACTER && values[2] == 0);
    for (int i = 0; i < 3; ++i) {
        assert(vc_terminal_key_available() == 1);
        assert(vc_terminal_read_event(&data));
        assert(values[0] == 'r' && values[1] == VC_TERMINAL_EVENT_CHARACTER && values[2] == 0);
    }
    assert(vc_terminal_key_available() == 0);
    fputs("True\r\n", stdout);
    return 0;
}
