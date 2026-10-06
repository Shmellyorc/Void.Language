#!/usr/bin/env python3
"""Focused #318 KeyAvailable and Ctrl+C input-control checks."""
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
project = "Tests/ConsoleInputControl"


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
exe = root / project / "bin" / "ConsoleInputControl"

# Redirected input: KeyAvailable is invalid and terminal signal-mode access reports I/O failure.
result = subprocess.run([str(exe)], input=b"", capture_output=True, timeout=20)
require(result.returncode == 0, result.stderr.decode(errors="replace"))
lines = result.stdout.splitlines()
require(len(lines) == 4, repr(result.stdout))
for line in lines:
    require(line == b"True", repr(result.stdout))

# Real terminal semantics through a PTY. KeyAvailable must not block or consume the key.
if os.name == 'nt':
    from windows_console import run_terminal
    result = run_terminal(root, project, 'ConsoleInputControl', [(3, 'x', 88, 0), (7, '\x03', 67, 8)], stdin_only=True)
    lines = result['stdout'].splitlines()
    for line in lines[:3]:
        require(line == b'True', repr(result))
    assert len(lines[:3]) == 3, result
    for line in lines[3:7]:
        require(line == b'True', repr(result))
    assert len(lines[3:7]) == 4 and result['sent'] == 2, result
    require(result['returncode'] == 0 and not result['timed_out'], repr(result))
    require(not result['stderr'], repr(result))
    require(result['restored'], 'console settings were not restored')
    tail = lines[7:]
    require(len(tail) == 2, repr(result))
    for line in tail:
        require(line == b'True', repr(result))
else:
    master, slave = pty.openpty()
    before = termios.tcgetattr(slave)
    process = subprocess.Popen(
        [str(exe)], cwd=root, stdin=slave, stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    assert process.stdout is not None
    prefix = [process.stdout.readline() for _ in range(3)]
    for line in prefix:
        require(line == b"True\n", repr(prefix))

    # The program is now polling KeyAvailable. One ordinary key must become available without Enter.
    os.write(master, b"x")
    ordinary = [process.stdout.readline() for _ in range(4)]
    for line in ordinary:
        require(line == b"True\n", repr(ordinary))

    # TreatControlCAsInput is now true; Ctrl+C must arrive as input, not terminate the process.
    os.write(master, b"\x03")
    out, err = process.communicate(timeout=20)
    after = termios.tcgetattr(slave)
    os.close(master)
    os.close(slave)
    require(process.returncode == 0, err.decode(errors="replace"))
    require(not err, err.decode(errors="replace"))
    require(before == after, "terminal settings were not restored")
    tail = out.splitlines()
    require(len(tail) == 2, repr(out))
    for line in tail:
        require(line == b"True", repr(out))


# Low-level terminal control capabilities are StandardLibrary-only.
diagnostics = [
    ("RuntimeTerminalKeyAvailableAccess", b"Runtime.TerminalKeyAvailable is reserved for Void.IO.StandardTerminal"),
    ("RuntimeTerminalSignalProcessingAccess", b"Runtime.TerminalSignalProcessingEnabled is reserved for Void.IO.StandardTerminal"),
    ("RuntimeTerminalSetSignalProcessingAccess", b"Runtime.TerminalSetSignalProcessingEnabled is reserved for Void.IO.StandardTerminal"),
]
for name, expected in diagnostics:
    result = invoke("check", f"Tests/ConsoleInputControlDiagnostics/{name}")
    require(result.returncode != 0 and expected in result.stderr, result.stderr.decode(errors="replace"))

console = (root / "StandardLibrary/Void/Console.void").read_text()
terminal = (root / "StandardLibrary/Void/IO/StandardTerminal.void").read_text()
compiler_source = (root / "Compiler/src/compiler.c").read_text()
semantic_source = (root / "Compiler/src/semantic.c").read_text()

require(
    "public static bool KeyAvailable" in console
    and "public static bool TreatControlCAsInput" in console
    and "StandardTerminal.KeyAvailable()" in console
)
require(
    "Runtime.TerminalKeyAvailable()" in terminal
    and "Runtime.TerminalSignalProcessingEnabled()" in terminal
    and "Runtime.TerminalSetSignalProcessingEnabled(!value)" in terminal
)
require(
    "vc_terminal_key_available" in compiler_source
    and "vc_terminal_signal_processing_enabled" in compiler_source
    and "vc_terminal_set_signal_processing_enabled" in compiler_source
    and "Runtime.ConsoleKeyAvailable" not in compiler_source + semantic_source + terminal
)
# CancelKeyPress is deliberately deferred: no unsafe OS-signal/event bridge is introduced in #318.
require("CancelKeyPress" not in console and "sigaction(" not in compiler_source and "SIGINT" not in compiler_source)

with tempfile.TemporaryDirectory(prefix="void318-strict-") as temporary:
    generated = root / project / ".void" / "ConsoleInputControl.c"
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
