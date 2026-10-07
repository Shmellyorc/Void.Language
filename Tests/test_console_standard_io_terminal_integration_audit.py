#!/usr/bin/env python3
"""Focused #320 Console / standard IO / terminal integration audit."""
from pathlib import Path
import os
import re
import select
import shlex
import subprocess
import tempfile
import sys
if os.name != 'nt':
    import pty
    import termios
import time

root = Path(__file__).resolve().parents[1]
compiler = root / "bin" / "voidc"
project = "Tests/ConsoleStandardIoTerminalIntegrationAudit"
name = "ConsoleStandardIoTerminalIntegrationAudit"
input_data = b"first\rQ" + "β".encode() + b"\nlast\r\n"


def require(condition: bool, detail: str = "") -> None:
    if not condition:
        raise SystemExit(detail or "requirement failed")
    print("True")


def invoke(*args: str) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run([str(compiler), *args], cwd=root, capture_output=True)


def run_redirected(executable: Path) -> tuple[bytes, bytes]:
    result = subprocess.run(
        [str(executable)],
        cwd=root,
        input=input_data,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=30,
    )
    require(result.returncode == 0, result.stderr.decode(errors="replace"))
    newline = b'\r\n' if os.name == 'nt' else b'\n'
    require(result.stdout == b"DIRECT\nTEXT-\xce\xa9" + newline, repr(result.stdout))
    lines = result.stderr.splitlines()
    require(lines.count(b"True") == 24, repr(result.stderr))
    require(lines.count(b"RAWERR") == 1, repr(result.stderr))
    return result.stdout, result.stderr


def run_terminal_posix(executable: Path) -> tuple[bytes, bytes]:
    master, slave = pty.openpty()
    before = termios.tcgetattr(slave)
    process = subprocess.Popen(
        [str(executable)], cwd=root, stdin=slave, stdout=slave,
        stderr=subprocess.PIPE, close_fds=True,
    )
    assert process.stderr is not None
    stream = b""
    error = b""
    pending = b""
    position = [0, 0]
    sent_a = False
    sent_control_c = False
    deadline = time.monotonic() + 30.0
    while process.poll() is None and time.monotonic() < deadline:
        ready, _, _ = select.select([master, process.stderr], [], [], 0.05)
        if process.stderr in ready:
            chunk = os.read(process.stderr.fileno(), 4096)
            if chunk:
                error += chunk
        if master in ready:
            try:
                chunk = os.read(master, 4096)
            except OSError:
                chunk = b""
            if chunk:
                stream += chunk
                pending += chunk
                while True:
                    query = pending.find(b"\x1b[6n")
                    if query < 0:
                        if len(pending) > 512:
                            pending = pending[-128:]
                        break
                    prefix = pending[:query]
                    for match in re.finditer(br"\x1b\[(\d+);(\d+)H", prefix):
                        position[0] = int(match.group(2)) - 1
                        position[1] = int(match.group(1)) - 1
                    os.write(master, f"\x1b[{position[1] + 1};{position[0] + 1}R".encode())
                    pending = pending[query + 4:]

        true_count = sum(1 for line in error.splitlines() if line == b"True")
        if true_count >= 6 and not sent_a:
            os.write(master, b"a")
            sent_a = True
        if true_count >= 10 and not sent_control_c:
            os.write(master, b"\x03")
            sent_control_c = True

    if process.poll() is None:
        process.kill()
    _, remaining_error = process.communicate(timeout=5)
    if remaining_error:
        error += remaining_error
    after = termios.tcgetattr(slave)
    os.close(master)
    os.close(slave)

    require(process.returncode == 0, error.decode(errors="replace"))
    require(before == after, "terminal settings were not restored")
    require(error.splitlines().count(b"True") == 19, repr(error))
    require(sent_a and sent_control_c, "interactive input was not delivered")

    sequences = [
        b"\x1b[92m",                 # Green foreground
        b"\x1b[44m",                 # DarkBlue background
        b"\x1b[0m",                  # ResetColor
        b"\x1b[2J",                  # Clear
        b"\x1b[H",                   # home
        b"\x1b[?25l",                # cursor hidden
        b"\x1b[?25h",                # cursor visible
        b"\x1b[2;3H",                # SetCursorPosition(2, 1)
        b"\x1b]0;VOID #320\x07",     # title setter
        "Ω".encode(),
        b"!",
    ]
    for sequence in sequences:
        require(sequence in stream, repr(stream))
    require(stream.count(b"\x1b[6n") == 2, repr(stream))
    return stream, error


def run_terminal(executable: Path) -> tuple[bytes, bytes]:
    if os.name != 'nt':
        return run_terminal_posix(executable)
    from windows_console import run_terminal as run_windows
    result = run_windows(root, project, name, [(6, 'a', 65, 0), (10, '\x03', 67, 8)])
    stream, error = result['trace'], result['stderr']
    require(result['returncode'] == 0 and not result['timed_out'], repr(result))
    require(result['restored'], 'console input settings were not restored')
    require(error.splitlines().count(b'True') == 19, repr(result))
    require(result['sent'] == 2, 'interactive input was not delivered')
    # Presentation bytes execute against CONOUT$; title/cursor APIs are observed
    # only after the real Windows API succeeds, including a title read-back.
    for sequence in [b'\x1b[92m', b'\x1b[44m', b'\x1b[0m', b'\x1b[2J', b'\x1b[H',
                     b'\x1b[?25l', b'\x1b[?25h', b'\x1b[2;3H',
                     b'TITLE:VOID #320\n', '\u03a9'.encode(), b'!']:
        require(sequence in stream, repr(stream))
    require(stream.count(b'QUERY_CURSOR\n') == 2, repr(stream))
    return stream, error


