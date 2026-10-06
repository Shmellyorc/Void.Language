#!/usr/bin/env python3
"""Focused #311 Console.ReadLine foundation checks."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
compiler = root / "bin" / "voidc"
project = "Tests/ConsoleReadLineFoundation"

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
    b"alpha\n"
    + "βeta".encode("utf-8") + b"\r\n"
    + b"gamma\rdelta\n"
    + b"\n"
    + b"nul\x00inside\n"
    + "emoji 😀".encode("utf-8") + b"\n"
    + (b"x" * 1024) + b"\n"
    + b"bad:\xc0\xaf\n"
    + b"tail"
)
exe = root / project / "bin" / "ConsoleReadLineFoundation"
result = subprocess.run([str(exe)], input=payload, capture_output=True, timeout=20)
lines = result.stdout.splitlines()
if result.returncode != 0 or lines != [b"True"] * 19:
    raise SystemExit(str(result.returncode) + "\n" + result.stdout.decode(errors="replace") + result.stderr.decode(errors="replace"))
for line in lines:
    print(line.decode())

result = invoke("check", "Tests/ConsoleReadLineFoundationDiagnostics/RuntimeStdinByteAccess")
require(result.returncode != 0 and b"Runtime.ReadStdinByte is reserved for Void.IO.StandardInput" in result.stderr,
        result.stderr.decode(errors="replace"))

console = (root / "StandardLibrary/Void/Console.void").read_text()
standard_input = (root / "StandardLibrary/Void/IO/StandardInput.void").read_text()
compiler_source = (root / "Compiler/src/compiler.c").read_text()
semantic_source = (root / "Compiler/src/semantic.c").read_text()
require("public static string ReadLine() { return Console.In.ReadLine(); }" in console)
require("Runtime.ReadStdinByte()" in standard_input and "Console.InputEncoding.GetString" in standard_input)
require("vc_stream_read_byte(stdin)" in compiler_source and "vc_stream_read_byte(FILE *stream)" in compiler_source)
require("Runtime.ReadStdinByte is reserved for Void.IO.StandardInput" in semantic_source)
require("Runtime.ConsoleReadLine" not in (compiler_source + semantic_source + standard_input))

# Recompile the generated translation unit under the project's strict C contract.
with tempfile.TemporaryDirectory(prefix="void311-strict-") as temporary:
    generated = root / project / ".void" / "ConsoleReadLineFoundation.c"
    result = subprocess.run(
        shlex.split(os.environ.get("CC", "cc")) +
        ["-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-O2",
         "-ICompiler/include", "-IRuntime/include", "-pthread", "-c",
         str(generated), "-o", str(Path(temporary) / "checked.o")],
        cwd=root, capture_output=True)
    require(result.returncode == 0 and not result.stderr, result.stderr.decode(errors="replace"))
