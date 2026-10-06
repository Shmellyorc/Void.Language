#!/usr/bin/env python3
"""Focused #319 interactive Console presentation checks."""
from pathlib import Path
import os
import re
import select
import shlex
import subprocess
import tempfile
if os.name != 'nt':
    import pty
    import termios
import time

root = Path(__file__).resolve().parents[1]
compiler = root / "bin" / "voidc"
project = "Tests/ConsolePresentationBasics"


def require(condition: bool, detail: str = "") -> None:
    if not condition:
        raise SystemExit(detail or "requirement failed")
    print("True")


def invoke(*args: str) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run([str(compiler), *args], cwd=root, capture_output=True)


result = invoke("check", project)
require(result.returncode == 0, result.stdout.decode() + result.stderr.decode())
result = invoke("build", project)
require(result.returncode == 0, result.stdout.decode() + result.stderr.decode())
exe = root / project / "bin" / "ConsolePresentationBasics"

# Redirected stdout: presentation APIs must not pretend a pipe is a terminal.
result = subprocess.run([str(exe)], stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=20)
require(result.returncode == 0, result.stderr.decode(errors="replace"))
require(result.stdout == b"", repr(result.stdout))
redirected = result.stderr.splitlines()
require(len(redirected) == 8, repr(result.stderr))
for line in redirected:
    require(line == b"True", repr(result.stderr))

# PTY-backed terminal presentation behavior.
if os.name == 'nt':
    from windows_console import run_terminal
    result = run_terminal(root, project, 'ConsolePresentationBasics')
    stream, error = result['trace'], result['stderr']
    require(result['returncode'] == 0 and not result['timed_out'], repr(result))
    require(result['restored'], 'console settings were not restored')
    terminal_lines = error.splitlines()
    require(len(terminal_lines) == 15, repr(error))
    for line in terminal_lines:
        require(line == b'True', repr(error))
    for sequence in [b'\x1b[91m', b'\x1b[44m', b'\x1b[0m', b'\x1b[2J', b'\x1b[H',
                     b'\x1b[?25l', b'\x1b[?25h', b'\x1b[3;5H', b'\x1b[3;8H',
                     b'\x1b[5;8H', b'TITLE:VOID #319\n']:
        require(sequence in stream, repr(stream))
    require(stream.count(b'QUERY_CURSOR\n') == 6, repr(stream))
else:
    master, slave = pty.openpty()
    before = termios.tcgetattr(slave)
    process = subprocess.Popen(
        [str(exe)], cwd=root, stdin=slave, stdout=slave, stderr=subprocess.PIPE, close_fds=True
    )
    assert process.stderr is not None
    stream = b""
    pending = b""
    position = [0, 0]
    deadline = time.monotonic() + 30.0
    while process.poll() is None and time.monotonic() < deadline:
        ready, _, _ = select.select([master], [], [], 0.1)
        if master not in ready:
            continue
        try:
            chunk = os.read(master, 4096)
        except OSError:
            break
        if not chunk:
            break
        stream += chunk
        pending += chunk
        while True:
            query = pending.find(b"\x1b[6n")
            if query < 0:
                if len(pending) > 256:
                    pending = pending[-64:]
                break
            prefix = pending[:query]
            for match in re.finditer(br"\x1b\[(\d+);(\d+)H", prefix):
                position[0] = int(match.group(2)) - 1
                position[1] = int(match.group(1)) - 1
            os.write(master, f"\x1b[{position[1] + 1};{position[0] + 1}R".encode())
            pending = pending[query + 4:]

    if process.poll() is None:
        process.kill()
    _, error = process.communicate(timeout=5)
    after = termios.tcgetattr(slave)
    os.close(master)
    os.close(slave)

    require(process.returncode == 0, error.decode(errors="replace"))
    require(before == after, "terminal settings were not restored")
    terminal_lines = error.splitlines()
    require(len(terminal_lines) == 15, repr(error))
    for line in terminal_lines:
        require(line == b"True", repr(error))

    expected_sequences = [
        b"\x1b[91m",       # Red foreground
        b"\x1b[44m",       # DarkBlue background
        b"\x1b[0m",        # ResetColor
        b"\x1b[2J",        # Clear screen
        b"\x1b[H",         # Home after Clear
        b"\x1b[?25l",      # hide cursor
        b"\x1b[?25h",      # show cursor
        b"\x1b[3;5H",      # SetCursorPosition(4, 2)
        b"\x1b[3;8H",      # CursorLeft = 7
        b"\x1b[5;8H",      # CursorTop = 4
        b"\x1b]0;VOID #319\x07",  # title setter
    ]
    for sequence in expected_sequences:
        require(sequence in stream, repr(stream))
    require(stream.count(b"\x1b[6n") == 6, repr(stream))


