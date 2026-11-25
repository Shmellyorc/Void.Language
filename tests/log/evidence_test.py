"""Exercise the logging index with receipts outside its source checkout."""
import hashlib
import json
from pathlib import Path
import runpy
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import engineering_storage


class LoggingEvidenceTests(unittest.TestCase):
    def setUp(self):
        scratch = tempfile.TemporaryDirectory()
        self.addCleanup(scratch.cleanup)
        root = Path(scratch.name).resolve()
        self.checkout = root / 'checkout'
        self.script = self.checkout / 'tests/log/evidence.py'
        self.script.parent.mkdir(parents=True)
        shutil.copyfile(ROOT / 'tests/log/evidence.py', self.script)
        self.readme = b'# Source freeze\n'
        (self.checkout / 'README.md').write_bytes(self.readme)
        self.evidence = root / 'external' / 'qualification/log'
        self.evidence.mkdir(parents=True)
        (self.evidence / 'validation-source-freeze.json').write_text(json.dumps(
            {'README.md': hashlib.sha256(self.readme).hexdigest()}))
        for profile, build in [('debug', 'debug'), ('release', 'release'), ('strict', 'sanitizers')]:
            (self.evidence / f'acceptance-{profile}-verified.log').write_text(
                '100% tests passed, 0 tests failed out of 31\n')
            detail = self.checkout / f'build/ci-official-{build}/Testing/Temporary/LastTest.log'
            detail.parent.mkdir(parents=True)
            detail.write_text(''.join(f'{i}/31 Testing: selection_{i}\nTest Passed.\nTest time = 0.01 sec\n'
                                      for i in range(1, 32)))
        self.incident = self.evidence / 'crashes/incident-review'
        self.copy = self.incident / 'inputs/source.void'
        self.copy.parent.mkdir(parents=True)
        self.copy.write_bytes(b'frozen source')
        old_copy = root / 'old-checkout/docs/log/evidence/crashes/incident-review/inputs/source.void'
        (self.incident / 'receipt.json').write_text(json.dumps({
            'sources': [{'copy': str(old_copy), 'preserved_sha256': hashlib.sha256(self.copy.read_bytes()).hexdigest()}]
        }))
        (self.incident / 'stderr.bin').write_bytes(b'fixture diagnostic')
        run = self.evidence / 'run-capture'
        run.mkdir()
        (run / 'receipt.json').write_text(json.dumps({'returncode': 7}))

    def run_indexer(self):
        def git(args, *, cwd, text=False):
            self.assertEqual(cwd, self.checkout)
            return {
                ('diff', '--name-only', '--diff-filter=d'): 'README.md\n',
                ('ls-files', '--others', '--exclude-standard'): '',
                ('show', 'HEAD:README.md'): self.readme,
                ('diff', '--cached', '--name-only'): b'',
                ('rev-parse', 'HEAD'): 'frozen-commit\n',
            }[tuple(args[1:])]

        with patch.object(sys, 'path', sys.path.copy()), \
                patch.object(engineering_storage, 'documentation_evidence', return_value=self.evidence), \
                patch('platform.platform', return_value='fixture platform'), \
                patch('subprocess.check_output', side_effect=git):
            runpy.run_path(str(self.script), run_name='__main__')

    def test_relocated_incident_and_run_use_evidence_relative_paths(self):
        self.run_indexer()
        incidents = json.loads((self.evidence / 'incident-index.json').read_text())
        receipts = json.loads((self.evidence / 'receipt-index.json').read_text())
        self.assertEqual(incidents[0]['directory'], 'crashes/incident-review')
        self.assertEqual(incidents[0]['source_snapshots'], 1)
        self.assertEqual(receipts[0]['receipt'], 'run-capture/receipt.json')
        self.assertEqual(receipts[0]['returncode'], 7)
        self.assertEqual(set(json.loads((self.evidence / 'source-inventory.json').read_text())), {'README.md'})

    def test_relocated_source_with_changed_bytes_is_rejected(self):
        self.copy.write_bytes(b'changed source')
        with self.assertRaises(AssertionError):
            self.run_indexer()


if __name__ == '__main__':
    unittest.main(verbosity=2)
