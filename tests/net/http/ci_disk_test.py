"""No deletion: disk budgeting, allowlist safety, workflow ordering controls."""
import importlib.util
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location('linux_disk', ROOT / 'tools/ci/linux_disk.py')
disk = importlib.util.module_from_spec(spec); spec.loader.exec_module(disk)


class DiskTests(unittest.TestCase):
    def test_preflight_boundaries_all_configurations(self):
        for configuration in disk.BUILD_BYTES:
            for phase in ('before-restore', 'before-build'):
                required = disk.requirements(configuration, phase)
                disk.check_space(required, required)
                with self.assertRaisesRegex(RuntimeError, 'deficit=1'):
                    disk.check_space(required - 1, required)
            self.assertGreater(disk.requirements(configuration, 'before-restore'),
                               disk.SDK_BYTES + disk.ARCHIVE_BYTES + disk.BUILD_BYTES[configuration])

    def test_separate_temp_filesystem_must_fit_archive(self):
        with tempfile.TemporaryDirectory() as tmp, patch.dict(os.environ, {'EXPERIMENT_CONFIGURATION': 'Debug'}):
            record = {'workspace_free': disk.requirements('Debug', 'before-restore'),
                      'temp_free': disk.ARCHIVE_BYTES - 1, 'same_filesystem': False}
            with patch.object(disk, 'snapshot', return_value=record):
                with self.assertRaisesRegex(RuntimeError, 'deficit=1'):
                    disk.preflight('before-restore', Path(tmp))
                record['temp_free'] += 1
                disk.preflight('before-restore', Path(tmp))

    def test_cleanup_exact_allowlist(self):
        self.assertEqual(disk.CLEANUP, ('/usr/local/lib/android', '/usr/share/dotnet', '/opt/ghc',
                                       '/usr/local/.ghcup', '/usr/share/swift'))
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); good = root / 'sdk'; good.mkdir()
            alias = root / 'alias'; alias.symlink_to(good, target_is_directory=True)
            with patch.object(disk, 'CLEANUP', (str(good), str(root / 'absent'))):
                self.assertEqual(disk.cleanup_targets(), [good])
            for redirected in (alias, alias / 'child'):
                with patch.object(disk, 'CLEANUP', (str(redirected),)):
                    with self.assertRaisesRegex(RuntimeError, 'redirected'):
                        disk.cleanup_targets()
            with patch.object(disk, 'CLEANUP', (str(good),)), patch.object(Path, 'is_mount', return_value=True):
                with self.assertRaisesRegex(RuntimeError, 'mounted'):
                    disk.cleanup_targets()
            self.assertTrue(good.is_dir())

    def test_developer_execution_refused_before_cleanup(self):
        with patch.dict(os.environ, {'GITHUB_ACTIONS': 'false'}), patch('sys.argv', ['disk', 'prepare']), patch.object(disk.subprocess, 'run') as run:
            with self.assertRaisesRegex(RuntimeError, 'restricted'):
                disk.main()
            run.assert_not_called()

    def test_workflow_preserves_order_and_matrix(self):
        # YAML parse is separately checked; these assertions preserve the exact
        # invocation placement without adding a YAML dependency to CTest.
        text = (ROOT / '.github/workflows/ci-link-experiment.yml').read_text()
        self.assertEqual(text.count('- {os:'), 7)
        for command in ('after-sdk', 'before-build'):
            self.assertIn("if: runner.os == 'Linux'\n        run: python tools/ci/linux_disk.py " + command, text)
        self.assertIn("if: runner.os == 'Linux'\n        run: |\n          python tests/net/http/ci_disk_test.py\n          python tools/ci/linux_disk.py prepare", text)
        self.assertLess(text.index('linux_disk.py prepare'), text.index('uses: actions/cache@v5'))
        self.assertLess(text.index('python tools/ci/llvm_sdk.py'), text.index('linux_disk.py after-sdk'))
        self.assertLess(text.index('linux_disk.py before-build'), text.index('- name: Build ${{ matrix.configuration }}'))
        self.assertIn('key: llvm-sdk-23.1.4-${{ runner.os }}-${{ runner.arch }}-v1', text)
        self.assertIn('ctest --test-dir build/${{ matrix.configuration }} --output-on-failure --output-junit ctest-results.xml', text)


if __name__ == '__main__':
    unittest.main(verbosity=2)
