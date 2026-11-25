"""Controlled native Stage B specimens only; never a publisher or install path."""
from pathlib import Path
import ast,hashlib,json,os,platform,re,shutil,subprocess,sys
b,sdk,out=map(lambda p:Path(p).resolve(),sys.argv[1:]);out.mkdir(parents=True)
windows=sys.platform=='win32';assert windows or (sys.platform=='darwin' and platform.machine()=='arm64')
ext='.exe' if windows else '';tools=sdk/'bin';emitter=b/('void_private_diagnostics'+ext);auditor=b/('void_publish_foundation'+ext)
native=(b/'void-native.txt').read_text().splitlines();assert native[0]=='VOID native 23.1.4 v2'
linker=['--ld-path='+native[2]] if native[2] else []
linker+=['-fuse-ld=lld','-Wl,/debug:dwarf'] if windows else []
source_root=out/'PRIVATE_PATH_B_7de94';source_root.mkdir();source=source_root/'fixture.void'
canaries=['PRIVATE_PATH_B_7de94','private_function_b_9ac53','source_snippet_b_1dfee']
sync='''func private_function_b_9ac53(value: i32) -> i32:
    if value > 0:
        return private_function_b_9ac53(value - 1)
    var source_snippet_b_1dfee = value - value
    return value / source_snippet_b_1dfee

func entry(value: i32) -> i32:
    defer {
        print("cleanup executed")
    }
    return private_function_b_9ac53(value)
'''
async_source='''async func tick() -> i32:
    return 1

'''+sync.split('func entry')[0]+'''async func worker(value: i32) -> i32:
    var ignored = await tick()
    return private_function_b_9ac53(value)

async func entry(value: i32) -> i32:
    defer {
        print("cleanup executed")
    }
    return await worker(value)
'''
rows=[];commands=[]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(args,expected=0):
 p=subprocess.run(list(map(str,args)),capture_output=True,text=True,timeout=90);i=len(commands)
 commands.append(dict(argv=list(map(str,args)),exit=p.returncode));(out/'commands.json').write_text(json.dumps(commands,indent=2));(out/(str(i)+'.stdout')).write_text(p.stdout);(out/(str(i)+'.stderr')).write_text(p.stderr)
 assert p.returncode==expected,(args,p.returncode,p.stdout,p.stderr);return p.stdout
def audit(image,revision='audit-stage-b'):
 result=json.loads(run([auditor,revision,'windows-x64' if windows else 'macos-arm64',image,*canaries]));assert result['inspected'],result;return result
def oracle(image):
 args=[tools/('llvm-readobj'+ext),'--coff-imports','--coff-exports','--unwind'] if windows else [tools/'llvm-objdump','--macho','--exports-trie','--bind','--lazy-bind','--unwind-info']
 value=run([*args,image])
 # llvm-readobj annotates unwind VAs with nlist names before stripping. Compare
 # every address/decoded field after removing only those name decorations.
 return '\n'.join(re.sub(r'(?<=: )[^\n()]+ (?=\(0x[0-9a-fA-F]+\)$)','',line) for line in value.splitlines() if str(image) not in line)
def image_base(image):
 import struct
 data=image.read_bytes();pe=struct.unpack_from('<I',data,60)[0];return struct.unpack_from('<Q',data,pe+24+24)[0]
