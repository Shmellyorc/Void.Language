"""Reject mismatched host-provider receipts without touching the real prefix."""
from pathlib import Path
import json,shutil,sys,tempfile
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from verify_host_deps import verify_prefix, build_identity
prefix=Path(sys.argv[1]);profile,system,architecture=sys.argv[2:5]
verify_prefix(prefix,profile,system,architecture)
checks=1
def rejected(path,selected_profile=profile,selected_system=system,selected_arch=architecture):
 global checks
 try:verify_prefix(path,selected_profile,selected_system,selected_arch)
 except (ValueError,OSError,KeyError):checks+=1;return
 raise AssertionError('mismatched provider receipt accepted')
rejected(prefix,selected_profile='wrong')
rejected(prefix,selected_system='wrong')
rejected(prefix,selected_arch='wrong')
with tempfile.TemporaryDirectory(prefix='VOID provider receipt ') as folder:
 copy=Path(folder);receipt=json.loads((prefix/'VOID-HOST-DEPENDENCIES.json').read_text())
 # Use symlinks nowhere: an unverified archive escape is itself a rejection.
 for name in receipt['files']:
  destination=copy/name;destination.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(prefix/name,destination)
 metadata=copy/'VOID-HOST-DEPENDENCIES.json'
 def put(record):metadata.write_text(json.dumps(record))
 original=json.dumps(receipt)
 receipt['pins']['libsodium']['sha256']='0'*64;put(receipt);rejected(copy)
 receipt=json.loads(original);name=next(iter(receipt['files']));receipt['files'][name]='0'*64;put(receipt);rejected(copy)
 receipt=json.loads(original);receipt['files']['../outside']='0'*64;put(receipt);rejected(copy)
 receipt=json.loads(original);del receipt['files']['lib/libsodium.lib' if system=='Windows' else 'lib/libsodium.a'];put(receipt);rejected(copy)
 receipt=json.loads(original);receipt['recipe_sha256']={'../outside':'0'*64};put(receipt);rejected(copy)
print(f'Pinned host provider receipt: {checks} admission/rejection checks passed')

# Exercise self-consistent forged receipts too: policy validation must reject them
# even when the receipt's own build-identity checksum is recomputed.
with tempfile.TemporaryDirectory(prefix='VOID patched receipt negatives ') as folder:
 copy=Path(folder);original=json.loads((prefix/'VOID-HOST-DEPENDENCIES.json').read_text())
 for name in original['files']:
  destination=copy/name;destination.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(prefix/name,destination)
 for change in ['old-schema','missing-source','wrong-patch','wrong-postimage','wrong-recipe','missing-recipe','wrong-sdk','wrong-build','missing-tests']:
  receipt=json.loads(json.dumps(original))
  if change=='old-schema':receipt['schema']=2
  elif change=='missing-source':del receipt['source_provenance']
  elif change=='wrong-patch':receipt['source_provenance']['patch']['sha256']='0'*64
  elif change=='wrong-postimage':receipt['source_provenance']['postimages'][next(iter(receipt['source_provenance']['postimages']))]='0'*64
  elif change=='wrong-recipe':receipt['recipe_sha256'][next(iter(receipt['recipe_sha256']))]='0'*64
  elif change=='missing-recipe':receipt['recipe_sha256'].pop(next(iter(receipt['recipe_sha256'])))
  elif change=='wrong-sdk':receipt['toolchain']['sdk']['version']='invalid'
  elif change=='missing-tests':receipt['upstream_tests']={}
  receipt['build_identity']=build_identity(receipt) if change!='wrong-build' else '0'*64
  (copy/'VOID-HOST-DEPENDENCIES.json').write_text(json.dumps(receipt))
  if change=='missing-tests' and system=='Windows':continue
  rejected(copy)
print(f'Approved backport receipt: {checks} admission/rejection checks passed')
