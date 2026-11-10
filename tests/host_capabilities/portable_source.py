"""Portable source APIs in normal/O2 JIT and independently linked native AOT.

This supplements the Linux Landlock/fault suite. Native AOT is independently
linked here; this test alone makes no claim that the checkout was inaccessible.
"""
from pathlib import Path
import hashlib,json,os,sys,tempfile,zipfile
import support as h
from staged_child import stage_child
compiler=Path(sys.argv[1]).resolve();child=Path(sys.argv[2]).resolve()
source=h.ROOT/'tests/host_capabilities/smoke.void';records=[]
def run(command,work,expected=None):
 # VOID console output preserves UTF-8 bytes, including LF, on Windows too.
 # native_text is only an oracle for CRT text streams.
 result=h.run(command,work,expected.encode('utf-8') if expected is not None else None,small=os.name!='nt',sources=(source,))
 records.append({'command':list(map(str,command)),'status':result.returncode,'stdout':result.stdout.decode(errors='replace'),'stderr':result.stderr.decode(errors='replace')})
 return result
crypto='true\ntrue\ntrue\nba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\ntrue true\ntrue true\ne3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855\ntrue\nfalse\nfalse\ntrue\ntrue\nfalse\n'
with tempfile.TemporaryDirectory(prefix='VOID portable source 雪 # % ') as folder:
 work=Path(folder).resolve();good=work/'package.zip';bad=work/'hostile.zip'
 staging=[];child=stage_child(child,work,staging)
 with zipfile.ZipFile(good,'w',compression=zipfile.ZIP_DEFLATED) as bundle:bundle.writestr('data',b'abc')
 with zipfile.ZipFile(bad,'w') as bundle:bundle.writestr('../escape',b'bad')
 process_source=h.ROOT/'tests/host_capabilities/portable_process.void'
 filesystem_source=h.ROOT/'tests/host_capabilities/portable_filesystem.void'
 for optimize in (False,True):
  flags=[] if optimize else ['--no-opt'];binary=work/('application-'+str(optimize)+('.exe' if os.name=='nt' else ''))
  process_binary=work/('process-'+str(optimize)+('.exe' if os.name=='nt' else ''))
  filesystem_binary=work/('filesystem-'+str(optimize)+('.exe' if os.name=='nt' else ''))
  run([compiler,'build',filesystem_source,'-o',filesystem_binary,*flags],work)
  run([compiler,'build',source,'-o',binary,*flags],work)
  run([compiler,'build',process_source,'-o',process_binary,*flags],work)
  for base in ([compiler,'run',source,*flags,'--no-cache','--'],[binary]):
   args=['args','','two words','"quote"','tail\\','雪😀','--app']
   expected=''.join(value.encode().hex()+'\n' for value in args)+'args\n'
   run([*base,*args],work,expected);run([*base,'crypto'],work,crypto)
   run([*base,'zip',good],work,'opened true\nentry data 3 false\nundrained true\n'+hashlib.sha256(b'abc').hexdigest()+'\neof true\ntrue true\nreleased true\n')
   run([*base,'zip',bad],work,'rejected\n')
  for base in ([compiler,'run',process_source,*flags,'--no-cache','--'],[process_binary]):run([*base,child,work],work,'23 false\ntrue\n')
  for index,base in enumerate(([compiler,'run',filesystem_source,*flags,'--no-cache','--'],[filesystem_binary])):
   run([*base,work/('private-'+str(optimize)+'-'+str(index))],work,'true\ntrue\n7\n0\ntrue\ntrue\ntrue\n7\n7\ntrue\ntrue\ntrue\n')
 record={'platform':sys.platform,'compiler_sha256':hashlib.sha256(compiler.read_bytes()).hexdigest(),'hard_main_stack_bytes':1048576 if os.name!='nt' else None,'independently_linked_aot':True,'checkout_denied':False,'records':records}
 record['staging']=staging
 destination=h.output_directory();destination.mkdir(parents=True,exist_ok=True)
 (destination/('portable-source-'+compiler.parent.name+'.json')).write_text(json.dumps(record,indent=2)+'\n')
print(f'{len(records)} portable source invocations; normal/O2 JIT and independent AOT passed')
