"""Ensure isolated crashes cannot be mistaken for successful exhaustion probes."""
import json
import subprocess
import tempfile
import unittest
from pathlib import Path, PureWindowsPath
from run import classify, cmake_path_definition, STACK_BYTES


class CrashEvidenceTests(unittest.TestCase):
    def evidence(self, windows=False):
        control = dict(event='stack_control', requested_bytes=STACK_BYTES, actual_bytes=STACK_BYTES,
                       stack_low=524288, stack_high=524288 + STACK_BYTES,
                       control='beginthreadex_reservation' if windows else 'pthread_explicit_mapping',
                       guard_low=0, guard_high=524288, alternate_signal_stack_bytes=65536,
                       exception_guarantee_request=65536)
        ready = dict(event='ready', depth=16, static_bytes=65536, slot_bytes=32, frame_bytes=32,
                     elements_per_home=(65536 - 160) // 32)
        return '\n'.join(json.dumps(x) for x in (control, ready))

    def outcome(self, error, windows=False, code=90, stdout=None):
        return classify(code, self.evidence(windows) if stdout is None else stdout,
                        json.dumps(error), 16, 65576, 64, 'Windows' if windows else 'Linux')[0]

    def test_posix_requires_fault_inside_recorded_guard(self):
        error = dict(event='stack_overflow', evidence='fault_in_low_guard', signal='SIGSEGV', fault_address='0x100')
        self.assertEqual(self.outcome(error), 'expected_overflow')
        error['fault_address'] = '0x90000'
        self.assertEqual(self.outcome(error), 'unexpected')

    def test_windows_requires_actual_stack_exception(self):
        error = dict(event='stack_overflow', evidence='windows_exception', exception_code=0xC00000FD)
        self.assertEqual(self.outcome(error, windows=True), 'expected_overflow')
        error['exception_code'] = 0xC0000005
        self.assertEqual(self.outcome(error, windows=True), 'unexpected')

    def test_signal_or_exit_alone_is_not_evidence(self):
        error = dict(event='stack_overflow', evidence='fault_in_low_guard', signal='SIGSEGV', fault_address='0x100')
        self.assertEqual(self.outcome(error, code=-11), 'unexpected')
        self.assertEqual(self.outcome(error, stdout=''), 'unexpected')
        self.assertEqual(self.outcome(error, stdout='not JSON'), 'unexpected')

    def test_oversized_claim_cannot_hide_setup_failure(self):
        output = self.evidence().replace('"actual_bytes": 1048576', '"actual_bytes": 8388608')
        error = dict(event='stack_overflow', evidence='fault_in_low_guard', signal='SIGBUS', fault_address='0x100')
        self.assertEqual(self.outcome(error, stdout=output), 'unexpected')

    def test_cleanup_failure_cannot_pass_after_successful_collection(self):
        start = self.evidence().replace('"depth": 16', '"depth": 1')
        passed = dict(event='passed', live_frames=1, balanced_cleanup=True, observed_frames=1,
                      min_stride_bytes=0, max_stride_bytes=0)
        cleanup = [dict(event='cleanup', operation=name, result=0, errno=0) for name in
                   ('sigaltstack_disable', 'sigaltstack_query_disabled', 'munmap_alternate')]
        output = start + '\n' + '\n'.join(json.dumps(x) for x in [*cleanup, passed])
        self.assertEqual(classify(0, output, '', 1, 65560, 64, 'Linux')[0], 'pass')
        self.assertEqual(classify(70, output, 'munmap failed: errno=22', 1, 65560, 64, 'Linux')[0], 'unexpected')
        cleanup[-1].update(result=-1, errno=22)
        output = start + '\n' + '\n'.join(json.dumps(x) for x in [*cleanup, passed])
        self.assertEqual(classify(0, output, '', 1, 65560, 64, 'Linux')[0], 'unexpected')
        output = start + '\n' + json.dumps(passed)
        self.assertEqual(classify(0, output, '', 1, 65560, 64, 'Linux')[0], 'unexpected')

    def test_windows_cache_paths_do_not_introduce_cmake_escapes(self):
        path = PureWindowsPath(r'D:\a\Void.Language\build with spaces\O0\objects')
        self.assertEqual(cmake_path_definition('PROBE_OBJECT_DIR', path),
                         '-DPROBE_OBJECT_DIR:PATH=D:/a/Void.Language/build with spaces/O0/objects')
        # Exercise CMake's second parsing boundary on any host. This is not
        # Windows compilation or execution: it reproduces the path escape only.
        with tempfile.TemporaryDirectory() as scratch:
            script = Path(scratch) / 'path.cmake'
            script.write_text('cmake_language(EVAL CODE "set(object \\"${PROBE_OBJECT_DIR}/frame-64.o\\")")\n',
                              encoding='utf-8')
            unsafe = subprocess.run(['cmake', f'-DPROBE_OBJECT_DIR={path}', '-P', str(script)],
                                    capture_output=True, text=True)
            safe = subprocess.run(['cmake', cmake_path_definition('PROBE_OBJECT_DIR', path), '-P', str(script)],
                                  capture_output=True, text=True)
            self.assertNotEqual(unsafe.returncode, 0)
            self.assertIn('Invalid character escape', unsafe.stderr)
            self.assertEqual(safe.returncode, 0, safe.stderr)


if __name__ == '__main__':
    unittest.main()
