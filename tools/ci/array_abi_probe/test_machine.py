"""Independent checks of actual C++ auditor, machine/MC facts and delivery gate."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import unittest


class MachineTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.output = args.artifacts.resolve()
        cls.records = json.loads((cls.output / 'results.json').read_text())
        cls.controls = json.loads((cls.output / 'controls.json').read_text())
        assert json.loads((cls.output / 'complete.json').read_text())['passed']
        cls.reports = {}
        for path in cls.output.rglob('*.json'):
            report = json.loads(path.read_text())
            if isinstance(report, dict) and 'boundary' in report and not path.stem.startswith(('bad-caller-', 'bad-incoming-')):
                cls.reports[path] = report

    def test_delivery_is_bound_to_final_bytes(self):
        self.assertTrue(self.reports)
        for path, r in self.reports.items():
            obj = path.with_suffix('.o')
            self.assertEqual(obj.exists(), r['certified'], str(path))
            if r['certified']:
                self.assertEqual(hashlib.sha256(obj.read_bytes()).hexdigest().upper(), r['private_object_sha256'])
                self.assertEqual(r['errors'], {})

    def test_original_plan_and_machine_charge(self):
        for r in self.records:
            for key, homes in (('caller', 3), ('callee', 1)):
                functions = r[key]['functions']
                self.assertEqual(len(functions), 1)
                f = next(iter(functions.values()))
                self.assertEqual(f['P'], homes * r['count'] * 4)
                self.assertEqual(f['R'], f['P'])
                self.assertEqual(f['C'], (f['P'] + f['frame'] - f['R'] + f['E'] + 15) // 16 * 16)
                self.assertEqual(r[key]['certified'], f['C'] <= 65536)
            self.assertEqual(r['caller']['certified'], r['count'] != 5461)

    def test_independent_standard_llc_frame_measurements(self):
        # C++ profile is explicitly generic, not llc's target-dependent empty CPU.
        checks = 0
        for path, r in self.reports.items():
            if path.stem not in ('caller', 'callee', 'semantics-caller', 'semantics-callee'):
                continue
            ir = path.with_suffix('.optimized.ll')
            obj = path.with_suffix('.independent.o')
            proc = subprocess.run([str(args.llvm_bin / 'llc'), '-mcpu=generic', '-O2',
                                   '-relocation-model=pic', '-filetype=obj', str(ir), '-o', str(obj)],
                                  capture_output=True, text=True, encoding='utf-8', errors='strict', timeout=120)
            path.with_suffix('.independent.log').write_text(proc.stdout + proc.stderr)
            self.assertEqual(proc.returncode, 0)
            measured = re.findall(r"stack frame size \((\d+)\).*?function '([^']+)'", proc.stderr)
            self.assertEqual({name: int(size) for size, name in measured},
                             {name: f['frame'] for name, f in r['functions'].items()})
            checks += 1
        self.assertEqual(checks, len(self.records) * 2 + len({(r['target'], r['mode']) for r in self.records}) * 2)

    def test_inlining_does_not_exempt_array_free_function(self):
        for path, r in self.reports.items():
            if path.stem != 'inline-array-free':
                continue
            f = r['functions']['originally_array_free']
            self.assertEqual(f['P'], 0)
            if r['mode'] == 'O2':
                self.assertFalse(r['certified'])
                self.assertEqual(f['R'], 0)
                self.assertTrue(any('unmatched allocation' in e for e in r['errors']['originally_array_free']))
                self.assertGreater(sum(o['size'] for o in f['objects'] if not o['spill']), 0)
            else:
                self.assertTrue(r['certified'])

    def test_combined_original_modules_keep_plan_and_reject_clones(self):
        for path, r in self.reports.items():
            if path.stem == 'combined':
                self.assertEqual(r['functions']['entry']['P'], 49152)
                self.assertEqual(r['certified'], r['mode'] == 'normal')
                if r['mode'] == 'O2':
                    self.assertTrue(any('unmatched allocation' in e for e in r['errors']['entry']))

    def test_reused_slots_earn_no_credit(self):
        for c in self.controls:
            if c['control'] != 'reuse':
                continue
            self.assertEqual(c['P'], 512)
            self.assertEqual(c['R'], 0)
            homes = [o for o in c['objects'] if not o['spill']]
            self.assertEqual(len(homes), 1, 'actual stack coloring must occur in this experiment')
            self.assertEqual(homes[0]['size'], 256)
            self.assertGreater(c['C'], c['P'])

    def test_dead_homes_remain_in_immutable_plan(self):
        for c in self.controls:
            if c['control'] == 'dead home remains charged':
                self.assertEqual(c['P'], 49168)
                self.assertLessEqual(c['R'], c['P'])

    def test_late_frame_and_sp_changes_block_delivery(self):
        count = 0
        for path, r in self.reports.items():
            if path.stem not in ('late-frame', 'late-sp'):
                continue
            self.assertFalse(r['certified'])
            self.assertFalse(path.with_suffix('.o').exists())
            self.assertTrue(any('changed during final emission' in e for es in r['errors'].values() for e in es))
            if path.stem == 'late-sp':
                self.assertTrue(any('encoded SP writes differ' in e for es in r['errors'].values() for e in es))
            count += 1
        self.assertEqual(count, 2 * len({(r['target'], r['mode']) for r in self.records}))

    def test_final_encoded_sp_effects(self):
        relaxations = 0
        for path, r in self.reports.items():
            if not r['certified']:
                continue
            for f in r['functions'].values():
                self.assertEqual(f['sp_writes'], f['encoded_sp_writes'])
                self.assertEqual(f['call_opcodes'], f['encoded_call_opcodes'])
                for mi, mc in zip(f['mi_sp_opcodes'], f['mc_sp_opcodes'], strict=True):
                    if mi != mc:
                        self.assertIn((mi, mc), (('ADD64ri32', 'ADD64ri8'), ('SUB64ri32', 'SUB64ri8')))
                        relaxations += 1
        self.assertGreater(relaxations, 0, 'exercise real MC encoding shortening')

    def test_windows_actual_fifth_argument_ownership(self):
        for r in self.records:
            if 'windows' not in r['target']:
                continue
            caller = r['semantic_caller']['functions']['snapshot_case']
            callee = r['semantic_callee']['functions']['checked_roundtrip']
            self.assertEqual(caller['outgoing'], 40)
            self.assertEqual(caller['machine_calls']['checked_roundtrip'], 40)
            self.assertEqual(caller['machine_calls']['fill'], 32)
            self.assertEqual(caller['proved_five_arg_edges'], ['checked_roundtrip'])
            self.assertEqual(callee['outgoing'], 32)
            self.assertEqual(callee['incoming'][0]['callee_entry_sp_offset'], 40)
            self.assertEqual(callee['incoming'][0]['sp_reference'], callee['frame'] + 40)
            self.assertTrue(callee['incoming'][0]['caller_owned'])
        for path, r in self.reports.items():
            if path.stem == 'unproved-incoming':
                self.assertFalse(r['certified'])
                self.assertTrue(any('incoming ownership' in e for es in r['errors'].values() for e in es))

    def test_encoded_corruption_and_missing_inventory_are_rejected(self):
        for path, r in self.reports.items():
            if path.stem not in ('late-encoded', 'omitted-inventory'):
                continue
            self.assertFalse(r['certified'])
            self.assertFalse(path.with_suffix('.o').exists())
            required = 'encoded SP writes differ' if path.stem == 'late-encoded' else 'unmatched allocation'
            self.assertTrue(any(required in e for es in r['errors'].values() for e in es))
            if path.stem == 'late-encoded':
                self.assertFalse(any('changed during final emission' in e for es in r['errors'].values() for e in es))

    def test_late_boundary_has_real_post_pei_evidence(self):
        changed = 0
        for path, r in self.reports.items():
            if not r['certified']:
                continue
            for f in r['functions'].values():
                old = f['after_pei']
                if f['changed_after_pei']:
                    self.assertEqual(f['frame'], old['frame'])
                    self.assertNotEqual(f['sp_writes'], old['sp_writes'])
                    changed += 1
        self.assertGreater(changed, 0)

    def test_wrong_windows_caller_evidence_cannot_certify_incoming(self):
        for mode in ('normal', 'O2'):
            directory = self.output / 'x86_64-pc-windows-msvc' / mode
            if not directory.exists():
                continue
            caller = directory / 'semantics-caller.json'
            for failure in ('shadow-only', 'no-store', 'stale-hash'):
                broken = json.loads(caller.read_text())
                fn = broken['functions']['snapshot_case']
                if failure == 'shadow-only':
                    fn['machine_calls']['checked_roundtrip'] = 32
                elif failure == 'no-store':
                    fn['proved_five_arg_edges'] = []
                else:
                    broken['private_object_sha256'] = '0' * 64
                proof = directory / ('bad-caller-' + failure + '.json')
                proof.write_text(json.dumps(broken))
                shutil.copyfile(caller.with_suffix('.o'), proof.with_suffix('.o'))
                report, obj = directory / ('bad-incoming-' + failure + '.json'), directory / ('bad-incoming-' + failure + '.o')
                self.assertFalse(obj.exists())
                proc = subprocess.run([str(self.output / 'machine-emit'), str(directory / 'semantics-callee.ll'), mode,
                                       str(report), str(obj), 'none', str(proof)],
                                      capture_output=True, text=True, timeout=120)
                self.assertEqual(proc.returncode, 1 if failure == 'stale-hash' else 2)
                self.assertFalse(obj.exists())
                if report.exists():
                    self.assertFalse(json.loads(report.read_text())['certified'])

    def test_linux_native_execution_and_small_stack(self):
        for r in self.records:
            if r['evidence'] == 'compile-only' or r['count'] == 5461:
                self.assertEqual(r['runs'], [])
            else:
                self.assertEqual(r['runs'], [dict(engine='independently linked AOT', stack=None),
                    dict(engine='independently linked AOT', stack=1048576),
                    dict(engine='standalone ORC certified-object loading', stack=1048576)])


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--artifacts', required=True, type=Path)
    p.add_argument('--llvm-bin', required=True, type=Path)
    args, remaining = p.parse_known_args()
    unittest.main(argv=[__file__, *remaining])
