"""Platform contracts for native acceptance fixtures."""
import os
import json
import re
import subprocess
import sys
from contextlib import ExitStack, contextmanager
from pathlib import Path
import time


def relocate(source, target):
    """Require a real fixture rename, with bounded Windows sharing retries."""
    deadline = time.monotonic() + 2
    attempt = 0
    while True:
        try:
            source.rename(target)
            return
        except OSError as error:
            if os.name != 'nt' or error.winerror not in (5, 32) or time.monotonic() >= deadline:
                raise
            attempt += 1
            print(f'Windows installation relocation denied: native={error.winerror}; retry={attempt}', flush=True)
            time.sleep(0.05)


def native_manifest(compiler):
    """Read the same v1/v2 link manifest used by the native CLI."""
    directory = Path(compiler).resolve().parent
    lines = (directory / 'void-native.txt').read_text(encoding='utf-8').splitlines()
    if not lines or lines[0] not in ('VOID native 23.1.4 v1', 'VOID native 23.1.4 v2'):
        raise ValueError('Unsupported native link manifest')
    return [line.replace('@VOID_NATIVE_DIR@', str(directory)) for line in lines]


def native_text(text):
    """Expected UTF-8 CRT text; never apply this to captured or wire bytes."""
    return text.replace('\n', os.linesep).encode('utf-8')


def windows_package_imports(readobj, image):
    """Inspect direct and delay-loaded imports of a native Windows fixture."""
    headers = subprocess.check_output([readobj, '--file-headers', '--coff-imports', image], text=True, timeout=30)
    assert 'IMAGE_FILE_MACHINE_AMD64 (0x8664)' in headers and 'Magic: 0x20B' in headers, headers
    imports = sorted(set(re.findall(r'^\s*Name:\s*(\S+\.dll)\s*$', headers, re.I | re.M)))
    crt = re.compile(r'(?:msvcrt|msvcr\d.*|msvcp\d.*|vcruntime\d.*|ucrtbased?|api-ms-win-crt-.*)\.dll', re.I)
    assert imports and not any(crt.fullmatch(name) for name in imports), headers
    print(f'{image.name} PE32+ AMD64 imports: {", ".join(imports)}', flush=True)


@contextmanager
def _windows_deny_fixture_reads(root, sid):
    # Retain WRITE_DAC and the original descriptor before denying access. An
    # owner with a standard token can restore by handle without SeRestorePrivilege.
    import ctypes as c
    from ctypes import wintypes as w

    kernel, security = c.windll.kernel32, c.windll.advapi32
    kernel.CreateFileW.argtypes = [w.LPCWSTR, w.DWORD, w.DWORD, w.LPVOID, w.DWORD, w.DWORD, w.HANDLE]
    kernel.CreateFileW.restype = w.HANDLE
    kernel.CloseHandle.argtypes = [w.HANDLE]
    kernel.LocalFree.argtypes = [w.LPVOID]
    kernel.LocalFree.restype = w.LPVOID
    pointer = c.POINTER(w.LPVOID)
    security.GetSecurityInfo.argtypes = [w.HANDLE, w.DWORD, w.DWORD, pointer, pointer, pointer, pointer, pointer]
    security.SetSecurityInfo.argtypes = [w.HANDLE, w.DWORD, w.DWORD, w.LPVOID, w.LPVOID, w.LPVOID, w.LPVOID]
    security.GetSecurityDescriptorControl.argtypes = [w.LPVOID, c.POINTER(w.WORD), c.POINTER(w.DWORD)]

    read_control, write_dac = 0x20000, 0x40000
    share_all, open_existing, directory = 7, 3, 0x02000000
    file_object, dacl_information = 1, 4
    dacl_protected, protect, unprotect = 0x1000, 0x80000000, 0x20000000
    handle = kernel.CreateFileW(str(root), read_control | write_dac, share_all, None, open_existing, directory, None)
    if handle == c.c_void_p(-1).value:
        raise c.WinError()
    descriptor, dacl = w.LPVOID(), w.LPVOID()
    try:
        status = security.GetSecurityInfo(handle, file_object, dacl_information, None, None,
                                          c.byref(dacl), None, c.byref(descriptor))
        if status:
            raise c.WinError(status)
        control, revision = w.WORD(), w.DWORD()
        if not security.GetSecurityDescriptorControl(descriptor, c.byref(control), c.byref(revision)):
            raise c.WinError()
        with ExitStack() as files:
            # Deny content opens even when the token can bypass directory
            # traversal checks. File reservations avoid inheritable deny ACEs.
            generic_read, open_reparse_point = 0x80000000, 0x00200000
            for path in root.rglob('*'):
                assert not path.is_symlink() and not path.is_junction(), path
                if path.is_dir():
                    continue
                pinned = kernel.CreateFileW(str(path), generic_read, 0, None, open_existing, open_reparse_point, None)
                if pinned == c.c_void_p(-1).value:
                    raise c.WinError()
                files.callback(kernel.CloseHandle, pinned)
            try:
                subprocess.run(['icacls.exe', root, '/deny', f'*{sid}:(RX)'],
                               check=True, capture_output=True, timeout=30)
                yield
            finally:
                files.close()
                flags = dacl_information | (protect if control.value & dacl_protected else unprotect)
                status = security.SetSecurityInfo(handle, file_object, flags, None, None, dacl, None)
                if status:
                    raise c.WinError(status)
    finally:
        if descriptor:
            kernel.LocalFree(descriptor)
        kernel.CloseHandle(handle)


