#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct VcNativeStringView
{
    const char *data;
    size_t length;
} VcNativeStringView;

typedef int32_t (*Void297Callback)(int32_t value);

static int32_t nul_release_count = 0;
static int32_t view_release_count = 0;
static int32_t callback_count = 0;

static char *copy_bytes(const char *data, size_t length, int nul_terminate)
{
    const size_t extra = nul_terminate != 0 ? 1u : 0u;
    char *memory = (char *)malloc(length + extra);
    if (memory == NULL)
        return NULL;
    if (length != 0u)
        memcpy(memory, data, length);
    if (nul_terminate != 0)
        memory[length] = '\0';
    return memory;
}

void void297_reset(void)
{
    nul_release_count = 0;
    view_release_count = 0;
    callback_count = 0;
}

const char *void297_owned_nul(void)
{
    return copy_bytes("owned-nul", sizeof("owned-nul") - 1u, 1);
}

VcNativeStringView void297_owned_view(void)
{
    static const char bytes[] = {'A', '\0', 'B'};
    char *memory = copy_bytes(bytes, sizeof(bytes), 0);
    return (VcNativeStringView){ .data = memory, .length = memory != NULL ? sizeof(bytes) : 0u };
}

const char *void297_null_nul(void)
{
    return NULL;
}

VcNativeStringView void297_empty_view(void)
{
    char *memory = copy_bytes("", 0u, 1);
    return (VcNativeStringView){ .data = memory, .length = 0u };
}

const char *void297_owned_then_callback(Void297Callback callback)
{
    char *memory = copy_bytes("callback-owned", sizeof("callback-owned") - 1u, 1);
    if (callback != NULL)
    {
        callback_count++;
        (void)callback(7);
    }
    return memory;
}

void void297_release_nul(void *data)
{
    if (data == NULL)
        return;
    nul_release_count++;
    free(data);
}

void void297_release_view(void *data)
{
    if (data == NULL)
        return;
    view_release_count++;
    free(data);
}

int32_t void297_nul_release_count(void)
{
    return nul_release_count;
}

int32_t void297_view_release_count(void)
{
    return view_release_count;
}

int32_t void297_callback_count(void)
{
    return callback_count;
}
