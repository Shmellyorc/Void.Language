"""Focused negative qualification using actual VOIDUP/Runtime on Linux.

Builds only a disposable VOIDUP application with ephemeral public test roots and
the existing small bootstrap. Does not build/install compiler tools or payloads.
"""
import argparse
import ast
import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
from types import SimpleNamespace
import unittest

import negative_security as checks

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', type=Path, required=True)
parser.add_argument('--evidence', type=Path, required=True)
args = parser.parse_args()
assert sys.platform == 'linux', 'Actual Linux behavior, not native macOS acceptance'
args.compiler = args.compiler.resolve(); args.evidence = args.evidence.resolve()
args.evidence.mkdir(parents=True, exist_ok=True)
os.environ['VOID_EVIDENCE_ROOT'] = str(args.evidence / 'retained')
from support import Service, encoded
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives.serialization import Encoding, PublicFormat

records = []


def run(command, expected=0, **kwargs):
    result = subprocess.run(list(map(str, command)), capture_output=True, timeout=180, **kwargs)
    argv = list(map(str, command))
    record = dict(command=argv, expected=expected, exit=result.returncode,
                  stdout=result.stdout.decode(errors='replace'), stderr=result.stderr.decode(errors='replace'))
    if '--root' in argv:
        root = Path(argv[argv.index('--root') + 1])
        state = dict(exists=os.path.lexists(root))
        if root.is_dir():
            state['children'] = sorted(p.name for p in root.iterdir())
            for name in ['control.json', 'voidup-root.json']:
                if (root / name).is_file(): state[name] = json.loads((root / name).read_bytes())
            for name in ['toolchains', 'metadata', 'transactions']:
                if (root / name).is_dir(): state[name] = sorted(p.name for p in (root / name).iterdir())
        home = (kwargs.get('env') or {}).get('HOME')
        if home:
            state['profile_sha256'] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                                      for p in Path(home).iterdir() if p.is_file()}
        record['root_state'] = state
    records.append(record)
    (args.evidence / 'commands.json').write_text(json.dumps(records, indent=2) + '\n')
    assert result.returncode == expected, records[-1]
    return result


