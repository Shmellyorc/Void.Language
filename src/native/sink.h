#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
int32_t vd_sink_i32(int32_t);
typedef struct vd_sink_resource vd_sink_resource;
typedef int32_t (*vd_sink_callback)(void*,int32_t);
vd_sink_resource* vd_sink_create(bool);
void vd_sink_destroy(vd_sink_resource*);
char* vd_sink_copy(const char*,size_t);
void vd_sink_free(char*);
void vd_sink_retain(vd_sink_resource*,const char*,size_t);
int32_t vd_sink_byte(vd_sink_resource*,uint64_t);
void vd_sink_register(vd_sink_resource*,vd_sink_callback,void*);
void vd_sink_unregister(vd_sink_resource*);
vd_sink_callback vd_sink_registered(vd_sink_resource*,void**);
int32_t vd_sink_dispatch(vd_sink_callback,void*,int32_t);
#ifdef __cplusplus
}
#endif
