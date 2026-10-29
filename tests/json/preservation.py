"""Exercise JSON's actual process adapter with disposable synthetic incidents."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import sys
import tempfile
import harness as h


stdout = b'JSON stdout\r\n\0\xff'
stderr = b'JSON stderr\r\n\0\xfe'
emit = f'import os; os.write(1, {stdout!r}); os.write(2, {stderr!r}); '
previous = os.environ.get('VOID_CRASH_EVIDENCE')
try:
    with tempfile.TemporaryDirectory(prefix='VOID JSON preservation ') as directory:
        work = Path(directory).resolve()
        source = work / 'json_input.void'
        raw = 'use json\r\nfunc main():\r\n    var value = json.parse("雪")\r\n'.encode('utf-8')
        source.write_bytes(raw)
        evidence = work / 'incidents'
        os.environ['VOID_CRASH_EVIDENCE'] = str(evidence)
        h.run([sys.executable, '-c', emit + 'raise SystemExit(1)', source], source,
              cwd=work, expected=stdout, status=1)
        assert not evidence.exists(), 'ordinary JSON diagnostic created an incident'
        commands = []
        for status in (86, 87):
            command = [sys.executable, '-c', emit + f'raise SystemExit({status})', source]
            commands.append(list(map(str, command)))
            result = h.run(command, source, cwd=work, expected=stdout, status=status)
            assert result.stderr == stderr
        command = [sys.executable, '-c', emit + 'import time; time.sleep(20)', source]
        commands.append(list(map(str, command)))
        try:
            h.run(command, source, cwd=work, timeout=.5)
        except subprocess.TimeoutExpired as error:
            assert error.output == stdout and error.stderr == stderr
        else:
            raise AssertionError('synthetic JSON timeout did not expire')
        receipts = list(evidence.glob('*/receipt.json'))
        assert len(receipts) == 3
        records = [json.loads(path.read_text(encoding='utf-8')) for path in receipts]
        assert sorted(record['command'] for record in records) == sorted(commands)
        assert sum(record['timed_out'] for record in records) == 1
        assert {record['returncode'] for record in records if not record['timed_out']} == {86, 87}
        library = (h.ROOT / 'stdlib/json.void').resolve()
        for receipt, record in zip(receipts, records):
            assert record['cwd'] == str(work)
            assert record['executable_sha256'] == h.digest(Path(sys.executable).resolve())
            assert (receipt.parent / 'stdout.bin').read_bytes() == stdout
            assert (receipt.parent / 'stderr.bin').read_bytes() == stderr
            for original, expected in ((source, raw), (library, library.read_bytes())):
                entry = next(item for item in record['sources'] if item['original'] == str(original))
                assert Path(entry['copy']).name.endswith('_crash.void')
                assert Path(entry['copy']).read_bytes() == expected
                assert entry['original_sha256'] == entry['preserved_sha256'] == hashlib.sha256(expected).hexdigest()
                assert entry['captured_at'] == 'subprocess_launch'
                assert Path(entry['copy']).stat().st_mode & 0o222 == 0
            adapter = next(item for item in record['provenance_inputs']
                           if item['original'] == str(Path(h.__file__).resolve()))
            assert Path(adapter['copy']).read_bytes() == Path(h.__file__).read_bytes()
finally:
    if previous is None:
        os.environ.pop('VOID_CRASH_EVIDENCE', None)
    else:
        os.environ['VOID_CRASH_EVIDENCE'] = previous
print('JSON adapter: ordinary rejection and 3 reserved-status/timeout receipts verified')
