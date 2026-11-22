"""Offline Mach-O relocation orchestration; Apple commands are modeled, not executed.

The Clang fixture uses the load references/RPATH from the pinned 23.1.4 ARM64
archive. Native codesign, dyld execution and Apple's editor still need macOS.
"""
from contextlib import ExitStack
import copy
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import voidup_macos as macos


def details(dependencies=(), rpaths=(), identifier=None):
    return dict(dependencies=list(dependencies), rpaths=list(rpaths),
                identifier=identifier, minimum=['14.0'])


class Relocation(unittest.TestCase):
    def setUp(self):
        self.stack = ExitStack()
        self.addCleanup(self.stack.close)
        self.root = Path(self.stack.enter_context(tempfile.TemporaryDirectory(prefix='VOID Mach-O '))).resolve()
        self.prefix, self.sdk = self.root / 'payload', self.root / 'producer-sdk'
        self.records, self.inputs, self.commands = {}, {}, []
        self.reject_edit = self.bad_signature = self.ignore_edits = False
        self.stack.enter_context(patch.object(macos.sys, 'platform', 'darwin'))
        self.stack.enter_context(patch.object(macos, 'inspect', side_effect=self.inspect))
        self.stack.enter_context(patch.object(macos.subprocess, 'run', side_effect=self.command))

    def image(self, path, metadata):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(('signed fixture ' + str(path)).encode())
        self.records[path] = copy.deepcopy(metadata)
        self.inputs[path.read_bytes()] = copy.deepcopy(metadata)
        return path

    def inspect(self, image):
        image = Path(image)
        if image not in self.records:
            self.records[image] = copy.deepcopy(self.inputs[image.read_bytes()])
        return copy.deepcopy(self.records[image])

    def command(self, argv, **kwargs):
        self.commands.append(argv)
        image = Path(argv[-1])
        if argv[0] == '/usr/bin/install_name_tool':
            if self.reject_edit:
                raise subprocess.CalledProcessError(1, argv, stderr='file not in an order that can be processed '
                    '(link edit information does not fill the __LINKEDIT segment)')
            if not self.ignore_edits:
                record = self.records[image]
                index = 1
                while index < len(argv) - 1:
                    option, value = argv[index:index+2]
                    if option == '-delete_rpath':
                        record['rpaths'].remove(value); index += 2
                    elif option == '-id':
                        record['identifier'] = value; index += 2
                    elif option == '-change':
                        record['dependencies'] = [argv[index+2] if p == value else p for p in record['dependencies']]
                        index += 3
                    else:
                        raise AssertionError(argv)
        elif argv[0] == '/usr/bin/codesign':
            if '--verify' in argv:
                self.assertIn('--strict', argv)
                if self.bad_signature:
                    raise subprocess.CalledProcessError(1, argv)
        else:
            raise AssertionError(argv)
        return subprocess.CompletedProcess(argv, 0)

    def clang(self, rpaths=('@loader_path/../lib',)):
        metadata = details(['/usr/lib/libSystem.B.dylib', '/usr/lib/libz.1.dylib', '/usr/lib/libc++.1.dylib'], rpaths)
        original = self.image(self.sdk / 'bin/clang-23', metadata)
        image = self.image(self.prefix / 'sdk/bin/clang', metadata)
        (self.prefix / 'sdk/lib').mkdir(parents=True, exist_ok=True)
        return image, original

    def relocate(self, image, original, **kwargs):
        return macos.relocate(self.prefix, {image: original}, [self.prefix, self.sdk], **kwargs)

    def test_pinned_clang_is_byte_preserved_and_strictly_verified(self):
        image, original = self.clang()
        before = image.read_bytes()
        self.reject_edit = True  # The exact demonstrated Apple failure is fatal if invoked.
        result = self.relocate(image, original)
        self.assertEqual(image.read_bytes(), before)
        self.assertEqual(result['sdk/bin/clang']['rpaths'], ['@loader_path/../lib'])
        self.assertEqual(self.commands, [['/usr/bin/codesign', '--verify', '--strict', str(image)]])

    def aliased_prefix(self):
        # Model macOS /var -> /private/var with a real ancestor symlink, not
        # mocked resolve/relative_to calls. Payload files themselves are regular.
        private_var = self.root / 'private/var'
        private_var.mkdir(parents=True)
        alias = self.root / 'var'
        try:
            alias.symlink_to(private_var, target_is_directory=True)
        except (OSError, NotImplementedError):
            self.skipTest('This host does not permit fixture directory symlinks')
        self.prefix = private_var / 'folders/qualification/payload'
        return alias / 'folders/qualification/payload'

    def test_var_alias_inventory_uses_canonical_image_paths(self):
        alias = self.aliased_prefix()
        image, original = self.clang()
        aliased_image = alias / 'sdk/bin/clang'
        before = image.read_bytes()
        self.reject_edit = True
        for prefix, images in [(alias, {aliased_image: original}),
                               (self.prefix, {aliased_image: original}),
                               (alias, {image: original}),
                               (alias, {image: original, aliased_image: original})]:
            with self.subTest(prefix=prefix, images=list(images)):
                self.commands.clear()
                result = macos.relocate(prefix, images, [self.prefix, self.sdk])
                self.assertEqual(result, {'sdk/bin/clang': self.records[image]})
                self.assertEqual(image.read_bytes(), before)
                self.assertEqual(self.commands, [['/usr/bin/codesign', '--verify', '--strict', str(image)]])

    def test_var_alias_recursive_dependency_inventory_is_relative(self):
        alias = self.aliased_prefix()
        library = self.image(self.sdk / 'lib/libneeded.dylib', details(rpaths=['@loader_path']))
        image, original = self.clang()
        self.records[image]['dependencies'].append(str(library))
        result = macos.relocate(alias, {alias / 'sdk/bin/clang': original}, [alias, self.sdk])
        self.assertEqual(set(result), {'sdk/bin/clang', 'sdk/lib/void-dependencies/libneeded.dylib'})
        self.assertIn('@loader_path/../lib/void-dependencies/libneeded.dylib', result['sdk/bin/clang']['dependencies'])
        self.assertEqual(sum('--verify' in c for c in self.commands), 2)

    def test_package_image_escapes_are_rejected_before_apple_commands(self):
        image, original = self.clang()
        outside = self.image(self.root / 'outside/clang', details(rpaths=['/producer/lib']))
        link = self.prefix / 'sdk/bin/escape'
        directory = self.prefix / 'escape'
        try:
            link.symlink_to(outside)
            directory.symlink_to(outside.parent, target_is_directory=True)
        except (OSError, NotImplementedError):
            self.skipTest('This host does not permit fixture symlinks')
        before = outside.read_bytes()
        for escaped in [outside, link, directory / outside.name, self.prefix / '../outside/clang']:
            with self.subTest(image=escaped), self.assertRaisesRegex(ValueError, 'outside package'):
                self.relocate(escaped, original)
            self.assertEqual(self.commands, [])
            self.assertEqual(outside.read_bytes(), before)

    def test_dependency_copy_symlink_escapes_are_rejected_before_writing(self):
        for kind in ['directory', 'file']:
            with self.subTest(kind=kind):
                self.prefix = self.root / ('payload-' + kind)
                image, original = self.clang()
                library = self.image(self.sdk / 'lib/libneeded.dylib', details())
                self.records[image]['dependencies'].append(str(library))
                outside = self.root / ('outside-' + kind)
                outside.mkdir()
                sentinel = outside / library.name
                sentinel.write_bytes(b'outside package must not change')
                destination = self.prefix / 'sdk/lib/void-dependencies'
                try:
                    if kind == 'directory':
                        destination.symlink_to(outside, target_is_directory=True)
                    else:
                        destination.mkdir()
                        (destination / library.name).symlink_to(sentinel)
                except (OSError, NotImplementedError):
                    self.skipTest('This host does not permit fixture symlinks')
                with self.assertRaisesRegex(ValueError, 'outside package'):
                    self.relocate(image, original)
                self.assertEqual(sentinel.read_bytes(), b'outside package must not change')
                self.assertEqual(self.commands, [])

    def test_dependency_directory_cannot_escape_package(self):
        image, original = self.clang()
        for destination in ['../outside', str(self.root / 'absolute-outside')]:
            with self.subTest(destination=destination), self.assertRaisesRegex(ValueError, 'outside package'):
                self.relocate(image, original, dependency_directory=destination)
        self.assertEqual(self.commands, [])

    def test_unsafe_search_paths_are_removed_not_silently_retained(self):
        for unsafe in ['/opt/homebrew/opt/llvm/lib', '/Users/runner/work/build/lib', str(self.sdk / 'lib'),
                       str(self.prefix / 'sdk/lib'), '../lib', '', '@rpath/lib', '@loader_path_extra/lib',
                       '@loader_path/../../../outside']:
            with self.subTest(path=unsafe):
                self.commands.clear()
                image, original = self.clang(('@loader_path/../lib', unsafe))
                result = self.relocate(image, original)
                self.assertEqual(result['sdk/bin/clang']['rpaths'], ['@loader_path/../lib'])
                edit = next(c for c in self.commands if c[0] == '/usr/bin/install_name_tool')
                self.assertEqual(edit[1:-1], ['-delete_rpath', unsafe])
                self.assertEqual([c[1] for c in self.commands],
                                 ['--display', '--remove-signature', '-delete_rpath', '--force', '--verify'])

    def test_symlink_search_path_escape_is_removed(self):
        image, original = self.clang(('@loader_path/../lib/escape',))
        outside = self.root / 'outside'; outside.mkdir()
        try:
            (self.prefix / 'sdk/lib/escape').symlink_to(outside, target_is_directory=True)
        except (OSError, NotImplementedError):
            self.skipTest('This host does not permit fixture directory symlinks')
        result = self.relocate(image, original)
        self.assertEqual(result['sdk/bin/clang']['rpaths'], [])

    def test_bare_loader_and_internal_executable_paths_survive(self):
        image, original = self.clang(('@loader_path', '@executable_path/../lib'))
        self.reject_edit = True
        result = self.relocate(image, original)
        self.assertEqual(result['sdk/bin/clang']['rpaths'], ['@loader_path', '@executable_path/../lib'])

    def test_shared_executable_rpath_must_stay_inside_for_every_entry(self):
        image, original = self.clang(('@executable_path/../../lib',))
        other = self.image(self.prefix / 'bin/voidc', details())
        result = macos.relocate(self.prefix, {image: original, other: other}, [self.prefix, self.sdk])
        self.assertEqual(result['sdk/bin/clang']['rpaths'], [])

    def test_recursive_arm64_dependency_closure_is_still_relocated(self):
        lib = self.sdk / 'lib'
        self.image(lib / 'libfirst.dylib', details(['@rpath/libsecond.dylib'], [str(lib)], '@rpath/libfirst.dylib'))
        self.image(lib / 'libsecond.dylib', details(['@rpath/libfirst.dylib', '/usr/lib/libSystem.B.dylib'],
                                                   ['@loader_path'], '@rpath/libsecond.dylib'))
        image, original = self.clang()
        self.records[image]['dependencies'].append('@rpath/libfirst.dylib')
        result = self.relocate(image, original)
        self.assertEqual(set(result), {'sdk/bin/clang', 'sdk/lib/void-dependencies/libfirst.dylib',
                                      'sdk/lib/void-dependencies/libsecond.dylib'})
        self.assertIn('@loader_path/../lib/void-dependencies/libfirst.dylib', result['sdk/bin/clang']['dependencies'])
        self.assertEqual(result['sdk/lib/void-dependencies/libfirst.dylib']['dependencies'], ['@loader_path/libsecond.dylib'])
        self.assertEqual(result['sdk/lib/void-dependencies/libsecond.dylib']['dependencies'][0], '@loader_path/libfirst.dylib')
        self.assertEqual(sum('--verify' in c for c in self.commands), 3)

    def test_unadmitted_dependency_is_rejected(self):
        image, original = self.clang()
        outside = self.image(self.root / 'homebrew/libbad.dylib', details())
        self.records[image]['dependencies'].append(str(outside))
        with self.assertRaisesRegex(ValueError, 'outside admitted'):
            self.relocate(image, original)

    def test_unresolved_dependency_is_rejected(self):
        image, original = self.clang()
        self.records[image]['dependencies'].append('@rpath/libmissing.dylib')
        with self.assertRaisesRegex(ValueError, 'Unresolved Mach-O dependency'):
            self.relocate(image, original)

    def test_post_edit_rpath_audit_is_not_skipped(self):
        image, original = self.clang(('/opt/homebrew/lib',))
        self.ignore_edits = True
        with self.assertRaisesRegex(ValueError, 'RPATH'):
            self.relocate(image, original)

    def test_post_edit_dependency_audit_is_not_skipped(self):
        image, original = self.clang()
        library = self.image(self.sdk / 'lib/libneeded.dylib', details())
        self.records[image]['dependencies'].append(str(library))
        self.ignore_edits = True
        with self.assertRaisesRegex(ValueError, 'dependency escaped'):
            self.relocate(image, original)

    def test_strict_signature_failure_is_not_ignored_on_unchanged_image(self):
        image, original = self.clang()
        self.bad_signature = True
        with self.assertRaises(subprocess.CalledProcessError):
            self.relocate(image, original)

    def test_necessary_edit_failure_is_not_ignored(self):
        image, original = self.clang(('/producer/llvm/lib',))
        self.reject_edit = True
        with self.assertRaises(subprocess.CalledProcessError):
            self.relocate(image, original)

    def test_host_guard_preserved(self):
        image, original = self.clang()
        with patch.object(macos.sys, 'platform', 'linux'), self.assertRaisesRegex(ValueError, 'native macOS'):
            self.relocate(image, original)


class Inspection(unittest.TestCase):
    def inspect(self, arch='arm64', minimum='14.0', platform='1'):
        loads = f'cmd LC_BUILD_VERSION\n cmdsize 32\n platform {platform}\n minos {minimum}\n'
        with patch.object(macos, 'output', side_effect=[arch, loads, 'image:\n /usr/lib/libSystem.B.dylib (version)\n', 'image:\n']):
            return macos.inspect(Path('/payload/image'))

    def test_native_macos_14_floor_passes(self):
        self.assertEqual(self.inspect()['minimum'], ['14.0'])

    def test_newer_floor_different_arch_and_platform_fail(self):
        for arguments in [dict(minimum='14.1'), dict(arch='x86_64'), dict(arch='arm64 x86_64'), dict(platform='IOS')]:
            with self.subTest(arguments=arguments), self.assertRaises(ValueError):
                self.inspect(**arguments)


if __name__ == '__main__':
    unittest.main()
