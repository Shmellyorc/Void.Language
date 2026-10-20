#include "fixture.h"
#include <stdlib.h>
#include <string.h>
#define VD_SCALAR(name,type) type vd_fixture_##name(type value){return value;}
VD_SCALAR(i8,int8_t) VD_SCALAR(i16,int16_t) VD_SCALAR(i32,int32_t) VD_SCALAR(i64,int64_t)
VD_SCALAR(u8,uint8_t) VD_SCALAR(u16,uint16_t) VD_SCALAR(u32,uint32_t) VD_SCALAR(u64,uint64_t)
VD_SCALAR(f32,float) VD_SCALAR(f64,double) VD_SCALAR(bool,bool)
#undef VD_SCALAR
struct vd_fixture_resource {const char* retained;size_t size;vd_fixture_callback callback;void* context;};
vd_fixture_resource* vd_fixture_create(bool success){return success?(vd_fixture_resource*)calloc(1,sizeof(vd_fixture_resource)):NULL;}
void vd_fixture_destroy(vd_fixture_resource* r){free(r);}
size_t vd_fixture_terminated(const char* s){return strlen(s);}
size_t vd_fixture_length(const char* s,size_t n){(void)s;return n;}
char* vd_fixture_copy(const char* s,size_t n){char* out=(char*)malloc(n?n:1);if(out && n)memcpy(out,s,n);return out;}
void vd_fixture_free(char* p){free(p);}
void vd_fixture_retain(vd_fixture_resource* r,const char* p,size_t n){r->retained=p;r->size=n;}
int32_t vd_fixture_byte(vd_fixture_resource* r,uint64_t n){return n<r->size?(unsigned char)r->retained[n]:-1;}
void vd_fixture_register(vd_fixture_resource* r,vd_fixture_callback callback,void* context){r->callback=callback;r->context=context;}
void vd_fixture_unregister(vd_fixture_resource* r){r->callback=NULL;r->context=NULL;}
vd_fixture_callback vd_fixture_registered(vd_fixture_resource* r,void** context){*context=r->context;return r->callback;}
int32_t vd_fixture_dispatch(vd_fixture_callback callback,void* context,int32_t input){return callback?callback(context,input):-1;}
