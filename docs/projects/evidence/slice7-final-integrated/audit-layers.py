"""Reverse/replay accepted Slice 7 patches in an explicitly disposable directory."""
from pathlib import Path
from hashlib import sha256
import json, os, re, shutil, subprocess, sys

root=Path.cwd()
patch_root=root/'docs/projects/evidence'
scratch=Path(sys.argv[1]).resolve(strict=True)
assert scratch.is_dir() and not any(scratch.iterdir()) and not scratch.is_relative_to(root)
env=os.environ | {'GIT_CEILING_DIRECTORIES':str(scratch.parent)}
gates=('slice7-g1','slice7-g2','slice7-g3','slice7-g3-final','slice7-g4-linux',
       'slice7-g4-conventional','slice7-raylib','slice7-g5','slice7-g5-release-refusal')
patches={}
touched=set()

def sha(path):
    return sha256(path.read_bytes()).hexdigest()

for gate in gates:
    patch=patch_root/gate/'source.patch'
    names=[]
    for line in patch.read_text().splitlines():
        if line.startswith('+++ b/'):
            name=line.removeprefix('+++ b/').split('\t',1)[0]
            assert not Path(name).is_absolute() and '..' not in Path(name).parts
            names.append(name)
    assert names
    patches[gate]=(patch,names)
    touched.update(names)
for name in touched:
    out=scratch/name
    out.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(root/name,out)

def apply(patch, reverse):
    argv=['git','--no-optional-locks','apply','--no-index']
    if reverse:
        argv+=['--reverse']
    subprocess.run([*argv,'--check',str(patch)],cwd=scratch,env=env,check=True)
    subprocess.run([*argv,str(patch)],cwd=scratch,env=env,check=True)

for gate in reversed(gates):
    apply(patches[gate][0],True)

prior={}
for line in (patch_root/'slice7-inventory.txt').read_text().splitlines():
    fields=line.split(' | ',3)
    if len(fields)==4 and fields[0] in (' M','??'):
        state,layer,name,digest=fields
        prior[name]=digest

restored={}
untouched=0
for name,digest in prior.items():
    candidate=(scratch if name in touched else root)/name
    # The audit inventory's own final hash has no meaningful self-reference.
    if digest in ('(self)','self','-'):
        continue
    assert sha(candidate)==digest,(name,sha(candidate),digest)
    if name in touched:
        restored[name]=digest
    else:
        untouched+=1

old_head_only={}
new_product=[]
for name in sorted(touched-prior.keys()):
    candidate=scratch/name
    head=subprocess.run(['git','--no-optional-locks','show','HEAD:'+name],cwd=root,capture_output=True)
    if head.returncode==0:
        digest=sha256(head.stdout).hexdigest()
        assert sha(candidate)==digest,name
        old_head_only[name]=digest
    else:
        assert not candidate.exists(),name
        new_product.append(name)

for gate in gates:
    apply(patches[gate][0],False)
for name in touched:
    assert sha(scratch/name)==sha(root/name),name

print(json.dumps(dict(
    starting_inventory_paths=len(prior), byte_identical_untouched=untouched,
    reconstructed_prior_working_tree_paths=restored, reconstructed_head_only_paths=old_head_only,
    new_slice7_product_paths=new_product, roundtrip_unique_paths=len(touched),
    ordered_layers=[dict(gate=gate,sha256=sha(patches[gate][0]),files=patches[gate][1]) for gate in gates],
),indent=2))
