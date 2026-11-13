"""Ownership/signature groups consumed by generate.py, using native-bind ABI 3.

Policy is attached to C declarations, not inferred from an opaque function name.
C addresses remain behind the existing Runtime's sealed resource ledger.
"""
import re

# Public structs with native allocations, GPU/audio identities or borrowed views.
OWNERS = {
    'Image': ('UnloadImage', 'value.data != NULL'),
    'Texture': ('UnloadTexture', 'value.id != 0'),
    'RenderTexture': ('UnloadRenderTexture', 'value.id != 0'),
    'Font': ('UnloadFont', 'value.texture.id != 0'),
    'Mesh': ('UnloadMesh', 'value.vertices != NULL'),
    'Shader': ('UnloadShader', 'value.id != 0'),
    'Material': ('UnloadMaterial', 'value.maps != NULL'),
    'Model': ('UnloadModel', 'value.meshes != NULL'),
    'Wave': ('UnloadWave', 'value.data != NULL'),
    'AudioStream': ('UnloadAudioStream', 'value.buffer != NULL'),
    'Sound': ('UnloadSound', 'value.stream.buffer != NULL'),
    'Music': ('UnloadMusicStream', 'value.ctxData != NULL'),
    'FilePathList': ('UnloadDirectoryFiles', 'value.paths != NULL'),
    'GlyphInfo': (None, 'true'), 'MaterialMap': (None, 'true'),
    'Transform': (None, 'true'), 'BoneInfo': (None, 'true'),
    'ModelSkeleton': (None, 'true'), 'ModelAnimation': (None, 'true'),
    'AutomationEventList': ('UnloadAutomationEventList', 'value.events != NULL'),
}
# Pointer allocations with known native lengths and exact terminal releases.
POINTER_RESULTS = {
    'LoadRandomSequence': ('int', 'p0', 'UnloadRandomSequence'),
    'LoadFileData': ('unsigned char', 'out_count', 'UnloadFileData'),
    'CompressData': ('unsigned char', 'out_count', 'MemFree'),
    'DecompressData': ('unsigned char', 'out_count', 'MemFree'),
    'EncodeDataBase64': ('char', 'out_count', 'MemFree'),
    'DecodeDataBase64': ('unsigned char', 'out_count', 'MemFree'),
    'ComputeMD5': ('unsigned int', '4', None),
    'ComputeSHA1': ('unsigned int', '5', None),
    'ComputeSHA256': ('unsigned int', '8', None),
    'ExportImageToMemory': ('unsigned char', 'out_count', 'MemFree'),
    'LoadImageColors': ('Color', 'p0->value->width*p0->value->height', 'UnloadImageColors'),
    'LoadImagePalette': ('Color', 'out_count', 'UnloadImagePalette'),
    'LoadCodepoints': ('int', 'out_count', 'UnloadCodepoints'),
    'LoadWaveSamples': ('float', '(size_t)p0->value->frameCount*p0->value->channels', 'UnloadWaveSamples'),
    'LoadFontData': ('GlyphInfo', 'out_count', 'font_data'),
    'LoadMaterials': ('Material', 'out_count', 'materials'),
    'LoadModelAnimations': ('ModelAnimation', 'out_count', 'animations'),
    'LoadTextLines': ('char*', 'out_count', 'lines'),
    'TextSplit': ('char*', 'out_count', None),
}
ALLOCATED_STRINGS = {'LoadFileText':'UnloadFileText','LoadUTF8':'UnloadUTF8',
    'TextReplaceAlloc':'MemFree','TextReplaceBetweenAlloc':'MemFree','TextInsertAlloc':'MemFree'}
OUT_INT = {'dataSize','compDataSize','outputSize','fileSize','colorCount','glyphCount','materialCount','animCount','codepointSize','utf8Size','frames'}
OUT_FUNCTIONS = {'LoadFileData','CompressData','DecompressData','EncodeDataBase64','DecodeDataBase64',
    'ExportImageToMemory','LoadImagePalette','LoadFontData','LoadMaterials','LoadModelAnimations',
    'LoadCodepoints','LoadTextLines','TextSplit','GetCodepoint','GetCodepointNext','GetCodepointPrevious',
    'CodepointToUTF8','LoadImageAnim','LoadImageAnimFromMemory'}
UNLOAD_BUFFERS = {'UnloadRandomSequence','UnloadFileData','UnloadFileText','UnloadImageColors',
    'UnloadImagePalette','UnloadFontData','UnloadUTF8','UnloadCodepoints','UnloadTextLines',
    'UnloadModelAnimations','UnloadWaveSamples','MemFree'}
# Buffer extents documented by raylib.h or the pinned implementation.
COUNT_ARGS = {
    'SetWindowIcons': {'images':'count'},
    'DrawLineStrip': {'points':'pointCount'}, 'DrawTriangleFan': {'points':'pointCount'},
    'DrawTriangleStrip': {'points':'pointCount'}, 'DrawTriangleStrip3D': {'points':'pointCount'},
    'CheckCollisionPointPoly': {'points':'pointCount'},
    'LoadFontEx': {'codepoints':'codepointCount'},
    'LoadFontFromMemory': {'fileData':'dataSize','codepoints':'codepointCount'},
    'LoadFontData': {'fileData':'dataSize','codepoints':'codepointCount'},
    'GenImageFontAtlas': {'glyphs':'glyphCount'},
    'DrawTextCodepoints': {'codepoints':'codepointCount'},
    'MeasureTextCodepoints': {'codepoints':'length'}, 'LoadUTF8': {'codepoints':'length'},
    'DrawMeshInstanced': {'transforms':'instances'},
}
BORROWED_RESULTS = {'GetFontDefault','GetShapesTexture','GetGlyphInfo'}
RETAIN_INPUT = {'LoadMusicStreamFromMemory': 'data'}
SPECIAL = {'InitWindow','CloseWindow','InitAudioDevice','CloseAudioDevice','SetWindowTitle',
           'GetWindowHandle','MemAlloc','MemRealloc','TextCopy','TextAppend','GetCodepointPrevious',
           'LoadModelFromMesh','LoadSoundAlias','SetMaterialTexture','GenImageFontAtlas','SetWindowIcons','SetShapesTexture','LoadDroppedFiles','UnloadDroppedFiles'}


