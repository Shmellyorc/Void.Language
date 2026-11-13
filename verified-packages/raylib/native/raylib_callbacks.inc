/* Context-free raylib routing only. Managed state, code generations and callback
 * retirement belong to G5. No managed address is stored in these I/O buffers. */
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOUSER
#define NOGDI
#include <windows.h>
static void vd_cb_yield(void) { Sleep(0); }
#else
#include <threads.h>
static void vd_cb_yield(void) { thrd_yield(); }
#endif
#include <stdio.h>
#include <stdarg.h>

enum { VD_CB_TRACE=1, VD_CB_LOAD_DATA, VD_CB_SAVE_DATA, VD_CB_LOAD_TEXT,
       VD_CB_SAVE_TEXT, VD_CB_STREAM, VD_CB_PROCESSOR, VD_CB_MIXED,
       VD_CB_SLOTS=16, VD_CB_LIMIT=64*1024*1024 };
struct VdCallbackTarget { unsigned kind; VdBase* parent; AudioStream stream; };
struct VdCallbackRegistration {
    unsigned kind,slot; VdBase* parent; AudioStream stream;
    VdRaylibEvent callback; void* context;
    atomic_uint entered; atomic_bool active,failed; atomic_ullong calls;
    atomic_flag busy;
    unsigned char *input,*name,*output; size_t input_size,name_size,output_size;
    bool output_ready;
};
static atomic_flag vd_cb_gate=ATOMIC_FLAG_INIT;
static VdCallbackRegistration* vd_cb_globals[5];
static VdCallbackRegistration* vd_cb_audio[VD_CB_SLOTS];
static unsigned vd_cb_global_count,vd_cb_native_calls;
static const void* vd_cb_global_thread;
static _Thread_local unsigned char vd_cb_thread;
static _Thread_local unsigned vd_cb_call_depth;
static _Thread_local VdCallbackRegistration* vd_cb_current;
static _Thread_local VdCallbackTarget* vd_cb_target;
static void vd_cb_lock(void) { while(atomic_flag_test_and_set(&vd_cb_gate))vd_cb_yield(); }
static void vd_cb_unlock(void) { atomic_flag_clear(&vd_cb_gate); }

