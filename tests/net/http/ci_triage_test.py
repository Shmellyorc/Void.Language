"""Regressions for CI 41307437931: Git bytes, archive identity and curl scope."""
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
from http_archive import acquire


def digest(data):
    return hashlib.sha256(data).hexdigest()


class CheckoutTests(unittest.TestCase):
    def test_every_reviewed_blob_survives_windows_checkout_filter(self):
        policy = json.loads((ROOT / 'third_party/curl/VOID-PROVENANCE.json').read_text())
        expected = {'third_party/curl/' + p: h for p, h in policy['pristine_sha256'].items()}
        for p, correction in policy['local_corrections'].items():
            expected['third_party/curl/' + p] = correction['corrected_sha256']
        for review in policy['approved_corrections'].values():
            expected[review['review_diff']] = review['sha256']
        inputs = ''.join(f'HEAD:{p} {p}\n' for p in expected).encode()
        filtered = subprocess.check_output(['git', '-c', 'core.autocrlf=true', 'cat-file', '--batch', '--filters'],
                                           cwd=ROOT, input=inputs)
        stream = io.BytesIO(filtered)
        for path, sha in expected.items():
            with self.subTest(path=path):
                header = stream.readline().split()
                self.assertEqual(b'blob', header[1], header)
                self.assertEqual(sha, digest(stream.read(int(header[2]))))
                self.assertEqual(b'\n', stream.read(1))
                self.assertEqual(sha, digest((ROOT / path).read_bytes()))
        self.assertFalse(stream.read())
        print(f'Windows checkout filters: {len(expected)} source/review blobs verified')


class WindowsPathTests(unittest.TestCase):
    def test_static_only_cares_import_in_both_profiles(self):
        # CMake/platform simulation: verified fixture files, not MSVC binaries.
        for profile in ('Debug', 'Release'):
            with self.subTest(profile=profile), tempfile.TemporaryDirectory(prefix='VOID cares import ') as directory:
                root = Path(directory); prefix = root / 'prefix'; (prefix / 'lib').mkdir(parents=True)
                files = {}
                for name in ('cares.lib', 'nghttp2.lib', 'psl.lib', 'zs.lib' if profile == 'Release' else 'zsd.lib'):
                    path = prefix / 'lib' / name; path.write_bytes(b'fixture static archive')
                    files['lib/' + name] = digest(path.read_bytes())
                manifest = dict(profile=profile, platform='Windows', architecture='x86_64',
                                pins_sha256=digest((ROOT / 'third_party/http-versions.json').read_bytes()),
                                files=files, selected_dependencies=[])
                (prefix / 'VOID-HTTP-DEPENDENCIES.json').write_text(json.dumps(manifest))
                (root / 'CMakeLists.txt').write_text(
                    'cmake_minimum_required(VERSION 3.24)\nproject(cares_import NONE)\n'
                    'set(WIN32 TRUE)\nset(APPLE FALSE)\nset(CMAKE_SYSTEM_NAME Windows)\n'
                    'set(CMAKE_SYSTEM_PROCESSOR AMD64)\n' +
                    f'set(CMAKE_BUILD_TYPE {profile})\nset(Python3_EXECUTABLE "{Path(sys.executable).as_posix()}")\n' +
                    f'set(VOID_HTTP_DEPS_ROOT "{prefix.as_posix()}")\n' +
                    f'set(CMAKE_CURRENT_FUNCTION_LIST_DIR "{(ROOT / "cmake").as_posix()}")\n' +
                    f'include("{(ROOT / "cmake/HttpNativeDependencies.cmake").as_posix()}")\n' +
                    'void_import_native_http_dependencies()\n' +
                    'file(WRITE "${CMAKE_BINARY_DIR}/selected.txt" "${CARES_LIBRARY}")\n')
                result = subprocess.run(['cmake', '-G', 'Ninja' if os.name == 'nt' else 'Unix Makefiles', '-S', str(root), '-B', str(root / 'build')],
                                        capture_output=True, text=True)
                self.assertEqual(0, result.returncode, result.stdout + result.stderr)
                self.assertEqual((prefix / 'lib/cares.lib').as_posix(), (root / 'build/selected.txt').read_text())
                (prefix / 'lib/cares.lib').unlink()
                result = subprocess.run(['cmake', '-S', str(root), '-B', str(root / 'build')],
                                        capture_output=True, text=True)
                self.assertNotEqual(0, result.returncode)
                self.assertIn('digest mismatch: lib/cares.lib', result.stderr)

    def test_psl_native_separators_and_spaces(self):
        with tempfile.TemporaryDirectory(prefix='VOID PSL paths ') as temporary:
            root = Path(temporary); source = root / 'a source'; (source / 'src').mkdir(parents=True)
            (source / 'include').mkdir()
            for name in ('psl.c', 'lookup_string_in_fixed_set.c'):
                (source / 'src' / name).write_text('/* fixture */')
            (source / 'include/libpsl.h').write_text('/* fixture */')
            (source / 'src/psl-make-dafsa').write_text('import pathlib,sys\npathlib.Path(sys.argv[-1]).write_text("/* fixture */")\n')
            data = root / 'a data.dat'; data.write_text('com\n')
            # Native backslashes reach CMake exactly as in the failed -D values.
            args = ['cmake', '-S', str(ROOT / 'cmake/http-psl'), '-B', str(root / 'build'),
                    '-DPSL_SOURCE=' + str(source).replace('/', chr(92)),
                    '-DPSL_DATA=' + str(data).replace('/', chr(92))]
            result = subprocess.run(args, capture_output=True, text=True)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertTrue((root / 'build/suffixes_dafsa.h').exists())


