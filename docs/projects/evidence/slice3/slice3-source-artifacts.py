from pathlib import Path
import json,subprocess,hashlib,difflib,tempfile
r=Path.cwd();e=r/'docs/projects/evidence';checkpoint=json.loads((r/'build/slice3-start/checkpoint.json').read_text());paths=sorted(v[3:] for v in subprocess.check_output(['git','status','--porcelain=v1','-uall','-z']).decode().split('\0') if v and not v[3:].startswith(('docs/projects/','docs/log/evidence/lsp-llvm23-')))
def head(n):
 p=subprocess.run(['git','show','HEAD:'+n],capture_output=True);return p.stdout if p.returncode==0 else None
def diff(n,old,new):
 if old==new:return ''
 return 'diff --git a/'+n+' b/'+n+'\n'+('new file mode 100644\n' if old is None else '')+''.join(difflib.unified_diff([] if old is None else old.decode().splitlines(True),new.decode().splitlines(True),fromfile='/dev/null' if old is None else 'a/'+n,tofile='b/'+n))
records=[]
for kind in ['slice3','baseline']:
 before={n:(r/'build/slice3-start'/n).read_bytes() if kind=='slice3' and n in checkpoint['files_sha256'] else head(n) for n in paths};target=e/('slice3-source.patch' if kind=='slice3' else 'slice3-baseline-source.patch');target.write_text(''.join(diff(n,before[n],(r/n).read_bytes()) for n in paths))
 subprocess.run(['git','apply','--reverse','--check',target],check=True,capture_output=True)
 with tempfile.TemporaryDirectory(prefix='void-s3-patch-') as temp:
  d=Path(temp)
  for n,data in before.items():
   if data is not None:(d/n).parent.mkdir(parents=True,exist_ok=True);(d/n).write_bytes(data)
  subprocess.run(['git','apply','--check',target],cwd=d,check=True,capture_output=True);subprocess.run(['git','apply',target],cwd=d,check=True,capture_output=True)
  for n in paths:assert (d/n).read_bytes()==(r/n).read_bytes(),n
 records.append({'kind':kind,'path':str(target.relative_to(r)),'bytes':target.stat().st_size,'sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'changed_paths':[n for n in paths if before[n]!=(r/n).read_bytes()],'source_scope':paths,'reverse_check':'PASS','forward_reconstruction':'PASS'})
(r/'build/slice3-patches.json').write_text(json.dumps(records,indent=2)+'\n');print([(v['kind'],len(v['changed_paths']),v['bytes']) for v in records])
