from pathlib import Path
import hashlib,json,os,shutil,subprocess,tempfile
env=dict(os.environ,GIT_OPTIONAL_LOCKS='0')
def git(*args):return subprocess.check_output(['git',*args],env=env)
def status(raw):return {os.fsdecode(line[3:]):line[:2].decode() for line in raw.split(b'\0') if line}
def sha(data):return hashlib.sha256(data).hexdigest()
start=Path('build/slice5-start')
old=status((start/'status').read_bytes())
new=status(git('status','--porcelain=v1','-z','-uall'))
assert set(old)<=set(new)
changed={path for path in old if Path(path).read_bytes()!=(start/'files'/path).read_bytes()}
source=sorted(changed|{path for path in set(new)-set(old) if not path.startswith(('docs/projects/','docs/log/evidence/','docs/os/evidence/'))})
output=Path('docs/projects/evidence/slice5-source.patch').resolve()
records={}
with tempfile.TemporaryDirectory(prefix='VOID S5 source reconstruction ') as temp:
 root=Path(temp);(root/'before').mkdir();(root/'after').mkdir()
 for n in source:
  after=Path(n).read_bytes();before=None
  if n in old:before=(start/'files'/n).read_bytes()
  elif new[n]==' M':before=git('show','HEAD:'+n)
  for side,data in [('before',before),('after',after)]:
   if data is None:continue
   dst=root/side/n;dst.parent.mkdir(parents=True,exist_ok=True);dst.write_bytes(data)
  records[n]={'before':sha(before) if before is not None else None,'after':sha(after)}
 result=subprocess.run(['git','diff','--no-index','--binary','--','before','after'],cwd=root,capture_output=True)
 assert result.returncode==1,result.stderr
 patch=result.stdout
 for prefix in (b'a/',b'b/'):
  for sub in (b'before/',b'after/'):patch=patch.replace(prefix+sub,prefix)
 output.write_bytes(patch)
 subprocess.run(['git','apply','--check',str(output)],cwd=root/'before',check=True)
 subprocess.run(['git','apply',str(output)],cwd=root/'before',check=True)
 for n in source:assert (root/'before'/n).read_bytes()==Path(n).read_bytes(),n
 subprocess.run(['git','apply','--check','--reverse',str(output)],check=True)
Path('build/slice5/source-hashes.json').write_text(json.dumps(records,indent=2)+'\n')
print(len(source),'source/documentation paths,',len(changed),'amended prior paths')
print('\n'.join(source))
