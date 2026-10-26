"""Synthetic signal-only check of exact, non-overwriting crash preservation."""
from pathlib import Path
import hashlib
import json
import os
import sys
import tempfile
import subprocess
from crash import ROOT, run, install

install()

with tempfile.TemporaryDirectory(prefix='VOID crash preservation selftest ') as directory:
    root = Path(directory)
    original = root / 'arbitrary_name.void'
    original.write_bytes(b'# synthetic preservation input\r\nfunc main():\r\n    print("x")\r\n')
    expected = hashlib.sha256(original.read_bytes()).hexdigest()
    evidence = ROOT / 'docs/text-encoding/evidence/synthetic-preservation'
    evidence.mkdir(parents=True, exist_ok=True)
    kept = Path(tempfile.mkdtemp(prefix='selftest-', dir=evidence))
    os.environ['VOID_CRASH_EVIDENCE'] = str(kept)
    script = 'import os,signal; os.kill(os.getpid(), signal.SIGSEGV)'
    if os.name == 'nt':
        script = 'import os; os.abort()'
    else:
        script = 'import resource; resource.setrlimit(resource.RLIMIT_CORE,(0,0)); ' + script
    for repeat in range(2):
        result = run([sys.executable, '-c', script, original], cwd=root, capture_output=True)
        assert result.returncode != 0
    edited = 'func main():\n    print("雪\\0")\n'
    message = json.dumps({'jsonrpc': '2.0', 'method': 'textDocument/didOpen',
                          'params': {'textDocument': {'uri': original.as_uri(),
                                                     'version': 2, 'text': edited}}}).encode()
    framed = b'Content-Length: ' + str(len(message)).encode() + b'\r\n\r\n' + message
    direct = subprocess.Popen([sys.executable, '-c', 'import sys; sys.stdin.buffer.read(); ' + script,
                               str(original)], cwd=root, stdin=subprocess.PIPE,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    direct.communicate(framed)
    receipts = list(kept.glob('*/receipt.json'))
    assert len(receipts) == 3
    for receipt in receipts:
        record = json.loads(receipt.read_text())
        entry = next(item for item in record['sources'] if item['original'] == str(original))
        assert entry['sha256'] == expected
        copy = Path(entry['copy'])
        assert copy.name == 'arbitrary_name_crash.void'
        assert copy.read_bytes() == original.read_bytes()
        assert record['executable_sha256']
        if record.get('editor_snapshots'):
            snapshot = record['editor_snapshots'][0]
            assert Path(snapshot['copy']).read_bytes() == edited.encode()
            assert snapshot['sha256'] == hashlib.sha256(edited.encode()).hexdigest()
    assert sum(bool(json.loads(p.read_text()).get('editor_snapshots')) for p in receipts) == 1
print('Three synthetic signal incidents retained once each, including exact editor snapshot; no real source crash claimed')
