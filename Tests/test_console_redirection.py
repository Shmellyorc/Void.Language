#!/usr/bin/env python3
"""Focused #314 Console redirection and SetIn/SetOut/SetError checks."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
compiler = root / "bin" / "voidc"
project = "Tests/ConsoleRedirection"


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

exe = root / project / "bin" / "ConsoleRedirection"
result = subprocess.run([str(exe)], input=b"native\n", capture_output=True, timeout=20)
require(result.returncode == 0, result.stderr.decode(errors="replace"))
lines = result.stdout.splitlines()
require(len(lines) == 27, result.stdout.decode(errors="replace"))
for line in lines:
    require(line == b"True", result.stdout.decode(errors="replace"))

result = invoke("check", "Tests/ConsoleRedirectionDiagnostics/RuntimeStandardStreamTerminalAccess")
require(
    result.returncode != 0
    and b"Runtime.StandardStreamIsTerminal is reserved for Void.IO.StandardStreams" in result.stderr,
    result.stderr.decode(errors="replace"),
)

result = invoke("check", "Tests/ConsoleRedirectionDiagnostics/SetInWrongType")
require(
    result.returncode != 0 and b"type 'Console' has no matching static method 'SetIn'" in result.stderr,
    result.stderr.decode(errors="replace"),
)

console = (root / "StandardLibrary/Void/Console.void").read_text()
streams = (root / "StandardLibrary/Void/IO/StandardStreams.void").read_text()
compiler_source = (root / "Compiler/src/compiler.c").read_text()
semantic_source = (root / "Compiler/src/semantic.c").read_text()

require(
    "private static TextReader _in" in console
    and "private static TextWriter _out" in console
    and "private static TextWriter _error" in console
    and "private static readonly TextReader _in" not in console
)
require(
    "public static void SetIn(TextReader newIn)" in console
    and "public static void SetOut(TextWriter newOut)" in console
    and "public static void SetError(TextWriter newError)" in console
    and 'ArgumentNullException("newIn")' in console
    and 'ArgumentNullException("newOut")' in console
    and 'ArgumentNullException("newError")' in console
)
require(
    "public static bool IsInputRedirected" in console
    and "public static bool IsOutputRedirected" in console
    and "public static bool IsErrorRedirected" in console
    and "!StandardStreams.IsTerminal(0)" in console
    and "!StandardStreams.IsTerminal(1)" in console
    and "!StandardStreams.IsTerminal(2)" in console
)
require(
    "internal static class StandardStreams" in streams
    and "Runtime.StandardStreamIsTerminal(stream)" in streams
)
require(
    "vc_standard_stream_is_terminal(" in compiler_source
    and "_isatty(descriptor)" in compiler_source
    and "isatty(descriptor)" in compiler_source
    and "STDIN_FILENO" in compiler_source
    and "STDOUT_FILENO" in compiler_source
    and "STDERR_FILENO" in compiler_source
)
require(
    "Runtime.StandardStreamIsTerminal is reserved for Void.IO.StandardStreams" in semantic_source
)
require(
    "Runtime.ConsoleSetIn" not in compiler_source + semantic_source + console
    and "Runtime.ConsoleSetOut" not in compiler_source + semantic_source + console
    and "Runtime.ConsoleSetError" not in compiler_source + semantic_source + console
    and "Runtime.ConsoleIsInputRedirected" not in compiler_source + semantic_source + console
)

# Recompile generated C under the project's strict C contract.
with tempfile.TemporaryDirectory(prefix="void314-strict-") as temporary:
    generated = root / project / ".void" / "ConsoleRedirection.c"
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
