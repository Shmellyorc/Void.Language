#include "hooks_redirect.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    if (strcmp(argv[1], "leak-malloc") == 0)
    {
        assert(malloc(16) != NULL);
        return 0;
    }
    if (strcmp(argv[1], "leak-calloc") == 0)
    {
        assert(calloc(1, 16) != NULL);
        return 0;
    }
    if (strcmp(argv[1], "leak-realloc") == 0)
    {
        void *memory = realloc(NULL, 16);
        assert(memory != NULL);
        memory = realloc(memory, 32);
        assert(memory != NULL);
        return 0;
    }
    if (strcmp(argv[1], "balanced") != 0) return 2;

    free(NULL);
    void *memory = calloc(1, 16);
    if (getenv("VOID_TEST_STATE_OOM") != NULL) assert(memory == NULL);
    else assert(memory != NULL);
    free(memory);

    /* calloc failure injection must not affect native path-buffer malloc. */
    unsigned char *bytes = malloc(16);
    assert(bytes != NULL);
    bytes[0] = 42;
    unsigned char *resized = realloc(bytes, 32);
    assert(resized != NULL && resized[0] == 42);
    bytes = resized;
    resized = realloc(bytes, SIZE_MAX);
    assert(resized == NULL && bytes[0] == 42);
    free(bytes);

    memory = realloc(NULL, 16);
    assert(memory != NULL);
    memory = realloc(memory, 0);
    free(memory); /* Accept the native zero-size result, NULL or allocated. */
    free(realloc(NULL, 0));
    free(malloc(0));
    free(calloc(0, 16));
    return 0;
}
