from pathlib import Path
import tempfile,shutil,re,json,os,sys,shlex,subprocess,hashlib,platform
root=Path.cwd();build=Path('/home/shmellyorc/.local/state/void-log-performance/framing-candidate-hAebdlqN/build')
d=Path(tempfile.mkdtemp(prefix='length-dispatch-',dir=build.parent.parent));Path('/tmp/void-length-dispatch-current').write_text(str(d));shutil.copy2(__file__,d/'static.py')
sys.path.insert(0,str(root/'tests/log'));import support as h
h.EVIDENCE=d/'receipts';h.EVIDENCE.mkdir();os.environ['VOID_CRASH_EVIDENCE']=str(d/'crashes')
original=subprocess.check_output(['git','show','HEAD:src/runtime/text_case.cpp'],text=True);candidate=(root/'src/runtime/text_case.cpp').read_text()
assert hashlib.sha256(original.encode()).hexdigest()=='a977bdb662633f28ac373a713f6daf4920cc62ebc2b40d3f840b54dd2fd6e24a'
(d/'original-text_case.cpp').write_text(original);(d/'candidate-text_case.cpp').write_text(candidate);(d/'candidate.patch').write_bytes(subprocess.check_output(['git','diff','--','src/runtime/text_case.cpp']))
folder=build/'CMakeFiles/void_log_host.dir';f=dict(re.findall(r'^(CXX_\w+) = (.*)$',(folder/'flags.make').read_text(),re.M));link=shlex.split((folder/'link.txt').read_text());flags=[link[0]]
for n in ['CXX_DEFINES','CXX_INCLUDES','CXX_FLAGS']:flags+=shlex.split(f[n])
def run(label,args):
 p=h.run(args,cwd=build,sources=[d],timeout=600);(d/(label+'.stdout')).write_bytes(p.stdout);(d/(label+'.stderr')).write_bytes(p.stderr);print(label,p.returncode,flush=True)
for name,text in [('original',original),('candidate',candidate)]:
 (d/(name+'-plain.cpp')).write_text(text.replace('#include "unicode_case_tables.inc"','#include "'+str(root/'src/runtime/unicode_case_tables.inc')+'"'))
 run('compile-'+name,[*flags,'-c',d/(name+'-plain.cpp'),'-o',d/(name+'-plain.o')])
 run('disassembly-'+name,[root/'.toolchains/llvm-23.1.4/bin/llvm-objdump','--disassemble','--demangle','--reloc','--no-show-raw-insn',d/(name+'-plain.o')])
 run('symbols-'+name,[root/'.toolchains/llvm-23.1.4/bin/llvm-nm','--print-size','--demangle','--size-sort',d/(name+'-plain.o')])
identity={'root':str(root),'retained_build':str(build),'head':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'uname':platform.uname()._asdict(),'compiler':subprocess.check_output([link[0],'--version'],text=True),'flags':flags,'env':{k:v for k,v in os.environ.items() if k in ['PATH','LANG','LC_ALL','LC_CTYPE','VOID_STDLIB_ROOT','LD_LIBRARY_PATH','CC','CXX']},'hashes':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [build/'libvoid_runtime.a',root/'stdlib/log.void',d/'candidate-text_case.cpp',d/'original-text_case.cpp']}}
(d/'identity.json').write_text(json.dumps(identity,indent=2)+'\n');print('STATIC READY',d)
