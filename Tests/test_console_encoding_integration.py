#!/usr/bin/env python3
"""Focused #316 Console InputEncoding/OutputEncoding integration checks."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
compiler = root / "bin" / "voidc"
project = "Tests/ConsoleEncodingIntegration"


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

payload = "βeta\n".encode("utf-8") + b"\xe2\x28\xa1\n" + b"\xe2\x28\xff"
exe = root / project / "bin" / "ConsoleEncodingIntegration"
result = subprocess.run([str(exe)], input=payload, capture_output=True, timeout=20)
require(result.returncode == 0, result.stderr.decode(errors="replace"))
lines = result.stdout.splitlines()
require(len(lines) == 14, result.stdout.decode(errors="replace"))
for line in lines:
    require(line == b"True", result.stdout.decode(errors="replace"))

result = invoke("check", "Tests/ConsoleEncodingIntegrationDiagnostics/EncodingWrongType")
require(
    result.returncode != 0 and b"cannot assign 'string' to 'Encoding'" in result.stderr,
    result.stderr.decode(errors="replace"),
)

console = (root / "StandardLibrary/Void/Console.void").read_text()
encoding = (root / "StandardLibrary/Void/Text/Encoding.void").read_text()
standard_input = (root / "StandardLibrary/Void/IO/StandardInput.void").read_text()
standard_output = (root / "StandardLibrary/Void/IO/StandardOutput.void").read_text()

require(
    "public static Encoding InputEncoding" in console
    and "public static Encoding OutputEncoding" in console
    and "SupportsConsoleStreaming" in console
)
require(
    "internal virtual bool SupportsConsoleStreaming" in encoding
    and "internal override bool SupportsConsoleStreaming" in encoding
    and "internal virtual bool ThrowsOnInvalidBytes" in encoding
    and "internal override bool ThrowsOnInvalidBytes" in encoding
)
require(
    "Console.InputEncoding.GetString(bytes, 0, length)" in standard_input
    and "Console.InputEncoding.ThrowsOnInvalidBytes" in standard_input
)
require(
    "Runtime.StandardStreamWriteUtf8(stream, value)" in standard_output
    and "StandardStreams.Flush(stream)" in standard_output
    and "Runtime.WriteUtf8Stdout" not in standard_output
    and "Runtime.WriteUtf8Stderr" not in standard_output
)

# Console still admits only UTF-8 streaming encodings. Valid VOID strings can therefore
# use the generalized low-level UTF-8 standard-stream write without a temporary byte[].
compiler_source = (root / "Compiler/src/compiler.c").read_text()
semantic_source = (root / "Compiler/src/semantic.c").read_text()
require(
    "Runtime.ConsoleInputEncoding" not in compiler_source + semantic_source
    and "Runtime.ConsoleOutputEncoding" not in compiler_source + semantic_source
)

with tempfile.TemporaryDirectory(prefix="void316-strict-") as temporary:
    generated = root / project / ".void" / "ConsoleEncodingIntegration.c"
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
