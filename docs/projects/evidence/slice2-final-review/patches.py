from pathlib import Path
import subprocess,json,hashlib,difflib,tempfile
r=Path.cwd();out=r/'docs/projects/evidence';start=r/'build/slice2-final-review-start';checkpoint=json.loads((start/'checkpoint.json').read_text())
paths=json.loads((out/'slice2-local.json').read_text())['patches'][0]['source_scope']
def head(n):
 p=subprocess.run(['git','show','HEAD:'+n],capture_output=True);return p.stdout if p.returncode==0 else None
def piece(n,old,new):
 if old==new:return ''
 return 'diff --git a/'+n+' b/'+n+'\n'+('new file mode 100644\n' if old is None else '')+''.join(difflib.unified_diff([] if old is None else old.decode().splitlines(True),new.decode().splitlines(True),fromfile='/dev/null' if old is None else 'a/'+n,tofile='b/'+n))
records=[]
for kind,filename in [('review','slice2-final-review-source.patch'),('baseline','slice2-final-baseline-source.patch')]:
 before={n:((start/n).read_bytes() if n in checkpoint['files_sha256'] else head(n)) if kind=='review' else head(n) for n in paths}
 target=out/filename;target.write_text(''.join(piece(n,before[n],(r/n).read_bytes()) for n in paths))
 subprocess.run(['git','apply','--reverse','--check',target],check=True,capture_output=True)
 with tempfile.TemporaryDirectory(prefix='void-s2-final-patch-') as temp:
  d=Path(temp)
  for n,data in before.items():
   if data is not None:(d/n).parent.mkdir(parents=True,exist_ok=True);(d/n).write_bytes(data)
  subprocess.run(['git','apply','--check',target],cwd=d,check=True,capture_output=True);subprocess.run(['git','apply',target],cwd=d,check=True,capture_output=True)
  for n in paths:assert (d/n).read_bytes()==(r/n).read_bytes(),n
 records.append({'kind':kind,'path':str(target.relative_to(r)),'bytes':target.stat().st_size,'sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'changed_paths':[n for n in paths if before[n]!=(r/n).read_bytes()],'reverse_check':'PASS','forward_reconstruction':'PASS'})
(r/'build/slice2-final-review-patches.json').write_text(json.dumps(records,indent=2)+'\n');print([(a['kind'],len(a['changed_paths']),a['bytes']) for a in records])
