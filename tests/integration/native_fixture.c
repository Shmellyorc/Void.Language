#include "native_fixture.h"
#include <stdlib.h>
#include <string.h>
struct fixture_resource { fixture_callback callback; void* context; };
static int resources;
int32_t fixture_twice(int32_t value) { return value * 2; }
size_t fixture_text_length(const char* text) { return strlen(text); }
fixture_resource* fixture_create(void) {
    fixture_resource* r = (fixture_resource*)calloc(1, sizeof(fixture_resource));
    if (r) ++resources;
    return r;
}
void fixture_register(fixture_resource* r, fixture_callback callback, void* context) {
    r->callback = callback; r->context = context;
}
int32_t fixture_invoke(fixture_resource* r, int32_t value) {
    return r->callback ? r->callback(r->context, value) : -1;
}
void fixture_unregister(fixture_resource* r) { r->callback = NULL; r->context = NULL; }
void fixture_destroy(fixture_resource* r) { if (r) { --resources; free(r); } }
int fixture_resources(void) { return resources; }
