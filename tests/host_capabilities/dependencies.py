"""Reject mismatched host-provider receipts without touching the real prefix."""
from pathlib import Path
import json,shutil,sys,tempfile
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from verify_host_deps import verify_prefix
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
