#include <stdint.h>
int32_t vc_sequence_pair(int32_t a, int32_t b) { return a * 10 + b; }

int32_t vc_sequence_read(int32_t *pointer, int32_t ignored) { (void)ignored; return *pointer; }
