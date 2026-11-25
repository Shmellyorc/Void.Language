"""Local guard checks; these do not claim native macOS qualification."""
import copy
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools/ci'))
spec = importlib.util.spec_from_file_location('publisher_stage_b', ROOT / 'tools/ci/publisher_stage_b.py')
q = importlib.util.module_from_spec(spec)
spec.loader.exec_module(q)


class QualificationGuards(unittest.TestCase):
    def test_macho_oracle_removes_only_id_commands(self):
        image = Path('/tmp/fixture.dylib')
        needed = b'NeededLibraries [\n  @rpath/self.dylib\n  @rpath/self.dylib\n  /usr/lib/libSystem.B.dylib\n]\n'
        identity = b'/tmp/fixture.dylib:\n@rpath/self.dylib\n'
        self.assertEqual(q.macho_dependencies(needed, identity, image),
                         ['/usr/lib/libSystem.B.dylib', '@rpath/self.dylib'])
        self.assertEqual(q.macho_dependencies(needed, b'/tmp/fixture.dylib:\n', image),
                         ['/usr/lib/libSystem.B.dylib', '@rpath/self.dylib', '@rpath/self.dylib'])
        for invalid in (b'', b'/tmp/wrong:\n', b'/tmp/fixture.dylib:\nmissing\n'):
            with self.assertRaises(RuntimeError):
                q.macho_dependencies(needed, invalid, image)

    def test_nonexact_dependency_restores_are_removed_before_build(self):
        for kind in ('http', 'host'):
            for hit in ('false', ''):
                with self.subTest(kind=kind, hit=hit), tempfile.TemporaryDirectory() as folder:
                    root = Path(folder)
                    prefix = root / '.toolchains' / kind / 'prefix'
                    prefix.mkdir(parents=True)
                    (prefix / 'unverified.a').write_bytes(b'stale partial restore')
                    task = q.Qualification('Release', root / 'work')
                    q.save(task.work / (kind + '-plan.json'), {'prefix': str(prefix)})
                    def build(label, *args, **kwargs):
                        self.assertEqual(label, kind + '-build')
                        self.assertFalse(prefix.exists())
                        raise RuntimeError('reached fresh build')
                    with mock.patch.object(q, 'ROOT', root), mock.patch.dict(q.os.environ,
                            {kind.upper() + '_DEPS_CACHE_HIT': hit}), mock.patch.object(task, 'run', side_effect=build):
                        with self.assertRaisesRegex(RuntimeError, 'reached fresh build'):
                            getattr(task, kind)()

    def test_http_rebuild_and_exact_reuse_both_validate(self):
        for cached in (False, True):
            with self.subTest(cached=cached), tempfile.TemporaryDirectory() as folder:
                root = Path(folder)
                prefix = root / 'prefix'
                prefix.mkdir()
                task = q.Qualification('Release', root / 'work')
                plan = {'prefix': str(prefix), 'identity': {'profile': 'Release'}}
                q.save(task.work / 'http-plan.json', plan)
                pins = root / 'third_party/http-versions.json'
                q.save(pins, {'dependencies': []})
                def build(*args, **kwargs):
                    self.assertFalse(prefix.exists())
                    prefix.mkdir()
                with mock.patch.object(q, 'ROOT', root), mock.patch.dict(q.os.environ,
                        {'HTTP_DEPS_CACHE_HIT': 'true' if cached else 'false'}), \
                        mock.patch.object(task, 'run', side_effect=build), \
                        mock.patch.object(task, 'acquire_sources'), \
                        mock.patch.object(q.http, 'verify_prefix', side_effect=RuntimeError('validation rejected')) as verify:
                    with self.assertRaisesRegex(RuntimeError, 'validation rejected'):
                        task.http()
                    verify.assert_called_once_with(plan, task.evidence, cached=cached)
                self.assertFalse((prefix / 'VOID-HTTP-CI-IDENTITY.json').exists())

    def test_report_renames_preserve_baseline_bytes_and_remove_case_clashes(self):
        paths = subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT).decode().split('\0')
        for old, new in q.REPORT_RENAMES.items():
            original = subprocess.check_output(['git', 'show', q.BASELINE + ':' + old], cwd=ROOT)
            self.assertNotIn(old, paths)
            self.assertEqual(original, (ROOT / new).read_bytes())
        folded = [name.casefold() for name in paths if name]
        self.assertEqual(len(folded), len(set(folded)))

    def test_collect_retains_actual_unstaged_and_staged_changes_before_rejecting(self):
        for staged in (False, True):
            with self.subTest(staged=staged), tempfile.TemporaryDirectory() as folder:
                root = Path(folder) / 'checkout'
                root.mkdir()
                def git(*args):
                    return subprocess.run(['git', *args], cwd=root, check=True,
                                          stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                git('init')
                source = root / 'tracked.txt'
                source.write_text('original\n')
                git('add', 'tracked.txt')
                git('-c', 'user.name=Qualification Test', '-c', 'user.email=test@invalid',
                    '-c', 'commit.gpgsign=false', 'commit', '-m', 'fixture')
                source.write_text('changed\n')
                if staged:
                    git('add', 'tracked.txt')
                with mock.patch.object(q, 'ROOT', root), mock.patch.object(q, 'verify_candidate', return_value={}):
                    task = q.Qualification('Release', Path(folder) / 'work')
                    with self.assertRaisesRegex(RuntimeError, 'checkout differs'):
                        task.collect()
                self.assertEqual((task.evidence / 'collect-tracked-paths.stdout').read_text(), 'tracked.txt\n')
                self.assertIn('+changed', (task.evidence / 'collect-tracked-diff.stdout').read_text())
                self.assertFalse((task.evidence / 'qualification-summary.json').exists())

    def test_reviewed_source_and_patch(self):
        identity = q.verify_candidate()
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            paths = [*identity['source_sha256'], 'tools/ci/publisher-stage-b/identity.json',
                     'tools/ci/publisher-stage-b/candidate.patch']
            for name in paths:
                destination = root / name
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / name, destination)
            q.verify_candidate(root)
            source = root / 'src/publish.cpp'
            source.write_bytes(source.read_bytes() + b'\n// changed candidate\n')
            with self.assertRaisesRegex(RuntimeError, 'source checksum'):
                q.verify_candidate(root)
            shutil.copyfile(ROOT / 'src/publish.cpp', source)
            patch = root / 'tools/ci/publisher-stage-b/candidate.patch'
            patch.write_bytes(patch.read_bytes() + b'\n')
            with self.assertRaisesRegex(RuntimeError, 'patch checksum'):
                q.verify_candidate(root)

    def test_registration_rejects_missing_disabled_and_skipped_tests(self):
        document = {'tests': [{'name': name, 'command': ['existing-test'], 'properties': []} for name in q.TESTS]}
        q.validate_registration(document)
        for props in ([{'name': 'DISABLED', 'value': True}],
                      [{'name': 'SKIP_RETURN_CODE', 'value': 77}],
                      [{'name': 'SKIP_REGULAR_EXPRESSION', 'value': 'skip'}]):
            changed = copy.deepcopy(document)
            changed['tests'][0]['properties'] = props
            with self.assertRaises(RuntimeError):
                q.validate_registration(changed)
        for change in ('missing', 'duplicate', 'empty_command'):
            changed = copy.deepcopy(document)
            if change == 'missing':
                changed['tests'].pop()
            elif change == 'duplicate':
                changed['tests'].append(changed['tests'][0])
            else:
                changed['tests'][0]['command'] = []
            with self.assertRaises(RuntimeError):
                q.validate_registration(changed)

    def test_junit_requires_actual_complete_success(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'tests.xml'
            suite = ET.Element('testsuite', tests=str(len(q.TESTS)), failures='0', skipped='0')
            for name in q.TESTS:
                ET.SubElement(suite, 'testcase', name=name, status='run', time='0.1')
            ET.ElementTree(suite).write(path)
            q.validate_junit(path)
            for reason in ('failure', 'skipped', 'missing'):
                changed = copy.deepcopy(suite)
                if reason == 'missing':
                    changed.remove(changed[-1])
                else:
                    ET.SubElement(changed[0], reason)
                ET.ElementTree(changed).write(path)
                with self.assertRaises(RuntimeError):
                    q.validate_junit(path)

    def test_timeout_retains_partial_diagnostics_and_stops_its_child_group(self):
        with tempfile.TemporaryDirectory() as folder:
            task = q.Qualification('Release', Path(folder))
            with self.assertRaises(subprocess.TimeoutExpired):
                task.run('timeout-control', [sys.executable, '-c',
                    'import time; print("retained before timeout",flush=True); time.sleep(60)'], timeout=0.3)
            self.assertIn(b'retained before timeout', (task.evidence / 'timeout-control.stdout').read_bytes())
            record = json.loads((task.evidence / 'timeout-control.command.json').read_text())
            self.assertIn('timeout', record)
            self.assertNotIn('exit', record)


if __name__ == '__main__':
    unittest.main(verbosity=2)
