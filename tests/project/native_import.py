"""G25: real Clang import, editable values, reviewed SQLite ownership, and rejections.

SQLite inputs are independently downloaded/pinned by the caller; no network here.
"""
from pathlib import Path
from hashlib import sha256
import json,os,shutil,subprocess,sys,tempfile
compiler,clang=map(lambda p:Path(p).resolve(),sys.argv[1:3]);strict='ON' in sys.argv[3:]
sqlite=Path(sys.argv[4]).resolve() if len(sys.argv)>4 else None
windows=os.name=='nt';root=Path(__file__).resolve().parents[2];fixtures=root/'tests/ffi/native_import'
env=dict(os.environ)
if strict:env.update(ASAN_OPTIONS='halt_on_error=1:abort_on_error=1:detect_leaks=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
def run(*args,status=0,text=None,**kwargs):
 p=subprocess.run(list(map(str,args)),env=env,capture_output=True,timeout=300,**kwargs);output=(p.stdout+p.stderr).decode(errors='replace')
 assert p.returncode==status,(args,p.returncode,output)
 assert text is None or text in output,(args,output)
 assert not any(s in output for s in ['ERROR: AddressSanitizer','UndefinedBehaviorSanitizer:','LeakSanitizer: detected']),output
 for line in output.splitlines():
  if 'runtime error:' in line:assert status==1 and ': runtime error: Array operation failed during index access' in line,output
 return output
flags=['-shared','-fuse-ld=lld','-O1']+(['--target=x86_64-pc-windows-msvc','-fms-runtime-lib=static'] if windows else ['-fPIC'])
if strict:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all']
def generate(header,out,provider,source,language='c'):
 run(compiler,'native','import',header,'--clang',clang,'--provider',provider,'--out',out,'--source',source,'--language',language)
 return json.loads((out/'coverage.json').read_text())
def manifest_native(project,descriptor,library):
 native=project/'native';native.mkdir();shutil.copyfile(descriptor,native/'provider.descriptor')
 p=project/'void.project.toml';p.write_text(p.read_text()+f'\n[native.{"windows" if windows else "linux"}]\ndescriptor="provider.descriptor"\nlibrary="{library}"\nadapter="{library}"\n')
 return native
program='''use "./imported/api.void" as ffi
func main() -> i32:
    if ffi.vdi_call_0(1) != 101 or ffi.vdi_echo(ffi.vdi_record_0(40)).data != 42 or ffi.vdi_export != 11:
        return 6
    if ffi.g25_echo(ffi.g25_anonymous(40, 1.0)).n != 42:
        return 5
    var p = ffi.g25_shift(ffi.g25_point(5, 1.5), 3)
    var bundle = ffi.g25_bundle([p, p], [65 as i8, 66 as i8, 67 as i8, 68 as i8], 3.0)
    var transformed = ffi.g25_transform(bundle)
    if p.x != 8 or p.y != 2.0 or transformed.points[1].x != 13 or transformed.label[2] != 90 or transformed.gain != 6.0:
        return 1
    if ffi.g25_wide(5000000000) != 5000000017 or ffi.g25_unsigned_long(100) != 109 or ffi.g25_extent(10) != 13:
        return 2
    if ffi.g25_select(ffi.g25_negative) != 7 or not ffi.g25_is_positive(1):
        return 3
    if ffi.g25_negative != -3 or ffi.g25_positive != 7 or ffi.g25_base != 42 or ffi.g25_scale != 2.5 or ffi.g25_u64 != 18446744073709551615:
        return 4
    print("G25 VALUES:", ffi.g25_add(20, 22))
    return 0
'''
def execute(project,base,message):
 run(compiler,'check','--project',project)
 for profile in ['debug','release']:
  run(compiler,'run','--project',project,'--allow-native','--profile',profile,text=message)
  image=base/profile/('app.exe' if windows else 'app');run(compiler,'build','--project',project,'--allow-native','--profile',profile,'-o',image)
  if (project/'native').exists():shutil.copytree(project/'native',image.parent/'native',dirs_exist_ok=True)
  run(image,text=message,cwd=base)
with tempfile.TemporaryDirectory(prefix='VOID G25 native import ') as temporary:
 base=Path(temporary);project=base/'values';run(compiler,'new',project);source=next((project/'src').glob('*.void'));out=project/'src/imported'
 header=base/'values.h';shutil.copyfile(fixtures/'values.h',header)
 report=generate(header,out,'org.voidlang.g25.values','/src/imported/native.void');assert report['generated_functions']==11,report
 assert not any(r['category']!='generated' and r['name']!='G25_VALUES_H' for r in report['declarations']),report
 run(compiler,'native-bind',out/'provider.descriptor','imported.h','-o',out/'checked.c')
 library='values.dll' if windows else 'libvalues.so';native=manifest_native(project,out/'provider.descriptor',library)
 run(clang,'-std=c11',*flags,'-Wall','-Wextra','-Werror','-I'+str(out),'-I'+str(base),fixtures/'values.c',out/'imports.c',out/'checked.c','-o',native/library)
 source.write_text(program);execute(project,base/'values-exec','G25 VALUES: 42')
 # Existing native-bind checks actual Clang layouts, not just generated metadata.
 header.write_text(header.read_text().replace('float y;','double y;'))
 bad=subprocess.run([str(clang),'-std=c11','-fsyntax-only','-I'+str(out),str(out/'checked.c')],capture_output=True)
 assert bad.returncode!=0 and b'C layout' in bad.stderr,bad.stderr
 shutil.copyfile(fixtures/'values.h',header)
 native_source=out/'native.void';native_bytes=native_source.read_bytes();native_source.write_bytes(native_bytes.replace(b'func g25_add(arg0: i32',b'func g25_add(arg0: i64'))
 assert native_source.read_bytes()!=native_bytes
 run(compiler,'check','--project',project,status=1,text='Physical native signature differs');native_source.write_bytes(native_bytes)
 api=out/'api.void';original=api.read_bytes();api.write_bytes(original.replace(b'return native.g25_add(p0, p1)',b'return native.g25_add(p0, p1) + 1'))
 execute(project,base/'edited-exec','G25 VALUES: 43')
 inventory={p.name:sha256(p.read_bytes()).hexdigest() for p in out.iterdir()}
 run(compiler,'native','import',header,'--clang',clang,'--provider','org.voidlang.g25.values','--out',out,status=1,text='fresh output directory')
 assert inventory=={p.name:sha256(p.read_bytes()).hexdigest() for p in out.iterdir()}
 for label,args in [('missing',[base/'missing.h']),('provider',[header,'--provider','invalid']),('source',[header,'--source','/../native.void']),('parent',[header,'--out',base/'missing-parent/child'])]:
  destination=base/('failed-'+label);command=[compiler,'native','import',*args,'--clang',clang]
  if '--provider' not in args:command+=['--provider','org.voidlang.g25.failure']
  if '--out' not in args:command+=['--out',destination]
  run(*command,status=1);assert not destination.exists()
 only=base/'only.h';only.write_text('int only_varargs(int, ...);\n');coverage_only=base/'coverage-only'
 empty=generate(only,coverage_only,'org.voidlang.g25.empty','/native.void');assert not empty['descriptor_ready_for_review'] and not (coverage_only/'native.void').exists() and not (coverage_only/'provider.descriptor').exists()
 if os.environ.get('VOID_NATIVE_IMPORT_EVIDENCE'):shutil.copytree(project,Path(os.environ['VOID_NATIVE_IMPORT_EVIDENCE']))
 constants_header=base/'constants.h';constants_header.write_text('#define ANSWER (6*7)\nenum Plain { PlainNegative=-1 };\n');constants_out=base/'constants'
 constants=generate(constants_header,constants_out,'org.voidlang.g25.constants','/native.void');assert not constants['descriptor_ready_for_review'] and (constants_out/'api.void').exists()
 run(compiler,'check',constants_out/'api.void')
 wide_header=base/'wide-enum.h';wide_header.write_text('enum Wide { WideOut=18446744073709551614ULL, WideImplicit, WideReset=3, WideNext };\n');wide_out=base/'wide-enum'
 wide=generate(wide_header,wide_out,'org.voidlang.g25.wide','/native.void');wide_source=(wide_out/'api.void').read_text()
 scalar='i32' if windows else 'u64'
 assert 'wide_implicit' not in wide_source and 'wide_out' not in wide_source and f'wide_reset: {scalar} = 3' in wide_source and f'wide_next: {scalar} = 4' in wide_source,wide_source
 assert {row['name'] for row in wide['declarations'] if row['category']=='unsupported'}=={'WideOut','WideImplicit'}
 rejected=base/'rejected';coverage=generate(fixtures/'rejections.h',rejected,'org.voidlang.g25.rejected','/native.void')
 assert coverage['generated_functions']==1,coverage
 reasons={r['name']:r for r in coverage['declarations'] if r['category']!='generated'}
 for name in ['accepted','variadic','unresolved','Opaque','acquire_opaque','Callback','set_callback','Overlay','Flexible','Packed','Bits','STRING_MACRO','FUNCTION_MACRO']:assert name in reasons,(name,reasons)
 assert 'varargs' in reasons['variadic']['reason'] and 'duplicate' in reasons['accepted']['reason']
 assert 'const redefined: i32 = 2' in (rejected/'api.void').read_text() and 'const removed' not in (rejected/'api.void').read_text()
 assert 'variadic' not in (rejected/'native.void').read_text() and 'unresolved' not in (rejected/'native.void').read_text()
 # C++ support is only for explicit C-compatible entry points, not class/exception ABI.
 cpp=base/'cpp';run(compiler,'new',cpp);cppout=cpp/'src/imported';coverage=generate(fixtures/'cpp.hpp',cppout,'org.voidlang.g25.cpp','/src/imported/native.void','c++')
 assert coverage['generated_functions']==3,coverage
 assert any(r['name']=='overloaded' and 'extern C' in r['reason'] for r in coverage['declarations'])
 run(compiler,'native-bind',cppout/'provider.descriptor','imported.h','-o',cppout/'checked.c')
 lib='cpp.dll' if windows else 'libcpp.so';cppnative=manifest_native(cpp,cppout/'provider.descriptor',lib)
 run(clang,'-std=c++17',*flags,'-I'+str(cppout),fixtures/'cpp.cpp',cppout/'imports.cpp',cppout/'checked.cpp','-o',cppnative/lib)
 next((cpp/'src').glob('*.void')).write_text('use "./imported/api.void" as ffi\nfunc main() -> i32:\n    if ffi.c_shift(ffi.c_point(20, 1.0)).x != 21:\n        return 1\n    if not ffi.c_positive(1):\n        return 1\n    print("G25 CPP:", ffi.c_add(20, 22))\n    return 0\n')
 execute(cpp,base/'cpp-exec','G25 CPP: 42')
 print('G25 C values, exact layouts, aliases/enums/constants/arrays, editable normal/O2 JIT/AOT, C++ C linkage and rejection coverage passed',flush=True)
 if sqlite:
  sys.path.insert(0,str(fixtures));from sqlite_case import qualify
  qualify(base,sqlite,compiler,clang,flags,env,run,generate,execute)
