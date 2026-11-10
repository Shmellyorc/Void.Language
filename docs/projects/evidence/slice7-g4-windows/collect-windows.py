from hashlib import sha256
from pathlib import Path
import json
import re

audit = Path(__file__).resolve().parent
share = Path.home()/'VM/project-slice7-g4-windows'
native = audit/'native'
result = json.loads((share/'native-results.json').read_text())
assert result['status']=='PASS' and result['build']==19045
assert result['inputs']==json.loads((native/'input-sha256.json').read_text())
assert len(result['probes'])==14 and set(result['probes'].values())=={0}
assert result['artifacts']['child-good/startup.exe']==result['artifacts']['child-bad/startup.exe']==result['artifacts']['startup.exe']
for artifact, owner in (('child-good/void_g4_probe_dep.dll','good/void_g4_probe_dep.dll'),
                        ('child-bad/void_g4_probe_dep.dll','alternate/void_g4_probe_dep.dll')):
    assert result['artifacts'][artifact]==result['artifacts'][owner]
assert result['artifacts']['good/void_g4_probe_dep.dll']!=result['artifacts']['alternate/void_g4_probe_dep.dll']
for name, expected in result['inputs'].items():
    repository = audit/'probes'/name
    if not repository.exists():
        repository = audit.parent/'slice7-g4/checkpoint'/name
    assert sha256(repository.read_bytes()).hexdigest()==expected
    assert repository.read_bytes()==(share/name).read_bytes()
checks = {
    'probe-clean.log': ('bound IAT owner match=1','initializer marker at first post-load observation=-1'),
    'probe-collision.log': ('bound IAT owner match=0','initializer marker at first post-load observation=1'),
    'probe-late.log': ('preflight names and active redirects clear=1','bound IAT owner match=0'),
    'probe-thread.log': ('dependency basename loaded before setup=0','active redirect provider=0 dependency=1',
                         'bound IAT owner match=0','initializer marker at first post-load observation=1'),
    'probe-thread-default.log': ('active redirect provider=0 dependency=0','bound IAT owner match=1'),
    'probe-thread-deny.log': ('alternate and provider absent',),
    'probe-process-default.log': ('active redirect provider=0 dependency=1','initializer marker at first post-load observation=1'),
    'probe-process-deny.log': ('alternate and provider absent',),
    'probe-preload-thread.log': ('preloaded good dependency match=1','bound IAT owner match=0'),
    'probe-data.log': ('initializer=0 registered as executable module=0','ordinary executable load initializer=1'),
    'probe-forward.log': ('alternate absent after loading import-free image',
                          'forwarded export initializer after GetProcAddress=1','forwarded export owner match=0'),
    'probe-explicit.log': ('explicit table used C record, byte span, opaque token and terminal release',),
    'startup-good.log': ('child image matches held executable path before resume=1','child first-entry observation=42'),
    'startup-bad.log': ('child image matches held executable path before resume=1','child first-entry observation=121'),
}
for name, fragments in checks.items():
    output = (share/name).read_text()
    assert all(item in output for item in fragments),name
details = (share/'pe-facts.log').read_text()
files = {name: body for name,body in re.findall(r'^File: ([^\n]+)\n(.*?)(?=^File: |\Z)',details,re.M|re.S)}
for name in ('good/void_g4w_explicit.dll','good/void_g4w_forwarder.dll','good/void_g4_probe_dep.dll'):
    assert all(marker in files[name] for marker in ('ImportTableRVA: 0x0','DelayImportDescriptorRVA: 0x0',
               'TLSTableRVA: 0x0','AddressOfEntryPoint: 0x0'))
assert 'ForwardedTo: void_g4_probe_dep.g4_dependency' in files['good/void_g4w_forwarder.dll']
assert 'ForwardedTo:' not in files['good/void_g4w_explicit.dll']
for name in ('loader-probe.exe','launcher.exe','alternate/process-default.exe','startup.exe'):
    expected = ['KERNEL32.dll'] if name!='startup.exe' else ['KERNEL32.dll','void_g4_probe_dep.dll']
    assert sorted(re.findall(r'^Import \{\n  Name: (.*)$',files[name],re.M))==expected
outputs = ('native-results.json','input-sha256.json','native-console.log','vcvars.log','transfer.log',
           *(step['label']+'.log' for step in result['commands']))
preserved = {}
for name in outputs:
    path = share/name
    data = path.read_bytes()
    target = native/(path.stem+'.txt' if path.suffix=='.log' else name)
    if target.exists():
        assert target.read_bytes()==data,target
    else:
        target.write_bytes(data)
    preserved[name]=sha256(data).hexdigest()
for dest in (share/'preserved-evidence.json',native/'preserved-evidence.json'):
    dest.write_text(json.dumps(preserved,indent=2)+'\n')
print(f'Archived {len(outputs)} native outputs and verified {len(checks)} control transcripts.')
