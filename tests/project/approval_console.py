"""Drive a real isolated POSIX terminal or Windows 10 ConPTY, not a piped answer."""
import errno
import os
import queue
import re
import subprocess
import sys
import threading
import time

PROMPT = b"Type approve"
VT = re.compile(rb'\x1b(?:\[[0-?]*[ -/]*[@-~]|\][^\x07\x1b]*(?:\x07|\x1b\\))')


def _posix(argv, cwd, env):
    import pty
    import signal

    pid, fd = pty.fork()
    if pid == 0:
        try:
            os.chdir(cwd)
            os.execve(argv[0], argv, env)
        finally:
            os._exit(127)

    def read():
        try:
            return os.read(fd, 4096)
        except OSError as error:
            if error.errno == errno.EIO:
                return b''
            raise

    def finish(reached_eof):
        if not reached_eof:
            try:
                os.kill(pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
        os.close(fd)
        return os.waitstatus_to_exitcode(os.waitpid(pid, 0)[1])

    return read, lambda line: os.write(fd, line + b'\n'), finish


def _windows(argv, cwd, env):
    import ctypes as c
    from ctypes import wintypes as wt
    import msvcrt

    # A redirected parent can pass its stdout/stderr even to a process
    # attached to ConPTY. Reopen its actual devices with read access (needed
    # by GetConsoleMode), then pass those handles to the test executable.
    bootstrap = """
import os, subprocess, sys
with os.fdopen(os.open("CONIN$", os.O_RDONLY | os.O_BINARY), "rb", 0) as source, \
     os.fdopen(os.open("CONOUT$", os.O_RDWR | os.O_BINARY), "wb", 0) as sink:
    raise SystemExit(subprocess.run(sys.argv[1:], stdin=source, stdout=sink, stderr=sink).returncode)
"""
    argv = [sys.executable, '-I', '-c', bootstrap, *argv]
    k = c.WinDLL('kernel32', use_last_error=True)
    handle = wt.HANDLE
    size = c.c_size_t

    class Coord(c.Structure):
        _fields_ = [('x', c.c_short), ('y', c.c_short)]

    class Startup(c.Structure):
        _fields_ = [('cb', wt.DWORD)] + [(name, wt.LPWSTR) for name in ('reserved', 'desktop', 'title')] + [
            (name, wt.DWORD) for name in ('x', 'y', 'xsize', 'ysize', 'xchars', 'ychars', 'fill', 'flags')] + [
            ('show', wt.WORD), ('reserved_size', wt.WORD), ('reserved_data', c.c_void_p),
            ('in', handle), ('out', handle), ('error', handle)]

    class Extended(c.Structure):
        _fields_ = [('startup', Startup), ('attrs', c.c_void_p)]

    class Process(c.Structure):
        _fields_ = [('process', handle), ('thread', handle), ('pid', wt.DWORD), ('tid', wt.DWORD)]

    def signature(name, result, *args):
        method = getattr(k, name)
        method.argtypes, method.restype = args, result
        return method

    pipe = signature('CreatePipe', wt.BOOL, c.POINTER(handle), c.POINTER(handle), c.c_void_p, wt.DWORD)
    close = signature('CloseHandle', wt.BOOL, handle)
    create_console = signature('CreatePseudoConsole', c.c_long, Coord, handle, handle, wt.DWORD, c.POINTER(handle))
    close_console = signature('ClosePseudoConsole', None, handle)
    init = signature('InitializeProcThreadAttributeList', wt.BOOL, c.c_void_p, wt.DWORD, wt.DWORD, c.POINTER(size))
    delete = signature('DeleteProcThreadAttributeList', None, c.c_void_p)
    update = signature('UpdateProcThreadAttribute', wt.BOOL, c.c_void_p, wt.DWORD, size,
                       c.c_void_p, size, c.c_void_p, c.c_void_p)
    create = signature('CreateProcessW', wt.BOOL, wt.LPCWSTR, wt.LPWSTR, c.c_void_p, c.c_void_p,
                       wt.BOOL, wt.DWORD, c.c_void_p, wt.LPCWSTR, c.POINTER(Extended), c.POINTER(Process))
    wait = signature('WaitForSingleObject', wt.DWORD, handle, wt.DWORD)
    terminate = signature('TerminateProcess', wt.BOOL, handle, wt.UINT)
    exit_code = signature('GetExitCodeProcess', wt.BOOL, handle, c.POINTER(wt.DWORD))

    def checked(ok):
        if not ok:
            raise c.WinError(c.get_last_error())

    ir, iw, er, ew, con = (handle() for _ in range(5))
    process = Process()
    attributes = None
    try:
        checked(pipe(c.byref(ir), c.byref(iw), None, 0))
        checked(pipe(c.byref(er), c.byref(ew), None, 0))
        hr = create_console(Coord(200, 40), ir, ew, 0, c.byref(con))
        if hr:
            raise RuntimeError(f'CreatePseudoConsole failed: {hr:#x}')
        close(ir)
        ir = handle()
        close(ew)
        ew = handle()
        needed = size()
        init(None, 1, 0, c.byref(needed))
        storage = c.create_string_buffer(needed.value)
        checked(init(storage, 1, 0, c.byref(needed)))
        attributes = storage
        checked(update(attributes, 0, 0x20016, con, c.sizeof(con), None, None))
        startup = Extended()
        startup.startup.cb = c.sizeof(startup)
        startup.attrs = c.addressof(storage)
        block = c.create_unicode_buffer(''.join(f'{k}={v}\0' for k, v in sorted(env.items(), key=lambda p: p[0].upper())) + '\0')
        cmd = c.create_unicode_buffer(subprocess.list2cmdline(argv))
        # EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT
        checked(create(argv[0], cmd, None, None, False, 0x80400, block, str(cwd),
                       c.byref(startup), c.byref(process)))
        close(process.thread)
    except BaseException:
        if con:
            close_console(con)
        for h in (ir, iw, er, ew):
            if h:
                close(h)
        raise
    finally:
        if attributes is not None:
            delete(attributes)

    out = msvcrt.open_osfhandle(er.value, os.O_RDONLY | os.O_BINARY)
    inp = msvcrt.open_osfhandle(iw.value, os.O_WRONLY | os.O_BINARY)

    def finish(reached_eof):
        try:
            result = wait(process.process, 30000 if reached_eof else 0)
            if result == 258:
                terminate(process.process, 255)
                wait(process.process, 10000)
            elif result != 0:
                raise c.WinError(c.get_last_error())
            status = wt.DWORD()
            checked(exit_code(process.process, c.byref(status)))
            return status.value
        finally:
            close(process.process)
            os.close(inp)
            # The reader remains active while conhost flushes its output.
            close_console(con)

    return lambda: os.read(out, 4096), lambda line: os.write(inp, line + b'\r'), finish, lambda: os.close(out), lambda: wait(process.process, 0) == 0


def interact(compiler, args, cwd, env, *, reply=None, change_at_prompt=None, status=0, contains=b''):
    argv = [str(compiler), *map(str, args)]
    endpoint = (_windows if sys.platform == 'win32' else _posix)(argv, cwd, env)
    read, write, finish = endpoint[:3]
    cleanup, exited = endpoint[3:] if len(endpoint) > 3 else (lambda: None, lambda: False)
    chunks = queue.Queue()

    def reader():
        try:
            while data := read():
                chunks.put(data)
        except OSError as error:
            if error.errno != errno.EPIPE:
                chunks.put(error)
        finally:
            chunks.put(None)

    thread = threading.Thread(target=reader, daemon=True)
    thread.start()
    before = time.monotonic() + 120
    output = bytearray()
    answered, done = False, False
    try:
        while time.monotonic() < before:
            try:
                chunk = chunks.get(timeout=0.2)
            except queue.Empty:
                if exited():
                    # ConPTY keeps its pipe open until ClosePseudoConsole.
                    done = True
                    break
                continue
            if chunk is None:
                done = True
                break
            if isinstance(chunk, OSError):
                raise chunk
            output.extend(chunk)
            assert len(output) <= 256 * 1024, output[-2048:]
            if not answered and PROMPT in VT.sub(b'', output):
                assert reply is not None, output
                if change_at_prompt is not None:
                    change_at_prompt()
                write(reply)
                answered = True
    finally:
        code = finish(done)
        thread.join(5)
        cleanup()
    while not chunks.empty():
        chunk = chunks.get()
        if isinstance(chunk, bytes):
            output.extend(chunk)
    output = VT.sub(b'', output)
    assert done and code == status, (argv, code, output)
    assert answered == (reply is not None) and contains in output, (argv, output)
    assert all(s not in output for s in (b'AddressSanitizer', b'LeakSanitizer', b'UndefinedBehaviorSanitizer', b'runtime error:')), output
    return bytes(output)
