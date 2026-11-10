"""Real normal/O2 JIT and independent AOT, hostile-input and concurrency qualification."""
from pathlib import Path
import hashlib,io,json,os,shutil,struct,subprocess,sys,tempfile,time,zipfile
import support as h
compiler=Path(sys.argv[1]).resolve();child=Path(sys.argv[2]).resolve();small='--small-stack' in sys.argv
profile=next((p for p in ['Release','Debug','strict'] if ('strict' if p=='strict' else p.lower()) in compiler.parent.name.lower()),compiler.parent.name)
images=[]
source=h.ROOT/'tests/host_capabilities/smoke.void';records=[]
def run(command,work,expected=None,status=0,**kw):
 start=time.monotonic();r=h.run(command,work,expected,status,sources=(source,),small=small,**kw)
 records.append({'command':list(map(os.fsdecode,command)),'status':r.returncode,'stdout':r.stdout.decode('utf8',errors='replace'),'stderr':r.stderr.decode('utf8',errors='replace'),'seconds':round(time.monotonic()-start,3)})
 return r
crypto=b'''true
true
true
ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
true true
true true
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
true
false
false
true
true
false
'''
fs=b'''true
true
true
true
true
7
0
true
true
true
true
true
7061796c6f6164
true
7
true
true
true true
'''
args=['args','','two words','"quote"','tail\\','雪😀','#/%','--app','--no-opt',';touch NEVER','$(echo literal)']
arg_output=b''.join(v.encode().hex().encode()+b'\n' for v in args)+b'args\n'
process_args=['','two words','"quote"','tail\\','雪😀','#/%','--app',';touch NEVER']
process_output=b'child\n'+b''.join(v.encode().hex().encode()+b'\n' for v in process_args)+('雪=value'.encode().hex()+'\nenvironment 1 1\nleaked 0\nstatus 23 false 0\nrepeat 23\ntrue true\nrelative true\nmissing true\nenvironment true\n').encode()
with tempfile.TemporaryDirectory(prefix='VOID Phase A 雪 # % ') as folder:
 work=Path(folder);launcher=work/'child 雪 # %';shutil.copy2(child,launcher)
 lib=work/'lib';lib.mkdir()
 for pattern in ('libidn2.so*','libunistring.so*'):
  for path in (compiler.parent.parent.parent/'.toolchains/http/prefix/linux-aws-lc-5.12.0/Release/lib').glob(pattern):shutil.copy2(path,lib/path.name)
 # Prefix paths are selected from the same-build manifest, not ambient loader search.
 for line in (compiler.parent/'void-native.txt').read_text().splitlines():
  path=Path(line)
  if path.is_file() and '.so' in path.name:shutil.copy2(path,lib/path.name)
 env=dict(os.environ,PATH='/VOID-hostile-PATH',VOID_STDLIB_ROOT='/VOID-missing-stdlib',LD_LIBRARY_PATH=str(lib))
 script=work/'no-shebang';script.write_text('touch EXECUTED_SHELL\n');script.chmod(0o700)
 sentinel=work/'outside sentinel';sentinel.write_bytes(b'KEEP');os.chmod(sentinel,0o600)
 packages=[];zip_expected=[]
 payload=b'hello zip'
 good=work/'package.zip'
 with zipfile.ZipFile(good,'w') as z:z.writestr('assets/',b'');z.writestr('assets/hello.txt',payload,compress_type=zipfile.ZIP_DEFLATED)
 def expected(data=payload,name='assets/hello.txt',directory=True):
  return b'opened true\n'+(b'entry assets/ 0 true\n' if directory else b'')+f'entry {name} {len(data)} false\nundrained true\n'.encode()+hashlib.sha256(data).hexdigest().encode()+b'\neof true\ntrue true\nreleased true\n'
 packages.append(good);zip_expected.append(expected())
 wide=work/'zip64-local.zip'
 with zipfile.ZipFile(wide,'w') as z:
  with z.open('wide.bin','w',force_zip64=True) as f:f.write(payload)
 packages.append(wide);zip_expected.append(expected(name='wide.bin',directory=False))
 original=wide.read_bytes();eocd=original.rfind(b'PK\x05\x06');end=bytearray(original[eocd:]);_,_,_,count,size,offset,_=struct.unpack_from('<4H2IH',end,4)
 struct.pack_into('<4H2I',end,4,0,0,65535,65535,0xffffffff,0xffffffff)
 zip64end=struct.pack('<IQ2H2I4Q',0x06064b50,44,45,45,0,0,count,count,size,offset)
 zip64locator=struct.pack('<IIQI',0x07064b50,0,eocd,1)
 complete=work/'zip64-end.zip';complete.write_bytes(original[:eocd]+zip64end+zip64locator+end);packages.append(complete);zip_expected.append(expected(name='wide.bin',directory=False))
 class Streaming(io.BytesIO):
  def seekable(self):return False
  def seek(self,*args):raise io.UnsupportedOperation()
 streaming=Streaming()
 with zipfile.ZipFile(streaming,'w',compression=zipfile.ZIP_DEFLATED) as z:z.writestr('stream.bin',payload)
 descriptors=work/'descriptor.zip';descriptors.write_bytes(streaming.getvalue());packages.append(descriptors);zip_expected.append(expected(name='stream.bin',directory=False))
 for index,(name,kind) in enumerate([('../escape','path'),('/absolute','path'),('C:/drive','path'),('back\\slash','path'),('a/./b','path'),('NUL.txt','path'),('trailing.','path'),('a//b','path'),('a:b','path'),('snow 雪','path'),('a'*5000,'path'),('link','symlink'),('device','device'),('huge','bomb'),('codec','codec'),('extra','extra')]):
  path=work/f'hostile-{index}.zip'
  with zipfile.ZipFile(path,'w') as z:
   item=zipfile.ZipInfo(name)
   if kind in ('symlink','device'):item.create_system=3;item.external_attr=((0o120777 if kind=='symlink' else 0o020600)<<16)
   if kind=='codec':item.compress_type=zipfile.ZIP_BZIP2
   if kind=='extra':item.extra=struct.pack('<HH4s',0x7075,4,b'name')
   z.writestr(item,b'x'*(1048577 if kind=='bomb' else 1))
  packages.append(path);zip_expected.append(b'rejected\n')
 duplicate=work/'duplicate.zip'
 import warnings
 with warnings.catch_warnings():
  warnings.simplefilter('ignore')
  with zipfile.ZipFile(duplicate,'w') as z:z.writestr('same',b'a');z.writestr('same',b'b')
 packages.append(duplicate);zip_expected.append(b'rejected\n')
 for label,names in [('case-alias',['Case','case']),('file-parent',['file','file/child']),('type-alias',['entry','entry/'])]:
  path=work/(label+'.zip')
  with zipfile.ZipFile(path,'w') as z:
   for name in names:z.writestr(name,b'' if name.endswith('/') else b'x')
  packages.append(path);zip_expected.append(b'rejected\n')
 encrypted=work/'encrypted.zip';encrypted_bytes=bytearray(good.read_bytes())
 for signature,offset in [(b'PK\x03\x04',6),(b'PK\x01\x02',8)]:
  start=0
  while True:
   location=encrypted_bytes.find(signature,start)
   if location<0:break
   struct.pack_into('<H',encrypted_bytes,location+offset,struct.unpack_from('<H',encrypted_bytes,location+offset)[0]|1);start=location+4
 encrypted.write_bytes(encrypted_bytes);packages.append(encrypted);zip_expected.append(b'rejected\n')
 for label,mutate in [('truncated',lambda b:b[:-3]),('multi-disk',lambda b:b[:-18]+b'\x01\x00'+b[-16:])]:
  p=work/(label+'.zip');p.write_bytes(mutate(good.read_bytes()));packages.append(p);zip_expected.append(b'rejected\n')
 badcrc=work/'bad-crc.zip'
 with zipfile.ZipFile(badcrc,'w') as z:z.writestr('data.bin',payload)
 data=bytearray(badcrc.read_bytes());start=30+struct.unpack_from('<H',data,26)[0]+struct.unpack_from('<H',data,28)[0];data[start]^=1;badcrc.write_bytes(data)
 packages.append(badcrc);zip_expected.append(b'opened true\nentry data.bin 9 false\nundrained true\ncorrupt\n')
 combined=b''.join(f'case {n}\n'.encode()+out for n,out in enumerate(zip_expected,1))
 for optimize in (False,True):
  flags=[] if optimize else ['--no-opt']
  binary=work/('native-o2' if optimize else 'native-normal')
  run([compiler,'build',source,'-o',binary,*flags],work)
  images.append({'optimization':'O2' if optimize else 'normal','sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'elf_dynamic':subprocess.check_output(['readelf','-d',binary],text=True),'archive_symbols':[line for line in subprocess.check_output(['nm',binary],text=True).splitlines() if 'archive_read_support_' in line or 'archive_write_disk' in line]})
  for execution in ('jit','aot'):
   base=[compiler,'run',source,*flags,'--no-cache','--'] if execution=='jit' else [sys.executable,h.ROOT/'tests/host_capabilities/isolated_exec.py',work,binary]
   settings={} if execution=='jit' else {'env':env}
   run([*base,*args],work,arg_output,**settings)
   run([*base,'crypto'],work,crypto,**settings)
   private=work/f'capability-private-{optimize}-{execution}'
   run([*base,'fs',private],work,fs,**settings)
   assert (private/'release/data 雪 # %').read_bytes()==b'payload'
   assert (private/'control.json').read_bytes()==b'current'
   assert private.stat().st_mode&0o777==0o700
   assert (private/'release/data 雪 # %').stat().st_mode&0o777==0o600
   run([*base,'zip-many',*packages],work,combined,**settings)
   run([*base,'zip-limit',good],work,b'rejected\n',**settings)
   child_cwd=work/f'process-{optimize}-{execution}';child_cwd.mkdir(mode=0o700)
   process_expected=process_output.replace(b'leaked 0\n',b'cwd\n'+str(child_cwd).encode().hex().encode()+b'\nleaked 0\n')
   run([*base,'process',launcher,child_cwd],work,process_expected,**settings)
   run([*base,'signal',launcher],work,b'0 true 15\n',**settings)
   run([*base,'process-reject',script,work],work,b'rejected 17 8\n',**settings)
   linked=work/f'root-link-{optimize}-{execution}';linked.symlink_to(private,target_is_directory=True)
   public=work/f'public-{optimize}-{execution}';public.mkdir(mode=0o755);os.chmod(public,0o755)
   run([*base,'probe',private,public,linked,'relative'],work,b'admitted\ndenied 2\ndenied 14\ndenied 4\n',**settings)
   assert sentinel.read_bytes()==b'KEEP' and not (work/'NEVER').exists() and not (work/'EXECUTED_SHELL').exists() and not (work/'escape').exists()
  # Two independently executing managers contend on a held native lock.
  first=subprocess.Popen([binary,'lock',private],cwd=work,stdout=subprocess.PIPE,stderr=subprocess.PIPE,preexec_fn=h.stack if small else None)
  assert first.stdout.readline()==b'locked\n'
  run([binary,'lock',private],work,b'busy\n')
  out,err=first.communicate(timeout=15);assert first.returncode==0 and out==b'' and err==b'',(out,err)
  first=subprocess.Popen([binary,'lock',private],cwd=work,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
  assert first.stdout.readline()==b'locked\n';first.kill();first.communicate(timeout=15)
  run([binary,'lock',private],work,b'locked\n')
 # Cached Debug code must capture new per-Runtime arguments, never cached values.
 cached_source=work/source.name
 cached_source.write_bytes(source.read_bytes())
 for value in ['first','second 雪']:
  expected=(b'61726773\n'+value.encode().hex().encode()+b'\nargs\n')
  run([compiler,'run',cached_source,'--cache','--','args',value],work,expected)
 # Invalid UTF-8 is a checked args result even before main, not lossy replacement.
 error_source=work/'args-error.void';error_source.write_text('use os\nvar captured = os.args()\nfunc main():\n    if captured is Err(var reason):\n        print(reason.code, reason.operation)\n    else:\n        print("unexpected")\n')
 error_binary=work/'args-error';run([compiler,'build',error_source,'-o',error_binary],work)
 for base in ([compiler,'run',error_source,'--no-cache','--'],[error_binary]):
  run([*base,os.fsdecode(b'\xff')],work,b'4 os.args\n')
  run([*base,*(['x']*4097)],work,b'2 os.args\n')
 # Legacy numeric entry and option terminators keep their original behavior.
 legacy=work/'legacy.void';legacy.write_text('use os\nuse convert\nfunc entry(n: i32) -> i32:\n    return n + (convert.unwrap(os.args()).count as i32)\n')
 run([compiler,'run',legacy,'7','--no-opt','--no-cache'],work,b'7\n')
 run([compiler,'run',legacy,'--no-opt','--no-cache','--','8'],work,b'8\n')
 legacy_binary=work/'legacy';run([compiler,'build',legacy,'-o',legacy_binary],work);run([legacy_binary,'9'],work,b'9\n')
 # A dash-leading source can still be selected after an option terminator.
 dash=work/'-source.void';dash.write_text('func main() -> i32:\n    return 0\n');run([compiler,'run','--','-source.void','--','--app'],work,b'')
result={'schema':1,'platform':sys.platform,'profile':profile,'hard_stack_bytes':1048576 if small else None,'compiler':str(compiler),'compiler_sha256':hashlib.sha256(compiler.read_bytes()).hexdigest(),'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'checked_processes':len(records),'zip_cases_per_execution':len(packages),'aot_checkout_inaccessible':'Linux Landlock','records':records,'aot_images':images}
out=h.output_directory()/f'{compiler.parent.name}{"-stack" if small else ""}.json';out.write_text(json.dumps(result,ensure_ascii=True,indent=2)+'\n')
print(f'{len(records)} processes; {len(packages)} ZIP cases per normal/O2 JIT/AOT; stack_1MiB={small}; receipt={out}')