class FeatureTests(unittest.TestCase):
    def configure(self, absent=None, zero=False, missing=False):
        with tempfile.TemporaryDirectory(prefix='VOID curl feature scope ') as directory:
            root = Path(directory); child = root / 'child'; child.mkdir()
            features = ['USE_APPLE_IDN', 'USE_APPLE_SECTRUST', 'USE_OPENSSL']
            header = ''.join(f'#define {f} {0 if zero and f == absent else 1}\n'
                             for f in features if zero or f != absent)
            child.joinpath('CMakeLists.txt').write_text(
                'add_library(provider INTERFACE)\n' +
                ('' if missing else f'file(WRITE "${{CMAKE_CURRENT_BINARY_DIR}}/curl_config.h" "{header}")\n') +
                '\n'.join(f'set({f} ON)' for f in features))
            root.joinpath('CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.24)\nproject(curl_scope NONE)\n' +
                f'include("{(ROOT / "cmake/CurlProviderFeatures.cmake").as_posix()}")\n' +
                '\n'.join(f'set({f} {"ON" if absent or missing else "OFF"})' for f in features) +
                '\nadd_subdirectory(child)\nvoid_validate_curl_apple_features(provider)\n')
            return subprocess.run(['cmake', '-S', str(root), '-B', str(root / 'build')],
                                  capture_output=True, text=True)

    def test_enabled_generated_features_pass_despite_parent_scope(self):
        result = self.configure()
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)

    def test_each_missing_or_disabled_native_feature_is_rejected(self):
        for feature in ['USE_APPLE_IDN', 'USE_APPLE_SECTRUST', 'USE_OPENSSL']:
            for zero in [False, True]:
                with self.subTest(feature=feature, zero=zero):
                    result = self.configure(feature, zero)
                    self.assertNotEqual(0, result.returncode)
                    self.assertIn(feature + ' absent', result.stderr)

    def test_missing_configuration_fails_closed(self):
        result = self.configure(missing=True)
        self.assertNotEqual(0, result.returncode)
        self.assertIn('generated configuration missing', result.stderr)


class AcquisitionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name); self.archive = self.root / 'cache/dependency.tar.gz'
        self.receipt = self.root / 'evidence'
        self.body = b'pinned release archive bytes'
        owner = self
        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *args): pass
            def do_GET(self):
                if self.path == '/redirect':
                    self.send_response(302); self.send_header('Location', '/good'); self.end_headers(); return
                self.send_response(404 if self.path == '/missing' else 200)
                self.send_header('Content-Type', 'application/octet-stream'); self.end_headers()
                self.wfile.write(owner.body if self.path == '/good' else b'wrong bytes from server')
        self.server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        thread = threading.Thread(target=self.server.serve_forever, daemon=True); thread.start()
        self.addCleanup(self.server.server_close); self.addCleanup(self.server.shutdown)
        self.pin = dict(url=f'http://127.0.0.1:{self.server.server_port}/good', sha256=digest(self.body))

    def record(self):
        return json.loads((self.receipt / (self.archive.name + '.acquisition.json')).read_text())

    def test_verified_bytes_promoted_and_cache_reverified(self):
        self.assertEqual(self.archive, acquire(self.pin, self.archive, self.receipt))
        self.assertEqual(self.body, self.archive.read_bytes())
        self.assertEqual(200, self.record()['status'])
        self.assertEqual(digest(self.body), self.record()['actual_sha256'])
        self.pin['url'] = 'http://127.0.0.1:1/no-network-on-hit'
        acquire(self.pin, self.archive, self.receipt)
        self.assertTrue(self.record()['cached'])

    def test_successful_http_with_wrong_bytes_never_enters_cache(self):
        self.pin['url'] = self.pin['url'].replace('/good', '/bad')
        with self.assertRaisesRegex(RuntimeError, 'verification failed'):
            acquire(self.pin, self.archive, self.receipt)
        self.assertFalse(self.archive.exists())
        record = self.record(); rejected = (self.receipt / record['rejected_file']).read_bytes()
        self.assertEqual(200, record['status'])
        self.assertEqual(b'wrong bytes from server', rejected)
        self.assertEqual(digest(rejected), record['actual_sha256'])
        self.assertEqual(len(rejected), record['bytes'])
        self.assertEqual(self.pin['sha256'], record['expected_sha256'])

    def test_http_error_body_and_status_retained(self):
        self.pin['url'] = self.pin['url'].replace('/good', '/missing')
        with self.assertRaisesRegex(RuntimeError, 'verification failed'):
            acquire(self.pin, self.archive, self.receipt)
        self.assertEqual(404, self.record()['status'])
        self.assertFalse(self.archive.exists())
        self.assertTrue((self.receipt / self.record()['rejected_file']).exists())

    def test_corrupted_cache_rejected_without_replacement(self):
        self.archive.parent.mkdir(); self.archive.write_bytes(b'corrupted cache')
        with self.assertRaisesRegex(RuntimeError, 'verification failed'):
            acquire(self.pin, self.archive, self.receipt)
        self.assertEqual(b'corrupted cache', self.archive.read_bytes())
        self.assertTrue(self.record()['cached'])
        self.assertEqual('rejected', self.record()['result'])

    def test_network_failure_does_not_report_a_stale_partial_as_response(self):
        self.archive.parent.mkdir()
        partial = self.archive.with_name(self.archive.name + '.part')
        partial.write_bytes(b'previous rejected attempt')
        with patch('http_archive.urllib.request.urlopen', side_effect=OSError('connection failed')):
            with self.assertRaisesRegex(OSError, 'connection failed'):
                acquire(self.pin, self.archive, self.receipt)
        self.assertFalse(self.archive.exists())
        self.assertFalse(partial.exists())
        self.assertEqual('rejected', self.record()['result'])
        self.assertNotIn('rejected_file', self.record())

    def test_redirect_records_final_location_and_verifies_bytes(self):
        self.pin['url'] = self.pin['url'].replace('/good', '/redirect')
        acquire(self.pin, self.archive, self.receipt)
        self.assertTrue(self.record()['effective_url'].endswith('/good'))
        self.assertEqual(self.pin['url'], self.record()['url'])


if __name__ == '__main__':
    unittest.main(verbosity=2)