class NegativeSecurity(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='voidup-negative-security-')
        cls.work = Path(cls.temp.name).resolve()
        cls.private = [Ed25519PrivateKey.generate() for _ in range(3)]
        source = cls.work / 'source'; shutil.copytree(ROOT / 'apps/voidup', source)
        public = [key.public_key().public_bytes(Encoding.Raw, PublicFormat.Raw).hex() for key in cls.private]
        (source / 'trust_keys.void').write_text('const up_key_ids: [string] = ["test-1", "test-2", "test-3"]\n'
            'const up_public_keys: [string] = ' + json.dumps(public) + '\n')
        cls.bundle = cls.work / 'bundle'; (cls.bundle / 'bin').mkdir(parents=True)
        cls.manager = cls.bundle / 'bin/voidup'
        run([args.compiler, 'build', '--no-opt', source / 'main.void', '-o', cls.manager])
        sdk = ROOT / '.toolchains/llvm-23.1.4/bin'
        cls.initializer = cls.bundle / 'bin/voidup-init'
        run([sdk / 'clang++', '--no-default-config', '-std=c++17', '-O2',
             '--ld-path=' + str(sdk / 'ld.lld'), '-DVOIDUP_BOOTSTRAP_ID="negative-test"',
             ROOT / 'apps/voidup/launcher.cpp', '-o', cls.initializer])
        cls.parent = checks.admitted_parent(cls.work, args.compiler, run, cls.work)
        (args.evidence / 'application-provenance.json').write_text(json.dumps(dict(
            compiler=str(args.compiler), compiler_sha256=hashlib.sha256(args.compiler.read_bytes()).hexdigest(),
            manager_sha256=hashlib.sha256(cls.manager.read_bytes()).hexdigest(),
            source_sha256={p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT / 'apps/voidup').glob('*.void')},
            copied_source_changes=['Ephemeral public test roots in trust_keys.void only'],
            private_keys='In memory only; no user signing material read', native='Linux x86_64'), indent=2) + '\n')

    @classmethod
    def tearDownClass(cls):
        cls.private.clear(); cls.temp.cleanup()

    def setUp(self):
        self.case = Path(tempfile.mkdtemp(prefix='case-', dir=self.parent))
        self.home = self.case / 'home'; self.home.mkdir()
        self.profiles = {'.profile': b'# preserve profile\n', '.bashrc': b'# preserve bash\n', '.zshenv': b'# preserve zsh\n'}
        for name, content in self.profiles.items(): (self.home / name).write_bytes(content)
        self.environment = dict(os.environ, HOME=str(self.home), XDG_DATA_HOME=str(self.home / 'data'))
        now = int(time.time())
        release = dict(id='stable-current', host='linux-x86_64-ubuntu24.04', build_identity='0' * 64,
                       artifact_url='/packages/never-transfer.zip', artifact_bytes=22, artifact_sha256='0' * 64,
                       inventory_url='/inventories/never-transfer.json', inventory_bytes=1, inventory_sha256='0' * 64,
                       selection=dict(channel='stable', version='1.10.0'))
        self.catalog = dict(schema=1, domain='voidup-catalog-v1', serial=100, issued=now-60, expires=now+86400,
                            min_manager=2, llvm_version='23.1.4', revoked=[], releases=[release])
        self.fixture = self.case / 'fixtures'; self.fixture.mkdir()
        def signer(raw, signers):
            return encoded(dict(schema=1, signatures=[dict(key_id=f'test-{n}', signature=self.private[n-1].sign(b'VOIDUP CATALOG v1\n'+raw).hex()) for n in signers]))
        self.service = Service(self.fixture, signer=signer); self.service.__enter__()
        self.addCleanup(lambda: self.service.__exit__(None, None, None))

    def attempt(self, name, document=None, signers=(1, 2), root=None):
        document = document or self.catalog
        self.service.catalog(document, signers=signers)
        root = root or checks.fresh_root(self.case, name)
        result = run([self.initializer, '--root', root, '--no-path', '--catalog', self.service.url,
                      '--ca-file', self.service.ca], expected=1, env=self.environment)
        self.assertEqual({p.name: p.read_bytes() for p in self.home.iterdir()}, self.profiles)
        return root, result

    def signed_state(self, root, document):
        checks.rejected_state(root, catalog=document,
            raw_catalog=self.service.overrides['/catalog.json'][1],
            raw_signatures=self.service.overrides['/catalog.json.sig'][1])

    def test_genuine_one_signature_rejection(self):
        root, result = self.attempt('bad-signature', signers=(1,))
        checks.refusal(result, checks.AUTHENTICATE); checks.rejected_state(root)
        self.assertEqual(self.service.requests, ['/catalog.json', '/catalog.json.sig'])

    def test_raw_alias_is_filesystem_not_authentication(self):
        (self.case / 'private/var').mkdir(parents=True)
        (self.case / 'var').symlink_to('private/var', target_is_directory=True)
        root = self.case / 'var/raw-refusal'
        _, result = self.attempt('unused', signers=(1,), root=root)
        checks.filesystem_refusal(result, root)
        with self.assertRaises(AssertionError): checks.refusal(result, checks.AUTHENTICATE)
        self.assertEqual(self.service.requests, [])
        self.assertFalse((self.case / 'private/var/raw-refusal').exists())

    def test_authenticated_replay_preserves_trust_floor(self):
        self.service.catalog(self.catalog)
        root = checks.fresh_root(self.case, 'replay')
        options = [self.manager, '--root', root, '--catalog', self.service.url, '--ca-file', self.service.ca]
        run([*options, 'list', '--available'], env=self.environment)
        before = (root / 'control.json').read_bytes()
        raw = self.service.overrides['/catalog.json'][1]
        signatures = self.service.overrides['/catalog.json.sig'][1]
        replay = dict(self.catalog, serial=99); self.service.catalog(replay)
        result = run([*options, 'update'], expected=1, env=self.environment)
        checks.refusal(result, ('freshness', 'catalog serial replay'))
        self.assertEqual((root / 'control.json').read_bytes(), before)
        checks.rejected_state(root, catalog=self.catalog, raw_catalog=raw, raw_signatures=signatures)

    def test_ambiguous_signed_catalog(self):
        catalog = copy.deepcopy(self.catalog); catalog['serial'] = 102
        catalog['releases'].append(dict(catalog['releases'][0], id='ambiguous'))
        root, result = self.attempt('ambiguous', catalog)
        checks.refusal(result, checks.AMBIGUOUS); self.signed_state(root, catalog)

    def test_unsupported_signed_catalog(self):
        catalog = copy.deepcopy(self.catalog); catalog['serial'] = 103
        preview = copy.deepcopy(catalog['releases'][0]); preview['selection']['channel'] = 'preview'
        wrong = dict(catalog['releases'][0], id='wrong-host', host='windows-x86_64')
        catalog['releases'] = [preview, wrong]
        root, result = self.attempt('unsupported', catalog)
        checks.refusal(result, checks.UNSUPPORTED); self.signed_state(root, catalog)

    def test_legacy_setup_retains_existing_selection_refusal(self):
        catalog = copy.deepcopy(self.catalog); catalog['serial'] = 90; catalog['min_manager'] = 1
        del catalog['releases'][0]['selection']
        root, result = self.attempt('legacy', catalog)
        checks.refusal(result, checks.UNSUPPORTED); self.signed_state(root, catalog)

    def test_wrong_phase_diagnostics_cannot_pass(self):
        root, result = self.attempt('real-bad-signature', signers=(1,))
        original = json.loads(result.stderr)
        for operation in ['arguments', 'connect', 'io.create_directory_handle', 'root', 'install']:
            error = dict(original, operation=operation)
            wrong = subprocess.CompletedProcess([], 1, b'', json.dumps(error).encode())
            with self.subTest(operation=operation), self.assertRaises(AssertionError): checks.refusal(wrong, checks.AUTHENTICATE)
        wrong = subprocess.CompletedProcess([], 1, b'', json.dumps(dict(original, message='unrelated authentication error')).encode())
        with self.assertRaises(AssertionError): checks.refusal(wrong, checks.AUTHENTICATE)
        checks.rejected_state(root)

    def test_unexpected_publication_and_trust_mutation_rejected(self):
        for name in ['toolchain', 'bin', 'control', 'marker', 'metadata', 'transaction']:
            with self.subTest(mutation=name):
                root, result = self.attempt('mutated-' + name, signers=(1,))
                checks.refusal(result, checks.AUTHENTICATE); checks.rejected_state(root)
                if name == 'toolchain': (root / 'toolchains/unauthorized').mkdir()
                elif name == 'bin': (root / 'bin').mkdir()
                elif name == 'control':
                    control = json.loads((root / 'control.json').read_bytes()); control['serial'] = 1
                    (root / 'control.json').write_text(json.dumps(control))
                elif name == 'marker': (root / 'voidup-root.json').write_text('{"schema":1,"application":"foreign"}')
                elif name == 'metadata': (root / 'metadata/untrusted').write_text('untrusted')
                else: (root / 'transactions/unexpected').mkdir()
                with self.assertRaises(AssertionError): checks.rejected_state(root)

    def test_fresh_root_rejects_existing_names(self):
        for name in ['directory', 'file', 'link']:
            path = self.case / name
            if name == 'directory': path.mkdir()
            elif name == 'file': path.write_text('preserve')
            else: path.symlink_to(self.case / 'missing')
            with self.assertRaises(AssertionError): checks.fresh_root(self.case, name)

    def first_run_block(self, mutation=None, macos_modeled=False):
        # Execute the exact edited qualification statements, with a real local
        # manager/bootstrap. Positive installation/payload work stays outside
        # this focused test; the existing main root has a real trusted floor.
        self.service.catalog(self.catalog)
        install = checks.fresh_root(self.case, 'existing-installation')
        run([self.manager, '--root', install, '--catalog', self.service.url,
             '--ca-file', self.service.ca, 'list', '--available'], env=self.environment)
        original_control = (install / 'control.json').read_bytes()
        text = (ROOT / 'tests/voidup/first_run.py').read_text()
        tree = ast.parse(text)
        def assigned(node, name):
            return isinstance(node, ast.Assign) and any(isinstance(target, ast.Name) and target.id == name for target in node.targets)
        body = next(node.body for node in ast.walk(tree) if isinstance(node, ast.With)
                    and any(assigned(child, 'negative_parent') for child in node.body))
        start = next(i for i, node in enumerate(body) if assigned(node, 'negative_parent'))
        end = next(i for i in range(start, len(body)) if isinstance(body[i], ast.If)
                   and isinstance(body[i].test, ast.Name) and body[i].test.id == 'macos')
        def actual(command, *, env=None, expected=0, directory_diagnostic=None):
            command = list(command)
            if mutation == 'wrong-phase' and command[0] == self.initializer:
                (self.case / 'blocked-alias').symlink_to(self.case, target_is_directory=True)
                command[command.index('--root') + 1] = self.case / 'blocked-alias/must-not-publish'
            result = run(command, expected=expected, env=env)
            if command[0] == self.initializer:
                if mutation == 'profile': (self.home / '.profile').write_bytes(b'unintended change')
                if mutation == 'missing-state': shutil.rmtree(command[command.index('--root') + 1])
                if mutation == 'main-control': (install / 'control.json').write_bytes(b'unintended change')
            return result
        negative = checks
        if macos_modeled:
            # Execute the Mac-only qualification statements with a real Linux
            # no-follow refusal. Native /var metadata admission is not modeled
            # as native execution: only this test namespace changes its prefix.
            alias = self.work / (self.case.name + '-var'); alias.symlink_to(self.case, target_is_directory=True)
            negative = SimpleNamespace(**{name: getattr(checks, name) for name in
                ['admitted_parent', 'fresh_root', 'refusal', 'rejected_state', 'filesystem_refusal', 'AUTHENTICATE']},
                literal_var_parent=lambda parent: alias)
        namespace = dict(negative=negative, cases=self.case, compiler_proxy=args.compiler,
            run=actual, environment=self.environment, work=self.case, install=install,
            service=self.service, catalog=self.catalog, initializer=lambda path: [path, '--catalog', self.service.url],
            reinit=self.initializer, profile_before=self.profiles, home=self.home, macos=macos_modeled, diagnostic=None)
        exec(compile(ast.Module(body=body[start:end+1], type_ignores=[]), 'first_run.py negative block', 'exec'), namespace)
        self.assertEqual((install / 'control.json').read_bytes(), original_control)
        return namespace, body

    def test_exact_first_run_bad_signature_block(self):
        namespace, body = self.first_run_block()
        self.assertEqual(namespace['rejected'].parent, self.case)
        # Exercise the adjacent qualification statements too, with actual
        # authenticated ambiguous and unsupported catalogs and their state.
        def assigned(node, name):
            return isinstance(node, ast.Assign) and any(isinstance(target, ast.Name) and target.id == name for target in node.targets)
        start = next(i for i, node in enumerate(body) if assigned(node, 'ambiguous'))
        end = next(i for i in range(start, len(body)) if isinstance(body[i], ast.If)
                   and isinstance(body[i].test, ast.UnaryOp))
        release = self.catalog['releases'][0]
        preview = copy.deepcopy(release); preview['selection']['channel'] = 'preview'
        namespace.update(copy=copy, newer=copy.deepcopy(self.catalog), release=release,
                         preview=preview, wrong_host=dict(release, id='wrong-host', host='windows-x86_64'))
        # Stop before the unsafe-profile branch and exclude its preceding
        # service.catalog(newer), whose profile behavior is separately covered.
        exec(compile(ast.Module(body=body[start:end-1], type_ignores=[]), 'first_run.py adjacent negatives', 'exec'), namespace)

    def test_exact_first_run_rejects_wrong_phase(self):
        with self.assertRaises(AssertionError): self.first_run_block('wrong-phase')

    def test_exact_first_run_separate_macos_refusal_block_modeled(self):
        namespace, _ = self.first_run_block(macos_modeled=True)
        self.assertFalse(os.path.lexists(namespace['raw_root']))
        self.assertEqual(self.service.requests, ['/catalog.json', '/catalog.json.sig'] * 2)

    def test_exact_first_run_detects_profile_change(self):
        with self.assertRaises(AssertionError): self.first_run_block('profile')

    def test_exact_first_run_detects_missing_initialized_root(self):
        with self.assertRaises((AssertionError, OSError)): self.first_run_block('missing-state')

    def test_exact_first_run_detects_main_trust_change(self):
        with self.assertRaises(AssertionError): self.first_run_block('main-control')

    def test_parent_admission_rejects_replacement(self):
        parent = self.case / 'admission-parent'; parent.mkdir()
        def replaced(command):
            result = run(command)
            parent.rename(self.case / 'original-parent'); parent.mkdir()
            return result
        with self.assertRaises(AssertionError): checks.admitted_parent(parent, args.compiler, replaced, self.case)


if __name__ == '__main__': unittest.main(argv=[sys.argv[0]], verbosity=2)
