#ifndef VC_ENTROPY_H
#define VC_ENTROPY_H
#include <stdbool.h>
#include <stdint.h>

/* Returns OS entropy, or false on failure; zero is a valid entropy result. */
bool vc_native_entropy_seed(uint64_t *seed);
#endif