result = invoke("check", project)
require(result.returncode == 0, result.stdout.decode() + result.stderr.decode())
result = invoke("build", project)
require(result.returncode == 0, result.stdout.decode() + result.stderr.decode())
exe = root / project / "bin" / name
if os.name == 'nt': exe = exe.with_suffix('.exe')
require(exe.is_file(), str(exe))
run_redirected(exe)
run_terminal(exe)

# Publish is part of the final integration audit: the packaged executable must
# retain the same standard-IO behavior rather than only working from debug bin/.
result = invoke("publish", project)
require(result.returncode == 0, result.stdout.decode() + result.stderr.decode())
published = root / project / "publish" / name
if os.name == 'nt': published = published.with_suffix('.exe')
require(published.is_file(), str(published))
run_redirected(published)

# Namespace/path organization remains a hard invariant at the end of the block.
result = subprocess.run(
    [sys.executable, "Tests/test_standard_library_namespace_layout.py"],
    cwd=root, capture_output=True,
)
require(result.returncode == 0, result.stdout.decode() + result.stderr.decode())

console = (root / "StandardLibrary/Void/Console.void").read_text()
terminal = (root / "StandardLibrary/Void/IO/StandardTerminal.void").read_text()
standard_input = (root / "StandardLibrary/Void/IO/StandardInput.void").read_text()
standard_output = (root / "StandardLibrary/Void/IO/StandardOutput.void").read_text()
standard_streams = (root / "StandardLibrary/Void/IO/StandardStreams.void").read_text()
compiler_source = (root / "Compiler/src/compiler.c").read_text()
semantic_source = (root / "Compiler/src/semantic.c").read_text()
readme = (root / "Docs/README.md").read_text()

require(
    "public static int Read()" in console
    and "public static string ReadLine()" in console
    and "public static ConsoleKeyInfo ReadKey()" in console
    and "public static bool KeyAvailable" in console
)
require(
    "public static TextReader In" in console
    and "public static TextWriter Out" in console
    and "public static TextWriter Error" in console
    and "public static void SetIn" in console
    and "public static void SetOut" in console
    and "public static void SetError" in console
)
require(
    "public static Stream OpenStandardInput()" in console
    and "public static Stream OpenStandardOutput()" in console
    and "public static Stream OpenStandardError()" in console
)
require(
    "public static Encoding InputEncoding" in console
    and "public static Encoding OutputEncoding" in console
    and "public static bool TreatControlCAsInput" in console
)
require(
    "public static ConsoleColor ForegroundColor" in console
    and "public static void Clear()" in console
    and "public static int CursorLeft" in console
    and "public static string Title" in console
)
require("_pendingByte" in standard_input and "Console.InputEncoding" in standard_input)
require("Runtime.StandardStreamWriteUtf8(stream, value)" in standard_output and "StandardStreams.Flush(stream)" in standard_output)
require("Runtime.StandardStreamRead" in standard_input and "Runtime.StandardStreamWrite" in standard_streams and "Runtime.StandardStreamFlush" in standard_streams and "StandardStreamWriteUtf8" in compiler_source + semantic_source)
require("Runtime.TerminalReadEvent" in terminal and "Runtime.TerminalKeyAvailable" in terminal)
require("Runtime.TerminalPrepareOutput" in terminal and "Runtime.TerminalGetCursorPosition" in terminal)
require("_outIsStandard" in console and "StandardOutput.Write(value)" in console)
require("Runtime.Console" not in compiler_source + semantic_source + terminal + standard_streams)
require("CancelKeyPress" in readme and "deferred" in readme.lower())

# The low-level UTF-8 stream helper must remain StandardLibrary-only.
result = invoke("check", "Tests/ConsoleStandardIoTerminalIntegrationAuditDiagnostics/RuntimeStandardStreamWriteUtf8Access")
require(
    result.returncode != 0
    and b"Runtime.StandardStreamWriteUtf8 is reserved for Void.IO.StandardOutput" in result.stderr,
    result.stderr.decode(errors="replace"),
)

# Regression exposed by the final cumulative audit: a native atexit callback must still
# be able to use the public Console.WriteLine path after managed static-root teardown.
result = invoke("run", "Tests/NativeCallbacks")
require(
    result.returncode == 0 and result.stdout.splitlines()[-4:] == [b"True", b"12", b"37", b"99"],
    result.stdout.decode(errors="replace") + result.stderr.decode(errors="replace"),
)

# The final Console block must still be strict-C clean at generated-code level.
with tempfile.TemporaryDirectory(prefix="void320-strict-") as temporary:
    generated = root / project / ".void" / f"{name}.c"
    result = subprocess.run(
        shlex.split(os.environ.get("CC", "cc"))
        + [
            "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-O2",
            "-ICompiler/include", "-IRuntime/include", "-pthread", "-c",
            str(generated), "-o", str(Path(temporary) / "checked.o"),
        ],
        cwd=root, capture_output=True,
    )
    require(result.returncode == 0 and not result.stderr, result.stderr.decode(errors="replace"))
