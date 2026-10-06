"""Hidden native Windows console harness; no PTY emulation or mocked console APIs."""
from pathlib import Path
import json
import os
import shlex
import subprocess
import sys
import tempfile


def run_terminal(root, project, name, keys=(), stdin_only=False, native_probe=False):
    with tempfile.TemporaryDirectory(prefix='void-console-') as temporary:
        directory = Path(temporary)
        executable = directory / 'console.exe'
        generated = root / project / '.void' / (name + '.c')
        # Only the generated translation unit is instrumented. Every intercepted
        # call still executes against the real console; the trace records success.
        object_file = directory / 'generated.o'
        flags = ['-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2',
                 '-IRuntime/include', '-pthread']
        cc = shlex.split(os.environ.get('CC', 'cc'))
        unit = root / 'Tests/ConsoleNative/key_probe.c' if native_probe else generated
        defines = ['-DVOID_GENERATED_SOURCE="' + generated.as_posix() + '"'] if native_probe else []
        result = subprocess.run(cc + flags + defines + ['-include', 'Tests/ConsoleNative/hooks.h',
            '-c', str(unit), '-o', str(object_file)], cwd=root, capture_output=True)
        assert result.returncode == 0 and not result.stderr, (result.stdout, result.stderr)
        result = subprocess.run(cc + flags + [str(object_file), 'Tests/ConsoleNative/hooks.c',
            'Runtime/src/vc_thread.c', 'Runtime/src/vc_atomic.c', 'Runtime/src/vc_memory.c',
            'Runtime/src/vc_filesystem.c', '-o', str(executable)], cwd=root, capture_output=True)
        assert result.returncode == 0 and not result.stderr, (result.stdout, result.stderr)
        startup = subprocess.STARTUPINFO()
        startup.dwFlags = subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
        trace = directory / 'trace'
        trace.write_bytes(b'')
        result = subprocess.run([sys.executable, str(Path(__file__).resolve()),
            str(executable), str(root), str(trace), json.dumps(keys), str(int(stdin_only))],
            capture_output=True, timeout=45, startupinfo=startup,
            creationflags=subprocess.CREATE_NEW_CONSOLE)
        assert result.returncode == 0, (result.stdout, result.stderr)
        data = json.loads(result.stdout)
        data['stdout'] = bytes.fromhex(data['stdout'])
        data['stderr'] = bytes.fromhex(data['stderr'])
        data['trace'] = trace.read_bytes()
        return data


def worker():
    import ctypes as c
    from ctypes import wintypes as w
    import threading
    import time

    executable, root, trace, payload, stdin_only = sys.argv[1:]
    kernel = c.WinDLL('kernel32', use_last_error=True)
    user = c.WinDLL('user32', use_last_error=True)
    class COORD(c.Structure):
        _fields_ = [('x', c.c_short), ('y', c.c_short)]
    class RECT(c.Structure):
        _fields_ = [('left', c.c_short), ('top', c.c_short), ('right', c.c_short), ('bottom', c.c_short)]
    class KEY(c.Structure):
        _fields_ = [('down', w.BOOL), ('repeat', w.WORD), ('vk', w.WORD),
                    ('scan', w.WORD), ('char', w.WCHAR), ('control', w.DWORD)]
    class EVENT(c.Union):
        _fields_ = [('key', KEY), ('padding', c.c_byte * 16)]
    class RECORD(c.Structure):
        _fields_ = [('kind', w.WORD), ('event', EVENT)]
    kernel.GetConsoleMode.argtypes = [w.HANDLE, c.POINTER(w.DWORD)]
    kernel.SetConsoleWindowInfo.argtypes = [w.HANDLE, w.BOOL, c.POINTER(RECT)]
    kernel.SetConsoleScreenBufferSize.argtypes = [w.HANDLE, COORD]
    kernel.SetConsoleCursorPosition.argtypes = [w.HANDLE, COORD]
    kernel.WriteConsoleInputW.argtypes = [w.HANDLE, c.POINTER(RECORD), w.DWORD, c.POINTER(w.DWORD)]
    user.MapVirtualKeyW.argtypes = [w.UINT, w.UINT]
    user.MapVirtualKeyW.restype = w.UINT
    import msvcrt
    input_file = open('CONIN$', 'rb', buffering=0)
    output_file = open('CONOUT$', 'wb', buffering=0)
    # Match the POSIX PTY's viewport-only screen: inherited Windows scrollback
    # otherwise makes native buffer coordinates differ from ANSI row positions.
    output_handle = w.HANDLE(msvcrt.get_osfhandle(output_file.fileno()))
    window = RECT(0, 0, 79, 24)
    assert kernel.SetConsoleWindowInfo(output_handle, True, c.byref(window)), c.get_last_error()
    assert kernel.SetConsoleScreenBufferSize(output_handle, COORD(80, 25)), c.get_last_error()
    assert kernel.SetConsoleCursorPosition(output_handle, COORD(0, 0)), c.get_last_error()
    handle = w.HANDLE(msvcrt.get_osfhandle(input_file.fileno()))
    before = w.DWORD()
    assert kernel.GetConsoleMode(handle, c.byref(before)), c.get_last_error()
    environment = dict(os.environ, VOID_TEST_CONSOLE_TRACE=trace)
    process = subprocess.Popen([executable], cwd=root, env=environment,
        stdin=input_file, stdout=subprocess.PIPE if int(stdin_only) else output_file,
        stderr=subprocess.PIPE)
    output = bytearray()
    error = bytearray()
    def collect(file, data):
        while chunk := file.read(1):
            data.extend(chunk)
    threads = [threading.Thread(target=collect, args=(process.stderr, error))]
    if int(stdin_only):
        threads.append(threading.Thread(target=collect, args=(process.stdout, output)))
    for thread in threads:
        thread.start()
    keys = json.loads(payload)
    sent = 0
    deadline = time.monotonic() + 30
    while process.poll() is None and time.monotonic() < deadline:
        reports = bytes(output if int(stdin_only) else error).splitlines().count(b'True')
        while sent < len(keys) and reports >= keys[sent][0]:
            _, character, vk, control, *repeat = keys[sent]
            record = RECORD()
            record.kind = 1  # KEY_EVENT
            record.event.key = KEY(True, repeat[0] if repeat else 1, vk,
                user.MapVirtualKeyW(vk, 0), character, control)
            actual = w.DWORD()
            assert kernel.WriteConsoleInputW(handle, c.byref(record), 1, c.byref(actual)) and actual.value == 1
            sent += 1
        time.sleep(.005)
    timed_out = process.poll() is None
    if timed_out:
        process.kill()
    process.wait(timeout=5)
    for thread in threads:
        thread.join(timeout=5)
        assert not thread.is_alive()
    after = w.DWORD()
    assert kernel.GetConsoleMode(handle, c.byref(after)), c.get_last_error()
    input_file.close()
    output_file.close()
    print(json.dumps(dict(returncode=process.returncode, stdout=output.hex(), stderr=error.hex(),
        restored=before.value == after.value, sent=sent, timed_out=timed_out)))


if __name__ == '__main__':
    worker()
