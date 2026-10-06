#!/usr/bin/env python3
"""Focused #312 Console.Read/shared standard-input buffering checks."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
compiler = root / "bin" / "voidc"
project = "Tests/ConsoleReadSharedInputBuffering"


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

payload = (
    b"A" + "β".encode("utf-8") + b"rest\n"
    + b"line\rQnext\n"
    + b"\nhello\n"
    + b"\x00" + "😀".encode("utf-8") + b"z\n"
    + b"\xe2(\xa1OK\n"
    + b"\xe2\x82"
)
exe = root / project / "bin" / "ConsoleReadSharedInputBuffering"
result = subprocess.run([str(exe)], input=payload, capture_output=True, timeout=20)
lines = result.stdout.splitlines()
if result.returncode != 0 or lines != [b"True"] * 18:
    raise SystemExit(str(result.returncode) + "\n" + result.stdout.decode(errors="replace") + result.stderr.decode(errors="replace"))
for line in lines:
    print(line.decode())

result = invoke("check", "Tests/ConsoleReadSharedInputBufferingDiagnostics/ReadWrongArity")
require(
    result.returncode != 0
    and b"type 'Console' has no matching static method 'Read'" in result.stderr,
    result.stderr.decode(errors="replace"),
)

console = (root / "StandardLibrary/Void/Console.void").read_text()
standard_input = (root / "StandardLibrary/Void/IO/StandardInput.void").read_text()
compiler_source = (root / "Compiler/src/compiler.c").read_text()
semantic_source = (root / "Compiler/src/semantic.c").read_text()
require("public static int Read() { return Console.In.Read(); }" in console)
require(
    "internal static int Read()" in standard_input
    and "internal static string ReadLine()" in standard_input
    and standard_input.count("ReadByte()") >= 4
    and "_pendingByte" in standard_input
)
require(
    "Runtime.ConsoleRead" not in (compiler_source + semantic_source + standard_input)
    and "Runtime.ReadStdinScalar" not in (compiler_source + semantic_source + standard_input)
)

# Recompile the generated translation unit under the project's strict C contract.
with tempfile.TemporaryDirectory(prefix="void312-strict-") as temporary:
    generated = root / project / ".void" / "ConsoleReadSharedInputBuffering.c"
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
