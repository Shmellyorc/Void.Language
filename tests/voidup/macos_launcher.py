"""Offline macOS ZIP launcher contracts using a shell bootstrap fixture.

The receipt models Darwin ARM64 to select the macOS package format. Running
this on Linux checks portable shell behavior, not native macOS installation.
"""
from contextlib import redirect_stdout
import io
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
from types import SimpleNamespace as Args
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import voidup_release as release


@unittest.skipIf(os.name == 'nt', 'POSIX shell launcher contract')
class MacLauncher(unittest.TestCase):
    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory(prefix='voidup-macos-launcher-')
        self.addCleanup(self.scratch.cleanup)
        self.work = Path(self.scratch.name).resolve()
        self.prefix = self.work / 'producer'
        payload = {
            'bin/voidup': b'#!/bin/sh\nexit 98\n',
            'bin/voidup-init': b'#!/bin/sh\nset -eu\nprintf \'%s\\000\' "$PWD" "$0" "$@"\nexit "${VOIDUP_TEST_EXIT:-0}"\n',
            'share/notice': b'NONPRODUCTION shell fixture\n',
        }
        for name, data in payload.items():
            path = self.prefix / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            path.chmod(0o755 if name.startswith('bin/') else 0o644)
        self.files = {name: {'bytes': len(data), 'sha256': release.sha(self.prefix / name)}
                      for name, data in payload.items()}
        (self.prefix / 'build.json').write_text(json.dumps({
            'host': {'system': 'Darwin', 'machine': 'arm64'},
            'production_trust': False, 'trust_config_sha256': None, 'files': self.files,
        }))
        self.archive = self.work / 'download.zip'
        self.package(self.archive)
        self.download = self.work / '-download 雪 \' " $(printf injected) ; & # % [x] `'
        with zipfile.ZipFile(self.archive) as archive:
            archive.extractall(self.download)
            # Python's extractor does not restore ZIP Unix executable modes.
            # Apply the actual stored modes, as the native ditto extractor does.
            for member in archive.infolist():
                (self.download / member.filename).chmod(stat.S_IMODE(member.external_attr >> 16))
        self.launcher = self.download / 'Install VOID.command'
        self.unrelated = self.work / 'unrelated'
        self.unrelated.mkdir()
        minimal = self.work / 'os-utilities'
        minimal.mkdir()
        self.env = dict(os.environ, PATH=str(minimal), CDPATH=str(self.work / 'decoy'))
        probe = subprocess.run(['/bin/sh', '-c', 'command -v dirname'], env=self.env,
                               capture_output=True, timeout=10)
        self.assertNotEqual(probe.returncode, 0)
        self.assertEqual(list(minimal.iterdir()), [])

    def package(self, output):
        with patch.dict(os.environ, SOURCE_DATE_EPOCH='315532800'), redirect_stdout(io.StringIO()):
            release.bootstrap(Args(prefix=self.prefix, host='macos-aarch64',
                                   production=False, output=output))

    def test_extracted_launcher_arguments_cwd_and_exit_status(self):
        arguments = ['', 'two words', '雪😀', "'\"$() ; & # % * ? [x] `", 'ends\\', 'line\nbreak', '--root', 'literal']
        invocations = [
            ('absolute shell', ['/bin/sh', str(self.launcher)], self.unrelated),
            ('relative shell', ['/bin/sh', '--', str(self.launcher.relative_to(self.work))], self.work),
            ('bare shell', ['/bin/sh', self.launcher.name], self.download),
            ('direct executable', [str(self.launcher)], self.unrelated),
        ]
        for label, command, cwd in invocations:
            for status, operands in [(0, []), (37, arguments)]:
                with self.subTest(invocation=label, exit=status):
                    result = subprocess.run([*command, *operands], cwd=cwd,
                                            env=self.env | {'VOIDUP_TEST_EXIT': str(status)},
                                            capture_output=True, timeout=10)
                    self.assertEqual(result.returncode, status, result.stderr)
                    self.assertEqual(result.stderr, b'')
                    expected = [str(self.download), './bin/voidup-init', *operands]
                    self.assertEqual(result.stdout, b''.join(s.encode() + b'\0' for s in expected))

    def test_missing_bootstrap_fails_without_running_cwd_decoy(self):
        (self.download / 'bin/voidup-init').unlink()
        decoy = self.unrelated / 'bin/voidup-init'
        decoy.parent.mkdir()
        decoy.write_text('#!/bin/sh\nprintf cwd-decoy-ran\nexit 0\n')
        decoy.chmod(0o755)
        for command in ([str(self.launcher)], ['/bin/sh', str(self.launcher)]):
            with self.subTest(command=command):
                result = subprocess.run(command, cwd=self.unrelated, env=self.env,
                                        capture_output=True, timeout=10)
                self.assertEqual(result.returncode, 127, result.stderr)
                self.assertEqual(result.stdout, b'')
                self.assertIn(b'voidup-init', result.stderr)

    def test_package_integrity_modes_and_reproducibility(self):
        metadata = json.loads(Path(str(self.archive) + '.json').read_bytes())
        self.assertEqual(metadata['files'], self.files)
        self.assertFalse(metadata['production_trust'])
        self.assertEqual(metadata['host'], 'macos-aarch64')
        self.assertEqual(metadata['archive_bytes'], self.archive.stat().st_size)
        self.assertEqual(metadata['archive_sha256'], release.sha(self.archive))
        with zipfile.ZipFile(self.archive) as archive:
            self.assertIsNone(archive.testzip())
            self.assertEqual(set(archive.namelist()), {*self.files, 'Install VOID.command'})
            self.assertEqual(metadata['expanded_bytes'], sum(m.file_size for m in archive.infolist()))
            for name in self.files:
                self.assertEqual(archive.read(name), (self.prefix / name).read_bytes())
            for name in ['Install VOID.command', 'bin/voidup', 'bin/voidup-init']:
                self.assertEqual(archive.getinfo(name).external_attr >> 16, 0o100755)
            self.assertEqual(archive.getinfo('share/notice').external_attr >> 16, 0o100644)
        subprocess.run(['/bin/sh', '-n', str(self.launcher)], check=True, timeout=10)
        second = self.work / 'repeat.zip'
        self.package(second)
        self.assertEqual(second.read_bytes(), self.archive.read_bytes())
        self.assertEqual(Path(str(second) + '.json').read_bytes(), Path(str(self.archive) + '.json').read_bytes())


if __name__ == '__main__':
    unittest.main(verbosity=2)
