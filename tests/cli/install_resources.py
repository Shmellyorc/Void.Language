"""Installed resource alias, containment, checksum and missing-file regressions."""
from pathlib import Path
import hashlib
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from verify_install import verify_native_resources

class Resources(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='VOID inventory 雪 ')
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.prefix = self.base / 'installation'
        (self.prefix / 'bin').mkdir(parents=True)
        (self.prefix / 'lib/void').mkdir(parents=True)
        (self.base / 'nested').mkdir()
    def inventory(self, names):
        lines = ['VOID native resources 1']
        for name in names:
            file = self.prefix / 'lib/void' / name
            file.write_bytes(b'archive fixture')
            lines.append(hashlib.sha256(file.read_bytes()).hexdigest() + '\t@VOID_NATIVE_DIR@/../lib/void/' + name)
        (self.prefix / 'bin/void-native-resources.txt').write_text('\n'.join(lines)+'\n')
    def test_windows_and_macos_names_through_alias(self):
        for names in [('void_runtime.lib','void_native_fixture.lib'),('libvoid_runtime.a','libvoid_native_fixture.a')]:
            self.inventory(names)
            verify_native_resources(self.base / 'nested/../installation')
    def test_symlink_prefix(self):
        self.inventory(('libvoid_runtime.a','fixture.a'))
        alias = self.base / 'alias'
        try: alias.symlink_to(self.prefix, target_is_directory=True)
        except OSError as e:
            if sys.platform != 'win32' or e.winerror != 1314:raise
            self.skipTest('Native symlink privilege unavailable: '+str(e))
        verify_native_resources(alias)
    @unittest.skipUnless(sys.platform == 'win32', 'Windows short-path API')
    def test_windows_short_path(self):
        import ctypes
        self.inventory(('void_runtime.lib','fixture.lib'))
        kernel=ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.GetShortPathNameW.argtypes=[ctypes.c_wchar_p,ctypes.c_wchar_p,ctypes.c_uint32]
        buffer=ctypes.create_unicode_buffer(32768)
        self.assertTrue(kernel.GetShortPathNameW(str(self.prefix),buffer,len(buffer)))
        if buffer.value == str(self.prefix):self.skipTest('8.3 aliases unavailable on this volume')
        verify_native_resources(Path(buffer.value))
    def test_checksum_tamper(self):
        self.inventory(('void_runtime.lib','fixture.lib'))
        (self.prefix/'lib/void/void_runtime.lib').write_bytes(b'tampered')
        with self.assertRaisesRegex(RuntimeError, 'checksum mismatch'): verify_native_resources(self.prefix)
    def test_missing_resource(self):
        self.inventory(('libvoid_runtime.a','fixture.a'))
        (self.prefix/'lib/void/libvoid_runtime.a').unlink()
        with self.assertRaises(FileNotFoundError): verify_native_resources(self.prefix)
    def test_traversal(self):
        self.inventory(('runtime.a','fixture.a'))
        path=self.prefix/'bin/void-native-resources.txt'
        path.write_text(path.read_text().replace('/lib/void/runtime.a','/lib/void/../../escape'))
        (self.prefix/'escape').write_bytes(b'archive fixture')
        with self.assertRaisesRegex(RuntimeError, 'does not belong'): verify_native_resources(self.prefix)
    def test_resource_symlink_escape(self):
        self.inventory(('runtime.a','fixture.a'))
        outside=self.base/'outside';outside.write_bytes(b'archive fixture')
        path=self.prefix/'lib/void/runtime.a';path.unlink()
        try:path.symlink_to(outside)
        except OSError as e:
            if sys.platform != 'win32' or e.winerror != 1314:raise
            self.skipTest('Native symlink privilege unavailable: '+str(e))
        with self.assertRaisesRegex(RuntimeError, 'does not belong'):verify_native_resources(self.prefix)

if __name__=='__main__':unittest.main()
