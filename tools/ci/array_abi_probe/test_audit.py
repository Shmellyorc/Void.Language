"""Regression checks against real generated MIR and deliberately faulty plans."""
from pathlib import Path
import argparse
import json
import re
import subprocess
import unittest
from address import ir as address_ir, semantics
from probe import ir as raw_ir
from audit import inspect, require_covered


class AuditTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.records = json.loads((args.artifacts / 'results.json').read_text())
        if not json.loads((args.artifacts / 'complete.json').read_text())['passed']:
            raise RuntimeError('incomplete experiment')
        cls.sample = next(r for r in cls.records if r['execution'] == 'native Linux x64' and
                          r['calling'] == 'address' and r['elements'] == 1024 and r['mode'] == 'normal')
        r = cls.sample
        cls.directory = args.artifacts / f"{r['target']}-address-1024-normal"
        cls.text = (cls.directory / 'caller.mir').read_text()
        cls.profiles = json.loads((args.artifacts / 'target-profiles.json').read_text())
        cls.entry_bytes = -cls.profiles[r['target']]['local_area_offset']
        cls.homes = {name: dict(bytes=4096, alignment=4) for name in ('input', 'snapshot', 'output')}
        cls.output = args.artifacts / 'audit-regressions'
        cls.output.mkdir(exist_ok=True)
        cls.commands = []

    def reject(self, text, homes):
        with self.assertRaises(ValueError):
            require_covered(inspect(text, homes, self.entry_bytes))

    def test_actual_frames_and_policy_boundary(self):
        for r in self.records:
            if r['calling'] != 'address':
                continue
            directory = args.artifacts / f"{r['target']}-address-{r['elements']}-{r['mode']}"
            for name, names in (('caller', ('input', 'snapshot', 'output')), ('callee', ('home',))):
                homes = {n: dict(bytes=r['payload_bytes'], alignment=4) for n in names}
                report = inspect((directory / (name + '.mir')).read_text(), homes, -self.profiles[r['target']]['local_area_offset'])
                require_covered(report)
                self.assertEqual(report, r['audit'][name])
                if name == 'caller' and r['elements'] in (5440, 5461):
                    self.assertEqual(report['within_budget'], r['elements'] == 5440)
                    self.assertEqual(report['missing_homes'], [])
                    if r['elements'] == 5461:
                        self.assertEqual(report['preoptimization_bytes'], 65536)
                        self.assertGreater(report['frame_bytes'], 65536)

    def test_missing_or_inexact_home(self):
        missing = dict(self.homes)
        del missing['snapshot']
        self.reject(self.text, missing)
        wrong = dict(self.homes, snapshot=dict(bytes=4092, alignment=4))
        self.reject(self.text, wrong)

    def test_frame_undercount(self):
        text = re.sub(r'(^  stackSize:)\s+\d+', r'\1 8', self.text, flags=re.M)
        self.reject(text, self.homes)

    def test_unproved_stack_shapes(self):
        for flag in ('hasOpaqueSPAdjustment', 'hasTailCall'):
            text, changed = re.subn(r'(^  ' + flag + r':)\s+false', r'\1 true', self.text, flags=re.M)
            self.assertEqual(changed, 1)
            self.reject(text, self.homes)
        text = re.sub(r'(^  maxAlignment:)\s+\d+', r'\1 32', self.text, flags=re.M)
        self.reject(text, self.homes)

    def test_real_raw_hidden_result_is_unaccounted(self):
        records = json.loads((args.raw_baseline / 'results.json').read_text())
        for r in records:
            if r.get('calling', 'raw') != 'raw':
                continue
            family = '-raw' if 'calling' in r else ''
            directory = args.raw_baseline / f"{r['target']}{family}-{r['elements']}-{r['mode']}"
            # The original raw generator and both independently compiled modules
            # are unchanged. Historical measurements remain explicitly historical.
            for name, caller in (('caller', True), ('callee', False)):
                self.assertEqual((directory / (name + '.ll')).read_text(),
                                 raw_ir(r['target'], r['data_layout'], r['elements'], caller))
            homes = {n: dict(bytes=r['payload_bytes'], alignment=4) for n in ('input', 'output')}
            report = inspect((directory / 'caller.mir').read_text(), homes, -self.profiles[r['target']]['local_area_offset'])
            self.assertTrue(any(h['size'] == r['payload_bytes'] for h in report['unknown_homes']))
            with self.assertRaises(ValueError):
                require_covered(report)

    def generate(self, addition, name, mode):
        r = self.sample
        source = address_ir(r['target'], r['data_layout'], 1024, True)
        source = source.replace(' %input = alloca', addition + '\n %input = alloca', 1)
        path = self.output / (name + '-' + mode + '.ll')
        path.write_text(source)
        commands = [[args.llvm_bin / 'opt', '-passes=verify', '-disable-output', path]]
        if mode == 'O2':
            optimized = path.with_name(path.stem + '-O2.ll')
            commands.append([args.llvm_bin / 'opt', '-S', '-passes=default<O2>', path, '-o', optimized])
            path = optimized
        mir = path.with_suffix('.mir')
        commands.append([args.llvm_bin / 'llc', '-O2', '-relocation-model=pic',
                         '-stop-after=prologepilog', path, '-o', mir])
        for argv in commands:
            result = subprocess.run([str(x) for x in argv], text=True, encoding='utf-8',
                                    errors='strict', capture_output=True, timeout=120)
            self.commands.append(dict(argv=[str(x) for x in argv], exit=result.returncode,
                                      output=result.stdout + result.stderr))
            (self.output / 'commands.json').write_text(json.dumps(self.commands, indent=2) + '\n')
            self.assertEqual(result.returncode, 0, result.stderr)
        return mir.read_text()

    def test_real_uninventoried_home(self):
        for mode in ('normal', 'O2'):
            text = self.generate(' %forgotten = alloca [4 x i32], align 4\n'
                                 ' call void @fill(ptr %forgotten, i64 4, i32 %seed)', 'forgotten', mode)
            report = inspect(text, self.homes, self.entry_bytes)
            self.assertTrue(any(h['name'] == 'forgotten' for h in report['unknown_homes']))
            with self.assertRaises(ValueError):
                require_covered(report)

    def test_dead_home_never_earns_budget_credit(self):
        for mode in ('normal', 'O2'):
            text = self.generate(' %dead_home = alloca [4 x i32], align 4', 'dead', mode)
            homes = dict(self.homes, dead_home=dict(bytes=16, alignment=4))
            report = inspect(text, homes, self.entry_bytes)
            require_covered(report)
            if mode == 'O2':
                self.assertIn('dead_home', report['missing_homes'])
                self.assertEqual(report['frame_bytes'], self.sample['audit']['caller']['frame_bytes'])
            else:
                self.assertNotIn('dead_home', report['missing_homes'])
                self.assertEqual(report['frame_bytes'], self.sample['audit']['caller']['frame_bytes'] + 16)
            self.assertEqual(report['conservative_total_bytes'],
                             self.sample['audit']['caller']['conservative_total_bytes'] + 16)

    def test_native_validation_rejects_unsafe_borrow_and_delayed_snapshot(self):
        r = self.sample
        for mode in ('normal', 'O2'):
            directory = args.artifacts / f"{r['target']}-address-1024-{mode}"
            source = semantics(r['target'], r['data_layout'], True)
            copy = ' call void @llvm.memcpy.p0.p0.i64(ptr align 4 %snapshot, ptr align 4 %input, i64 64, i1 false)\n'
            later = ' %next = call i32 @later(ptr %input, i64 16, i32 %mode, ptr %failure)\n'
            variants = dict(unsafe_borrow=source.replace('ptr %snapshot, i32 %next', 'ptr %input, i32 %next'),
                            delayed_snapshot=source.replace(copy, '').replace(later, later + copy))
            for name, text in variants.items():
                self.assertNotEqual(source, text)
                path = self.output / f'{name}-{mode}.ll'
                path.write_text(text)
                commands = [[args.llvm_bin / 'opt', '-passes=verify', '-disable-output', path]]
                if mode == 'O2':
                    optimized = path.with_name(path.stem + '-O2.ll')
                    commands.append([args.llvm_bin / 'opt', '-S', '-passes=default<O2>', path, '-o', optimized])
                    path = optimized
                obj, executable = path.with_suffix('.o'), path.with_suffix('')
                commands += [[args.llvm_bin / 'llc', '-O2', '-relocation-model=pic', '-filetype=obj', path, '-o', obj],
                             [args.llvm_bin / 'clang', '-O2', '-std=c11', '-Wall', '-Wextra', '-Werror',
                              '-DADDRESS_PROBE', Path(__file__).with_name('host.c').resolve(), obj,
                              directory / 'caller.o', directory / 'callee.o', directory / 'semantics-callee.o',
                              '-o', executable], [executable.resolve()]]
                for i, argv in enumerate(commands):
                    expected = 1 if i == len(commands) - 1 else 0
                    result = subprocess.run([str(x) for x in argv], text=True, encoding='utf-8',
                                            errors='strict', capture_output=True, timeout=120)
                    self.commands.append(dict(argv=[str(x) for x in argv], exit=result.returncode,
                                              expected=expected, output=result.stdout + result.stderr))
                    (self.output / 'commands.json').write_text(json.dumps(self.commands, indent=2) + '\n')
                    self.assertEqual(result.returncode, expected, result.stderr)
                    if expected:
                        self.assertEqual(result.stdout + result.stderr, '')

    def test_inlining_requires_storage_provenance(self):
        r = self.sample
        directory = args.artifacts / f"{r['target']}-address-4096-normal"
        native = args.artifacts / f"{r['target']}-address-4096-O2"
        linked = self.output / 'combined.ll'
        optimized = self.output / 'combined-O2.ll'
        mir, obj, executable = (self.output / n for n in ('combined.mir', 'combined.o', 'combined'))
        commands = [[args.llvm_bin / 'llvm-link', '-S', directory / 'caller.ll', directory / 'callee.ll', '-o', linked],
                    [args.llvm_bin / 'opt', '-S', '-passes=default<O2>', linked, '-o', optimized],
                    [args.llvm_bin / 'llc', '-O2', '-relocation-model=pic', '-stop-after=prologepilog', optimized, '-o', mir],
                    [args.llvm_bin / 'llc', '-O2', '-relocation-model=pic', '-filetype=obj', optimized, '-o', obj],
                    [args.llvm_bin / 'clang', '-O2', '-std=c11', '-Wall', '-Wextra', '-Werror', '-DADDRESS_PROBE',
                     Path(__file__).with_name('host.c').resolve(), obj,
                     native / 'semantics-caller.o', native / 'semantics-callee.o', '-o', executable],
                    [executable.resolve()]]
        for argv in commands:
            result = subprocess.run([str(x) for x in argv], text=True, encoding='utf-8',
                                    errors='strict', capture_output=True, timeout=120)
            self.commands.append(dict(argv=[str(x) for x in argv], exit=result.returncode,
                                      output=result.stdout + result.stderr))
            (self.output / 'commands.json').write_text(json.dumps(self.commands, indent=2) + '\n')
            self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, 'native array roundtrip passed\n')
        functions = re.split(r'(?m)^---\n(?=name:)', mir.read_text())
        entry = next(p for p in functions if re.match(r'name:\s+entry\s*\n', p))
        homes = {n: dict(bytes=16384, alignment=4) for n in ('input', 'snapshot', 'output')}
        report = inspect(entry, homes, self.entry_bytes)
        self.assertTrue(any(h['name'].startswith('home.i') for h in report['unknown_homes']))
        self.assertIn('snapshot', report['missing_homes'])
        self.assertIsNone(report['conservative_total_bytes'])
        with self.assertRaises(ValueError):
            require_covered(report)
        (self.output / 'inlining-audit.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--artifacts', type=Path, required=True)
    parser.add_argument('--raw-baseline', type=Path, required=True)
    parser.add_argument('--llvm-bin', type=Path, required=True)
    args = parser.parse_args()
    unittest.main(argv=[__file__], verbosity=2)
