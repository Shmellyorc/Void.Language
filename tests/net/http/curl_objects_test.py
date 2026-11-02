"""Compile-policy and real COFF import regression; no Windows runtime required."""
import json
import hashlib
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
SDK = ROOT / '.toolchains/llvm-23.1.4/bin'
EXT = '.exe' if os.name == 'nt' else ''


def run(argv):
    return subprocess.run(list(map(str, argv)), capture_output=True, text=True)


class CurlObjectsTests(unittest.TestCase):
    def test_static_and_object_library_compilation_policy(self):
        header = next((ROOT / '.toolchains/http/prefix').glob('*/*/include/libpsl.h'))
        manifest = json.loads((header.parent.parent / 'VOID-HTTP-DEPENDENCIES.json').read_text())
        self.assertEqual(manifest['pins_sha256'], hashlib.sha256((ROOT / 'third_party/http-versions.json').read_bytes()).hexdigest())
        self.assertEqual(manifest['files']['include/libpsl.h'], hashlib.sha256(header.read_bytes()).hexdigest())
        cc = SDK / ('clang' + EXT)
        configuration_cc = SDK / 'clang-cl.exe' if os.name == 'nt' else cc
        for shared in (False, True):
            with self.subTest(object_library=shared), tempfile.TemporaryDirectory(prefix='VOID curl COFF ') as directory:
                root = Path(directory); (root / 'vtls').mkdir()
                (root / 'vtls/keylog.c').write_text('int upstream_keylog;\n')
                # Header-only SDK stand-ins: this tests COFF imports, not the CRT.
                (root / 'stdio.h').write_text('typedef struct FILE FILE;\n')
                (root / 'time.h').write_text('typedef long long time_t;\n')
                (root / 'libpsl.h').write_bytes(header.read_bytes())
                (root / 'probe.c').write_text('''#include "libpsl.h"
int entry(void) {
  const psl_ctx_t *p = psl_builtin();
  int r = psl_check_version_number(0) + psl_builtin_outdated();
  r += psl_is_cookie_domain_acceptable(p, "a.com", "com");
  psl_free(psl_latest("unused")); return r;
}
''')
                targets = ('add_library(libcurl_object OBJECT probe.c vtls/keylog.c)\n'
                           'add_library(libcurl_static STATIC $<TARGET_OBJECTS:libcurl_object>)\n' if shared else
                           'add_library(libcurl_static STATIC probe.c vtls/keylog.c)\n')
                (root / 'CMakeLists.txt').write_text(
                    'cmake_minimum_required(VERSION 3.24)\nproject(curl_objects C)\n'
                    'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n' + targets +
                    'set(WIN32 TRUE)\nset(VOID_SANITIZERS ON)\n' +
                    f'include("{(ROOT / "cmake/CurlProviderObjects.cmake").as_posix()}")\n'
                    'void_configure_curl_objects(HTTP)\n')
                result = run(['cmake', '-G', 'Ninja' if os.name == 'nt' else 'Unix Makefiles',
                              '-S', root, '-B', root / 'build', '-DCMAKE_C_COMPILER=' + configuration_cc.as_posix(),
                              '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded'])
                self.assertEqual(0, result.returncode, result.stdout + result.stderr)
                commands = json.loads((root / 'build/compile_commands.json').read_text())
                probe = next(c['command'] for c in commands if c['file'].endswith('probe.c'))
                for macro in ('PSL_STATIC', 'NGHTTP2_STATICLIB', 'CARES_STATICLIB', '_WIN32_WINNT=0x0A00'):
                    self.assertIn('-D' + macro, probe)
                self.assertIn('-fsanitize=address,undefined', probe)
                self.assertFalse(any(c['file'].replace('\\', '/').endswith('/vtls/keylog.c') for c in commands))
                self.assertEqual(1, sum(c['file'].endswith('curl_keylog_disabled.c') for c in commands))
                definitions = [x for x in shlex.split(probe, posix=os.name != 'nt') if x.startswith('-D')]
                # The real pinned public header creates __imp_psl_* without the
                # static definition. Use six exact declarations from the failure.
                for negative in (False, True):
                    selected = [x for x in definitions if not (negative and x == '-DPSL_STATIC')]
                    obj = root / ('negative.obj' if negative else 'fixed.obj')
                    result = run([cc, '--target=x86_64-pc-windows-msvc', '-fms-runtime-lib=static',
                                  '-I', root, *selected, '-c', root / 'probe.c', '-o', obj])
                    self.assertEqual(0, result.returncode, result.stderr)
                    symbols = run([SDK / ('llvm-nm' + EXT), '--undefined-only', obj])
                    self.assertEqual(0, symbols.returncode, symbols.stderr)
                    imports = [line for line in symbols.stdout.splitlines() if '__imp_psl_' in line]
                    self.assertEqual(6 if negative else 0, len(imports), symbols.stdout)
                    self.assertEqual(6, len(symbols.stdout.splitlines()), symbols.stdout)
        print('Both curl layouts: static definitions, sanitizers and keylog ownership; six COFF import negative controls verified')


if __name__ == '__main__': unittest.main(verbosity=2)