def windows_isolated_application(image, args, *, blocked, cwd, env, timeout):
    """Deny reads of test-owned roots, then check denial and run in one child."""
    identity = subprocess.check_output(['whoami.exe', '/user', '/fo', 'csv', '/nh'], text=True)
    sid = re.search(r'\bS-1-[\d-]+', identity)[0]
    probes = []
    with ExitStack() as restore:
        for root, file in blocked:
            assert root.is_dir() and not root.is_symlink() and not root.is_junction() and (root / file).is_file()
            restore.enter_context(_windows_deny_fixture_reads(root, sid))
            probes.extend((root, root / file))
        code = '''
import subprocess
import sys
from pathlib import Path
count = int(sys.argv[1])
for index, name in enumerate(sys.argv[2:2 + count]):
    path = Path(name)
    try:
        if index % 2:
            with path.open("rb") as stream:
                stream.read(1)
        else:
            list(path.iterdir())
    except PermissionError:
        continue
    raise SystemExit(f"Isolation read unexpectedly succeeded: {path}")
raise SystemExit(subprocess.run(sys.argv[2 + count:], timeout=60).returncode)
'''
        return subprocess.run([sys.executable, '-B', '-c', code, str(len(probes)), *map(str, probes),
                               str(image), *args], cwd=cwd, env=env, capture_output=True, timeout=timeout)


def small_stack():
    """Restrict the native child before exec, not the Python launcher's stack."""
    import resource
    before = resource.getrlimit(resource.RLIMIT_STACK)
    target = 1048576
    try:
        resource.setrlimit(resource.RLIMIT_STACK, (target, target))
    except (OSError, ValueError) as error:
        raise RuntimeError(f'Cannot enforce 1 MiB stack; inherited limits={before}') from error
    observed = resource.getrlimit(resource.RLIMIT_STACK)
    assert observed == (target, target), (before, observed)
    os.write(2, ('VOID_TEST_STACK ' + json.dumps({'inherited': before, 'observed': observed}) + '\n').encode('ascii'))


def report_stack(logs):
    """Retain the child's queried limits in passing CTest output too."""
    prefix = b'VOID_TEST_STACK '
    records = [json.loads(line[len(prefix):]) for line in bytes(logs).splitlines() if line.startswith(prefix)]
    assert len(records) == 1 and records[0]['observed'] == [1048576, 1048576], records
    print('VOID_TEST_STACK ' + json.dumps(records[0]))