# Low-level terminal presentation capabilities remain StandardLibrary-only.
diagnostics = [
    ("RuntimeTerminalPrepareOutputAccess", b"Runtime.TerminalPrepareOutput is reserved for Void.IO.StandardTerminal"),
    ("RuntimeTerminalGetCursorPositionAccess", b"Runtime.TerminalGetCursorPosition is reserved for Void.IO.StandardTerminal"),
    ("RuntimeTerminalGetTitleAccess", b"Runtime.TerminalGetTitle is reserved for Void.IO.StandardTerminal"),
    ("RuntimeTerminalSetTitleAccess", b"Runtime.TerminalSetTitle is reserved for Void.IO.StandardTerminal"),
]
for name, expected in diagnostics:
    result = invoke("check", f"Tests/ConsolePresentationBasicsDiagnostics/{name}")
    require(result.returncode != 0 and expected in result.stderr, result.stderr.decode(errors="replace"))

console = (root / "StandardLibrary/Void/Console.void").read_text()
terminal = (root / "StandardLibrary/Void/IO/StandardTerminal.void").read_text()
compiler_source = (root / "Compiler/src/compiler.c").read_text()
semantic_source = (root / "Compiler/src/semantic.c").read_text()

require(
    "public static ConsoleColor ForegroundColor" in console
    and "public static ConsoleColor BackgroundColor" in console
    and "public static void ResetColor()" in console
    and "public static void Clear()" in console
)
require(
    "public static int CursorLeft" in console
    and "public static int CursorTop" in console
    and "public static bool CursorVisible" in console
    and "public static void SetCursorPosition(int left, int top)" in console
)
require("public static string Title" in console and "PlatformNotSupportedException" in terminal)
require(
    "Runtime.TerminalPrepareOutput()" in terminal
    and "Runtime.TerminalGetCursorPosition(position)" in terminal
    and "Runtime.TerminalGetTitle()" in terminal
    and "Runtime.TerminalSetTitle(value)" in terminal
)
require(
    "vc_terminal_prepare_output" in compiler_source
    and "vc_terminal_get_cursor_position" in compiler_source
    and "vc_terminal_get_title" in compiler_source
    and "vc_terminal_set_title" in compiler_source
    and "Runtime.ConsoleClear" not in compiler_source + semantic_source + terminal
    and "Runtime.ConsoleSetCursorPosition" not in compiler_source + semantic_source + terminal
)

with tempfile.TemporaryDirectory(prefix="void319-strict-") as temporary:
    generated = root / project / ".void" / "ConsolePresentationBasics.c"
    result = subprocess.run(
        shlex.split(os.environ.get("CC", "cc"))
        + [
            "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-O2",
            "-ICompiler/include", "-IRuntime/include", "-pthread", "-c",
            str(generated), "-o", str(Path(temporary) / "checked.o"),
        ],
        cwd=root,
        capture_output=True,
    )
    require(result.returncode == 0 and not result.stderr, result.stderr.decode(errors="replace"))
