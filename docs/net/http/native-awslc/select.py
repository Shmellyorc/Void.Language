"""Confirm pinned MultiSSL is global, with no credentials or network traffic."""
import hashlib
import json
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[4]
OUT=ROOT/'.toolchains/native-awslc'
rows=[]
for profile in ('Debug','Release'):
    for backend in ('openssl','schannel'):
        executable=OUT/profile/'multi/probe.exe'
        p=subprocess.run([str(executable),'--select',backend],capture_output=True,text=True,timeout=20)
        parsed=[json.loads(line) for line in p.stdout.splitlines() if line.startswith('{')]
        assert p.returncode==0,(profile,backend,p.stderr)
        assert {r['available_backend'] for r in parsed if 'available_backend' in r}=={'openssl','schannel'}
        assert {'first_selection':0,'global_init':0,'other_selection_after_init':2} in parsed
        rows.append({'profile':profile,'selected':backend,'records':parsed,
            'probe_sha256':hashlib.sha256(executable.read_bytes()).hexdigest(),
            'version':[line for line in p.stdout.splitlines() if not line.startswith('{')]})
(OUT/'selection.json').write_text(json.dumps(rows,indent=2)+'\n')
print('4 native MultiSSL selection controls passed; changing the active backend returns CURLSSLSET_TOO_LATE')
