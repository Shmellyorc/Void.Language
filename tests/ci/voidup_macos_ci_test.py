"""Focused command/cache/isolation contracts; no native execution claim."""
import copy
import json
import os
from pathlib import Path
import re
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
        self.assertFalse(result['deps-key'].startswith(self.plan['deps-key']))
        self.assertFalse(self.plan['deps-key'].startswith(result['deps-key']))
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


class CacheRestoreTests(unittest.TestCase):
    """Model Actions key matching/extraction; execute the real plan/seal checks.

    Native dependency builds and the external binary verifier are recorded, not
    executed. The restore gates and lookup-only input come from the real YAML.
    """
    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory()
        self.addCleanup(self.scratch.cleanup)
        self.root = Path(self.scratch.name)
        self.env = patch.dict(os.environ, {'RUNNER_TEMP': str(self.root),
            'INSTALL_QUALIFICATION_NAME': 'cache-model', 'HTTP_DEPS_CACHE_HIT': '',
            'HTTP_FULL_DEPS_CACHE_HIT': ''})
        self.env.start(); self.addCleanup(self.env.stop)
        with patch.object(install_smoke, 'SYSTEM', 'Darwin'):
            self.q = ci.MacQualification()
        self.q.http = self.root / 'http'; self.q.host_prefix = self.root / 'host'
        self.q.evidence.mkdir(parents=True)
        self.host = dict(system='Darwin', architecture='arm64', sdk='/native/sdk',
                         deployment_target='14.0', sdk_sha256='pinned-sdk', cmake='modeled', image_os='macos15')
        host_patch = patch.object(ci.http, 'host_identity', return_value=self.host)
        host_patch.start(); self.addCleanup(host_patch.stop)
        self.install, self.full = self.q.http_plan(), self.q.http_plan(complete=True)
        # Point only the verifier's destination at disposable storage. All
        # generated keys/identities still come from the actual recipe and pins.
        self.root_patch = patch.object(ci.http, 'ROOT', self.root)
        self.entries = {}; self.downloaded = []; self.lookups = []
        self.executed = []; self.native_verifications = []
        text = (ROOT / '.github/workflows/voidup-release.yml').read_text()
        self.steps = {}
        for block in re.split(r'(?m)^      - ', text)[1:]:
            fields = dict(re.findall(r'(?m)^\s+(id|if|uses|path|key|lookup-only): (.+)$', block))
            if fields.get('uses') == 'actions/cache/restore@v5' and fields.get('id', '').startswith('http-'):
                self.steps[fields['id']] = fields

    def populate(self, plan):
        self.q.http.mkdir(parents=True)
        manifest = self.q.http / 'VOID-HTTP-DEPENDENCIES.json'
        manifest.write_text(json.dumps(dict(platform='Darwin', architecture='arm64', sdk='/native/sdk',
            deployment_target='14.0', files={'lib/fixture.a': 'modeled-bytes'})))
        ci.http.save(self.q.http, 'VOID-HTTP-CI-IDENTITY.json',
            dict(identity=plan['identity'], manifest_sha256=ci.http.digest(manifest)))

    def restore(self, key, lookup=False):
        # GitHub tries exact first, then primary-key prefix matches, even with
        # no restore-keys. Dict insertion order models the newest partial entry.
        matched = key if key in self.entries else next((k for k in reversed(self.entries) if k.startswith(key)), '')
        output = {'cache-hit': str(matched == key).lower() if matched else '', 'cache-matched-key': matched}
        if lookup:
            self.lookups.append(matched)
        elif matched:
            self.populate(self.entries[matched]); self.downloaded.append(matched)
        return output

    def workflow(self, evict_full_after_lookup=False):
        outputs = {}
        for name in ['http-deps', 'http-full-deps-lookup', 'http-full-deps']:
            step = self.steps[name]
            execute = True
            for term in step['if'].split(' && '):
                if term == "runner.os == 'macOS'":continue
                match = re.fullmatch(r"steps.([\w-]+).outputs.cache-hit (==|!=) 'true'", term)
                self.assertIsNotNone(match, term)
                actual = outputs.get(match[1], {}).get('cache-hit', '')
                execute = execute and ((actual == 'true') if match[2] == '==' else (actual != 'true'))
            if not execute:continue
            field = re.fullmatch(r'\$\{\{ steps.http.outputs.([\w-]+) \}\}', step['key'])[1]
            key = self.install['deps-key'] if field == 'deps-key' else self.full['deps-key']
            self.assertEqual(step['path'], '${{ steps.http.outputs.prefix }}')
            outputs[name] = self.restore(key, step.get('lookup-only', 'false') == 'true')
            if evict_full_after_lookup and name == 'http-full-deps-lookup':
                self.entries.pop(self.full['deps-key'], None)
        os.environ['HTTP_DEPS_CACHE_HIT'] = outputs.get('http-deps', {}).get('cache-hit', '')
        os.environ['HTTP_FULL_DEPS_CACHE_HIT'] = outputs.get('http-full-deps', {}).get('cache-hit', '')
        return outputs

    def dependencies(self):
        def execute(label, argv, **kwargs):
            self.executed.append(label)
            if label == 'http-bootstrap':
                self.assertIn('--macos-install-only', argv); self.populate(self.install)
        def verify(evidence, label, argv):
            self.native_verifications.append(list(map(str, argv)))
            self.assertEqual(argv[-3:], ['Release', 'Darwin', 'arm64'])
        # Use real verify_prefix (including seal and native-command selection).
        # The existing provider verifier itself remains unchanged and unmocked
        # in production; unit execution records its subprocess here.
        with self.root_patch, patch.object(self.q, 'selected_http_plan', return_value=
                self.full if os.environ['HTTP_FULL_DEPS_CACHE_HIT'] == 'true' else self.install), \
                patch.object(self.q, 'run', side_effect=execute), patch.object(ci.http, 'run', side_effect=verify), \
                patch.object(self.q, 'verify_dependencies'):
            selected = self.q.selected_http_plan()
            selected = copy.deepcopy(selected); selected['prefix'] = 'http'
            with patch.object(self.q, 'selected_http_plan', return_value=selected):self.q.dependencies()

    def test_exact_installation_hit_reuses_and_verifies(self):
        self.entries[self.install['deps-key']] = self.install
        self.workflow(); self.dependencies()
        self.assertEqual(self.downloaded, [self.install['deps-key']]); self.assertEqual(self.lookups, [])
        self.assertEqual(self.executed, ['host-bootstrap']); self.assertEqual(len(self.native_verifications), 1)

    def test_exact_complete_hit_reuses_its_own_identity(self):
        self.entries[self.full['deps-key']] = self.full
        self.entries[self.full['deps-key'] + '-voidup-install-' + '0' * 64] = self.install
        self.workflow(); self.dependencies()
        self.assertEqual(self.downloaded, [self.full['deps-key']]); self.assertEqual(self.executed, ['host-bootstrap'])
        seal = json.loads((self.q.http / 'VOID-HTTP-CI-IDENTITY.json').read_text())
        self.assertEqual(seal['identity'], self.full['identity']); self.assertEqual(len(self.native_verifications), 1)

    def test_legacy_partial_match_is_not_extracted_and_cold_bootstraps(self):
        legacy = self.full['deps-key'] + '-voidup-install-' + '0' * 64
        self.entries[legacy] = self.install
        outputs = self.workflow()
        self.assertEqual(outputs['http-full-deps-lookup']['cache-hit'], 'false')
        self.assertEqual(self.lookups, [legacy]); self.assertFalse(self.q.http.exists())
        self.dependencies()
        self.assertEqual(self.downloaded, []); self.assertEqual(self.executed, ['http-bootstrap', 'host-bootstrap'])
        self.assertEqual(len(self.native_verifications), 1)

    def test_original_direct_restore_reproduces_the_collision(self):
        legacy = self.full['deps-key'] + '-voidup-install-' + '0' * 64
        self.entries[legacy] = self.install
        output = self.restore(self.full['deps-key'])
        self.assertEqual(output['cache-hit'], 'false'); self.assertTrue(self.q.http.exists())
        with self.assertRaisesRegex(RuntimeError, 'prefix exists on a cache miss'):self.dependencies()
        self.assertEqual(self.executed, []); self.assertEqual(self.native_verifications, [])

    def test_genuine_miss_bootstraps_and_seals(self):
        self.workflow(); self.dependencies()
        self.assertEqual(self.executed, ['http-bootstrap', 'host-bootstrap'])
        seal = json.loads((self.q.http / 'VOID-HTTP-CI-IDENTITY.json').read_text())
        self.assertEqual(seal['identity'], self.install['identity']); self.assertEqual(len(self.native_verifications), 1)

    def test_unexpected_prefix_is_not_adopted_or_removed(self):
        self.q.http.mkdir(); sentinel = self.q.http / 'unverified'; sentinel.write_text('preserve')
        self.workflow()
        with self.assertRaisesRegex(RuntimeError, 'prefix exists on a cache miss'):self.dependencies()
        self.assertEqual(sentinel.read_text(), 'preserve'); self.assertEqual(self.executed, [])

    def test_exact_key_with_corrupt_manifest_is_rejected(self):
        self.entries[self.install['deps-key']] = self.install; self.workflow()
        manifest = self.q.http / 'VOID-HTTP-DEPENDENCIES.json'; manifest.write_text(manifest.read_text() + '\n')
        with self.assertRaisesRegex(RuntimeError, 'identity/manifest mismatch'):self.dependencies()
        self.assertEqual(self.executed, []); self.assertEqual(self.native_verifications, [])

    def test_exact_install_key_cannot_adopt_a_complete_profile_seal(self):
        self.entries[self.install['deps-key']] = self.full; self.workflow()
        with self.assertRaisesRegex(RuntimeError, 'identity/manifest mismatch'):self.dependencies()
        self.assertEqual(self.executed, []); self.assertEqual(self.native_verifications, [])

    def test_diagnostic_driver_edit_changes_only_install_identity(self):
        with patch.object(ci, 'digest', return_value='changed-diagnostic-driver'):
            changed, full = self.q.http_plan(), self.q.http_plan(complete=True)
        self.assertNotEqual(changed['deps-key'], self.install['deps-key'])
        self.assertEqual(full, self.full); self.assertEqual(changed['sources-key'], self.install['sources-key'])
        self.assertFalse(changed['deps-key'].startswith(full['deps-key']))
        self.entries[self.install['deps-key']] = self.install; self.install = changed
        self.workflow(); self.dependencies()
        self.assertEqual(self.downloaded, []); self.assertEqual(self.executed, ['http-bootstrap', 'host-bootstrap'])

    def test_eviction_between_lookup_and_restore_fails_closed(self):
        self.entries[self.full['deps-key']] = self.full
        legacy = self.full['deps-key'] + '-voidup-install-' + '0' * 64
        self.entries[legacy] = self.install
        self.workflow(evict_full_after_lookup=True)
        with self.assertRaisesRegex(RuntimeError, 'prefix exists on a cache miss'):self.dependencies()
        self.assertEqual(self.executed, []); self.assertEqual(self.native_verifications, [])


if __name__ == '__main__':
    unittest.main(verbosity=2)