for kind,text in [('sync',sync),('async',async_source),('retained',sync.replace('    var source_snippet','    defer {\n        print("retained cleanup")\n    }\n    var source_snippet',1))]:
 source.write_text(text)
 for mode in ['O0','O2']:
  case=out/(kind+'-'+mode);case.mkdir();private=case/'private';private.mkdir();obj=private/'fixture.o';linked=private/('linked'+ext);final=case/('final'+ext);mapfile=private/'fixture.vdiag';symbols=private/('symbols.debug' if windows else 'fixture.dSYM')
  digest=hashlib.sha256((text+kind+mode).encode()+bytes.fromhex(sha(emitter))).hexdigest()
  p=subprocess.Popen(list(map(str,[emitter,'emit',source,obj,mapfile,final,mode,kind,digest])),stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
  try:
   assert p.stdout.readline()=='READY\n',p.stderr.read()
   run([tools/('clang++'+ext),*linker,obj,'-o',linked,*native[3:]])
   if windows:
    run([tools/'llvm-objcopy.exe','--only-keep-debug',linked,symbols]);run([tools/'llvm-objcopy.exe','--strip-all',linked,final])
   else:
    # Apple's -S -x inserts a local N_OPT radr://5614542 marker. Pinned
    # llvm-strip removes it while retaining symbols referenced by dyld.
    run([tools/'dsymutil',linked,'-o',symbols]);run([tools/'llvm-strip','--strip-all','-o',final,linked])
    uuids=[run([tools/'llvm-dwarfdump','--uuid',p]) for p in [linked,final,symbols]]
    assert len({re.search(r'UUID: (\S+)',u)[1] for u in uuids})==1,uuids
   p.stdin.write('FINAL\n');p.stdin.flush();stdout,stderr=p.communicate(timeout=60);assert p.returncode==0,(stdout,stderr)
  finally:
   if p.poll() is None:p.kill();p.communicate()
  report=audit(final);assert not report['static_symbols'],report
  assert report['debug']==windows,report
  assert all(f['code'] in ({'forbidden-string','debug-metadata'} if windows else {'forbidden-string'}) for f in report['findings']),report
  if windows:assert any(f['code']=='debug-metadata' for f in report['findings']),report
  assert any(f['code']=='forbidden-string' for f in report['findings'])==(kind=='retained'),report
  assert oracle(linked)==oracle(final),'Required dynamic symbols/unwind changed'
  assert b'VOID-PRIVATE-DIAGNOSTICS' not in final.read_bytes()
  names=run([tools/('llvm-nm'+ext),'--defined-only','--format=posix',linked]).splitlines();encoded=('/fixture.void::'+canaries[1]).encode().hex();name=next(n.split() for n in names if '__vd_'+encoded+'__void' in n)
  address=int(name[2],16)
  symbol_obj=symbols if windows else symbols/'Contents/Resources/DWARF'/linked.name
  # COFF nm emits VAs; symbolizer's relative-address mode uses the image base.
  relative=address-image_base(linked) if windows else address-0x100000000
  symbolicated=run([tools/('llvm-symbolizer'+ext),'--obj='+str(symbol_obj),*(['--relative-address'] if windows else []),hex(relative if windows else address)])
  assert canaries[0] in symbolicated,symbolicated
  if not windows and kind=='sync' and mode=='O0':
   # Capture a real ASLR-loaded PC at the controlled private function, then
   # recover its file address from the loaded image base and match its UUID.
   script="import json; t=lldb.debugger.GetSelectedTarget(); f=t.GetProcess().GetSelectedThread().GetFrameAtIndex(0); m=f.GetModule(); h=m.GetObjectFileHeaderAddress(); print('ASLR_JSON '+json.dumps(dict(pc=f.GetPC(),file_pc=f.GetPCAddress().GetFileAddress(),load_base=h.GetLoadAddress(t),file_base=h.GetFileAddress(),uuid=m.GetUUIDString())))"
   stopped=run(['xcrun','lldb','--batch','-o','breakpoint set -n '+name[0][1:],'-o','run 7','-o','script '+script,'--',final])
   loaded=json.loads(next(line.split('ASLR_JSON ',1)[1] for line in stopped.splitlines() if line.startswith('ASLR_JSON ')))
   assert loaded['pc']-loaded['load_base']+loaded['file_base']==loaded['file_pc']
   assert loaded['uuid'].lower()==re.search(r'UUID: (\S+)',uuids[1])[1].lower()
   restored=run([tools/'llvm-symbolizer','--obj='+str(symbol_obj),hex(loaded['file_pc'])]);assert canaries[0] in restored
   (case/'aslr.json').write_text(json.dumps(loaded,indent=2))

  error=run([final,'7'],expected=1);deep=subprocess.run([str(final),'40'],capture_output=True,text=True,timeout=30);assert deep.returncode==1
  failure=subprocess.run([str(final),'7'],capture_output=True,text=True,timeout=30);assert failure.returncode==1
  expected=('retained cleanup\n' if kind=='retained' else '')+'cleanup executed\n';assert failure.stdout==expected and failure.stderr.count('VOID-D1 ')>1
  assert deep.stderr.count('VOID-D1 ')==33 and 'Source call trace limited to 32 callers' in deep.stderr
  assert all(c not in failure.stderr for c in canaries)
  tokens=private/'tokens.txt';tokens.write_bytes(failure.stderr.encode('utf-8'))
  resolved=run([emitter,'resolve',mapfile,final,tokens]);assert all(c in resolved for c in canaries),resolved
  wrong=private/'wrong';wrong.write_bytes(final.read_bytes()+b'changed');empty=private/'empty';empty.write_bytes(b'')
  for m,i,message in [(mapfile,wrong,'Mismatched private diagnostic image'),(empty,final,'Missing private diagnostic map')]:
   q=subprocess.run(list(map(str,[emitter,'resolve',m,i,tokens])),capture_output=True,text=True,timeout=30);assert q.returncode==1 and message in q.stderr
  devobj=private/'dev.o';dev=private/('development'+ext);devfinal=private/('development-stripped'+ext)
  run([emitter,'development',source,devobj,mode,kind]);run([tools/('clang++'+ext),*linker,devobj,'-o',dev,*native[3:]])
  control=subprocess.run([str(dev),'7'],capture_output=True,text=True,timeout=30);assert control.returncode==1 and control.stdout==expected and all(c in control.stderr for c in canaries)
  run([tools/'llvm-objcopy.exe','--strip-all',dev,devfinal] if windows else [tools/'llvm-strip','--strip-all','-o',devfinal,dev]);assert any(f['code']=='forbidden-string' for f in audit(devfinal)['findings'])
  contaminated=private/('contaminated'+ext);contaminated.write_bytes(final.read_bytes()+mapfile.read_bytes());assert any(f['code']=='private-diagnostic-map' for f in audit(contaminated)['findings'])
  rows.append(dict(case=kind+'-'+mode,final_sha256=sha(final),linked_sha256=sha(linked),symbols_sha256=sha(symbol_obj),map_sha256=sha(mapfile),build_id=stdout.split()[1],report=report,relative_pc=hex(relative),symbolication=symbolicated,oracle_equal=True,exact_map=True,development_control=True,depth_tokens=33,metadata_reduction_qualified=not windows,retained_debug_directory_rejected=windows))
  (out/'results.json').write_text(json.dumps(rows,indent=2));print(kind,mode,'PASS',flush=True)
assert len(set(r['build_id'] for r in rows))==6
print('PASS six native Stage B specimens')
