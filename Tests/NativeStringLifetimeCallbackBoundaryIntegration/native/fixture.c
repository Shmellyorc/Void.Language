#include <stddef.h>
#include <stdint.h>

int32_t void299_call_nul(int32_t (*callback)(const char *))
{
    return callback("héllo");
}

int32_t void299_call_view(int32_t (*callback)(const char *, size_t))
{
    static const char text[] = {'A', '\0', 'B'};
    return callback(text, sizeof(text));
}

int32_t void299_call_pair(int32_t (*callback)(const char *, size_t, int32_t, const char *, size_t))
{
    static const char left[] = {'L', '\0', 'X'};
    static const char right[] = {'R', '\0', 'Y'};
    return callback(left, sizeof(left), 299, right, sizeof(right));
}

int32_t void299_call_throw(int32_t (*callback)(const char *, size_t))
{
    static const char text[] = {'E', '\0', 'X'};
    (void)callback(text, sizeof(text));
    return 77;
}
