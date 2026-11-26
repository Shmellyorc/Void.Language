#include <stdio.h>
#include <sys/resource.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc < 3)
        return 2;

    const struct rlimit stack = {1048576, 1048576};
    if (setrlimit(RLIMIT_STACK, &stack) != 0) {
        perror("setrlimit(RLIMIT_STACK)");
        return 2;
    }
    const struct rlimit file = {1, 1};
    if (argv[1][0] == '1' && setrlimit(RLIMIT_FSIZE, &file) != 0) {
        perror("setrlimit(RLIMIT_FSIZE)");
        return 2;
    }

    execv(argv[2], argv + 2);
    perror("execv");
    return 2;
}
