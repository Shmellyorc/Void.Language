from pathlib import Path
from collections import Counter
import hashlib,json,os,subprocess
env=dict(os.environ,GIT_OPTIONAL_LOCKS='0')
def git(*args):return subprocess.check_output(['git',*args],env=env)
def status(data):return {os.fsdecode(row[3:]):row[:2].decode() for row in data.split(b'\0') if row}
def index(data):
 rows={}
 for row in data.split(b'\0'):
  if row:
   metadata,name=row.split(b'\t',1);mode,blob,stage=metadata.split()
   assert stage==b'0';rows[name]=(mode,blob)
 return rows
def tree(data):
 rows={}
 for row in data.split(b'\0'):
  if row:
   metadata,name=row.split(b'\t',1);mode,kind,blob=metadata.split()
   rows[name]=(mode,blob)
 return rows
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
start=Path('build/slice5-start')
old=status((start/'status').read_bytes())
now=status(git('status','--porcelain=v1','-z','-uall'))
assert len(old)==768 and set(old)<=set(now)
assert git('rev-parse','HEAD').decode().strip()=='2f667ea338127b8f91c3136804377ef3fa5218d6'
live=git('ls-files','--stage','-z')
assert live==(start/'index').read_bytes()
assert index(live)==tree(git('ls-tree','-rz','HEAD'))
assert not git('diff','--cached','--name-status')
prior=(start/'files/docs/projects/evidence/slice4-inventory.txt').read_text().splitlines()
layers={r.split(' | ',2)[2]:r.split(' | ',2)[1] for r in prior if r.startswith((' M |','?? |'))}
assert set(layers)==set(old)
sources=json.loads(Path('build/slice5/source-hashes.json').read_text())
amended={name for name in old if sha(Path(name))!=sha(start/'files'/name)}
assert amended==set(sources)&set(old),amended
for name,hashes in sources.items():assert sha(Path(name))==hashes['after'],name
added=set(now)-set(old)-set(sources)
assert all(name=='docs/projects/SLICE5.md' or name.startswith(('docs/projects/evidence/slice5/','docs/projects/evidence/slice5-','docs/projects/evidence/slice5-local.json')) for name in added),added
counts=Counter(now.values())
stats={'total':len(now),'modified':counts[' M'],'untracked':counts['??'],'index_entries':len(index(live)),
       'starting_paths':len(old),'starting_preserved':len(old)-len(amended),'starting_amended':len(amended),
       's5_source_and_documentation_paths':len(sources),'new_s5_report_evidence_paths':len(added)}
rows=['HEAD 2f667ea338127b8f91c3136804377ef3fa5218d6','Indexed path/mode/blob/stage tuples match the starting index and HEAD. Staged diff empty.',json.dumps(stats,sort_keys=True),'','STATUS | LAYER | EXACT PATH']
for name,flag in sorted(now.items(),key=lambda pair:(pair[1],pair[0])):
 layer=layers.get(name,'S5 source/documentation' if name in sources else 'S5 report/evidence')
 if name in amended:layer='S5 amendment; original layer: '+layer
 rows.append(f'{flag} | {layer} | {name}')
Path('docs/projects/evidence/slice5-inventory.txt').write_text('\n'.join(rows)+'\n')
print(json.dumps(stats,sort_keys=True))