VdRaylibCallScope vd_raylib_call_begin(void) {
    vd_cb_lock();
    bool refused=(vd_cb_current && vd_cb_current->kind>=VD_CB_STREAM) ||
        (vd_cb_global_count && vd_cb_global_thread!=&vd_cb_thread);
    if(!refused){++vd_cb_native_calls;++vd_cb_call_depth;}
    vd_cb_unlock();
    vd_error=refused?3:0;
    if(refused&&vd_cb_current)atomic_store(&vd_cb_current->failed,true);
    return(VdRaylibCallScope){!refused};
}
void vd_raylib_call_end(VdRaylibCallScope* scope) {
    if(!scope->entered)return;
    vd_cb_lock();--vd_cb_native_calls;--vd_cb_call_depth;vd_cb_unlock();
}
bool vd_raylib_global_callbacks_live(void) {
    vd_cb_lock();bool value=vd_cb_global_count!=0;vd_cb_unlock();return value;
}
bool vd_raylib_api_trace_registered(void) {
    vd_cb_lock();bool value=vd_cb_globals[0]!=NULL;vd_cb_unlock();return value;
}
static VdCallbackRegistration* vd_cb_enter(unsigned index,bool audio) {
    vd_cb_lock();VdCallbackRegistration* p=audio?vd_cb_audio[index]:vd_cb_globals[index];
    if(p)atomic_fetch_add(&p->entered,1);
    vd_cb_unlock();return p;
}
static void vd_cb_leave(VdCallbackRegistration* p) { atomic_fetch_sub(&p->entered,1); }
static bool vd_cb_reserve(unsigned char** field,const void* bytes,size_t size) {
    if(size>VD_CB_LIMIT)return false;
    unsigned char* data=malloc(size+1);if(!data)return false;
    if(size)memcpy(data,bytes,size);data[size]=0;free(*field);*field=data;return true;
}
static int vd_cb_invoke(VdCallbackRegistration* p,const char* name,const void* input,size_t size,unsigned frames,int level) {
    if(!atomic_load(&p->active)||atomic_load(&p->failed)||
       (p->kind<VD_CB_STREAM && vd_cb_global_thread!=&vd_cb_thread) ||
       atomic_flag_test_and_set(&p->busy)) {
        atomic_store(&p->failed,true);vd_error=3;return 0;
    }
    size_t length=name?strlen(name):0;
    bool ready=vd_cb_reserve(&p->name,name,length)&&vd_cb_reserve(&p->input,input,size);
    p->name_size=length;p->input_size=size;p->output_ready=false;p->output_size=0;
    VdCallbackRegistration* previous=vd_cb_current;vd_cb_current=p;
    if(!ready)atomic_store(&p->failed,true);
    int status=ready?p->callback(p->context,frames,level):0;
    vd_cb_current=previous;atomic_fetch_add(&p->calls,1);
    /* G5 returns zero both for a captured managed fault and for its sealed
     * close-entry gate. G5 reports the former itself. During audio shutdown the
     * latter is ordinary cancellation, not an adapter failure. */
    if(status!=1 && status!=2 && !(status==0&&p->kind>=VD_CB_STREAM))atomic_store(&p->failed,true);
    if(atomic_load(&p->failed)){vd_error=3;status=0;}
    atomic_flag_clear(&p->busy);return status;
}
static void vd_cb_trace(int level,const char* format,va_list args) {
    VdCallbackRegistration* p=vd_cb_enter(0,false);if(!p)return;
    va_list copy;va_copy(copy,args);int length=vsnprintf(NULL,0,format,copy);va_end(copy);
    if(length<0||length>65536){atomic_store(&p->failed,true);vd_error=3;vd_cb_leave(p);return;}
    char* text=malloc((size_t)length+1);
    if(!text){atomic_store(&p->failed,true);vd_error=3;vd_cb_leave(p);return;}
    va_copy(copy,args);vsnprintf(text,(size_t)length+1,format,copy);va_end(copy);
    (void)vd_cb_invoke(p,text,NULL,0,0,level);free(text);vd_cb_leave(p);
}
static unsigned char* vd_cb_load_data(const char* name,int* size) {
    if(size)*size=0;VdCallbackRegistration* p=vd_cb_enter(1,false);if(!p)return NULL;
    int status=vd_cb_invoke(p,name,NULL,0,0,0);unsigned char* result=NULL;
    if(status==1&&p->output_ready){result=MemAlloc((unsigned)(p->output_size+1));if(result){memcpy(result,p->output,p->output_size);result[p->output_size]=0;if(size)*size=(int)p->output_size;}}
    vd_cb_leave(p);return result;
}
static bool vd_cb_save_data(const char* name,void* data,int size) {
    VdCallbackRegistration* p=vd_cb_enter(2,false);if(!p)return false;
    int status=size<0?0:vd_cb_invoke(p,name,data,(size_t)size,0,0);vd_cb_leave(p);return status==1;
}
static char* vd_cb_load_text(const char* name) {
    VdCallbackRegistration* p=vd_cb_enter(3,false);if(!p)return NULL;
    int status=vd_cb_invoke(p,name,NULL,0,0,0);char* result=NULL;
    if(status==1&&p->output_ready&&!memchr(p->output,0,p->output_size)){
        result=MemAlloc((unsigned)(p->output_size+1));if(result){memcpy(result,p->output,p->output_size);result[p->output_size]=0;}
    }
    vd_cb_leave(p);return result;
}
static bool vd_cb_save_text(const char* name,const char* text) {
    VdCallbackRegistration* p=vd_cb_enter(4,false);if(!p)return false;
    int status=vd_cb_invoke(p,name,text,strlen(text),0,0);vd_cb_leave(p);return status==1;
}
static void vd_cb_audio_invoke(unsigned slot,void* bytes,unsigned frames) {
    VdCallbackRegistration* p=vd_cb_enter(slot,true);if(!p)return;
    size_t stride=p->kind==VD_CB_STREAM?(size_t)p->stream.channels*(p->stream.sampleSize/8):2*sizeof(float);
    if(!stride||frames>VD_CB_LIMIT/stride){atomic_store(&p->failed,true);vd_cb_leave(p);return;}
    size_t size=(size_t)frames*stride;
    if(p->kind==VD_CB_STREAM)memset(bytes,p->stream.sampleSize==8?128:0,size);
    int status=vd_cb_invoke(p,NULL,bytes,size,frames,0);
    if(status==1&&p->output_ready&&p->output_size==size)memcpy(bytes,p->output,size);
    else if(status!=0){atomic_store(&p->failed,true);vd_error=3;}
    vd_cb_leave(p);
}
#define VD_AUDIO_SLOT(N) static void vd_cb_audio_##N(void* data,unsigned frames){vd_cb_audio_invoke(N,data,frames);}
VD_AUDIO_SLOT(0) VD_AUDIO_SLOT(1) VD_AUDIO_SLOT(2) VD_AUDIO_SLOT(3)
VD_AUDIO_SLOT(4) VD_AUDIO_SLOT(5) VD_AUDIO_SLOT(6) VD_AUDIO_SLOT(7)
VD_AUDIO_SLOT(8) VD_AUDIO_SLOT(9) VD_AUDIO_SLOT(10) VD_AUDIO_SLOT(11)
VD_AUDIO_SLOT(12) VD_AUDIO_SLOT(13) VD_AUDIO_SLOT(14) VD_AUDIO_SLOT(15)
#undef VD_AUDIO_SLOT
static AudioCallback const vd_cb_audio_functions[VD_CB_SLOTS]={
    vd_cb_audio_0,vd_cb_audio_1,vd_cb_audio_2,vd_cb_audio_3,
    vd_cb_audio_4,vd_cb_audio_5,vd_cb_audio_6,vd_cb_audio_7,
    vd_cb_audio_8,vd_cb_audio_9,vd_cb_audio_10,vd_cb_audio_11,
    vd_cb_audio_12,vd_cb_audio_13,vd_cb_audio_14,vd_cb_audio_15};
