"""Count raw CTest cases and apply only explicit, recorded retry results."""
from pathlib import Path
from hashlib import file_digest
from collections import Counter
import json, re, xml.etree.ElementTree as ET

root=Path.cwd()
evidence=root/'docs/projects/evidence/slice7-final-integrated'
work=root/'build/slice7-final-integrated'

def digest(path):
    with path.open('rb') as stream:
        return file_digest(stream,'sha256').hexdigest()

def raw(path):
    suite=ET.parse(path).getroot()
    output={}
    for case in suite.findall('testcase'):
        name=case.get('name')
        assert name and name not in output
        status=('fail' if case.find('failure') is not None else
                'skip' if case.find('skipped') is not None or case.get('status')=='notrun' else 'pass')
        output[name]=status
    assert int(suite.get('tests',len(output)))==len(output)
    return output

def count(data):
    totals=Counter(data.values())
    return {kind:totals[kind] for kind in ('pass','fail','skip')}

profiles={}
for system, modes in [('linux',('release','debug','strict')),('windows',('Release','Debug'))]:
    for mode in modes:
        folder=evidence/system
        initial=folder/(f'{mode}.xml' if system=='linux' else f'{mode}-regression.xml')
        results=raw(initial)
        retry_paths=sorted(folder.glob(f'{mode}-retry*.xml'))
        resolved=dict(results)
        retries=[]
        for path in retry_paths:
            attempt=raw(path)
            assert set(attempt)<={name for name,status in resolved.items() if status!='pass'},path
            resolved.update(attempt)
            retries.append(dict(path=str(path.relative_to(evidence)),totals=count(attempt),cases=attempt))
        public=folder/f'{mode}-public.txt'
        text=public.read_text(encoding='utf-8',errors='replace')
        pkg_ok='Integrated installed workflow passed' in text
        tests=dict(selected=len(results),first=count(results),
                   failures=sorted(name for name,status in results.items() if status=='fail'),
                   skipped=sorted(name for name,status in results.items() if status=='skip'),
                   retries=retries,final=count(resolved),
                   unresolved=sorted(name for name,status in resolved.items() if status!='pass'),
                   gates={name:status for name,status in resolved.items() if re.match(r'^ffi_g[1-5]_|^runtime_callbacks$',name)})
        entry=dict(ctest=tests,installed_workflow=pkg_ok,initial_receipt_sha256=digest(initial),
                   public_log_sha256=digest(public),raylib_trace_proofs=text.splitlines().count('RAYLIB trace invoked VOID'))
        if system=='linux':
            build=work/f'linux-{mode}'
            entry['direct_link_controls']=count(raw(folder/f'{mode}-direct-link-control.xml'))
            entry['compiler_sha256']=digest(build/'voidc')
            entry['language_server_sha256']=digest(build/'voidls')
            entry['isolated_package']= (folder/f'{mode}-isolated-package.txt').read_text().splitlines().count('RAYLIB trace invoked VOID')==2
        else:
            record=json.loads((folder/f'{mode}-selection.json').read_text())
            assert set(record['selected'])==set(results)
            entry['configured_tests']=len(record['selected'])+len(record['outside_scope'])
            entry['compiler_sha256']=json.loads((folder/'dependency-inputs.json').read_text())['configurations'][mode]['compiler']['sha256']
        profiles[f'{system}-{mode}']=entry
(evidence/'result-summary.json').write_text(json.dumps(profiles,indent=2)+'\n')
for label,entry in profiles.items():
    print(label,entry['ctest']['first'],entry['ctest']['final'],entry['installed_workflow'],entry['ctest']['unresolved'])
