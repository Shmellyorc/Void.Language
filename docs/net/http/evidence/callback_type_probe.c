/* Standalone control for the OpenSSL typed -> erased callback failure. */
#include <stdio.h>
#include <string.h>
struct Value { int result; };
__attribute__((noinline)) static void typed_callback(struct Value *value) {
    value->result = 7;
}
__attribute__((noinline)) static void adapter(void *opaque) {
    typed_callback((struct Value *)opaque);
}
int main(int argc, char **argv) {
    struct Value value = {0};
    void (*volatile callback)(void *) = argc == 2 && !strcmp(argv[1], "safe")
        ? adapter : (void (*)(void *))typed_callback;
    callback(&value);
    printf("result=%d\n", value.result);
    return value.result != 7;
}
