"""Reserved sanitizer exits and timeout evidence preserve exact bytes, then dispose synthetic data."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import sys
import tempfile
import harness as h
from crash import run

with tempfile.TemporaryDirectory(prefix='VOID random synthetic preservation ') as directory:
    root=Path(directory);source=root/'main.void'
    raw=b'# exact source\r\nfunc main():\r\n    print("bytes")\r\n'
    source.write_bytes(raw)
    # Automatic native caching adds a directory whose name also matches
    # '*.void'. It must never prevent preservation of actual source bytes.
    (root/'.void/cache/v1').mkdir(parents=True)
    evidence=root/'evidence';os.environ['VOID_CRASH_EVIDENCE']=str(evidence)
    for status in (86,87):
        result=run([sys.executable,'-c',f'import sys;sys.exit({status})'],cwd=root,capture_output=True,source_inputs=(source,root/'.void'),provenance_inputs=(Path(__file__),root/'.void'))
        assert result.returncode==status
    try:
        run([sys.executable,'-c','import time;print("timeout",flush=True);time.sleep(20)'],cwd=root,capture_output=True,timeout=.1,source_inputs=(source,),provenance_inputs=(Path(__file__),))
    except subprocess.TimeoutExpired:
        pass
    else:
        raise AssertionError('synthetic timeout did not expire')
    records=[json.loads(p.read_text()) for p in evidence.glob('*/receipt.json')]
    assert len(records)==3
    for record in records:
        preserved=next(x for x in record['sources'] if x['original']==str(source))
        assert Path(preserved['copy']).read_bytes()==raw
        assert preserved['sha256']==hashlib.sha256(raw).hexdigest()
    assert sum(x['timed_out'] for x in records)==1
print('Random preservation: 3 synthetic reserved-status/timeout incidents verified and disposed')
