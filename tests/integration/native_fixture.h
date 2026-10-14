#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct fixture_resource fixture_resource;
typedef int32_t (*fixture_callback)(void*, int32_t);
int32_t fixture_twice(int32_t value);
size_t fixture_text_length(const char* text);
fixture_resource* fixture_create(void);
void fixture_register(fixture_resource*, fixture_callback, void*);
int32_t fixture_invoke(fixture_resource*, int32_t);
void fixture_unregister(fixture_resource*);
void fixture_destroy(fixture_resource*);
int fixture_resources(void);
// Synchronous provider retaining a binding-owned, length-aware buffer. Releasing
// clears every provider reference before the binding destroys its byte owner.
void fixture_retain_text(fixture_resource*, const char*, size_t);
int fixture_text_byte(fixture_resource*, size_t);
void fixture_release_text(fixture_resource*);
#ifdef __cplusplus
}
#endif
