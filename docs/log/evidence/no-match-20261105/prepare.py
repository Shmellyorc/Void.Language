from pathlib import Path
import tempfile, shutil, re, json, os, sys, shlex, subprocess, hashlib, platform
root=Path.cwd(); old=Path('/home/shmellyorc/.local/state/void-log-performance/translation-nfyczkon'); build=Path('/home/shmellyorc/.local/state/void-log-performance/framing-candidate-hAebdlqN/build')
d=Path(tempfile.mkdtemp(prefix='no-match-',dir=old.parent));Path('/tmp/void-no-match-current').write_text(str(d));shutil.copy2(__file__,d/'prepare.py')
sys.path.insert(0,str(root/'tests/log'));import support as h
h.EVIDENCE=d/'receipts';h.EVIDENCE.mkdir();os.environ['VOID_CRASH_EVIDENCE']=str(d/'crashes')
original=subprocess.check_output(['git','show','HEAD:src/runtime/text_case.cpp'],text=True);candidate=(root/'src/runtime/text_case.cpp').read_text()
(d/'original-text_case.cpp').write_text(original);(d/'candidate-text_case.cpp').write_text(candidate);(d/'candidate.patch').write_bytes(subprocess.check_output(['git','diff','--','src/runtime/text_case.cpp']))
for name in ['base.cpp','original.cpp','components.cpp','components.void','performance_metrics.hpp']:shutil.copy2(old/name,d/name)
base=(d/'base.cpp').read_text().replace('for(int command:{11})','for(int command:{11,18})')
base=base.replace('auto after=runtime->memory_snapshot();std::size_t bytes=', 'auto after=runtime->memory_snapshot();check(before.heap.payload_bytes==after.heap.payload_bytes && before.handles==after.handles,"post original resources");std::size_t bytes=')
(d/'base.cpp').write_text(base)
shutil.copytree(old/'stdlib',d/'stdlib')
assert (d/'stdlib/log.void').read_bytes()==(root/'stdlib/log.void').read_bytes()+b'\n'+(root/'tests/log/performance_framing.void').read_bytes()
for name,text in [('original',original),('candidate',candidate)]:
 (d/(name+'-plain.cpp')).write_text(text.replace('#include "unicode_case_tables.inc"','#include "'+str(root/'src/runtime/unicode_case_tables.inc')+'"'))
for target,source in [('ownership',root/'tests/text_encoding/ownership.cpp'),('log-host',root/'tests/log/host.cpp')]:
 (d/(target+'.cpp')).write_text(source.read_text().replace('#include "../managed/probe.hpp"','#include "'+str(root/'tests/managed/probe.hpp')+'"'))
folder=build/'CMakeFiles/void_log_host.dir';f=dict(re.findall(r'^(CXX_\w+) = (.*)$',(folder/'flags.make').read_text(),re.M));link=shlex.split((folder/'link.txt').read_text());flags=[link[0]]
for n in ['CXX_DEFINES','CXX_INCLUDES','CXX_FLAGS']:flags+=shlex.split(f[n])
def run(label,args):
 p=h.run(args,cwd=build,sources=[d],timeout=600);(d/(label+'.stdout')).write_bytes(p.stdout);(d/(label+'.stderr')).write_bytes(p.stderr);print(label,p.returncode,flush=True)
for name in ['original-plain','candidate-plain','original','components','ownership','log-host']:
 run('compile-'+name,[*flags,'-c',d/(name+'.cpp'),'-o',d/(name+'.o')])
for host in ['original','components']:
 for version in ['original','candidate']:
  label=host+'-'+version;args=link.copy();args[args.index('CMakeFiles/void_log_host.dir/tests/log/host.cpp.o')]=str(d/(host+'.o'));args.insert(args.index('libvoid_runtime.a'),str(d/(version+'-plain.o')))
  args=[x.replace('--dependency-file=CMakeFiles/void_log_host.dir/link.d','--dependency-file='+str(d/(label+'.link.d'))) for x in args];args[args.index('-o')+1]=str(d/label);run('link-'+label,args)
for target,host in [('void_text_ownership','ownership'),('void_log_host','log-host')]:
 args=shlex.split((build/'CMakeFiles'/(target+'.dir')/'link.txt').read_text());args[args.index('CMakeFiles/'+target+'.dir/tests/'+('text_encoding/ownership.cpp.o' if host=='ownership' else 'log/host.cpp.o'))]=str(d/(host+'.o'));args.insert(args.index('libvoid_runtime.a'),str(d/'candidate-plain.o'));args=[x.replace('--dependency-file=CMakeFiles/'+target+'.dir/link.d','--dependency-file='+str(d/(host+'.link.d'))) for x in args];args[args.index('-o')+1]=str(d/host);run('link-'+host,args)
identity={'root':str(root),'retained_build':str(build),'head':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'uname':platform.uname()._asdict(),'compiler':subprocess.check_output([link[0],'--version'],text=True),'flags':flags,'env':dict(os.environ),'hashes':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [build/'libvoid_runtime.a',root/'stdlib/log.void',d/'candidate-text_case.cpp',d/'original-text_case.cpp']}}
(d/'identity.json').write_text(json.dumps(identity,indent=2)+'\n');print('READY',d)
