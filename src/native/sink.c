#include "sink.h"
#include <stdlib.h>
#include <string.h>
int32_t vd_sink_i32(int32_t value){return value;}
struct vd_sink_resource {const char* retained;size_t size;vd_sink_callback callback;void* context;};
vd_sink_resource* vd_sink_create(bool success){return success?(vd_sink_resource*)calloc(1,sizeof(vd_sink_resource)):NULL;}
void vd_sink_destroy(vd_sink_resource* r){free(r);}
char* vd_sink_copy(const char* s,size_t n){char* out=(char*)malloc(n?n:1);if(out && n)memcpy(out,s,n);return out;}
void vd_sink_free(char* p){free(p);}
void vd_sink_retain(vd_sink_resource* r,const char* p,size_t n){r->retained=p;r->size=n;}
int32_t vd_sink_byte(vd_sink_resource* r,uint64_t n){return n<r->size?(unsigned char)r->retained[n]:-1;}
void vd_sink_register(vd_sink_resource* r,vd_sink_callback callback,void* context){r->callback=callback;r->context=context;}
void vd_sink_unregister(vd_sink_resource* r){r->callback=NULL;r->context=NULL;}
vd_sink_callback vd_sink_registered(vd_sink_resource* r,void** context){*context=r->context;return r->callback;}
int32_t vd_sink_dispatch(vd_sink_callback callback,void* context,int32_t input){return callback?callback(context,input):-1;}
