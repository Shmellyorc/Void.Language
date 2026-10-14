"""Ensure isolated crashes cannot be mistaken for successful exhaustion probes."""
import json
import unittest
from run import classify, STACK_BYTES


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


if __name__ == '__main__':
    unittest.main()
