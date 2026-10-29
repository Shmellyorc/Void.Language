"""Test CMake's real URL/hash fallback, pinned packages and required discovery.

python3 docs/llvm23/evidence/ci40895556674/dependency_probe.py
Uses the downloaded upstream archive plus the previously verified local zstd/XML
archives. Local HTTP responses inject failures; no production download is mocked.
Linux linking/API checks do not establish the Windows /MT ABI.
"""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import hashlib
import json
import re
import subprocess
import tempfile
import threading

root = Path(__file__).resolve().parents[4]
work = root / '.toolchains/ci40895556674/dependency-probe'
work.mkdir(parents=True, exist_ok=True)
work = Path(tempfile.mkdtemp(prefix='run-', dir=work))
sdk = root / '.toolchains/llvm-23.1.4/bin'
source = (root / 'cmake/dependencies/CMakeLists.txt').read_text(encoding='utf-8')
urls = ['https://zlib.net/fossils/zlib-1.3.2.tar.gz',
        'https://github.com/madler/zlib/releases/download/v1.3.2/zlib-1.3.2.tar.gz']
archive = root / '.toolchains/ci40895556674/zlib-1.3.2.tar.gz'
digest = 'bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16'
assert hashlib.sha256(archive.read_bytes()).hexdigest() == digest
assert f'URL_HASH SHA256={digest}' in source and all(url in source for url in urls)
requests = []


class ArchiveServer(BaseHTTPRequestHandler):
    def do_GET(self):
        requests.append(self.path)
        if self.path.startswith('/missing/'):
            self.send_error(404)
            return
        payload = archive.read_bytes() if self.path.startswith('/good/') else b'wrong archive bytes\n'
        self.send_response(200)
        self.send_header('Content-Length', str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def log_message(self, *_):
        pass


def run(command, log, okay=True):
    result = subprocess.run(list(map(str, command)), cwd=root, capture_output=True, text=True, timeout=240)
    log.write_text(result.stdout + result.stderr, encoding='utf-8')
    assert (result.returncode == 0) == okay, (command, log, result.returncode)
    return result.stdout + result.stderr


compiler_flags = ['-DCMAKE_C_COMPILER=' + str(sdk / 'clang'),
                  '-DCMAKE_CXX_COMPILER=' + str(sdk / 'clang++'), '-DCMAKE_BUILD_TYPE=Release',
                  '-DCMAKE_EXE_LINKER_FLAGS=--ld-path=' + str(sdk / 'ld.lld'),
                  '-DFETCHCONTENT_QUIET=OFF']
server = ThreadingHTTPServer(('127.0.0.1', 0), ArchiveServer)
thread = threading.Thread(target=server.serve_forever, daemon=True)
thread.start()
base = f'http://127.0.0.1:{server.server_port}'
cases = []
try:
    # Isolate the exact zlib declaration and settings for fast negative checks.
    zlib_only = source[:source.index('set(ZSTD_BUILD_SHARED')]
    for name, primary, secondary, success in [
            ('hash_mismatch_fallback', 'bad', 'good', True),
            ('unavailable_fallback', 'missing', 'good', True),
            ('all_corrupt_rejected', 'bad', 'also-bad', False)]:
        case = work / name
        case.mkdir(exist_ok=True)
        controlled = zlib_only.replace(urls[0], f'{base}/{primary}/zlib-1.3.2.tar.gz').replace(
            urls[1], f'{base}/{secondary}/zlib-1.3.2.tar.gz')
        (case / 'CMakeLists.txt').write_text(controlled, encoding='utf-8')
        before = len(requests)
        output = run(['cmake', '-S', case, '-B', case / 'build', *compiler_flags],
                     case / 'configure.txt', success)
        observed = requests[before:]
        assert any(path.startswith('/' + primary + '/') for path in observed), observed
        assert any(path.startswith('/' + secondary + '/') for path in observed), observed
        if primary == 'bad':
            assert digest in output and 'hash' in output.lower(), output
        cases.append({'case': name, 'configuration_succeeded': success, 'requests': observed})

    # Build all three real pinned static packages with the updated declaration.
    full = work / 'full'
    full.mkdir(exist_ok=True)
    controlled = source.replace(urls[0], f'{base}/bad/zlib-1.3.2.tar.gz').replace(
        urls[1], f'{base}/good/zlib-1.3.2.tar.gz')
    for filename in ('zstd-1.5.7.tar.gz', 'libxml2-2.15.4.tar.xz'):
        cached = root / '.toolchains/wrench-inspection' / filename
        assert cached.is_file(), cached
        controlled, count = re.subn(r'https://\S*/' + re.escape(filename), cached.as_uri(), controlled)
        assert count == 1, (filename, count)
    (full / 'CMakeLists.txt').write_text(controlled, encoding='utf-8')
    prefix = full / 'install'
    run(['cmake', '-S', full, '-B', full / 'build', *compiler_flags,
         '-DCMAKE_INSTALL_PREFIX=' + str(prefix)], full / 'configure.txt')
    run(['cmake', '--build', full / 'build', '--target', 'install', '-j3'], full / 'build.txt')
    consumer = work / 'consumer'
    run(['cmake', '-S', root / 'tests/toolchain', '-B', consumer, *compiler_flags,
         '-DCMAKE_PREFIX_PATH=' + str(prefix)], work / 'consumer-configure.txt')
    run(['cmake', '--build', consumer, '-j3'], work / 'consumer-build.txt')
    output = run(['ctest', '--test-dir', consumer, '-V'], work / 'consumer-test.txt')
    assert 'zlib headers=1.3.2 runtime=1.3.2' in output and 'LibXml2 headers=2.15.4' in output, output
    missing = []
    for package in ('ZLIB', 'zstd', 'LibXml2'):
        log = run(['cmake', '-S', root / 'tests/toolchain', '-B', work / ('missing-' + package),
                   *compiler_flags, '-DCMAKE_PREFIX_PATH=' + str(prefix),
                   '-DCMAKE_DISABLE_FIND_PACKAGE_' + package + '=TRUE'],
                  work / ('missing-' + package + '.txt'), False)
        assert package in log and 'REQUIRED' in log, log
        missing.append(package)
finally:
    server.shutdown()
    server.server_close()
    thread.join()
print(json.dumps({'native_host': 'Linux', 'zlib_sha256': digest, 'fallback_cases': cases,
                  'raw_logs': str(work),
                  'real_static_packages': ['zlib 1.3.2', 'zstd 1.5.7', 'LibXml2 2.15.4'],
                  'real_API_link_run': output, 'required_dependency_negative_checks': missing,
                  'Windows_MSVCRT_verified': False, 'passed': True}, indent=2))
