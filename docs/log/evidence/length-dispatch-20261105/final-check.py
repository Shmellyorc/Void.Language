from pathlib import Path
import sys,os,re,shlex,json,shutil
root=Path.cwd();d=Path(Path('/tmp/void-length-dispatch-current').read_text());build=Path('/home/shmellyorc/.local/state/void-log-performance/framing-candidate-hAebdlqN/build')
sys.path.insert(0,str(root/'tests/log'));import support as h
h.EVIDENCE=d/'receipts';os.environ['VOID_CRASH_EVIDENCE']=str(d/'crashes');shutil.copy2(__file__,d/'final-check.py')
def run(label,args,cwd=build):
 p=h.run(args,cwd=cwd,small=True,sources=[d],timeout=600);(d/(label+'.stdout')).write_bytes(p.stdout);(d/(label+'.stderr')).write_bytes(p.stderr);print(label,p.returncode,flush=True);return p
# Record the optimized linked code as well as the inspected TU.
nm=root/'.toolchains/llvm-23.1.4/bin/llvm-nm';objdump=root/'.toolchains/llvm-23.1.4/bin/llvm-objdump'
p=run('linked-symbols',[nm,'--print-size','--demangle',d/'components-candidate'])
for line in p.stdout.decode().splitlines():
 if 'vd::Runtime::translate_scalars(' not in line or not (' T ' in line or 'ShortAsciiNoMatch::prove' in line):continue
 addr,size,*_=line.split();start=int(addr,16);end=start+int(size,16);name='helper' if 'ShortAscii' in line else 'main'
 run('linked-'+name,[objdump,'--disassemble','--demangle','--no-show-raw-insn',f'--start-address={start}',f'--stop-address={end}',d/'components-candidate'])
# Use the same native/JIT byte/error/alias/failure oracles against production.
link=shlex.split((build/'CMakeFiles/void_text_ownership.dir/link.txt').read_text())
for host in ['correctness','native-extra']:
 args=link.copy();args[args.index('CMakeFiles/void_text_ownership.dir/tests/text_encoding/ownership.cpp.o')]=str(d/(host+'.o'));args.insert(args.index('libvoid_runtime.a'),str(d/'original-plain.o'));label='control-'+host;args=[x.replace('--dependency-file=CMakeFiles/void_text_ownership.dir/link.d','--dependency-file='+str(d/(label+'.link.d'))) for x in args];args[args.index('-o')+1]=str(d/label);run('link-'+label,args)
 work=d/'control-oracles';work.mkdir(exist_ok=True)
 run('check-'+label,[d/label,d/'translation_no_match.void',root/'tests/text_encoding/text.void',work])
print('FINAL CHECKS COMPLETE')
