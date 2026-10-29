"""Disposable checks of filesystem harness evidence before temporary inputs vanish."""
from pathlib import Path
import hashlib
import json
import os
import sys
import tempfile

import execution


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_source(record, original, expected):
    entry = next(item for item in record['sources'] if item['original'] == str(original.resolve()))
    preserved = Path(entry['copy'])
    assert preserved.name == original.stem + '_crash.void', preserved
    assert preserved.read_bytes() == expected
    expected_hash = hashlib.sha256(expected).hexdigest()
    assert entry['original_sha256'] == expected_hash
    assert entry['preserved_sha256'] == expected_hash
    assert entry['sha256'] == expected_hash
    assert digest(preserved) == expected_hash
    if os.name != 'nt':
        assert preserved.stat().st_mode & 0o222 == 0


def main():
    previous_evidence = os.environ.get('VOID_CRASH_EVIDENCE')
    with tempfile.TemporaryDirectory(prefix='VOID disposable io preservation ') as directory:
        root = Path(directory)
        evidence = root / 'incidents'
        inputs = root / 'inputs'
        inputs.mkdir()
        working = root / 'execution'
        working.mkdir()
        fixture = inputs / 'file_roundtrip.void'
        companion = inputs / 'shared_values.void'
        original = b'# exact generated fixture\r\nuse shared_values\r\nfunc main():\r\n    print("x")\r\n'
        companion_bytes = b'const answer = 7\n'
        fixture.write_bytes(original)
        companion.write_bytes(companion_bytes)
        crasher = root / 'synthetic_process.py'
        crasher.write_text('''import os
import sys

os.write(1, b"synthetic stdout\\x00\\n")
os.write(2, b"synthetic stderr\\x00\\n")
if sys.argv[1] == "diagnostic":
    sys.exit(1)
if os.name == "nt":
    import ctypes
    ctypes.windll.kernel32.ExitProcess(0xC0000005)
else:
    import resource
    import signal
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    os.kill(os.getpid(), signal.SIGSEGV)
''')
        os.environ['VOID_CRASH_EVIDENCE'] = str(evidence)
        try:
            # Ordinary rejected source remains an expected diagnostic, without an incident.
            execution.run(
                [sys.executable, crasher, 'diagnostic', 'check', fixture],
                working,
                expected=None,
                success=False,
                source_inputs=(fixture, companion),
            )
            assert not evidence.exists()
            commands = (
                [sys.executable, crasher, 'crash', 'check', fixture],
                [sys.executable, crasher, 'crash', '--async', fixture],
                [sys.executable, crasher, 'crash', 'linked-aot'],
            )
            for serial, command in enumerate(commands, 1):
                try:
                    execution.run(
                        command,
                        working,
                        expected=None,
                        source_inputs=(fixture, companion),
                    )
                except AssertionError as failure:
                    status = failure.args[0][1]
                    assert status < 0 or status >= 0x80000000
                else:
                    raise AssertionError('The synthetic abnormal exit was not rejected')
                receipts = sorted(evidence.glob('*/receipt.json'))
                assert len(receipts) == serial, receipts
                records = [json.loads(receipt.read_text()) for receipt in receipts]
                record = next(item for item in records if item['command'] == list(map(str, command)))
                assert record['cwd'] == str(working.resolve())
                assert record['returncode'] == status
                if os.name != 'nt':
                    assert record['signal'] == 'SIGSEGV'
                assert record['executable_sha256'] == digest(Path(record['executable']))
                verify_source(record, fixture, original)
                verify_source(record, companion, companion_bytes)
                generator = Path(execution.__file__).resolve()
                provenance = next(item for item in record['provenance_inputs']
                                  if item['original'] == str(generator))
                assert Path(provenance['copy']).read_bytes() == generator.read_bytes()
                assert provenance['sha256'] == digest(generator)
                assert provenance['original_sha256'] == provenance['preserved_sha256']
                receipt = next(path for path in receipts if json.loads(path.read_text()) == record)
                assert (receipt.parent / 'stdout.bin').read_bytes() == b'synthetic stdout\x00\n'
                assert (receipt.parent / 'stderr.bin').read_bytes() == b'synthetic stderr\x00\n'
            assert len({receipt.parent for receipt in receipts}) == 3
            # The linked executable command has no source argument and runs outside inputs.
            assert not any(str(argument).endswith('.void') for argument in commands[-1])
            assert not list(working.glob('*.void'))
            # The normal TemporaryDirectory cleanup runs only after all exact-byte checks.
        finally:
            if previous_evidence is None:
                os.environ.pop('VOID_CRASH_EVIDENCE', None)
            else:
                os.environ['VOID_CRASH_EVIDENCE'] = previous_evidence
    assert not root.exists()
    print('IO preservation passed: compiler check, async runner and linked AOT abnormal exits; '
          'exact sources, companion, generator, command and streams verified; '
          'ordinary rejection excluded; all synthetic evidence disposed')


if __name__ == '__main__':
    main()
