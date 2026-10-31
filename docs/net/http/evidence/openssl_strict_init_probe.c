/* Isolates the observed dependency failure without curl or VOID Runtime. */
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <stdio.h>
#include <sys/resource.h>
int main(void) {
    struct rlimit stack;
    if (getrlimit(RLIMIT_STACK, &stack)) return 2;
    fprintf(stderr, "stack_soft=%llu stack_hard=%llu\n",
            (unsigned long long)stack.rlim_cur, (unsigned long long)stack.rlim_max);
    int status = RAND_status();
    OPENSSL_cleanup();
    printf("RAND_status=%d\n", status);
    return status == 1 ? 0 : 1;
}
