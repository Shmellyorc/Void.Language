"""Focused command/cache/isolation contracts; no native execution claim."""
import copy
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/ci'))
import install_smoke
import voidup_macos_ci as ci
from voidup_isolation import hidden_producer_paths


class IsolationTests(unittest.TestCase):
    def test_restores_inputs_after_consumer_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            paths = [Path(directory) / name for name in ('build', 'SDK with spaces', 'stdlib')]
            for path in paths:
                path.mkdir()
                (path / 'input').write_bytes(b'original')
            with self.assertRaisesRegex(RuntimeError, 'consumer failed'):
                with hidden_producer_paths(paths):
                    self.assertTrue(all(not path.exists() for path in paths))
                    raise RuntimeError('consumer failed')
            self.assertTrue(all((path / 'input').read_bytes() == b'original' for path in paths))

    def test_partial_setup_restores_prior_inputs_without_overwriting_held_path(self):
        with tempfile.TemporaryDirectory() as directory:
            first, second = [Path(directory) / name for name in ('first', 'second')]
            first.mkdir(); second.mkdir()
            held = second.with_name('second.voidup-held')
            held.write_bytes(b'preserve')
            with self.assertRaisesRegex(RuntimeError, 'Cannot isolate'):
                with hidden_producer_paths([first, second]):
                    self.fail('Consumer must not start')
            self.assertTrue(first.is_dir() and second.is_dir())
            self.assertEqual(held.read_bytes(), b'preserve')


class MacPlanTests(unittest.TestCase):
    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory()
        self.addCleanup(self.scratch.cleanup)
        self.environment = patch.dict(os.environ, {'RUNNER_TEMP': self.scratch.name,
            'INSTALL_QUALIFICATION_NAME': 'focused', 'HTTP_DEPS_CACHE_HIT': 'false',
            'HTTP_FULL_DEPS_CACHE_HIT': 'false'})
        self.environment.start(); self.addCleanup(self.environment.stop)
        with patch.object(install_smoke, 'SYSTEM', 'Darwin'):
            self.q = ci.MacQualification()
        self.q.http = Path(self.scratch.name) / 'http'
        self.q.host_prefix = Path(self.scratch.name) / 'host'
        self.q.evidence.mkdir(parents=True)
        self.plan = {'identity': {'sdk': 'pinned'}, 'prefix': 'http',
                     'deps-key': 'native-profile', 'sources-key': 'sources'}

    def test_install_profile_cannot_collide_with_full_http_cache(self):
        with patch.object(ci.http, 'make_plan', side_effect=lambda _: copy.deepcopy(self.plan)):
            result = self.q.http_plan()
        self.assertNotEqual(result['deps-key'], self.plan['deps-key'])
        self.assertEqual(result['sources-key'], self.plan['sources-key'])
        self.assertIn('voidup_install_only', result['identity'])

    def test_complete_cache_keeps_its_original_identity_and_key(self):
        with patch.object(ci.http, 'make_plan', side_effect=lambda _: copy.deepcopy(self.plan)), \
                patch.dict(os.environ, {'HTTP_FULL_DEPS_CACHE_HIT': 'true'}):
            self.assertEqual(self.q.selected_http_plan(), self.plan)

    def test_complete_cache_hit_verifies_original_seal_without_rebuilding(self):
        self.q.http.mkdir()
        manifest = self.q.http / 'VOID-HTTP-DEPENDENCIES.json'
        manifest.write_text('{}')
        with patch.dict(os.environ, {'HTTP_FULL_DEPS_CACHE_HIT': 'true'}), \
                patch.object(ci.http, 'make_plan', side_effect=lambda _: copy.deepcopy(self.plan)), \
                patch.object(ci.http, 'verify_prefix', return_value=manifest) as verify, \
                patch.object(self.q, 'run') as execute, patch.object(self.q, 'verify_dependencies'):
            self.q.dependencies()
        self.assertEqual(verify.call_args.args[0], self.plan)
        self.assertTrue(verify.call_args.kwargs['cached'])
        self.assertEqual([c.args[0] for c in execute.call_args_list], ['host-bootstrap'])
        self.assertFalse((self.q.http / 'VOID-HTTP-CI-IDENTITY.json').exists())

    def test_missing_http_manifest_blocks_cmake(self):
        with patch.object(self.q, 'http_plan', return_value=self.plan), \
                patch.object(ci.Qualification, 'configure') as configure:
            with self.assertRaises(FileNotFoundError):
                self.q.configure()
            configure.assert_not_called()

    def test_cache_hit_verifies_seal_and_never_rebuilds_http(self):
        self.q.http.mkdir()
        manifest = self.q.http / 'VOID-HTTP-DEPENDENCIES.json'
        manifest.write_text('{}')
        with patch.dict(os.environ, {'HTTP_DEPS_CACHE_HIT': 'true'}), \
                patch.object(self.q, 'http_plan', return_value=self.plan), \
                patch.object(ci.http, 'verify_prefix', return_value=manifest) as verify, \
                patch.object(self.q, 'run') as execute, patch.object(self.q, 'verify_dependencies'):
            self.q.dependencies()
        self.assertTrue(verify.call_args.kwargs['cached'])
        self.assertEqual([c.args[0] for c in execute.call_args_list], ['host-bootstrap'])

    def test_invalid_cache_stops_before_host_bootstrap(self):
        with patch.dict(os.environ, {'HTTP_DEPS_CACHE_HIT': 'true'}), \
                patch.object(self.q, 'http_plan', return_value=self.plan), \
                patch.object(ci.http, 'verify_prefix', side_effect=RuntimeError('seal mismatch')), \
                patch.object(self.q, 'run') as execute:
            with self.assertRaisesRegex(RuntimeError, 'seal mismatch'):
                self.q.dependencies()
            execute.assert_not_called()

    def test_cache_miss_cannot_adopt_existing_http_prefix(self):
        self.q.http.mkdir()
        with patch.object(self.q, 'http_plan', return_value=self.plan), patch.object(self.q, 'run') as execute:
            with self.assertRaisesRegex(RuntimeError, 'prefix exists on a cache miss'):
                self.q.dependencies()
            execute.assert_not_called()

    def test_failed_bootstrap_does_not_fabricate_manifest_or_seal(self):
        with patch.object(self.q, 'http_plan', return_value=self.plan), \
                patch.object(self.q, 'run', side_effect=RuntimeError('bootstrap failed')) as execute:
            with self.assertRaisesRegex(RuntimeError, 'bootstrap failed'):
                self.q.dependencies()
        self.assertIn('--macos-install-only', execute.call_args.args[1])
        self.assertFalse(self.q.http.exists())

    def test_apple_cache_flag_reaches_configure_without_forcing_lto(self):
        self.q.work.mkdir(exist_ok=True)
        with patch.object(ci.Qualification, 'run') as execute:
            self.q.run('configure', ['cmake', '-S', '.', '-B', 'build'])
        command = execute.call_args.args[1]
        self.assertEqual(command[-1], '-DCMAKE_EXE_LINKER_FLAGS:STRING=-Wl,-cache_path_lto,"' + str(self.q.work / 'thinlto-cache') + '"')
        self.assertFalse(any('-flto' in argument for argument in command))

    def test_missing_voidup_result_cannot_report_pass(self):
        with patch.dict(os.environ, {'INSTALL_PRIOR_JOB_STATUS': 'success'}), \
                patch.object(ci.Qualification, 'report') as report:
            self.q.report()
            self.assertEqual(os.environ['INSTALL_PRIOR_JOB_STATUS'], 'failure')
            report.assert_called_once()


if __name__ == '__main__':
    unittest.main(verbosity=2)
