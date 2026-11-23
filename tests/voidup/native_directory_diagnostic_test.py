"""Focused instrumentation checks, not a native macOS qualification."""
import ast
import errno
import json
import os
from pathlib import Path
import subprocess
import tempfile
import types
import unittest
from unittest.mock import patch

import native_directory_diagnostic as diagnostic

ROOT = Path(__file__).resolve().parents[2]
CPP = r'''
#include <filesystem>
#include <iostream>
#include "src/runtime/host_directory.cpp"
extern "C" int __real_openat(int,const char*,int,...);
extern "C" int __real_fstatat(int,const char*,struct stat*,int);
extern "C" ssize_t __real_write(int,const void*,size_t);
extern "C" int __wrap_openat(int fd,const char* name,int flags,...) {
    if(std::getenv("SWAP_COMPONENT") && std::string(name)=="victim") {
        auto parent=std::filesystem::path(std::getenv("HOME"));
        std::filesystem::rename(parent/name,parent/"retired");
        std::filesystem::create_directory_symlink(std::getenv("OUTSIDE"),parent/name);
    }
    return __real_openat(fd,name,flags);
}
extern "C" int __wrap_fstatat(int fd,const char* name,struct stat* info,int flags) {
    if(std::getenv("FAIL_METADATA")){errno=EACCES;return -1;}
    return __real_fstatat(fd,name,info,flags);
}
extern "C" ssize_t __wrap_write(int fd,const void* data,size_t size) {
    if(fd==2 && std::getenv("FAIL_DIAGNOSTIC_WRITE")){errno=EIO;return -1;}
    return __real_write(fd,data,size);
}
int main(int argc,char** argv) {
    vd::host::Result result;auto held=vd::host::root(argv[1],true,false,result);
    auto saved=errno;
    std::cout<<"{\"kind\":"<<result.kind<<",\"code\":"<<result.code<<",\"errno\":"<<saved<<"}\n";
}
'''


