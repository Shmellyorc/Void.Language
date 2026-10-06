#!/usr/bin/env python3
"""Focused #317 ConsoleKey/ConsoleKeyInfo/ReadKey checks."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
if os.name != 'nt':
    import pty
    import termios

root = Path(__file__).resolve().parents[1]
compiler = root / "bin" / "voidc"
project = "Tests/ConsoleKeyReadKey"


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

exe = root / project / "bin" / "ConsoleKeyReadKey"

# ReadKey rejects redirected stdin while the public types remain usable.
result = subprocess.run([str(exe)], input=b"", capture_output=True, timeout=20)
require(result.returncode == 0, result.stderr.decode(errors="replace"))
pipe_lines = result.stdout.splitlines()
require(len(pipe_lines) == 13, repr(result.stdout))
for line in pipe_lines:
    require(line == b"True", repr(result.stdout))

# Exercise real terminal semantics through a PTY, not a pipe.
if os.name == 'nt':
    from windows_console import run_terminal
    keys = [(12, 'a', 65, 0), (12, 'A', 65, 16), (12, '\x01', 65, 8),
            (12, 'x', 88, 2), (12, '\0', 38, 0), (12, '\0', 39, 8),
            (12, '\0', 116, 0), (12, '\u03b2', 0, 0), (12, '\r', 13, 0),
            (12, 'z', 90, 0), (12, '\x1b', 27, 0)]
    result = run_terminal(root, project, 'ConsoleKeyReadKey', keys, stdin_only=True)
    lines = result['stdout'].splitlines()
    for line in lines[:12]:
        require(line == b'True', repr(result))
    assert len(lines[:12]) == 12 and result['sent'] == len(keys), result
    require(result['returncode'] == 0 and not result['timed_out'], repr(result))
    require(not result['stderr'], repr(result))
    require(result['restored'], 'console settings were not restored')
    remaining = lines[12:]
    require(len(remaining) == 11, repr(result))
    for line in remaining[:9]:
        require(line == b'True', repr(result))
    require(remaining[9] == b'zTrue', repr(result))
    require(remaining[10] == b'True', repr(result))
    # Internal assertions retain the cumulative guarded-check total while
    # covering surrogate pairs and repeat records in the production helpers.
    probe = run_terminal(root, project, 'ConsoleKeyReadKey',
        [(1, '\ud83d', 0, 0), (1, '\ude42', 0, 0), (1, 'r', 82, 0, 3)],
        stdin_only=True, native_probe=True)
    assert probe['returncode'] == 0 and not probe['timed_out'], probe
    assert probe['stdout'] == b'True\r\nTrue\r\n' and not probe['stderr'], probe
    assert probe['restored'] and probe['sent'] == 3, probe
else:
    master, slave = pty.openpty()
    before = termios.tcgetattr(slave)
    process = subprocess.Popen(
        [str(exe)], cwd=root, stdin=slave, stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    assert process.stdout is not None
    prefix = [process.stdout.readline() for _ in range(12)]
    for line in prefix:
        require(line == b"True\n", repr(prefix))

    payload = (
        b"a"                  # ordinary A key
        b"A"                  # inferred Shift+A
        b"\x01"              # Ctrl+A
        b"\x1bx"             # Alt+X
        b"\x1b[A"            # UpArrow
        b"\x1b[1;5C"         # Ctrl+RightArrow
        b"\x1b[15~"          # F5
        + "β".encode("utf-8") # Unicode scalar
        + b"\r"              # Enter
        + b"z"                # ReadKey(false), must echo
        + b"\x1b"            # Escape by itself
    )
    os.write(master, payload)
    out, err = process.communicate(timeout=20)
    after = termios.tcgetattr(slave)
    os.close(master)
    os.close(slave)
    require(process.returncode == 0, err.decode(errors="replace"))
    require(not err, err.decode(errors="replace"))
    require(before == after, "terminal settings were not restored")
    remaining = out.splitlines()
    require(len(remaining) == 11, repr(out))
    for line in remaining[:9]:
        require(line == b"True", repr(out))
    require(remaining[9] == b"zTrue", repr(out))
    require(remaining[10] == b"True", repr(out))


# The native event primitive is StandardLibrary-only.
result = invoke("check", "Tests/ConsoleKeyReadKeyDiagnostics/RuntimeTerminalReadEventAccess")
require(
    result.returncode != 0
    and b"Runtime.TerminalReadEvent is reserved for Void.IO.StandardTerminal" in result.stderr,
    result.stderr.decode(errors="replace"),
)

console = (root / "StandardLibrary/Void/Console.void").read_text()
key = (root / "StandardLibrary/Void/ConsoleKey.void").read_text()
mods = (root / "StandardLibrary/Void/ConsoleModifiers.void").read_text()
info = (root / "StandardLibrary/Void/ConsoleKeyInfo.void").read_text()
terminal = (root / "StandardLibrary/Void/IO/StandardTerminal.void").read_text()
compiler_source = (root / "Compiler/src/compiler.c").read_text()
semantic_source = (root / "Compiler/src/semantic.c").read_text()

require(
    "public static ConsoleKeyInfo ReadKey()" in console
    and "public static ConsoleKeyInfo ReadKey(bool intercept)" in console
    and "Console.In is StandardTextReader" in console
)
require(
    "public enum ConsoleKey" in key
    and "F24 = 135" in key
    and "OemClear = 254" in key
    and "public enum ConsoleModifiers" in mods
    and "Alt = 1" in mods and "Shift = 2" in mods and "Control = 4" in mods
)
require(
    "public readonly struct ConsoleKeyInfo" in info
    and "public char KeyChar" in info
    and "public ConsoleKey Key" in info
    and "public ConsoleModifiers Modifiers" in info
)
require(
    "Runtime.TerminalReadEvent(data)" in terminal
    and "Runtime.ConsoleReadKey" not in terminal + compiler_source + semantic_source
    and "vc_terminal_read_event" in compiler_source
)
require(
    "#include <termios.h>" in compiler_source
    and "tcsetattr(STDIN_FILENO, TCSANOW, &original)" in compiler_source
    and "#include <conio.h>" in compiler_source
    and "ReadConsoleInputW" in compiler_source and "dwControlKeyState" in compiler_source
)

with tempfile.TemporaryDirectory(prefix="void317-strict-") as temporary:
    generated = root / project / ".void" / "ConsoleKeyReadKey.c"
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