static void vd_cb_global_set(unsigned kind,bool enabled) {
    switch(kind){
    case VD_CB_TRACE:SetTraceLogCallback(enabled?vd_cb_trace:NULL);break;
    case VD_CB_LOAD_DATA:SetLoadFileDataCallback(enabled?vd_cb_load_data:NULL);break;
    case VD_CB_SAVE_DATA:SetSaveFileDataCallback(enabled?vd_cb_save_data:NULL);break;
    case VD_CB_LOAD_TEXT:SetLoadFileTextCallback(enabled?vd_cb_load_text:NULL);break;
    case VD_CB_SAVE_TEXT:SetSaveFileTextCallback(enabled?vd_cb_save_text:NULL);break;
    }
}
static void vd_cb_disconnect(VdCallbackRegistration* p) {
    bool installed=false;
    vd_cb_lock();
    VdCallbackRegistration** location=p->kind<VD_CB_STREAM?&vd_cb_globals[p->kind-1]:&vd_cb_audio[p->slot];
    installed=*location==p;
    if(installed&&p->kind<VD_CB_STREAM){vd_cb_global_set(p->kind,false);*location=NULL;--vd_cb_global_count;}
    vd_cb_unlock();
    if(installed){
        if(p->kind<VD_CB_STREAM){ /* already uninstalled under the call-entry gate */ }
        else if(p->kind==VD_CB_STREAM)SetAudioStreamCallback(p->stream,NULL);
        else if(p->kind==VD_CB_PROCESSOR)DetachAudioStreamProcessor(p->stream,vd_cb_audio_functions[p->slot]);
        else DetachAudioMixedProcessor(vd_cb_audio_functions[p->slot]);
    }
    /* The mixer mutex covers the complete native trampoline. Keep its route
     * published until that barrier has returned, including G5-sealed entries. */
    vd_cb_lock();if(*location==p)*location=NULL;atomic_store(&p->active,false);vd_cb_unlock();
    while(atomic_load(&p->entered))vd_cb_yield();
}
static VdCallbackTarget* vd_cb_target_make(unsigned kind,VdBase* parent,AudioStream stream) {
    vd_error=0;if(vd_cb_current){vd_error=3;return NULL;}
    VdCallbackTarget* p=calloc(1,sizeof *p);if(!p){vd_error=2;return NULL;}
    p->kind=kind;p->parent=parent;p->stream=stream;vd_keep(parent);return p;
}
VdCallbackTarget* vd_raylib_callback_target_new(uint32_t kind) {
    if(kind<VD_CB_TRACE||kind>VD_CB_SAVE_TEXT){vd_error=1;return NULL;}
    if(vd_audio||IsAudioDeviceReady()){vd_error=3;return NULL;}
    return vd_cb_target_make(kind,NULL,(AudioStream){0});
}
VdCallbackTarget* vd_raylib_callback_target_stream(const VdAudioStream* stream,uint32_t kind) {
    if((kind!=VD_CB_STREAM&&kind!=VD_CB_PROCESSOR)||!vd_live(&stream->base)||!stream->value->buffer){vd_error=1;return NULL;}
    return vd_cb_target_make(kind,(VdBase*)&stream->base,*stream->value);
}
VdCallbackTarget* vd_raylib_callback_target_mixed(void) {
    if(!vd_audio){vd_error=1;return NULL;}
    return vd_cb_target_make(VD_CB_MIXED,&vd_audio->base,(AudioStream){0});
}
int32_t vd_raylib_callback_target_close(VdCallbackTarget* p) {
    if(vd_cb_target==p)vd_cb_target=NULL;vd_drop(p->parent);free(p);return 0;
}
void vd_raylib_callback_activate(const VdCallbackTarget* p) {
    vd_error=0;if(vd_cb_current||(p->parent&&!vd_live(p->parent))){vd_error=3;return;}vd_cb_target=(VdCallbackTarget*)p;
}
VdCallbackRegistration* vd_raylib_callback_register(VdRaylibEvent callback,void* context) {
    vd_error=0;VdCallbackTarget* target=vd_cb_target;vd_cb_target=NULL;
    if(!target||vd_cb_current){vd_error=3;return NULL;}
    VdCallbackRegistration* p=calloc(1,sizeof *p);if(!p){vd_error=2;return NULL;}
    p->kind=target->kind;p->parent=target->parent;p->stream=target->stream;p->callback=callback;p->context=context;
    atomic_init(&p->active,false);atomic_init(&p->failed,false);atomic_init(&p->entered,0);atomic_init(&p->calls,0);atomic_flag_clear(&p->busy);
    VdCallbackRegistration* previous=NULL;
    vd_cb_lock();
    if(p->kind<VD_CB_STREAM){
        if(vd_cb_native_calls||vd_audio||IsAudioDeviceReady()||vd_raylib_scoped_trace_active() ||
           (vd_cb_global_count&&vd_cb_global_thread!=&vd_cb_thread)){
            vd_cb_unlock();free(p);vd_error=3;return NULL;
        }
        /* No provider call is in flight. Replace under the same admission gate:
         * never open a zero-hook window in which a foreign call could enter. */
        previous=vd_cb_globals[p->kind-1];
        if(previous){atomic_store(&previous->active,false);vd_cb_global_set(p->kind,false);}
        else ++vd_cb_global_count;
        vd_cb_globals[p->kind-1]=p;vd_cb_global_thread=&vd_cb_thread;
        atomic_store(&p->active,true);vd_keep(p->parent);vd_cb_global_set(p->kind,true);
        vd_cb_unlock();return p;
    }else{
        for(unsigned i=0;i<VD_CB_SLOTS;++i)if(vd_cb_audio[i]&&p->kind==VD_CB_STREAM&&vd_cb_audio[i]->kind==VD_CB_STREAM&&vd_cb_audio[i]->stream.buffer==p->stream.buffer){previous=vd_cb_audio[i];break;}
    }
    vd_cb_unlock();
    if(previous){
        SetAudioStreamCallback(previous->stream,NULL);
        vd_cb_lock();if(vd_cb_audio[previous->slot]==previous)vd_cb_audio[previous->slot]=NULL;atomic_store(&previous->active,false);vd_cb_unlock();
        while(atomic_load(&previous->entered))vd_cb_yield();
    }
    vd_cb_lock();
    {
        unsigned i=0;for(;i<VD_CB_SLOTS&&vd_cb_audio[i];++i){}
        if(i==VD_CB_SLOTS){vd_cb_unlock();free(p);vd_error=2;return NULL;}
        p->slot=i;vd_cb_audio[i]=p;atomic_store(&p->active,true);
    }
    vd_keep(p->parent);vd_cb_unlock();
    if(p->kind==VD_CB_STREAM)SetAudioStreamCallback(p->stream,vd_cb_audio_functions[p->slot]);
    else if(p->kind==VD_CB_PROCESSOR)AttachAudioStreamProcessor(p->stream,vd_cb_audio_functions[p->slot]);
    else if(p->kind==VD_CB_MIXED)AttachAudioMixedProcessor(vd_cb_audio_functions[p->slot]);
    return p;
}
int32_t vd_raylib_callback_close(VdCallbackRegistration* p) {
    vd_cb_disconnect(p);bool failed=atomic_load(&p->failed);
    free(p->input);free(p->name);free(p->output);vd_drop(p->parent);free(p);return failed?-1:0;
}
uint64_t vd_raylib_callback_count(const VdCallbackRegistration* p) {
    vd_error=atomic_load(&p->failed)?3:0;return atomic_load(&p->calls);
}
bool vd_raylib_callback_active(const VdCallbackRegistration* p) {
    vd_error=atomic_load(&p->failed)?3:0;return atomic_load(&p->active);
}
uint32_t vd_raylib_callback_kind(const VdCallbackRegistration* p) { return p->kind; }
size_t vd_raylib_callback_input_size(uint32_t which) {
    vd_error=0;if(!vd_cb_current||which>1){vd_error=1;return 0;}
    return which?vd_cb_current->input_size:vd_cb_current->name_size;
}
int64_t vd_raylib_callback_read(uint32_t which,size_t at,uint8_t* data,size_t offset,size_t count) {
    (void)offset;vd_error=0;if(!vd_cb_current||which>1){vd_error=1;return -1;}
    size_t length=which?vd_cb_current->input_size:vd_cb_current->name_size;
    const unsigned char* source=which?vd_cb_current->input:vd_cb_current->name;
    if(at>length||count>length-at){vd_error=1;return -1;}if(count)memcpy(data,source+at,count);return (int64_t)count;
}
void vd_raylib_callback_output_size(size_t size) {
    vd_error=0;if(!vd_cb_current||size>VD_CB_LIMIT){vd_error=1;return;}
    unsigned char* output=calloc(size+1,1);if(!output){vd_error=2;return;}
    free(vd_cb_current->output);vd_cb_current->output=output;vd_cb_current->output_size=size;vd_cb_current->output_ready=true;output[size]=0;
}
int64_t vd_raylib_callback_write(size_t at,const uint8_t* data,size_t offset,size_t count) {
    (void)offset;vd_error=0;
    if(!vd_cb_current||!vd_cb_current->output_ready||at>vd_cb_current->output_size||count>vd_cb_current->output_size-at){vd_error=1;return -1;}
    if(count)memcpy(vd_cb_current->output+at,data,count);return(int64_t)count;
}
void vd_raylib_api_TraceLog(int32_t level,const VdBuffer* message) {
    VdRaylibCallScope scope __attribute__((cleanup(vd_raylib_call_end)))=vd_raylib_call_begin();if(!scope.entered)return;
    const char* text=vd_text(message);if(vd_error||!text)return;
    if(level<LOG_ALL||level>LOG_NONE){vd_error=1;return;}TraceLog(level,"%s",text);
}