class NativeDiagnostic(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.scratch = tempfile.TemporaryDirectory(prefix='voidup-first-run-diagnostic-')
        cls.addClassCleanup(cls.scratch.cleanup)
        cls.work = Path(cls.scratch.name).resolve()
        source = cls.work / 'probe.cpp'; source.write_text(CPP)
        cls.binaries = {}
        sdk = ROOT / '.toolchains/llvm-23.1.4/bin'
        for enabled in (False, True):
            binary = cls.work / ('on' if enabled else 'off')
            subprocess.run([str(sdk / 'clang++'), '--no-default-config', '-std=c++20',
                '-ffunction-sections', '-fdata-sections', '-I' + str(ROOT), '-I' + str(ROOT / 'include'),
                '-I' + str(ROOT / 'src/runtime'), *(['-DVOIDUP_DIRECTORY_DIAGNOSTIC=1'] if enabled else []),
                str(source), str(ROOT / 'src/runtime/host_error.cpp'), '--ld-path=' + str(sdk / 'ld.lld'),
                '-Wl,--gc-sections', '-Wl,--wrap=openat', '-Wl,--wrap=fstatat', '-Wl,--wrap=write',
                '-o', str(binary)], check=True, capture_output=True)
            cls.binaries[enabled] = binary

    def setUp(self):
        self.home = self.work / diagnostic.HOME_LEAF
        self.home.mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=self.home)
        self.addCleanup(self.temp.cleanup)
        self.case = Path(self.temp.name)
        self.evidence = self.case / 'evidence'; self.evidence.mkdir()
        self.outside = self.case / 'DO_NOT_LOG_CREDENTIAL_TOKEN'; self.outside.mkdir()
        self.link = self.case / 'linked'; self.link.symlink_to(self.outside, target_is_directory=True)

    def run_probe(self, requested, enabled=True, **environment):
        result = subprocess.run([self.binaries[enabled], requested], text=True, capture_output=True, check=True,
            env=os.environ | {'HOME': str(self.home), 'OUTSIDE': str(self.outside)} | environment)
        marker = [json.loads(line[len(diagnostic.MARKER):]) for line in result.stderr.splitlines() if line.startswith(diagnostic.MARKER)]
        return json.loads(result.stdout), marker, result.stderr

    def retained(self, stderr):
        diagnostic.retain(stderr.encode(), diagnostic.begin(self.home), self.evidence)
        return json.loads((self.evidence / 'native-directory-failure.json').read_text())

    def test_actual_request_error_and_flags_preserved(self):
        requested = self.link / 'Library'
        original, none, quiet = self.run_probe(requested, False)
        result, records, stderr = self.run_probe(requested)
        self.assertEqual(original, result); self.assertEqual(result['code'], errno.ENOTDIR)
        self.assertEqual((none, quiet), ([], ''))
        record = records[0]
        self.assertEqual((record['requested'], record['operation'], record['component']), (str(requested), 'openat', 'linked'))
        self.assertTrue(record['open_flags'] & os.O_NOFOLLOW); self.assertTrue(record['open_flags'] & os.O_DIRECTORY)
        self.assertEqual(record['before']['type'], 'symlink')
        report = self.retained(stderr)
        self.assertEqual(report['captures'][0]['runtime'], record)
        self.assertNotIn('DO_NOT_LOG_CREDENTIAL_TOKEN', stderr + json.dumps(report))
        self.assertEqual(list(self.outside.iterdir()), [])

    def test_missing_and_non_directory_components(self):
        file = self.case / 'file'; file.write_text('preserve')
        for parent, code, classification in [('missing', errno.ENOENT, 'missing_component_observed'),
                                              ('file', errno.ENOTDIR, 'non_directory_component_observed')]:
            with self.subTest(parent=parent):
                result, records, stderr = self.run_probe(self.case / parent / 'leaf')
                self.assertEqual(result['code'], code); self.assertEqual(records[0]['errno'], code)
                self.assertEqual(self.retained(stderr)['captures'][0]['classification'], classification)
        self.assertEqual(file.read_text(), 'preserve')

    def test_mkdir_failure_is_distinct(self):
        result, records, _ = self.run_probe(self.link)
        self.assertEqual(result['code'], errno.EEXIST)
        self.assertEqual(records[0]['operation'], 'mkdirat'); self.assertTrue(self.link.is_symlink())

    def test_component_replacement_is_observed(self):
        victim = self.home / 'victim'; victim.mkdir()
        self.addCleanup(lambda: (self.home / 'retired').rmdir())
        self.addCleanup(victim.unlink)
        result, records, stderr = self.run_probe(victim / 'leaf', SWAP_COMPONENT='1')
        self.assertEqual(result['code'], errno.ENOTDIR)
        self.assertEqual((records[0]['before']['type'], records[0]['after']['type']), ('directory', 'symlink'))
        self.assertEqual(self.retained(stderr)['captures'][0]['classification'], 'change_observed_across_failed_syscall')

    def test_success_and_out_of_scope_are_quiet(self):
        result, records, stderr = self.run_probe(self.case / 'created')
        self.assertEqual(result['kind'], 0); self.assertEqual((records, stderr), ([], ''))
        result, records, stderr = self.run_probe(self.link / 'leaf', HOME='/home/unrelated-person')
        self.assertEqual(result['code'], errno.ENOTDIR); self.assertEqual((records, stderr), ([], ''))

    def test_diagnostic_io_errors_preserve_original_errno(self):
        original, _, _ = self.run_probe(self.link / 'leaf', False)
        for name in ['FAIL_METADATA', 'FAIL_DIAGNOSTIC_WRITE']:
            with self.subTest(name=name):
                result, _, _ = self.run_probe(self.link / 'leaf', **{name: '1'})
                self.assertEqual(result, original)

    def test_missing_capture_never_reconstructs_a_path(self):
        report = self.retained('{"operation":"io.create_directory_handle","native_code":20}\n')
        self.assertEqual(report['status'], 'runtime_capture_absent'); self.assertEqual(report['captures'], [])
        secret = diagnostic.MARKER + json.dumps(dict(schema=1, requested='DO_NOT_LOG_CREDENTIAL_TOKEN'))
        report = self.retained(secret)
        self.assertEqual(report['status'], 'capture_rejected_by_privacy_scope')
        self.assertNotIn('DO_NOT_LOG_CREDENTIAL_TOKEN', json.dumps(report))

    def test_actual_unexpected_request_is_not_discarded(self):
        file = self.work / 'unexpected-file'; file.write_text('preserve')
        self.addCleanup(file.unlink)
        _, records, stderr = self.run_probe(file / 'leaf')
        report = self.retained(stderr)
        self.assertEqual(records[0]['requested'], str(file / 'leaf'))
        self.assertEqual(report['captures'][0]['runtime'], records[0])
        self.assertEqual(report['captures'][0]['replay'], [])

    def test_post_observation_error_retains_actual_capture(self):
        _, records, stderr = self.run_probe(self.link / 'leaf')
        with patch.object(diagnostic, 'inspect', side_effect=PermissionError(errno.EACCES, 'not logged')):
            report = self.retained(stderr)
        self.assertEqual(report['captures'][0]['runtime'], records[0])
        self.assertEqual(report['collection_error']['errno'], errno.EACCES)

    def test_standard_var_classification_is_only_a_model(self):
        meta = dict(errno=0, device=1, inode=2, uid=0, type='symlink', link_target='/private/var')
        record = dict(before=meta, after=meta, component='var', physical_parent='/', link_target='/private/var')
        self.assertEqual(diagnostic.classify(record, meta, meta), 'standard_var_alias_observed_without_identity_change')
        self.assertEqual(diagnostic.classify(record, meta, meta | {'inode': 3}), 'var_metadata_changed_between_fixture_observations')
        self.assertEqual(diagnostic.classify(record, meta, meta | {'uid': 501}), 'var_metadata_changed_between_fixture_observations')

    def test_original_fixture_command_and_failure_assertion(self):
        source = ast.parse((ROOT / 'tests/voidup/first_run.py').read_text())
        function = next(node for node in source.body if isinstance(node, ast.FunctionDef) and node.name == 'run')
        calls = []
        def invoke(argv, **kwargs):
            calls.append((argv, kwargs)); return subprocess.CompletedProcess(argv, 1, b'', b'original error\n')
        context = dict(time=types.SimpleNamespace(monotonic=lambda: 0), subprocess=types.SimpleNamespace(run=invoke),
            ROOT=ROOT, windows=True, rows=[], a=types.SimpleNamespace(isolate_producer=False, no_evidence=False, evidence=self.evidence),
            retain_directory_diagnostic=lambda *args: calls.append(('diagnostic', args)))
        exec(compile(ast.Module(body=[function], type_ignores=[]), '<original fixture run>', 'exec'), context)
        env = {'HOME': str(self.home), 'PATH': 'restricted-original-path'}
        argv = ['/bin/sh', 'Install VOID.command', '--ca-file', 'fixture-ca']
        with self.assertRaises(AssertionError):context['run'](argv, env=env, directory_diagnostic={'actual': 'context'})
        self.assertEqual(calls[0][0], argv); self.assertIs(calls[0][1]['env'], env)
        self.assertEqual(calls[1][0], 'diagnostic'); self.assertEqual(context['rows'][0]['exit'], 1)


if __name__ == '__main__':
    unittest.main(verbosity=2)