def extend(nodes, inventory, aliases, pod, scalar, void_types, snake, resolve):
    original_snake=snake
    snake=lambda name: original_snake(name).replace(' ', '_').replace('*', 'pointer')
    scalar=dict(scalar, **{'long':'i64','unsigned short':'u16'})
    void_types=dict(void_types, **{'long':'i64','unsigned short':'u16'})
    decls, body, resources, records, binds = [], [], [], [], []
    def canon(t):
        if t.endswith(' *'):
            base=t[:-2]
            const=base.startswith('const ')
            return ('const ' if const else '')+canon(base[6:] if const else base)+' *'
        return resolve(t, aliases)
    def owner(t): return t in OWNERS and t not in pod
    def ctype(t): return 'Vd'+t
    def ownername(t): return 'owner_'+snake(t)
    def maptype(t):
        t=canon(t)
        if t in scalar:return scalar[t]
        if t in pod:return 'record:'+snake(t)
        if owner(t):return 'borrow:'+ownername(t)
        raise ValueError(t)
    def stype(t):
        t=canon(t)
        if t in void_types:return void_types[t]
        return 'native.'+snake(t) if t in pod else snake(t)
    def emit(name, symbol, result, params, cresult, cparams, code):
        decls.append(f'VOID_RAYLIB_EXPORT {cresult} {symbol}({", ".join(cparams) or "void"});')
        guard='' if name=='api_error' else '    VdRaylibCallScope vd_scope __attribute__((cleanup(vd_raylib_call_end)))=vd_raylib_call_begin(); if(!vd_scope.entered) '+('return;' if cresult=='void' else f'return ({cresult}){{0}};')+'\n'
        body.append(f'{cresult} {symbol}({", ".join(cparams) or "void"}) {{\n'+guard+code+'\n}')
        binds.append('bind '+name+' '+symbol+' '+result+(' '+' '.join(params) if params else ''))
    # All buffers carry their allocator, typed stride/count and view generation.
    # A NativeResource owns each wrapper; parent refs are internal native borrows.
    header = r'''
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdatomic.h>
typedef struct VdBase VdBase;
typedef struct VdBuffer VdBuffer;
struct VdBase { size_t refs; uint64_t epoch, parent_epoch; VdBase* parent; void (*destroy)(VdBase*); bool owned; };
struct VdBuffer { VdBase base; void* data; size_t count, stride; unsigned kind; size_t child_count; void (*release)(VdBuffer*); VdBase** keepers; uint64_t* epochs; };
static _Thread_local int32_t vd_error;
static bool vd_live(const VdBase* b) {
    return b && b->refs && (!b->parent || (vd_live(b->parent) && (!b->parent_epoch || b->parent_epoch == b->parent->epoch)));
}
static void vd_keep(VdBase* b) { if (b) ++b->refs; }
static void vd_drop(VdBase* b) {
    if (!b || --b->refs) return;
    VdBase* parent=b->parent;
    if (b->destroy) b->destroy(b);
    free(b);
    vd_drop(parent);
}
static void vd_base(VdBase* b, bool owned, VdBase* parent, void (*destroy)(VdBase*)) {
    b->refs=1; b->epoch=1; b->owned=owned; b->parent=parent;
    b->parent_epoch=parent&&!owned?parent->epoch:0; b->destroy=destroy; vd_keep(parent);
}
static void vd_change(VdBase* b) {
    ++b->epoch;
    if (b->parent && b->parent_epoch) { vd_change(b->parent); b->parent_epoch=b->parent->epoch; }
}
static void vd_buffer_destroy(VdBase* b) {
    VdBuffer* p=(VdBuffer*)b;
    if (b->owned && p->release) p->release(p);
    if (p->keepers) for (size_t i=0;i<p->count;++i) vd_drop(p->keepers[i]);
    free(p->keepers); free(p->epochs);
}
static void vd_mem_release(VdBuffer* p) { MemFree(p->data); }
static VdBuffer* vd_buffer(void* data, size_t count, size_t stride, unsigned kind,
                           bool owned, VdBase* parent, void (*release)(VdBuffer*)) {
    if (count > SIZE_MAX/(stride?stride:1)) { vd_error=2; return NULL; }
    VdBuffer* p=calloc(1,sizeof *p);
    if (!p) { vd_error=2; return NULL; }
    vd_base(&p->base,owned,parent,vd_buffer_destroy);
    p->data=data; p->count=count; p->stride=stride; p->kind=kind; p->release=release;
    return p;
}
static VdBuffer* vd_copy(const void* data, size_t count, size_t stride, unsigned kind) {
    if (count > 64*1024*1024/(stride?stride:1)) { vd_error=2; return NULL; }
    size_t bytes=count*stride;
    void* copy=MemAlloc((unsigned int)(bytes+1));
    if (!copy) { vd_error=2; return NULL; }
    if (bytes) memcpy(copy,data,bytes);
    ((unsigned char*)copy)[bytes]=0;
    VdBuffer* p=vd_buffer(copy,count,stride,kind,true,NULL,vd_mem_release);
    if (!p) MemFree(copy);
    return p;
}
static const char* vd_text(const VdBuffer* p) {
    if (!vd_live(&p->base) || p->stride!=1 || (p->kind!=1 && p->kind!=3)) { vd_error=1; return NULL; }
    if (p->kind==3) return NULL;
    if (memchr(p->data,0,p->count)) {vd_error=1;return NULL;}
    return (const char*)p->data;
}
static bool vd_buffer_live(const VdBuffer* p) {
    if (!vd_live(&p->base) || (p->count && !p->data)) return false;
    if (p->keepers) for(size_t i=0;i<p->count;++i)
        if (p->keepers[i] && (!vd_live(p->keepers[i]) || p->keepers[i]->epoch!=p->epochs[i])) return false;
    return true;
}
static bool vd_bytes_live(const VdBuffer* p) {
    return vd_buffer_live(p) && p->stride==1 && (p->kind==0 || p->kind==1 || p->kind==3);
}
static bool vd_extent(const VdBuffer* p, size_t count, size_t stride) {
    if (!vd_buffer_live(p) || count>SIZE_MAX/(stride?stride:1) || count*stride>p->count*p->stride) {vd_error=1;return false;}
    return true;
}
'''
    # C definitions are emitted once into the generated implementation. Header
    # contains forward declarations only, sufficient for native-bind's prototype checks.
    decls += ['typedef struct VdBuffer VdBuffer;']
    resources.append('resource owner_buffer VdBuffer pointer null buffer_new buffer_close owner-calls shutdown-owner consume-always')
    body.append(header)
    body.append('static VdAutomationEventList* vd_active_events;')
    body += [r'''
// UnloadModel deliberately leaves materials' GPU objects to the C caller.
// This owner takes that responsibility, deduplicating shared shader/texture IDs
// before calling the upstream releases. Only public MaterialMapIndex slots are
// exposed by this binding; no private map-count constant is guessed.
static void vd_unload_materials(Material* materials, size_t count) {
    if(!materials)return;
    Shader fallback=LoadShader(NULL,NULL);
    for(size_t i=0;i<count;++i) {
        for(size_t j=0;j<i;++j)
            if(materials[i].shader.id==materials[j].shader.id) {materials[i].shader=fallback;break;}
        if(!materials[i].maps)continue;
        for(int slot=0;slot<=MATERIAL_MAP_BRDF;++slot) {
            unsigned id=materials[i].maps[slot].texture.id;
            if(!id)continue;
            bool duplicate=false;
            for(size_t j=0;j<=i&&!duplicate;++j) {
                if(!materials[j].maps)continue;
                int end=j==i?slot:MATERIAL_MAP_BRDF+1;
                for(int k=0;k<end;++k) if(materials[j].maps[k].texture.id==id){duplicate=true;break;}
            }
            if(duplicate)materials[i].maps[slot].texture.id=0;
        }
    }
    for(size_t i=0;i<count;++i){UnloadMaterial(materials[i]);materials[i].maps=NULL;}
}
static void vd_unload_model(Model value) {
    vd_unload_materials(value.materials,(size_t)value.materialCount);
    MemFree(value.currentPose);MemFree(value.boneMatrices);
    UnloadModel(value);
}
''']
    emit('api_error','vd_raylib_api_error','i32',[],'int32_t',[],'    return vd_error;')
    emit('buffer_new','vd_raylib_buffer_new','owner:owner_buffer',['c_size','u32'],'VdBuffer*',['size_t count','uint32_t kind'],
         '    vd_error=0;\n    if(kind!=0 && kind!=1 && kind!=3){vd_error=1;return NULL;}\n    if (count > 64*1024*1024) { vd_error=2; return NULL; }\n    void* data=MemAlloc((unsigned int)(count+1));\n    if (!data) { vd_error=2; return NULL; }\n    memset(data,0,count+1);\n    VdBuffer* p=vd_buffer(data,count,1,kind,true,NULL,vd_mem_release);\n    if (!p) MemFree(data);\n    return p;')
    emit('buffer_close','vd_raylib_buffer_close','i32',['close:owner_buffer'],'int32_t',['VdBuffer* p'],'    vd_drop(&p->base); return 0;')
    emit('buffer_count','vd_raylib_buffer_count','c_size',['borrow:owner_buffer'],'size_t',['const VdBuffer* p'],'    vd_error=0; if (!vd_live(&p->base)) {vd_error=1;return 0;} return p->count;')
    emit('buffer_write','vd_raylib_buffer_write','i64',['mut:owner_buffer','c_size','in:3:4:65536','c_size','c_size'],'int64_t',
         ['VdBuffer* p','size_t at','const uint8_t* data','size_t offset','size_t count'],
         '    (void)offset; vd_error=0;\n    if (!vd_bytes_live(p) || at>p->count*p->stride || count>p->count*p->stride-at) { vd_error=1;return -1;}\n    if (!p->base.owned || p->base.parent || p->base.refs!=1) {vd_error=1;return -1;}\n    if (count) memcpy((uint8_t*)p->data+at,data,count);\n    vd_change(&p->base);\n    return (int64_t)count;')
    emit('buffer_read','vd_raylib_buffer_read','i64',['borrow:owner_buffer','c_size','out:3:4:65536','c_size','c_size'],'int64_t',
         ['const VdBuffer* p','size_t at','uint8_t* data','size_t offset','size_t count'],
         '    (void)offset; vd_error=0;\n    if (!vd_bytes_live(p) || at>p->count*p->stride || count>p->count*p->stride-at) { vd_error=1;return -1;}\n    if (count) memcpy(data,(const uint8_t*)p->data+at,count);\n    return (int64_t)count;')
    emit('buffer_text_length','vd_raylib_buffer_text_length','c_size',['borrow:owner_buffer'],'size_t',['const VdBuffer* p'],
         '    vd_error=0;if(!vd_bytes_live(p)){vd_error=1;return 0;}const char* end=memchr(p->data,0,p->count);return end?(size_t)(end-(const char*)p->data):p->count;')
    body += [r'''
static void vd_strings_release(VdBuffer* p) {
    char** values=p->data;
    for(size_t i=0;i<p->count;++i) MemFree(values[i]);
    MemFree(values);
}
static VdBuffer* vd_strings_copy(char** values, size_t count) {
    if(count>1048576){vd_error=2;return NULL;}
    char** data=MemAlloc((unsigned int)((count?count:1)*sizeof(char*)));
    if(!data){vd_error=2;return NULL;}memset(data,0,(count?count:1)*sizeof(char*));
    VdBuffer* result=vd_buffer(data,count,sizeof(char*),30,true,NULL,vd_strings_release);
    if(!result){MemFree(data);return NULL;}
    for(size_t i=0;i<count;++i) {
        size_t length=strlen(values[i]);data[i]=MemAlloc((unsigned int)(length+1));
        if(!data[i]){vd_error=2;vd_drop(&result->base);return NULL;}memcpy(data[i],values[i],length+1);
    }
    return result;
}
''']
    emit('strings_new','vd_raylib_strings_new','owner:owner_buffer',['c_size'],'VdBuffer*',['size_t count'],r'''
    vd_error=0;if(count>1048576){vd_error=2;return NULL;}
    char** data=MemAlloc((unsigned int)((count?count:1)*sizeof(char*)));if(!data){vd_error=2;return NULL;}
    memset(data,0,(count?count:1)*sizeof(char*));VdBuffer* p=vd_buffer(data,count,sizeof(char*),30,true,NULL,vd_strings_release);
    if(!p)MemFree(data);return p;''')
    emit('strings_set','vd_raylib_strings_set','void',['mut:owner_buffer','c_size','borrow:owner_buffer'],'void',
         ['VdBuffer* p','size_t index','const VdBuffer* text'],r'''
    vd_error=0;const char* value=vd_text(text);
    if(vd_error||!value||!vd_buffer_live(p)||!p->base.owned||p->base.refs!=1||p->kind!=30||index>=p->count){vd_error=1;return;}
    char* copy=MemAlloc((unsigned int)(text->count+1));if(!copy){vd_error=2;return;}
    memcpy(copy,value,text->count+1);MemFree(((char**)p->data)[index]);((char**)p->data)[index]=copy;vd_change(&p->base);''')
    emit('strings_get','vd_raylib_strings_get','owner:owner_buffer',['borrow:owner_buffer','c_size'],'VdBuffer*',
         ['const VdBuffer* p','size_t index'],r'''
    vd_error=0;if(!vd_buffer_live(p)||p->kind!=30||index>=p->count){vd_error=1;return NULL;}
    const char* text=((const char*const*)p->data)[index];if(!text){vd_error=1;return NULL;}
    return vd_copy(text,strlen(text),1,1);''')
    decls += ['typedef struct VdWindow VdWindow;','typedef struct VdAudio VdAudio;']
    resources += [
        'resource owner_window VdWindow pointer null init_window close_window owner-calls shutdown-owner consume-always',
        'resource owner_audio VdAudio pointer null init_audio_device close_audio_device owner-calls shutdown-owner consume-always']
    body += [r'''
struct VdWindow { VdBase base; VdBuffer* title; };
struct VdAudio { VdBase base; };
static VdWindow* vd_window;
static VdBase* vd_shapes;
static VdAudio* vd_audio;
static void vd_destroy_window(VdBase* base) {
    VdWindow* p=(VdWindow*)base;
    CloseWindow(); vd_window=NULL; vd_drop(&p->title->base);
}
static void vd_destroy_audio(VdBase* base) {
    (void)base; CloseAudioDevice(); vd_audio=NULL;
}
''']
    emit('init_window','vd_raylib_api_InitWindow','owner:owner_window',['i32','i32','borrow:owner_buffer'],
         'VdWindow*',['int32_t width','int32_t height','const VdBuffer* title'],r'''
    vd_error=0; const char* text=vd_text(title);
    if(vd_error || !text || width<=0 || height<=0 || width>16384 || height>16384 || vd_window || IsWindowReady()) {vd_error=1;return NULL;}
    VdBuffer* copy=vd_copy(text,title->count,1,1); if(!copy)return NULL;
    VdWindow* p=calloc(1,sizeof *p); if(!p){vd_drop(&copy->base);vd_error=2;return NULL;}
    InitWindow(width,height,(const char*)copy->data);
    if(!IsWindowReady()){CloseWindow();vd_drop(&copy->base);free(p);return NULL;}
    vd_base(&p->base,true,NULL,vd_destroy_window);p->title=copy;vd_window=p;return p;''')
    emit('close_window','vd_raylib_api_CloseWindow','i32',['close:owner_window'],'int32_t',['VdWindow* p'],'    if(vd_shapes){SetShapesTexture(GetFontDefault().texture,(Rectangle){0,0,1,1});VdBase* old=vd_shapes;vd_shapes=NULL;vd_drop(old);}vd_drop(&p->base);return 0;')
    emit('init_audio_device','vd_raylib_api_InitAudioDevice','owner:owner_audio',[],'VdAudio*',[],r'''
    vd_error=0;if(vd_audio||IsAudioDeviceReady()||vd_raylib_global_callbacks_live()){vd_error=1;return NULL;}
    VdAudio* p=calloc(1,sizeof *p);if(!p){vd_error=2;return NULL;}
    InitAudioDevice();if(!IsAudioDeviceReady()){free(p);return NULL;}
    vd_base(&p->base,true,NULL,vd_destroy_audio);vd_audio=p;return p;''')
    emit('close_audio_device','vd_raylib_api_CloseAudioDevice','i32',['close:owner_audio'],'int32_t',['VdAudio* p'],'    vd_drop(&p->base);return 0;')
    emit('set_window_title','vd_raylib_api_SetWindowTitle','void',['borrow:owner_buffer'],'void',['const VdBuffer* title'],r'''
    vd_error=0;const char* text=vd_text(title);if(vd_error||!text||!vd_window){vd_error=1;return;}
    VdBuffer* copy=vd_copy(text,title->count,1,1);if(!copy)return;
    SetWindowTitle((const char*)copy->data);vd_drop(&vd_window->title->base);vd_window->title=copy;''')
    emit('get_window_handle','vd_raylib_api_GetWindowHandle','owner:owner_buffer',[],'VdBuffer*',[],r'''
    vd_error=0;if(!vd_window){vd_error=1;return NULL;}void* value=GetWindowHandle();
    return value?vd_buffer(value,0,1,4,false,&vd_window->base,NULL):NULL;''')
    actual_owners=[t for t in OWNERS if owner(t)]
    for t in actual_owners:
        n=snake(t);ct=ctype(t);on=ownername(t);unload,valid=OWNERS[t]
        decls.append(f'typedef struct {ct} {ct};')
        resources.append(f'resource {on} {ct} pointer null {on}_new {on}_close owner-calls shutdown-owner consume-always')
        body += [f'struct {ct} {{ VdBase base; {t} storage; {t}* value; int32_t out_count; VdBase* retained; VdBuffer* extra; }};']
        destroy=f'if (b->owned) {unload}(*p->value);' if unload else '(void)p;'
        if t=='Model':destroy='if(b->owned)vd_unload_model(*p->value);'
        if t=='Sound':destroy='if(b->owned){if(p->out_count==1)UnloadSoundAlias(*p->value);else UnloadSound(*p->value);}'
        if t=='AutomationEventList':
            destroy='if(vd_active_events==p){StopAutomationEventRecording();SetAutomationEventList(NULL);vd_active_events=NULL;} '+destroy
        body += [f'static void vd_destroy_{n}(VdBase* b) {{ {ct}* p=({ct}*)b; {destroy} vd_drop(p->retained); if(p->extra)vd_drop(&p->extra->base); }}',
                 f'static {ct}* vd_own_{n}({t} value, bool owned, VdBase* parent) {{',
                 f'    {ct}* p=calloc(1,sizeof *p); if (!p) {{ vd_error=2;'+(f' if (owned) {"vd_unload_model" if t=="Model" else unload}(value);' if unload else '')+' return NULL; }',
                 f'    vd_base(&p->base,owned,parent,vd_destroy_{n}); p->storage=value; p->value=&p->storage; return p;','}']
        emit(on+'_new','vd_raylib_'+on+'_new','owner:'+on,[],ct+'*',[],f'    vd_error=0; return vd_own_{n}(({t}){{0}},false,NULL);')
        emit(on+'_close','vd_raylib_'+on+'_close','i32',['close:'+on],'int32_t',[ct+'* p'],'    vd_drop(&p->base); return 0;')
        record=next(x for x in nodes if x.get('kind')=='RecordDecl' and x.get('name')==t)
        for f in record.get('inner',[]):
            if f.get('kind')!='FieldDecl':continue
            ft=canon(f['type']['qualType']);fn=snake(f['name'])
            if fn=='format':fn='pixel_format' # VOID reserves format for display hooks.
            if ft not in scalar and ft not in pod:continue
            # Address-bearing subfields require parented views and are added by
            # their explicit ownership policy rather than a numeric cast.
            symbol=f'vd_raylib_{n}_get_{fn}'
            emit(n+'_get_'+fn,symbol,maptype(ft),['borrow:'+on],ft,[f'const {ct}* p'],
                 f'    vd_error=0; if (!vd_live(&p->base)) {{vd_error=1;return ({ft}){{0}};}} return p->value->{f["name"]};')
    buffer_types={t:i+10 for i,t in enumerate(['int','unsigned int','float','Vector2','Vector3','Matrix','Color','Rectangle','Transform','Image','GlyphInfo','Material','ModelAnimation','AutomationEvent','unsigned short','BoneInfo','Mesh','MaterialMap'])}
    buffer_types.update({'unsigned char':0,'char':1})
    def array_name(t):
        return {'int':'int_array','unsigned int':'uint_array','float':'float_array','unsigned short':'ushort_array','unsigned char':'byte_buffer','char':'byte_buffer'}.get(t,snake(t)+'_array')
    for t,kind in buffer_types.items():
        if t in ('unsigned char','char'):continue
        n=array_name(t)
        if t not in scalar and t not in pod and not owner(t):continue
        size_expr='sizeof('+t+')'
        emit('buffer_'+snake(t)+'_new','vd_raylib_buffer_'+snake(t)+'_new','owner:owner_buffer',['c_size'],'VdBuffer*',['size_t count'],
             f'    vd_error=0; if(count>64*1024*1024/{size_expr}) {{vd_error=2;return NULL;}}\n    void* data=MemAlloc((unsigned int)(count*{size_expr}+1)); if(!data){{vd_error=2;return NULL;}} memset(data,0,count*{size_expr}+1);\n    VdBuffer* p=vd_buffer(data,count,{size_expr},{kind},true,NULL,vd_mem_release); if(!p)MemFree(data);\n'+
             ('    if(p){p->keepers=calloc(count?count:1,sizeof(VdBase*));p->epochs=calloc(count?count:1,sizeof(uint64_t));if(!p->keepers||!p->epochs){vd_error=2;vd_drop(&p->base);return NULL;}}\n' if owner(t) else '')+'    return p;')
        getter='buffer_'+snake(t)+'_get';setter='buffer_'+snake(t)+'_set'
        guard=f'    vd_error=0; if(!vd_buffer_live(p)||p->kind!={kind}||index>=p->count){{vd_error=1;'
        if owner(t):
            rt='owner:'+ownername(t);crt=ctype(t)+'*'
            getcode=guard+'return NULL;}\n'+f'    return vd_own_{snake(t)}((({t}*)p->data)[index],false,(VdBase*)&p->base);'
            setparams=['mut:owner_buffer','c_size','borrow:'+ownername(t)];setcparams=['VdBuffer* p','size_t index','const '+ctype(t)+'* value']
            setcode=guard+'return;}\n    if(!vd_live(&value->base)){vd_error=1;return;}\n    for(const VdBase* ancestor=&value->base;ancestor;ancestor=ancestor->parent)if(ancestor==&p->base){vd_error=1;return;}\n'+f'    if(!p->keepers){{vd_error=1;return;}}\n    vd_keep((VdBase*)&value->base); vd_drop(p->keepers[index]); p->keepers[index]=(VdBase*)&value->base; p->epochs[index]=value->base.epoch;\n    (({t}*)p->data)[index]=*value->value; vd_change(&p->base);'
            sgettype=snake(t)+'?';sget=f'    return _wrap_{snake(t)}(value)';svalue='value.owner'
        else:
            rt=maptype(t);crt=t
            getcode=guard+f'return ({t}){{0}};}}\n    return ((const {t}*)p->data)[index];'
            setparams=['mut:owner_buffer','c_size',rt];setcparams=['VdBuffer* p','size_t index',t+' value']
            setcode=guard+f'return;}}\n    if(!p->base.owned||p->base.refs!=1){{vd_error=1;return;}}\n    (({t}*)p->data)[index]=value; vd_change(&p->base);'
            sgettype=stype(t);sget='    _check()\n    return value';svalue='value'
        emit(getter,'vd_raylib_'+getter,rt,['borrow:owner_buffer','c_size'],crt,['const VdBuffer* p','size_t index'],getcode)
        emit(setter,'vd_raylib_'+setter,'void',setparams,'void',setcparams,setcode)
    # Nested owner fields never create a second allocation owner. Their native
    # parent is retained, and mutation invalidates old views before the next use.
    field_extents={
        'Image': {'data':('unsigned char','vd_image_bytes(*p->value)*(p->out_count>0?(size_t)p->out_count:1)')},
        'Font': {'recs':('Rectangle','p->value->glyphCount'),'glyphs':('GlyphInfo','p->value->glyphCount')},
        'Mesh': {**{f:('float','(size_t)p->value->vertexCount*'+str(k)) for f,k in
                  [('vertices',3),('texcoords',2),('texcoords2',2),('normals',3),('tangents',4),('boneWeights',4),('animVertices',3),('animNormals',3)]},
                 'colors':('unsigned char','(size_t)p->value->vertexCount*4'),
                 'boneIndices':('unsigned char','(size_t)p->value->vertexCount*4'),
                 'indices':('unsigned short','(size_t)p->value->triangleCount*3')},
        'Material': {'maps':('MaterialMap','MATERIAL_MAP_BRDF+1')},
        'ModelSkeleton': {'bones':('BoneInfo','p->value->boneCount'),'bindPose':('Transform','p->value->boneCount')},
        'Model': {'meshes':('Mesh','p->value->meshCount'),'materials':('Material','p->value->materialCount'),
                  'meshMaterial':('int','p->value->meshCount'),'currentPose':('Transform','p->value->skeleton.boneCount'),
                  'boneMatrices':('Matrix','p->value->skeleton.boneCount')},
        'Wave': {'data':('unsigned char','(size_t)p->value->frameCount*p->value->channels*(p->value->sampleSize/8)')},
        'FilePathList': {'paths':('char*','p->value->count')},
        'AutomationEventList': {'events':('AutomationEvent','p->value->count')},
    }
    body += [r"""
static size_t vd_image_bytes(Image image) {
    size_t bytes=0;int w=image.width,h=image.height;
    for(int i=0;i<image.mipmaps&&w>0&&h>0;++i){int size=GetPixelDataSize(w,h,image.format);if(size<0)return 0;bytes+=(size_t)size;w=w>1?w/2:1;h=h>1?h/2:1;}
    return bytes;
}
"""]
    for t in actual_owners:
        n=snake(t);on=ownername(t);ct=ctype(t)
        record=next(x for x in nodes if x.get('kind')=='RecordDecl' and x.get('name')==t)
        for f in record.get('inner',[]):
            if f.get('kind')!='FieldDecl':continue
            field=f['name'];ft=canon(f['type']['qualType']);fn=snake(field)
            if ft in scalar or ft in pod:continue # emitted above
            get=n+'_get_'+fn
            if owner(ft):
                emit(get,'vd_raylib_'+get,'owner:'+ownername(ft),['borrow:'+on],ctype(ft)+'*',[f'const {ct}* p'],
                     f'    vd_error=0;if(!vd_live(&p->base)){{vd_error=1;return NULL;}} return vd_own_{snake(ft)}(p->value->{field},false,(VdBase*)&p->base);')
                public=snake(ft)
            else:
                public='opaque_handle';element=None;count='0';kind=4;data=f'p->value->{field}'
                extra=''
                if field in field_extents.get(t,{}):element,count=field_extents[t][field]
                fixed=re.fullmatch(r'(.+?)\s*\[(\d+)\]',ft)
                if fixed:element,count=canon(fixed[1]),fixed[2]
                if t=='ModelAnimation' and field=='keyframePoses':
                    kind=31;count='p->value->keyframeCount';public='pose_array';extra='if(value)value->child_count=(size_t)p->value->boneCount;'
                elif element:
                    kind=30 if element=='char*' else buffer_types[element]
                    public='string_array' if element=='char*' else array_name(element)
                stride='sizeof('+element+')' if element else 'sizeof(void*)' if kind==31 else '1'
                emit(get,'vd_raylib_'+get,'owner:owner_buffer',['borrow:'+on],'VdBuffer*',[f'const {ct}* p'],
                     f'    vd_error=0;if(!vd_live(&p->base)){{vd_error=1;return NULL;}}'+('' if fixed else f'if(!{data})return NULL;')+'\n'
                     f'    VdBuffer* value=vd_buffer((void*){data},(size_t)({count}),{stride},{kind},false,(VdBase*)&p->base,NULL);{extra}return value;')
    emit('poses_get','vd_raylib_poses_get','owner:owner_buffer',['borrow:owner_buffer','c_size'],'VdBuffer*',['const VdBuffer* p','size_t index'],
         f'    vd_error=0;if(!vd_buffer_live(p)||p->kind!=31||index>=p->count){{vd_error=1;return NULL;}}return vd_buffer(((Transform**)p->data)[index],p->child_count,sizeof(Transform),{buffer_types["Transform"]},false,(VdBase*)&p->base,NULL);')
    for t in ('Image','Font'):
        n=snake(t)
        emit(n+'_output_count','vd_raylib_'+n+'_output_count','i32',['borrow:'+ownername(t)],'int32_t',[f'const {ctype(t)}* p'],
             '    vd_error=0;if(!vd_live(&p->base)){vd_error=1;return 0;}return p->out_count;')
    outcomes={name:{'void_name':snake(name),'native_abi':'ABI 3 sealed resource / exact prototype in generated descriptor',
        'ownership':'Explicit context owner; native close waits for dependent native resources. OS handle is a parented opaque borrow.' if name!='SetWindowTitle' else 'Owned terminated UTF-8 retained until title replacement/window destruction.',
        'implementation':'checked native context adapter','blocker':None,'test_coverage':'native compilation; representative context lifecycle tests in examples/raylib-consumer/src/subsystems.void'}
        for name in ('InitWindow','CloseWindow','InitAudioDevice','CloseAudioDevice','SetWindowTitle','GetWindowHandle')}
    functions={n['name']:n for n in nodes if n.get('kind')=='FunctionDecl'}
    # Native out parameters become named VOID result fields or owner metadata.
    decls += ['typedef struct VdIntOut { int32_t value; int32_t size; } VdIntOut;',
              'typedef struct VdCollisionOut { bool hit; Vector2 point; } VdCollisionOut;']
    records += ['record int_out VdIntOut 8 4 2','field value value i32 0','field size size i32 4',
                'record collision_out VdCollisionOut 12 4 2','field hit hit c_bool 0','field point point record:vector2 4']
    for name,(element,count,release) in POINTER_RESULTS.items():
        if not release:continue
        symbol='vd_release_'+name
        if release=='font_data':action='UnloadFontData((GlyphInfo*)p->data,(int)p->count);'
        elif release=='materials':action='vd_unload_materials((Material*)p->data,p->count); MemFree(p->data);'
        elif release=='animations':action='UnloadModelAnimations((ModelAnimation*)p->data,(int)p->count);'
        elif release=='lines':action='UnloadTextLines((char**)p->data,(int)p->count);'
        else:action=f'{release}(({element}*)p->data);'
        body.append(f'static void {symbol}(VdBuffer* p) {{ {action} }}')
    for name,release in ALLOCATED_STRINGS.items():
        body.append(f'static void vd_release_{name}(VdBuffer* p) {{ {release}((char*)p->data); }}')
    def manual(name, result, parameters, cresult, cparams, code, ownership):
        emit(snake(name),'vd_raylib_api_'+name,result,parameters,cresult,cparams,code)
        outcomes[name]={'void_name':snake(name),'native_abi':{'result':result,'parameters':parameters},
            'ownership':ownership,'implementation':'audited thin adapter and existing native resource ledger',
            'blocker':None,'test_coverage':'native compile/link; representative ownership/runtime tests in examples/raylib-consumer/src/subsystems.void'}
    outcomes['MemAlloc']={'void_name':'mem_alloc','native_abi':'owner:owner_buffer c_size u32','ownership':'Native MemAlloc storage, released once with MemFree by the ledger.',
        'implementation':'existing buffer allocator adapter','blocker':None,'test_coverage':'allocation/cleanup runtime tests'}
    manual('MemRealloc','c_bool',['mut:owner_buffer','u32'],'bool',['VdBuffer* p','uint32_t size'],r'''
    vd_error=0;if(!vd_bytes_live(p)||!p->base.owned||p->base.parent||p->base.refs!=1||p->stride!=1||size>64*1024*1024){vd_error=1;return false;}
    void* value=MemRealloc(p->data,size+1);if(!value)return false;p->data=value;p->count=size;
    ((uint8_t*)value)[size]=0;vd_change(&p->base);return true;''',
        'Resizes the same sealed owner; allocation failure preserves the original storage. Live dependent borrows forbid realloc.')
    manual('LoadDroppedFiles','owner:owner_file_path_list',[],'VdFilePathList*',[],r"""
    vd_error=0;if(!vd_window){vd_error=1;return NULL;}
    FilePathList raw=LoadDroppedFiles();VdBuffer* copy=vd_strings_copy(raw.paths,raw.count);UnloadDroppedFiles(raw);
    if(!copy)return NULL;FilePathList snapshot={.count=raw.count,.paths=copy->data};
    copy->base.owned=false;vd_drop(&copy->base);return vd_own_file_path_list(snapshot,true,NULL);""",
        'Deep snapshot of the native event list; acknowledges the upstream dropped-file allocation immediately, so later input polling cannot invalidate the source result.')
    outcomes['UnloadDroppedFiles']={'void_name':'unload_dropped_files','native_abi':'close:owner_file_path_list',
        'ownership':'Consumes the copied event snapshot; the original upstream drop allocation was acknowledged by LoadDroppedFiles.',
        'implementation':'terminal snapshot release','blocker':None,'test_coverage':'native compilation and window lifecycle test'}
    manual('SetShapesTexture','void',['borrow:owner_texture','record:rectangle'],'void',
        ['const VdTexture* texture','Rectangle source'],r"""
    vd_error=0;if(!vd_window||!vd_live(&texture->base)){vd_error=1;return;}
    vd_keep((VdBase*)&texture->base);SetShapesTexture(*texture->value,source);
    vd_drop(vd_shapes);vd_shapes=(VdBase*)&texture->base;""",
        'Retains the selected texture until replacement or explicit window close; the global shapes borrow cannot dangle.')
    manual('SetWindowIcons','void',['borrow:owner_buffer','i32'],'void',['const VdBuffer* p','int32_t count'],f'''
    vd_error=0;if(count<1||p->kind!={buffer_types['Image']}||!vd_extent(p,(size_t)count,sizeof(Image))){{vd_error=1;return;}}
    SetWindowIcons((Image*)p->data,count);''',
        'Synchronous image-array borrow; every underlying image remains retained and generation-checked.')
    manual('GetCodepointPrevious','record:int_out',['borrow:owner_buffer','c_size'],'VdIntOut',['const VdBuffer* p','size_t position'],r'''
    vd_error=0;const char* text=vd_text(p);if(vd_error||!text||!position||position>p->count){vd_error=1;return(VdIntOut){0};}
    int size=0;int value=GetCodepointPrevious(text+position,&size);return(VdIntOut){value,size};''',
        'Explicit byte cursor into valid UTF-8; returned value/size preserve the native out parameter.')
    manual('TextCopy','i32',['mut:owner_buffer','borrow:owner_buffer'],'int32_t',['VdBuffer* dst','const VdBuffer* src'],r'''
    vd_error=0;const char* text=vd_text(src);if(vd_error||!text||!vd_bytes_live(dst)||(!dst->base.owned||dst->base.parent||dst->base.refs!=1)||!vd_extent(dst,src->count+1,1)){vd_error=1;return 0;}
    int count=TextCopy((char*)dst->data,text);vd_change(&dst->base);return count;''',
        'Caller-owned writable capacity is checked including the terminator; return is the native copied count.')
    manual('TextAppend','i32',['mut:owner_buffer','borrow:owner_buffer','i32'],'int32_t',['VdBuffer* dst','const VdBuffer* src','int32_t position'],r'''
    vd_error=0;const char* text=vd_text(src);
    if(vd_error||!text||!vd_bytes_live(dst)||position<0||(size_t)position>dst->count||(!dst->base.owned||dst->base.parent||dst->base.refs!=1)||!vd_extent(dst,(size_t)position+src->count+1,1)){vd_error=1;return 0;}
    int at=position;TextAppend((char*)dst->data,text,&at);vd_change(&dst->base);return at;''',
        'Capacity includes the append and terminator; the returned position is the updated native in/out argument.')
    manual('TextJoin','owner:owner_buffer',['borrow:owner_buffer','i32','borrow:owner_buffer'],'VdBuffer*',
        ['const VdBuffer* list','int32_t count','const VdBuffer* separator'],r'''
    vd_error=0;const char* delimiter=vd_text(separator);
    if(vd_error||!delimiter||count<0||list->kind!=30||!vd_extent(list,(size_t)count,sizeof(char*))){vd_error=1;return NULL;}
    for(int i=0;i<count;++i)if(!((char**)list->data)[i]){vd_error=1;return NULL;}
    const char* value=TextJoin((char**)list->data,count,delimiter);return value?vd_copy(value,strlen(value),1,1):NULL;''',
        'Static native result is copied before another native call; input string list and delimiter remain borrowed.')
    manual('LoadModelFromMesh','owner:owner_model',['mut:owner_mesh'],'VdModel*',['VdMesh* mesh'],r'''
    vd_error=0;if(!vd_window||!vd_live(&mesh->base)||!mesh->base.owned||mesh->base.refs!=1){vd_error=1;return NULL;}
    VdModel* p=calloc(1,sizeof *p);if(!p){vd_error=2;return NULL;}
    Model value=LoadModelFromMesh(*mesh->value);
    vd_base(&p->base,true,&vd_window->base,vd_destroy_model);p->storage=value;p->value=&p->storage;
    mesh->base.owned=false;*mesh->value=(Mesh){0};return p;''',
        'Transfers mesh allocations into the model and consumes the source mesh owner only after successful acquisition.')
    manual('SetMaterialTexture','void',['mut:owner_material','i32','mut:owner_texture'],'void',
        ['VdMaterial* material','int32_t map','VdTexture* texture'],r'''
    vd_error=0;if(!vd_live(&material->base)||!vd_live(&texture->base)||!material->base.owned||material->base.refs!=1||!texture->base.owned||texture->base.refs!=1||map<0||map>MATERIAL_MAP_BRDF||!material->value->maps){vd_error=1;return;}
    Texture old=material->value->maps[map].texture;
    if(old.id!=texture->value->id){Material temporary=LoadMaterialDefault();temporary.maps[MATERIAL_MAP_ALBEDO].texture=old;UnloadMaterial(temporary);}
    SetMaterialTexture(material->value,map,*texture->value);texture->base.owned=false;*texture->value=(Texture){0};vd_change(&material->base);''',
        'Explicit texture ownership transfer; previous map texture is released, and the consumed texture cannot be reused.')
    manual('LoadSoundAlias','owner:owner_sound',['borrow:owner_sound'],'VdSound*',['const VdSound* source'],r'''
    vd_error=0;if(!vd_live(&source->base)||!source->value->stream.buffer){vd_error=1;return NULL;}
    VdSound* p=calloc(1,sizeof *p);if(!p){vd_error=2;return NULL;}
    Sound value=LoadSoundAlias(*source->value);if(!value.stream.buffer){free(p);return NULL;}
    vd_base(&p->base,true,(VdBase*)&source->base,vd_destroy_sound);p->storage=value;p->value=&p->storage;p->out_count=1;return p;''',
        'Alias uses UnloadSoundAlias and retains the source sound and audio device until its terminal release.')
    outcomes['UnloadSoundAlias']={'void_name':'unload_sound_alias','native_abi':'close:owner_sound',
        'ownership':'Sound owner records whether the native allocation is an alias and calls its matching unload exactly once.',
        'implementation':'existing terminal resource release','blocker':None,'test_coverage':'alias ownership runtime tests'}
    manual('GenImageFontAtlas','owner:owner_image',['borrow:owner_buffer','i32','i32','i32','i32'],'VdImage*',
        ['const VdBuffer* glyphs','int32_t count','int32_t font_size','int32_t padding','int32_t method'],f'''
    vd_error=0;if(count<1||font_size<1||padding<0||glyphs->kind!={buffer_types['GlyphInfo']}||!vd_extent(glyphs,(size_t)count,sizeof(GlyphInfo))){{vd_error=1;return NULL;}}
    Rectangle* recs=NULL;Image value=GenImageFontAtlas((const GlyphInfo*)glyphs->data,&recs,count,font_size,padding,method);
    VdImage* image=vd_own_image(value,true,NULL);if(!image){{MemFree(recs);return NULL;}}
    image->extra=vd_buffer(recs,(size_t)count,sizeof(Rectangle),{buffer_types['Rectangle']},true,NULL,vd_mem_release);
    if(!image->extra){{MemFree(recs);vd_drop(&image->base);return NULL;}}return image;''',
        'Image owns its CPU pixels and returned glyph rectangles; rectangle views retain the atlas image.')
    emit('image_atlas_rects','vd_raylib_image_atlas_rects','owner:owner_buffer',['borrow:owner_image'],
        'VdBuffer*',['const VdImage* image'],r'''
    vd_error=0;if(!vd_live(&image->base)){vd_error=1;return NULL;}if(!image->extra)return NULL;
    return vd_buffer(image->extra->data,image->extra->count,image->extra->stride,image->extra->kind,false,(VdBase*)&image->base,NULL);''')
    for entry in inventory['functions']:
        if entry['void_name'] is not None:continue
        name=entry['name'];node=functions[name]
        if name in outcomes:continue
        result=canon(node['type']['qualType'].split('(',1)[0].strip())
        ps=[p for p in node.get('inner',[]) if p.get('kind')=='ParmVarDecl']
        terminal=next((t for t in actual_owners if OWNERS[t][0]==name),None)
        if terminal:
            n=snake(terminal)
            outcomes[name]={'void_name':snake(name),'native_abi':{'result':'i32','parameters':['close:'+ownername(terminal)]},
                'ownership':f'Consumes the ledger owner; native destruction calls {name} once after dependent views drain.',
                'implementation':'existing native resource terminal release','blocker':None,'test_coverage':'native prototype/layout compilation; representative ownership tests in examples/raylib-consumer/src/subsystems.void, not individual execution of every declaration'}
            continue
        if name in UNLOAD_BUFFERS:
            element=next((spec[0] for producer,spec in POINTER_RESULTS.items() if spec[2]==name),None)
            element={'UnloadFontData':'GlyphInfo','UnloadTextLines':'char*','UnloadModelAnimations':'ModelAnimation'}.get(name,element)
            if name in ('UnloadFileText','UnloadUTF8','MemFree'):element='unsigned char'
            if element is None:continue
            public=array_name(element) if element!='char*' else 'string_array'
            extra=', count: i32' if len(ps)>1 else ''
            outcomes[name]={'void_name':snake(name),'native_abi':'existing close:owner_buffer',
                'ownership':f'Sealed buffer owns its original allocator and count; terminal release invokes {name} for matching producer.',
                'implementation':'native buffer terminal release','blocker':None,'test_coverage':'native compilation; representative allocated-result tests in examples/raylib-consumer/src/subsystems.void'}
            continue
        if node.get('variadic') or any('Callback' in p['type']['qualType'] for p in ps):continue
        if name in SPECIAL:continue
        pointer_result=name in POINTER_RESULTS or result in ('char *','const char *')
        mutable_camera=name in ('UpdateCamera','UpdateCameraPro')
        collision=name=='CheckCollisionLines'
        out_int=name in OUT_FUNCTIONS
        int_pair=name in ('GetCodepoint','GetCodepointNext')
        transformed_result='Camera3D' if mutable_camera else 'VdCollisionOut' if collision else 'VdIntOut' if int_pair else result
        error_return='return;' if transformed_result=='void' else 'return NULL;' if pointer_result or owner(result) else f'return ({transformed_result}){{0}};'
        params=[];cparams=[];args=[];preamble=['    vd_error=0;'];failed=None;mut=[]
        if out_int:preamble.append('    int out_count=0;')
        if collision:preamble.append('    Vector2 out_point={0};')
        names={p.get('name'):i for i,p in enumerate(ps)}
        for i,p in enumerate(ps):
            ct=canon(p['type']['qualType']);pn=p['name'];v='p'+str(i)
            if out_int and ct=='int *' and (pn in OUT_INT or pn=='count'):
                args.append('&out_count');continue
            if collision and pn=='collisionPoint':args.append('&out_point');continue
            if mutable_camera and i==0:
                params.append('record:camera3_d');cparams.append('Camera3D '+v);args.append('&'+v)
                continue
            if ct=='const char *':
                optional=name in ('LoadShader','LoadShaderFromMemory','LoadAutomationEventList')
                params.append('borrow:owner_buffer');cparams.append('const VdBuffer* '+v);args.append('text'+str(i))
                preamble.append(f'    const char* text{i}=vd_text({v}); if(vd_error){{{error_return}}}')
            elif ct in scalar or ct in pod:
                params.append(maptype(ct));cparams.append(ct+' '+v);args.append(v)
            elif ct.endswith(' *') and (ct[:-2].removeprefix('const ') in buffer_types or ct[:-2].removeprefix('const ')=='void') and not (owner(ct[:-2]) and pn not in COUNT_ARGS.get(name,{})):
                element=ct[:-2].removeprefix('const ')
                is_write=name=='SetPixelColor'
                params.append(('mut:' if is_write else 'borrow:')+'owner_buffer');cparams.append(('' if is_write else 'const ')+'VdBuffer* '+v)
                args.append('('+ct+')('+v+'->count?'+v+'->data:NULL)')
                if element in ('void','unsigned char'):
                    preamble.append(f'    if(!vd_bytes_live({v})){{vd_error=1;{error_return}}}')
                if is_write:
                    preamble.append(f'    if(!{v}->base.owned||{v}->base.parent||{v}->base.refs!=1){{vd_error=1;{error_return}}}')
                count_name=COUNT_ARGS.get(name,{}).get(pn)
                if not count_name:
                    if pn in ('points','transforms'):count_name=next((p['name'] for p in ps if p['name'] in ('pointCount','instances')),None)
                    elif pn=='kernel':count_name='kernelSize'
                    elif pn in ('fileData','data','compData'):count_name=next((p['name'] for p in ps if p['name'] in ('dataSize','compDataSize','sampleCount','frameCount')),None)
                if count_name and count_name in names:
                    ci=names[count_name];stride='1' if element in ('void','unsigned char') else 'sizeof('+element+')'
                    count_expr=f'p{ci}'
                    if name=='UpdateSound':stride='p0->value->stream.channels*(p0->value->stream.sampleSize/8)'
                    if name=='UpdateAudioStream':stride='p0->value->channels*(p0->value->sampleSize/8)'
                    preamble.append(f'    if(p{ci}<0 || !vd_extent({v},(size_t){count_expr},{stride})){{{error_return}}}')
                elif name in ('ComputeMD5','ComputeSHA1','ComputeSHA256','ComputeCRC32','SaveFileData','ExportDataAsCode','CompressData','DecompressData','EncodeDataBase64'):
                    ci=names.get('dataSize',names.get('compDataSize'))
                    if ci is None:failed=ct;break
                    preamble.append(f'    if(p{ci}<0 || !vd_extent({v},(size_t)p{ci},1)){{{error_return}}}')
                elif name in ('SetShaderValue','SetShaderValueV'):
                    ui=names['uniformType'];count='p'+str(names['count']) if 'count' in names else '1'
                    preamble.append(f'    if(p{ui}<SHADER_UNIFORM_FLOAT||p{ui}>SHADER_UNIFORM_SAMPLER2D||({count})<0||!vd_extent({v},(size_t)({count}),(p{ui}==SHADER_UNIFORM_SAMPLER2D?1:p{ui}%4+1)*4)){{{error_return}}}')
                elif name in ('UpdateTexture','UpdateTextureRec','GetPixelColor','SetPixelColor'):
                    if name=='UpdateTexture':size='GetPixelDataSize(p0->value->width,p0->value->height,p0->value->format)'
                    elif name=='UpdateTextureRec':size='GetPixelDataSize((int)p1.width,(int)p1.height,p0->value->format)'
                    elif name=='GetPixelColor':size='GetPixelDataSize(1,1,p1)'
                    else:size='GetPixelDataSize(1,1,p2)'
                    preamble.append(f'    int pixel_bytes={size}; if(pixel_bytes<=0||!vd_extent({v},(size_t)pixel_bytes,1)){{{error_return}}}')
                else:failed=ct;break
                if element not in ('void','unsigned char'):
                    preamble.append(f'    if({v}->kind!={buffer_types[element]}||{v}->stride!=sizeof({element})){{vd_error=1;{error_return}}}')
                if is_write:mut.append(v)
            elif owner(ct) or (ct.endswith(' *') and owner(ct[:-2])):
                base=ct[:-2] if ct.endswith(' *') else ct
                changes=ct.endswith(' *') or (name.startswith(('Set','Update','Play','Pause','Resume','Stop','Upload','GenMeshTangents','GenTextureMipmaps')) and i==0)
                params.append(('mut:' if changes else 'borrow:')+ownername(base));cparams.append(('' if changes else 'const ')+ctype(base)+'* '+v)
                args.append(v+'->value' if ct.endswith(' *') else '*'+v+'->value')
                preamble.append(f'    if (!vd_live(&{v}->base)) {{vd_error=1;{error_return}}}')
                if changes:mut.append(v)
                if ct.endswith(' *') and base in ('Image','Wave','Mesh','Texture'):
                    preamble.append(f'    if(!{v}->base.owned||{v}->base.refs!=1){{vd_error=1;{error_return}}}')
                    if base=='Image':
                        preamble.append(f'    if({v}->out_count>1){{vd_error=1;{error_return}}}')
            else:failed=ct;break
        if failed:continue
        if name=='UpdateSound':
            ci=names.get('sampleCount',names.get('frameCount'))
            preamble.append(f'    if(p{ci}<0||(unsigned int)p{ci}>p0->value->frameCount){{vd_error=1;{error_return}}}')
        if name=='ImageKernelConvolution':
            ci=names['kernelSize'];preamble.append(f'    if(p{ci}<1){{vd_error=1;{error_return}}}')
        if name=='UpdateMeshBuffer':
            preamble.append(f'    if(p1<0||p3<0||p4<0){{vd_error=1;{error_return}}}')
        if name=='LoadMaterials':preamble.append('    if(!vd_window){vd_error=1;return NULL;}')
        if name=='UploadMesh':
            preamble.append('    if(!vd_window){vd_error=1;return;}if(!p0->retained){p0->retained=&vd_window->base;vd_keep(p0->retained);}')
        if name=='LoadMusicStreamFromMemory':
            i=names['data'];ci=names['dataSize']
            preamble.append('    if(!vd_audio){vd_error=1;return NULL;}')
            preamble.append(f'    VdBuffer* music_data=vd_copy(p{i}->data,(size_t)p{ci},1,0);if(!music_data)return NULL;')
            args[i]='(const unsigned char*)music_data->data'
        call=f'{name}({", ".join(args)})'
        if pointer_result:
            ret='owner:owner_buffer';cret='VdBuffer*'
            rawtype=node['type']['qualType'].split('(',1)[0].strip()
            code='\n'.join(preamble)+f'\n    {rawtype} raw={call}; if(!raw)return NULL;\n'
            owned=False
            if name in POINTER_RESULTS:
                element,count,release=POINTER_RESULTS[name];kind=buffer_types.get(element,30 if element=='char*' else 0)
                count=count.replace('p0->value->width*p0->value->height','(size_t)p0->value->width*p0->value->height')
                if release:
                    code+=f'    if((int64_t)({count})<0){{vd_error=1;return NULL;}}\n    VdBuffer* value=vd_buffer(raw,(size_t)({count}),sizeof({element}),{kind},true,{"&vd_window->base" if name=="LoadMaterials" else "NULL"},vd_release_{name});\n    if(!value){{VdBuffer rejected={{0}};rejected.data=raw;rejected.count=(size_t)({count});vd_release_{name}(&rejected);}}\n    return value;'
                elif element=='char*':code+=f'    return vd_strings_copy(raw,(size_t)({count}));'
                else:code+=f'    return vd_copy(raw,(size_t)({count}),sizeof({element}),{kind});'
                public=array_name(element) if element!='char*' else 'string_array';owned=True
            elif name in ALLOCATED_STRINGS:
                public='byte_buffer';owned=True
                code+=f'    VdBuffer* value=vd_buffer(raw,strlen(raw),1,1,true,{"&vd_window->base" if name=="LoadMaterials" else "NULL"},vd_release_{name});if(!value){ALLOCATED_STRINGS[name]}(raw);return value;'
            elif name=='CodepointToUTF8':
                public='byte_buffer';owned=True
                code+='    return vd_copy(raw,(size_t)out_count,1,1);'
            else:
                public='byte_buffer';owned=True;code+='    return vd_copy(raw,strlen(raw),1,1);'
        elif owner(result):
            ret='owner:'+ownername(result);cret=ctype(result)+'*'
            parent='(VdBase*)&p0->base' if name in BORROWED_RESULTS and ps and owner(canon(ps[0]['type']['qualType'])) else 'NULL'
            if (result in ('Texture','RenderTexture','Font','Shader','Material','Model') or result=='Mesh' and name.startswith('GenMesh')) and parent=='NULL':
                preamble.append('    if(!vd_window){vd_error=1;return NULL;}');parent='&vd_window->base'
            if result in ('AudioStream','Sound','Music'):
                preamble.append('    if(!vd_audio){vd_error=1;return NULL;}');parent='&vd_audio->base'
            own='false' if name in BORROWED_RESULTS else 'true'
            valid=OWNERS[result][1]
            failure='vd_drop(&music_data->base);' if name=='LoadMusicStreamFromMemory' else ''
            if own=='true' and OWNERS[result][0]:
                terminal='vd_unload_model' if result=='Model' else OWNERS[result][0]
                failure=f'{terminal}(value);'+failure
            code='\n'.join(preamble)+f'\n    {result} value={call};\n    if(!({valid})){{{failure}return NULL;}}\n    {ctype(result)}* wrapped=vd_own_{snake(result)}(value,{own},{parent});'
            if out_int:code+='\n    if(wrapped)wrapped->out_count=out_count;'
            if name in RETAIN_INPUT:
                code+='\n    if(wrapped)wrapped->retained=&music_data->base;else vd_drop(&music_data->base);'
            code+='\n    return wrapped;'
        elif int_pair:
            ret='record:int_out';cret='VdIntOut'
            code='\n'.join(preamble)+f'\n    int value={call}; return (VdIntOut){{value,out_count}};'
        elif collision:
            ret='record:collision_out';cret='VdCollisionOut'
            code='\n'.join(preamble)+f'\n    bool value={call}; return (VdCollisionOut){{value,out_point}};'
        elif mutable_camera:
            ret='record:camera3_d';cret='Camera3D'
            code='\n'.join(preamble)+f'\n    {call}; return p0;'
        elif result in scalar or result in pod:
            ret=maptype(result);cret='int64_t' if result=='long' else result
            code='\n'.join(preamble)+'\n    '+('return ' if result!='void' else '')+call+';'
            if mut:
                if result=='void':code+='\n'+''.join(f'    vd_change(&{v}->base);\n' for v in mut)
                else:continue
            if name=='SetAutomationEventList':code+='\n    vd_active_events=p0;'
        else:continue
        symbol='vd_raylib_api_'+name;exposed=snake(name)
        emit(exposed,symbol,ret,params,cret,cparams,code)
        outcomes[name]={'void_name':exposed,'native_abi':{'result':ret,'parameters':params},
            'ownership':'owned native result with recorded allocator/extent and dependent-resource retention' if owner(result) or pointer_result else 'checked synchronous values and resource borrows; out parameters are explicit result fields',
            'implementation':'generated ownership/extent adapter and existing native-bind transport','blocker':None,
            'test_coverage':'native prototype/layout compilation; representative subsystem runtime tests in examples/raylib-consumer/src/subsystems.void, not individual execution of every declaration'}
    return dict(declarations=decls,body=body,api=[],resources=resources,records=records,bindings=binds,outcomes=outcomes)
