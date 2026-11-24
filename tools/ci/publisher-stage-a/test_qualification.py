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
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools/ci'))
spec = importlib.util.spec_from_file_location('publisher_stage_a', ROOT / 'tools/ci/publisher_stage_a.py')
q = importlib.util.module_from_spec(spec)
spec.loader.exec_module(q)


class QualificationGuards(unittest.TestCase):
    def test_reviewed_source_and_patch(self):
        identity = q.verify_candidate()
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            paths = [*identity['source_sha256'], 'tools/ci/publisher-stage-a/identity.json',
                     'tools/ci/publisher-stage-a/candidate.patch']
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
            patch = root / 'tools/ci/publisher-stage-a/candidate.patch'
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
