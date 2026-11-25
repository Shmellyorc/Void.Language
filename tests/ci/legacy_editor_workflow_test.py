"""Local checks for the pinned editor checkout and its failure collector."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import textwrap
import unittest

ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = (ROOT / '.github/workflows/ci-macos-package-qualification.yml').read_text(encoding='utf-8')


class LegacyEditorWorkflowTests(unittest.TestCase):
    def setUp(self):
        self.pin = re.search(r'(?m)^ +PROJECT_EDITOR_LEGACY_SHA: ([0-9a-f]{40})$', WORKFLOW)[1]
        checkout = re.search(r'(?ms)^      - name: Check out the pinned legacy editor server sources\n(.*?)(?=^      - )', WORKFLOW)[1]
        self.checkout = re.search(r'(?m)^ +path: (.+)$', checkout)[1]
        self.assertIn('ref: ${{ env.PROJECT_EDITOR_LEGACY_SHA }}', checkout)
        self.assertIn('sparse-checkout-cone-mode: false', checkout)
        self.patterns = checkout.split('sparse-checkout: |\n', 1)[1].split()

    def pinned_file(self, path):
        return subprocess.check_output(['git', 'show', f'{self.pin}:{path}'], cwd=ROOT)

    def test_sparse_checkout_includes_the_exact_pinned_review_patches(self):
        provenance = json.loads(self.pinned_file('third_party/curl/VOID-PROVENANCE.json'))
        patches = provenance['approved_corrections']
        self.assertEqual(set(patches), {'cookie_enumeration', 'http2_window'})
        self.assertEqual({path for path in self.patterns if path.startswith('/docs/')},
                         {'/' + patch['review_diff'] for patch in patches.values()})
        for patch in patches.values():
            self.assertEqual(hashlib.sha256(self.pinned_file(patch['review_diff'])).hexdigest(),
                             patch['sha256'])

    def test_failure_collector_reads_pinned_producer_and_current_http_locations(self):
        producer = self.pinned_file('tools/setup_host_deps_native.py').decode()
        producer_path = re.search(r"evidence=ROOT/'([^']+)'/", producer)[1]
        failure_step = WORKFLOW.split('name: Collect bounded failure diagnostics\n', 1)[1]
        script = textwrap.dedent(failure_step.split("<<'PY'\n", 1)[1].split('\n          PY', 1)[0])
        with tempfile.TemporaryDirectory() as folder:
            scratch = Path(folder)
            workspace, work, external = (scratch / name for name in ('working copy', 'run', 'external'))
            (work / 'failure').mkdir(parents=True)
            fixtures = {
                workspace / self.checkout / producer_path / 'dependencies-macos-arm64-Debug-1/older.json': b'older legacy receipt',
                workspace / self.checkout / producer_path / 'dependencies-macos-arm64-Debug-2/legacy.json': b'latest legacy receipt',
                external / 'qualification/voidup/dependencies-macos-arm64-Debug-3/unrelated.json': b'unrelated external receipt',
                external / 'qualification/net/http/dependencies-macos-aws-lc-5.12.0-arm64-Debug-1/http.json': b'current HTTP receipt',
            }
            for path, contents in fixtures.items():
                path.parent.mkdir(parents=True)
                path.write_bytes(contents)
            env = dict(os.environ, PACKAGE_WORK=str(work), GITHUB_WORKSPACE=str(workspace),
                       PACKAGE_CONFIGURATION='Debug', PACKAGE_PROJECT_APPLICATION='true',
                       VOID_EVIDENCE_ROOT=str(external))
            subprocess.run([sys.executable, '-B', '-c', script], cwd=scratch, env=env, check=True)
            collected = {path.name.split('-', 1)[1]: path.read_bytes()
                         for path in (work / 'failure').iterdir()}
            self.assertEqual(collected, {'legacy.json': b'latest legacy receipt',
                                         'http.json': b'current HTTP receipt'})


if __name__ == '__main__':
    unittest.main(verbosity=2)
