"""Verify Math's inherited crash boundary using disposable synthetic incidents."""
import argparse
from pathlib import Path
import hashlib
import json
import os
import subprocess
import sys
import tempfile
import bootstrap as h

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('compiler',type=Path)
arguments=parser.parse_args()
compiler=arguments.compiler.resolve()
assert compiler.is_file() and h.IDENTITY is not None, 'compiler identity was not captured'
identity=json.loads(h.IDENTITY.read_text())
assert identity['compiler']==str(compiler)
assert identity['compiler_sha256']==hashlib.sha256(compiler.read_bytes()).hexdigest()

stdout=b'original Math stdout\r\n\x00\xff'
stderr=b'original Math stderr\r\n\x00\xfe'
emit=f'import os,time;os.write(1,{stdout!r});os.write(2,{stderr!r});'
previous=os.environ.get('VOID_CRASH_EVIDENCE')
with tempfile.TemporaryDirectory(prefix='VOID math synthetic preservation ') as directory:
    root=Path(directory)
    source=root/'main.void'
    original=b'use "./relocated/nested/math.void"\r\nfunc main():\r\n    pass\r\n'
    source.write_bytes(original)
    imported=root/'relocated'/'nested'/'math.void'
    imported.parent.mkdir(parents=True)
    imported.write_bytes(b'# superseded bytes must not be reconstructed\n')
    edited=b'# exact edited imported source\r\nfunc value() -> f64:\r\n    return 3.141592653589793\r\n'
    imported.write_bytes(edited)
    evidence=root/'synthetic-evidence'
    os.environ['VOID_CRASH_EVIDENCE']=str(evidence)
    try:
        # Expected normal diagnostics are distinct from abnormal exits.
        h.run([sys.executable,'-c',emit+'raise SystemExit(1)'],root,
              expected=stdout,status=1,sources=(source,))
        assert not evidence.exists(), 'normal diagnostic created a crash incident'
        # Cover both the inherited crash.run boundary and the Math h.run adapter.
        result=h.preserving_run([sys.executable,'-c',emit+'raise SystemExit(87)'],
                                cwd=root,capture_output=True,source_inputs=(source,))
        assert result.returncode==87 and result.stdout==stdout and result.stderr==stderr
        result=h.run([sys.executable,'-c',emit+'raise SystemExit(87)'],root,
                     expected=stdout,status=87,sources=(source,))
        assert result.stderr==stderr
        try:
            h.run([sys.executable,'-c',emit+'time.sleep(20)'],root,
                  timeout=.5,sources=(source,))
        except subprocess.TimeoutExpired as failure:
            assert failure.output==stdout and failure.stderr==stderr
        else:
            raise AssertionError('synthetic timeout did not expire')
        receipts=list(evidence.glob('*/receipt.json'))
        assert len(receipts)==3, receipts
        records=[json.loads(receipt.read_text()) for receipt in receipts]
        assert sum(record['timed_out'] for record in records)==1
        assert sum(record['returncode']==87 for record in records)==2
        for receipt,record in zip(receipts,records):
            incident=receipt.parent
            assert record['cwd']==str(root.resolve())
            assert record['executable_sha256']==hashlib.sha256(Path(record['executable']).read_bytes()).hexdigest()
            assert (incident/'stdout.bin').read_bytes()==stdout
            assert (incident/'stderr.bin').read_bytes()==stderr
            for expected_source,raw in ((source,original),(imported,edited)):
                preserved=next(item for item in record['sources'] if item['original']==str(expected_source.resolve()))
                assert Path(preserved['copy']).name.endswith('_crash.void')
                assert Path(preserved['copy']).read_bytes()==raw
                expected_hash=hashlib.sha256(raw).hexdigest()
                assert preserved['original_sha256']==preserved['preserved_sha256']==preserved['sha256']==expected_hash
            provenance={item['original']:item for item in record['provenance_inputs']}
            for original_path in (h.IDENTITY,Path(__file__).resolve(),Path(h.__file__).resolve()):
                preserved=provenance[str(original_path)]
                raw=original_path.read_bytes()
                assert Path(preserved['copy']).read_bytes()==raw
                assert preserved['sha256']==hashlib.sha256(raw).hexdigest()
            copied_identity=json.loads(Path(provenance[str(h.IDENTITY)]['copy']).read_text())
            assert copied_identity==identity
    finally:
        if previous is None:os.environ.pop('VOID_CRASH_EVIDENCE',None)
        else:os.environ['VOID_CRASH_EVIDENCE']=previous
print('Math preservation: 3 synthetic reserved-87/timeout incidents, edited nested sources, compiler identity and original output verified and disposed')
