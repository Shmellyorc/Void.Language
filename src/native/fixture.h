#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
#define VD_SCALAR(name,type) type vd_fixture_##name(type);
VD_SCALAR(i8,int8_t) VD_SCALAR(i16,int16_t) VD_SCALAR(i32,int32_t) VD_SCALAR(i64,int64_t)
VD_SCALAR(u8,uint8_t) VD_SCALAR(u16,uint16_t) VD_SCALAR(u32,uint32_t) VD_SCALAR(u64,uint64_t)
VD_SCALAR(f32,float) VD_SCALAR(f64,double) VD_SCALAR(bool,bool)
#undef VD_SCALAR
typedef struct vd_fixture_resource vd_fixture_resource;
typedef int32_t (*vd_fixture_callback)(void*,int32_t);
vd_fixture_resource* vd_fixture_create(bool);
void vd_fixture_destroy(vd_fixture_resource*);
size_t vd_fixture_terminated(const char*);
size_t vd_fixture_length(const char*,size_t);
char* vd_fixture_copy(const char*,size_t);
void vd_fixture_free(char*);
void vd_fixture_retain(vd_fixture_resource*,const char*,size_t);
int32_t vd_fixture_byte(vd_fixture_resource*,uint64_t);
void vd_fixture_register(vd_fixture_resource*,vd_fixture_callback,void*);
void vd_fixture_unregister(vd_fixture_resource*);
vd_fixture_callback vd_fixture_registered(vd_fixture_resource*,void**);
int32_t vd_fixture_dispatch(vd_fixture_callback,void*,int32_t);
#ifdef __cplusplus
}
#endif
