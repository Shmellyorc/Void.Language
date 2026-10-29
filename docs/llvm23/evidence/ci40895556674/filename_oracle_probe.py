"""Exercise the real kernel fixture's Apple rejection oracle on Linux.

python3 docs/llvm23/evidence/ci40895556674/filename_oracle_probe.py build/llvm23-release
Only the two fixture syscalls and its Apple branch selector are changed in a
temporary source copy. Runtime, directory enumeration and validation stay real.
Injected results are oracle coverage, never native macOS observations.
"""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[4]
build = Path(sys.argv[1]).resolve()
sdk = root / '.toolchains/llvm-23.1.4/bin'
source = (root / 'tests/io/kernel.cpp').read_text(encoding='utf-8')
seam = r'''
// Test-only fixture seam; no Runtime syscall is replaced.
#include <cstdlib>
#include <cstring>
int oracle_open(const char* path, int, unsigned) {
    const char* mode = std::getenv("VOID_FILENAME_ORACLE");
    if (!std::strcmp(mode, "creation_error")) { errno = EACCES; return -1; }
    if (!std::strcmp(mode, "directory_change")) {
        auto other = std::filesystem::path(path).parent_path() / "unexpected-entry";
        int fd = ::open(other.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (fd < 0 || ::close(fd) != 0) std::abort();
    }
    errno = EILSEQ;
    return -1;
}
int oracle_lstat(const char* path, struct stat* info) {
    const char* mode = std::getenv("VOID_FILENAME_ORACLE");
    if (!std::strcmp(mode, "success")) { info->st_mode = S_IFREG; return 0; }
    if (!std::strcmp(mode, "eilseq")) { errno = EILSEQ; return -1; }
    if (!std::strcmp(mode, "unrelated_error")) { errno = EACCES; return -1; }
    return ::lstat(path, info);  // Real ENOENT: no malformed entry was created.
}
'''


def instrument(text):
    selector = '#ifdef __APPLE__\n        // APFS'
    creation = '::open(invalid.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600)'
    assert text.count(selector) == text.count(creation) == 1
    assert text.count('::lstat(invalid.c_str(),') == 2
    return text.replace(selector, '#if 1\n        // APFS').replace(creation,
        'oracle_open(invalid.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600)').replace(
        '::lstat(invalid.c_str(),', 'oracle_lstat(invalid.c_str(),').replace('namespace {', seam + '\nnamespace {', 1)


results = []
with tempfile.TemporaryDirectory(prefix='VOID filename oracle ') as directory:
    work = Path(directory)
    transient = work / 'kernel.cpp'
    transient.write_text(instrument(source), encoding='utf-8')
    executable = work / 'kernel-oracle'
    command = [sdk / 'clang++', '-std=c++20', '-O2', '-fno-rtti', '-Wall', '-Wextra', '-Wpedantic',
               '-I' + str(root / 'include'), transient, build / 'libvoid_runtime.a',
               build / 'libvoid_native_fixture.a', '-ldl', '-pthread',
               '--ld-path=' + str(sdk / 'ld.lld'), '-o', executable]
    subprocess.run(list(map(str, command)), check=True)
    cases = [('absent', 0, ''), ('eilseq', 0, ''),
             ('success', 1, 'lookup must report absence or invalid encoding'),
             ('unrelated_error', 1, 'lookup must report absence or invalid encoding'),
             ('creation_error', 1, 'unexpected native invalid-filename creation failure'),
             ('directory_change', 1, 'rejected filename creation changed directory entries')]
    for mode, status, diagnostic in cases:
        result = subprocess.run([str(executable), str(work / mode)], capture_output=True, text=True,
                                env={**os.environ, 'VOID_FILENAME_ORACLE': mode}, timeout=30)
        assert result.returncode == status and diagnostic in result.stderr, (mode, result)
        if status == 0:
            assert 'native filesystem kernel:' in result.stdout, result
        results.append({'injected_case': mode, 'status': result.returncode,
                        'stdout': result.stdout, 'stderr': result.stderr})
print(json.dumps({'native_host': sys.platform, 'native_macOS_observation': False,
                  'kernel_source_sha256': hashlib.sha256(source.encode()).hexdigest(),
                  'cases': results, 'passed': True}, indent=2))
