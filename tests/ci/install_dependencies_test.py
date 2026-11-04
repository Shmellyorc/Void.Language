"""Installation-only dependency command plans; no downloads or native builds."""
import contextlib
import importlib.util
import io
import os
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, ROOT / path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


native = load('http_native_install_plan', 'tools/setup_http_deps_native.py')
smoke = load('install_dependency_plan', 'tools/ci/install_smoke.py')


def plan(system, profile='Release', install_only=False):
    with tempfile.TemporaryDirectory(prefix='void dependency commands ') as directory:
        root = Path(directory).resolve()
        (root / 'third_party').mkdir()
        (root / 'third_party/http-versions.json').write_bytes((ROOT / 'third_party/http-versions.json').read_bytes())
        (root / 'docs/net/http/evidence').mkdir(parents=True)
        for name in ('tools/setup_http_deps_native.py', 'tools/http_archive.py', 'cmake/http-psl/CMakeLists.txt'):
            target = root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes((ROOT / name).read_bytes())
        argv = ['setup_http_deps_native.py', '--profile', profile, '--jobs', '3']
        if install_only:
            argv.append('--macos-install-only')
        with patch.object(native, '__file__', str(root / 'tools/setup_http_deps_native.py')), \
                patch.object(native.platform, 'system', return_value=system), \
                patch.object(native.platform, 'platform', return_value=system + ' command fixture'), \
                patch.object(native.platform, 'machine', return_value='arm64' if system == 'Darwin' else 'AMD64'), \
                patch.dict(os.environ, {'SDKROOT': str(root), 'VSCMD_ARG_TGT_ARCH': 'x64'}), \
                patch.object(sys, 'argv', argv), patch.object(native, 'acquire'), \
                patch.object(native.tarfile, 'open'), patch.object(native.shutil, 'which', return_value='ninja'), \
                patch.object(native.subprocess, 'run', return_value=subprocess.CompletedProcess([], 0)) as execute, \
                contextlib.redirect_stdout(io.StringIO()):
            native.main()
        return [[arg.replace(str(root), '<ROOT>') for arg in call.args[0]] for call in execute.call_args_list]


def aws_configure(commands):
    return next(command for command in commands if command[:2] == ['cmake', '-S']
                and Path(command[command.index('-B') + 1]).name == 'aws-lc')


class DependencyCommandTests(unittest.TestCase):
    def test_macos_default_retains_tools_and_static_ssl_crypto(self):
        for profile in ('Release', 'Debug', 'strict'):
            with self.subTest(profile=profile):
                command = aws_configure(plan('Darwin', profile))
                self.assertIn('-DBUILD_TOOL=ON', command)
                self.assertIn('-DBUILD_SHARED_LIBS=OFF', command)
                self.assertIn('-DBUILD_LIBSSL=ON', command)
                self.assertIn('-DCMAKE_INSTALL_LIBDIR=lib', command)
                self.assertIn('-DBUILD_TESTING=OFF', command)

    def test_installation_changes_only_aws_tool_selection(self):
        for profile in ('Release', 'Debug', 'strict'):
            with self.subTest(profile=profile):
                default = plan('Darwin', profile)
                installation = plan('Darwin', profile, install_only=True)
                self.assertIn('-DBUILD_TOOL=OFF', aws_configure(installation))
                self.assertEqual(default, [[arg.replace('-DBUILD_TOOL=OFF', '-DBUILD_TOOL=ON')
                                           for arg in command] for command in installation])
                self.assertTrue(any(command[:2] == ['cmake', '--install'] and Path(command[2]).name == 'aws-lc'
                                    for command in installation))

    def test_windows_default_preserves_schannel_dependency_selection(self):
        commands = plan('Windows')
        configured = [command for command in commands if command[:2] == ['cmake', '-S']]
        self.assertEqual(len(configured), 4)
        self.assertFalse(any('aws-lc' in command[2] for command in configured))
        self.assertFalse(any(arg.startswith('-DBUILD_TOOL=') for command in commands for arg in command))

    def test_native_option_rejected_outside_macos_before_work(self):
        for system in ('Windows', 'Linux'):
            with self.subTest(system=system), patch.object(native.platform, 'system', return_value=system), \
                    patch.object(sys, 'argv', ['native', '--macos-install-only']), \
                    patch.object(native.subprocess, 'run') as execute, contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as rejected:
                    native.main()
                self.assertEqual(rejected.exception.code, 2)
                execute.assert_not_called()

    def test_macos_forwarder_preserves_explicit_and_default_arguments(self):
        for extra in ([], ['--macos-install-only']):
            with self.subTest(extra=extra), patch('platform.system', return_value='Darwin'), \
                    patch.object(sys, 'argv', ['setup_http_deps.py', '--profile', 'Release', *extra]), \
                    patch('subprocess.call', return_value=0) as execute:
                with self.assertRaises(SystemExit) as forwarded:
                    runpy.run_path(str(ROOT / 'tools/setup_http_deps.py'), run_name='__main__')
                self.assertEqual(forwarded.exception.code, 0)
                self.assertEqual(execute.call_args.args[0], [sys.executable,
                    str(ROOT / 'tools/setup_http_deps_native.py'), '--profile', 'Release', *extra])

    def test_linux_forwarder_rejects_macos_option(self):
        with patch('platform.system', return_value='Linux'), \
                patch.object(sys, 'argv', ['setup_http_deps.py', '--macos-install-only']), \
                patch('subprocess.run') as execute, contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as rejected:
                runpy.run_path(str(ROOT / 'tools/setup_http_deps.py'), run_name='__main__')
            self.assertEqual(rejected.exception.code, 2)
            execute.assert_not_called()

    def test_install_smoke_opts_in_only_on_macos(self):
        for system in ('Linux', 'Windows', 'Darwin'):
            with self.subTest(system=system), tempfile.TemporaryDirectory() as directory, \
                    patch.object(smoke, 'SYSTEM', system), patch.dict(os.environ, {
                        'RUNNER_TEMP': directory, 'INSTALL_QUALIFICATION_NAME': 'qualification'}):
                q = smoke.Qualification()
                q.http = Path(directory) / 'fresh-http-prefix'
                with patch.object(q, 'run') as execute, patch.object(smoke.shutil, 'copy2'):
                    q.dependencies()
                bootstrap = execute.call_args_list[0].args[1]
                expected = [sys.executable, 'tools/setup_http_deps.py', '--profile', 'Release', '--jobs', '3']
                if system == 'Darwin':
                    expected.append('--macos-install-only')
                self.assertEqual(bootstrap, expected)
                self.assertEqual(execute.call_args_list[1].args[0], 'http-verify')

    def test_required_macos_archive_and_header_contract(self):
        consumer = (ROOT / 'cmake/HttpNativeDependencies.cmake').read_text()
        for resource in ('${VOID_HTTP_DEPS_ROOT}/lib/libssl.a', '${VOID_HTTP_DEPS_ROOT}/lib/libcrypto.a',
                         '${VOID_HTTP_DEPS_ROOT}/include'):
            self.assertIn(resource, consumer)
        for caller in ('tools/ci/http_bootstrap.py', 'tools/setup_http_tests.py'):
            self.assertNotIn('--macos-install-only', (ROOT / caller).read_text())


if __name__ == '__main__':
    unittest.main(verbosity=2)
