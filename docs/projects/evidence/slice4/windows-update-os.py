from pathlib import Path
import hashlib,json,traceback
out=Path(r'\\vboxsvr\VM\project-slice4')
root=Path.home()/'VoidCI42377496702-Native'
result={'status':'running','path':'stdlib/os.void','change':'Signature help documentation for omitted app_id'}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
try:
    data=json.loads((out/'native-results.json').read_text())
    target=root/result['path']
    before=data['source_inputs_sha256'][result['path']]
    assert sha(target)==before and result['path'] in data['before_overlay_sha256']
    result['stages']={profile:json.loads((out/(profile+'-result.json')).read_text())['stage'] for profile in ('Release','Debug')}
    assert all(stage=='build' for stage in result['stages'].values()),result
    incoming=out/'final-os.void'
    after=(out/'final-os.sha256').read_text().strip()
    assert sha(incoming)==after
    target.write_bytes(incoming.read_bytes())
    assert sha(target)==after
    result.update(status='PASS',before=before,after=after)
except BaseException:result.update(status='FAIL',error=traceback.format_exc())
(out/'os-doc-update.json').write_text(json.dumps(result,indent=2)+'\n')
