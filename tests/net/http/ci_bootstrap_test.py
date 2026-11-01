"""Focused CI cache policy tests; no transport stress or native-platform claim."""
import contextlib
import copy
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import runpy
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location('http_ci', ROOT / 'tools/ci/http_bootstrap.py')
ci = importlib.util.module_from_spec(spec); spec.loader.exec_module(ci)


def host(system='Linux'):
    result = {'system': system, 'architecture': 'arm64' if system == 'Darwin' else 'x86_64',
              'sdk_sha256': 'sdk', 'image_os': 'image', 'cmake': 'cmake test'}
    if system == 'Linux': result['libc'] = ['glibc', '2.39']
    if system == 'Windows': result.update(VCToolsVersion='14.44', WindowsSDKVersion='10.0', VSCMD_ARG_TGT_ARCH='x64')
    if system == 'Darwin': result.update(sdk='/native/sdk', sdk_version='26', xcode='26', deployment_target='14.0')
    return result


def plan(configuration='Release', system='Linux', override=None):
    with patch.object(ci, 'host_identity', return_value=override or host(system)):
        return ci.make_plan(configuration)


class IdentityTests(unittest.TestCase):
    def test_seven_profiles_have_distinct_native_keys(self):
        cases = [('Linux', n) for n in ['Release', 'Debug', 'Sanitizers']] + [(s, n) for s in ['Windows', 'Darwin'] for n in ['Release', 'Debug']]
        self.assertEqual(7, len({plan(n, s)['deps-key'] for s, n in cases}))
        self.assertEqual('strict', plan('Sanitizers')['profile'])

    def test_plan_roundtrips_json(self):
        for system in ['Linux', 'Windows', 'Darwin']:
            value = plan(system=system)
            self.assertEqual(value, json.loads(json.dumps(value)))

    def test_windows_fixture_and_sources_shared_across_profiles(self):
        a, b = plan('Debug', 'Windows'), plan('Release', 'Windows')
        self.assertEqual(a['fixtures-key'], b['fixtures-key'])
        self.assertEqual(a['sources-key'], b['sources-key'])
        self.assertNotEqual(a['deps-key'], b['deps-key'])

    def test_crt_sdk_changes_invalidate_windows_binary_caches(self):
        a = plan(system='Windows'); changed = host('Windows'); changed['VCToolsVersion'] = 'new'
        b = plan(system='Windows', override=changed)
        self.assertNotEqual(a['deps-key'], b['deps-key'])
        self.assertNotEqual(a['fixtures-key'], b['fixtures-key'])
        self.assertEqual(a['sources-key'], b['sources-key'])

    def test_macos_sdk_and_deployment_invalidate_prefix(self):
        a = plan(system='Darwin')
        for name in ['sdk', 'sdk_version', 'deployment_target']:
            changed = host('Darwin'); changed[name] = 'changed'
            self.assertNotEqual(a['deps-key'], plan(system='Darwin', override=changed)['deps-key'])

    def test_recipe_and_fixture_pins_invalidate_respective_keys(self):
        a = plan(); actual = ci.digest
        for name, field in [('tools/setup_http_deps_native.py', 'deps-key'), ('tools/http_archive.py', 'deps-key'), ('cmake/CurlProviderFeatures.cmake', 'deps-key'), ('cmake/NativeBuild.cmake', 'deps-key'), ('tests/net/http/python-versions.json', 'fixtures-key')]:
            with patch.object(ci, 'digest', side_effect=lambda p: 'changed' if str(p).endswith(name) else actual(p)):
                self.assertNotEqual(a[field], plan()[field])

    def test_native_host_rejects_architecture_mismatch(self):
        with patch.object(ci.platform, 'system', return_value='Darwin'), patch.object(ci.platform, 'machine', return_value='x86_64'):
            with self.assertRaisesRegex(RuntimeError, 'host mismatch'): ci.host_identity()


class RestoreTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.plan = plan(); self.prefix = self.root / self.plan['prefix']; self.prefix.mkdir(parents=True)
        self.manifest = self.prefix / 'VOID-HTTP-DEPENDENCIES.json'
        self.manifest.write_text(json.dumps({'platform': 'Linux', 'architecture': 'x86_64', 'files': {'lib/x.a': 'digest'}}))
        self.seal = self.prefix / 'VOID-HTTP-CI-IDENTITY.json'
        self.seal.write_text(json.dumps({'identity': self.plan['identity'], 'manifest_sha256': ci.digest(self.manifest)}))
        self.evidence = self.root / 'logs'; self.evidence.mkdir()
        self.root_patch = patch.object(ci, 'ROOT', self.root); self.root_patch.start(); self.addCleanup(self.root_patch.stop)

    def test_exact_cache_hit_verifies_manifest(self):
        with patch.object(ci, 'run') as execute:
            ci.verify_prefix(self.plan, self.evidence, cached=True)
        self.assertEqual(1, execute.call_count)
        self.assertIn('Release', execute.call_args.args[-1])

    def test_changed_manifest_rejected_before_subprocess(self):
        self.manifest.write_text(self.manifest.read_text() + '\n')
        with patch.object(ci, 'run') as execute, self.assertRaisesRegex(RuntimeError, 'identity/manifest'):
            ci.verify_prefix(self.plan, self.evidence, cached=True)
        execute.assert_not_called()

    def test_wrong_profile_identity_rejected(self):
        changed = copy.deepcopy(self.plan); changed['identity']['profile'] = 'Debug'
        with self.assertRaisesRegex(RuntimeError, 'identity/manifest'):
            ci.verify_prefix(changed, self.evidence, cached=True)

    def test_missing_seal_rejected(self):
        self.seal.unlink()
        with self.assertRaises(FileNotFoundError): ci.verify_prefix(self.plan, self.evidence, cached=True)

    def test_wrong_manifest_architecture_rejected(self):
        value = json.loads(self.manifest.read_text()); value['architecture'] = 'arm64'; self.manifest.write_text(json.dumps(value))
        with self.assertRaisesRegex(RuntimeError, 'architecture'): ci.verify_prefix(self.plan, self.evidence)

    def test_cache_miss_does_not_adopt_preexisting_prefix(self):
        with patch.object(sys, 'argv', ['http', 'bootstrap', '--evidence', str(self.evidence)]), patch.object(ci, 'make_plan', return_value=self.plan), patch.dict(os.environ, {'HTTP_DEPS_CACHE_HIT': ''}), patch.object(ci, 'run') as execute:
            with self.assertRaisesRegex(RuntimeError, 'cache miss'): ci.main()
        execute.assert_not_called()

    def test_cache_hit_never_builds_dependencies(self):
        with patch.object(sys, 'argv', ['http', 'bootstrap', '--evidence', str(self.evidence)]), patch.object(ci, 'make_plan', return_value=self.plan), patch.dict(os.environ, {'HTTP_DEPS_CACHE_HIT': 'true'}), patch.object(ci, 'run') as execute, patch.object(ci, 'fixture_preflight') as fixtures:
            ci.main()
        self.assertFalse(any('setup_http_deps.py' in str(call) for call in execute.call_args_list))
        self.assertTrue(any('setup_http_tests.py' in str(call) for call in execute.call_args_list))
        fixtures.assert_called_once()


class CertificateCacheTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name); (self.root / 'tools').mkdir()
        self.script = self.root / 'tools/setup_http_tests.py'; shutil.copy2(ROOT / 'tools/setup_http_tests.py', self.script)
        wheels = self.root / '.toolchains/http/test-python'; wheels.mkdir(parents=True)
        for source in (ROOT / '.toolchains/http/test-python').glob('*.whl'): shutil.copy2(source, wheels)
        pins = self.root / 'tests/net/http'; pins.mkdir(parents=True)
        shutil.copy2(ROOT / 'tests/net/http/python-versions.json', pins / 'python-versions.json')
        archive = self.root / '.toolchains/http/downloads/fixture.tar.gz'; archive.parent.mkdir(); archive.write_bytes(b'cache-control')
        policy = self.root / 'third_party/http-versions.json'; policy.parent.mkdir()
        policy.write_text(json.dumps({'dependencies': [{'name': 'aws-lc', 'archive': archive.name, 'sha256': ci.digest(archive)}]}))
        sdk = self.root / '.toolchains/llvm-23.1.4/void-sdk-identity.json'; sdk.parent.mkdir(); sdk.write_text('fixture sdk identity')
        self.prefix = self.root / '.toolchains/http/test-certificates'; (self.prefix / 'bin').mkdir(parents=True)
        self.binary = self.prefix / 'bin/openssl.exe'; self.binary.write_bytes(b'fixture-tool-bytes')
        self.manifest = self.prefix / 'VOID-HTTP-TEST-CERTIFICATES.json'
        self.manifest.write_text(json.dumps({'identity': {'pins_sha256': ci.digest(policy), 'recipe_sha256': ci.digest(self.script), 'sdk_sha256': ci.digest(sdk), 'platform': 'Windows', 'architecture': 'amd64', 'crt': 'MultiThreaded', 'VCToolsVersion': 'test', 'WindowsSDKVersion': 'test'}, 'files': {'bin/openssl.exe': ci.digest(self.binary)}}))

    def invoke(self):
        with patch.object(sys, 'argv', [str(self.script), '--certificates']), patch('platform.system', return_value='Windows'), patch('platform.machine', return_value='AMD64'), patch.dict(os.environ, {'VCToolsVersion': 'test', 'WindowsSDKVersion': 'test'}), patch('subprocess.run', side_effect=AssertionError('warm fixture must not rebuild')), contextlib.redirect_stdout(io.StringIO()):
            with self.assertRaises(SystemExit) as result: runpy.run_path(str(self.script), run_name='__main__')
        return result.exception.code

    def test_matching_fixture_skips_build_and_removes_obsolete_import(self):
        old = self.root / '.toolchains/http/test-python/site-packages/obsolete.py'; old.parent.mkdir(); old.write_text('obsolete')
        self.assertEqual(0, self.invoke()); self.assertFalse(old.exists())

    def test_corrupted_certificate_binary_rejected(self):
        self.binary.write_bytes(b'corrupted')
        self.assertIn('digest mismatch', self.invoke())

    def test_changed_certificate_identity_rejected(self):
        self.script.write_text(self.script.read_text() + '\n')
        self.assertIn('identity mismatch', self.invoke())


if __name__ == '__main__':
    unittest.main(verbosity=2)
