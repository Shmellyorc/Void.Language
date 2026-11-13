"""Typed last-twelve interfaces. G5 retains all managed callbacks and code leases."""
from pathlib import Path

ROOT=Path(__file__).resolve().parent
NAMES=['SetTraceLogCallback','SetLoadFileDataCallback','SetSaveFileDataCallback','SetLoadFileTextCallback','SetSaveFileTextCallback',
       'SetAudioStreamCallback','AttachAudioStreamProcessor','DetachAudioStreamProcessor','AttachAudioMixedProcessor','DetachAudioMixedProcessor']

def extend():
    declarations=[
        'typedef struct VdCallbackTarget VdCallbackTarget;',
        'typedef struct VdCallbackRegistration VdCallbackRegistration;',
        'typedef int32_t (*VdRaylibEvent)(void*,uint32_t,int32_t);',
        'typedef struct VdRaylibCallScope { bool entered; } VdRaylibCallScope;',
        'VOID_RAYLIB_EXPORT VdRaylibCallScope vd_raylib_call_begin(void);',
        'VOID_RAYLIB_EXPORT void vd_raylib_call_end(VdRaylibCallScope*);',
        'VOID_RAYLIB_EXPORT bool vd_raylib_global_callbacks_live(void);',
        'VOID_RAYLIB_EXPORT bool vd_raylib_api_trace_registered(void);']
    resources=[
        'resource callback_target VdCallbackTarget pointer null callback_target_new callback_target_close owner-calls shutdown-owner consume-always',
        'resource callback_registration VdCallbackRegistration pointer null callback_register callback_close owner-calls shutdown-owner consume-always']
    signatures=[
        ('callback_target_new','owner:callback_target',['u32'],'VdCallbackTarget*',['uint32_t']),
        ('callback_target_stream','owner:callback_target',['borrow:owner_audio_stream','u32'],'VdCallbackTarget*',['const VdAudioStream*','uint32_t']),
        ('callback_target_mixed','owner:callback_target',[],'VdCallbackTarget*',[]),
        ('callback_target_close','i32',['close:callback_target'],'int32_t',['VdCallbackTarget*']),
        ('callback_activate','void',['borrow:callback_target'],'void',['const VdCallbackTarget*']),
        ('callback_register','owner:callback_registration',['retained:raylib_event'],'VdCallbackRegistration*',['VdRaylibEvent','void*']),
        ('callback_close','i32',['close:callback_registration'],'int32_t',['VdCallbackRegistration*']),
        ('callback_count','u64',['borrow:callback_registration'],'uint64_t',['const VdCallbackRegistration*']),
        ('callback_active','c_bool',['borrow:callback_registration'],'bool',['const VdCallbackRegistration*']),
        ('callback_kind','u32',['borrow:callback_registration'],'uint32_t',['const VdCallbackRegistration*']),
        ('callback_input_size','c_size',['u32'],'size_t',['uint32_t']),
        ('callback_read','i64',['u32','c_size','out:3:4:65536','c_size','c_size'],'int64_t',['uint32_t','size_t','uint8_t*','size_t','size_t']),
        ('callback_output_size','void',['c_size'],'void',['size_t']),
        ('callback_write','i64',['c_size','in:2:3:65536','c_size','c_size'],'int64_t',['size_t','const uint8_t*','size_t','size_t'])]
    binds=[]
    for name,result,parameters,cresult,cparams in signatures:
        symbol='vd_raylib_'+name
        declarations.append(f'VOID_RAYLIB_EXPORT {cresult} {symbol}({", ".join(cparams) or "void"});')
        binds.append('bind '+name+' '+symbol+' '+result+(' '+' '.join(parameters) if parameters else ''))
    declarations.append('VOID_RAYLIB_EXPORT void vd_raylib_api_TraceLog(int32_t,const VdBuffer*);')
    binds.append('bind trace_log vd_raylib_api_TraceLog void i32 borrow:owner_buffer')
    outcomes={name:{'void_name':_snake(name),'exposure':'lifecycle_adapter','native_abi':'G5 retained context-first i32(u32,i32); owned target and bounded invocation-scoped byte I/O',
        'ownership':'G5 owns the managed closure/generation; native registration unregisters and drains before release. Global hooks require an exclusive application-thread window without an audio device; audio hooks retain their stream/device.',
        'implementation':'native context-free routing adapter to existing G5 callback ownership',
        'blocker':None,'native_limitation':'Global setters have no native concurrent-caller barrier; adapter rejects coexistence with audio and foreign-thread calls. PCM callbacks are synchronous, bounded and cannot call raylib recursively.',
        'test_coverage':'tests/project/raylib_callbacks.py; callback invocation/replacement/retirement/failure; real audio qualification'} for name in NAMES}
    for name in NAMES:
        outcomes[name]['lifecycle']=(
            'unregistration: consume the matching registration; native mixer mutex drains before G5 retirement' if name.startswith('Detach') else
            'retained global setter; synchronous invocation on the application thread; replacement or owned close unregisters; no audio coexistence' if name in NAMES[:5] else
            'retained stream-owned generator; foreign mixer-thread invocation; replacement or owned close unregisters and drains' if name=='SetAudioStreamCallback' else
            'retained stream-owned processor; ordered foreign mixer-thread invocation; matching detach/owned close unregisters and drains' if name=='AttachAudioStreamProcessor' else
            'retained audio-device-owned processor; ordered foreign mixer-thread invocation; matching detach/owned close unregisters and drains')
        if name.startswith('Detach'):
            outcomes[name]['native_abi']='G5 close:callback_registration -> i32; original native void detach uses the stream/function identity recorded by the matching attachment'
            outcomes[name]['implementation']='matching native detach through the existing G5 owned-registration terminal; kind checked before consumption'
    for name in ('TraceLog','TextFormat'):
        outcomes[name]={'void_name':_snake(name),'exposure':'safe_equivalent','native_abi':'Original C variadic declaration remains inventoried; no arbitrary varargs transport.',
          'ownership':'Owned UTF-8 text through TraceLog(level,"%s",text).' if name=='TraceLog' else 'Owned VOID string/result from existing format(value,spec) and interpolation.',
          'implementation':'typed text-and-level native adapter' if name=='TraceLog' else 'existing VOID formatter; one explicitly typed value/spec, no printf parser',
          'blocker':None,'native_limitation':'Arbitrary C varargs/va_list are intentionally not exposed.',
          'test_coverage':'tests/project/raylib_callbacks.py; percent-literal trace and supported format types'}
    outcomes['TraceLog']['native_abi']='Fixed void(i32, borrow:owner_buffer) adapter calls native TraceLog(level,"%s",text); original C varargs are not transported'
    outcomes['TextFormat']['native_abi']='Pure VOID generic value/spec -> result<string,string>; no C variadic call or native TextFormat binding'
    return dict(declarations=declarations,resources=resources,records=['callback raylib_event context-first i32 u32 i32'],bindings=binds,
                body=['#include "raylib_callbacks.inc"'],api=[],outcomes=outcomes)

def _snake(name):
    import re
    return re.sub(r'([a-z0-9])([A-Z])',r'\1_\2',re.sub(r'([A-Z]+)([A-Z][a-z])',r'\1_\2',name)).lower()
