#!/usr/bin/env python3
"""Focused compatibility regression checks; no milestone assignment."""
import os
import shlex
import tempfile
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
compiler = root / "bin/voidc"

def invoke(*args):
    return subprocess.run([str(compiler), *args], cwd=root, capture_output=True)

def require(condition, detail):
    if not condition:
        raise SystemExit(detail)
    print("True")

project = "Tests/StandardLibraryCompatibilityCleanup"
for command in ("check", "build"):
    result = invoke(command, project)
    require(result.returncode == 0, result.stdout.decode() + result.stderr.decode())
result = subprocess.run([str(root / project / "bin/StandardLibraryCompatibilityCleanup")], capture_output=True, timeout=20)
lines = result.stdout.splitlines()
if result.returncode != 0 or lines != [b"True"] * 81:
    raise SystemExit(str(result.returncode) + "\n" + result.stdout.decode() + result.stderr.decode())
for line in lines:
    print(line.decode())
for name, expected in (("ExceptionMessageWrite", "read-only"), ("PairKeyWrite", "read-only"), ("RawStdoutAccess", "unknown identifier 'Runtime'")):
    result = invoke("check", "Tests/StandardLibraryCompatibilityCleanupDiagnostics/" + name)
    require(result.returncode != 0 and expected.encode() in result.stderr, result.stderr.decode())
result = invoke("build", "Tests/StandardLibraryConsoleOutput")
if result.returncode != 0:
    raise SystemExit(result.stderr.decode())
result = subprocess.run([str(root / "Tests/StandardLibraryConsoleOutput/bin/Program")], capture_output=True, timeout=20)
newline = '\r\n' if os.name == 'nt' else '\n'
require(result.returncode == 0 and result.stdout == ("😀\0é" + newline * 2).encode(), repr(result.stdout) + result.stderr.decode())

with tempfile.TemporaryDirectory(prefix="void-cleanup-strict-") as temporary:
    result = subprocess.run(shlex.split(os.environ.get("CC", "cc")) +
        ["-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-O2",
         "-ICompiler/include", "-IRuntime/include", "-pthread", "-c",
         project + "/.void/StandardLibraryCompatibilityCleanup.c", "-o", str(Path(temporary) / "checked.o")],
        cwd=root, capture_output=True)
    require(result.returncode == 0 and not result.stderr, result.stderr.decode())

if os.name == 'nt' or Path("/dev/full").exists():
    result = invoke("build", "Tests/StandardLibraryConsoleWriteFailure")
    if result.returncode != 0:
        raise SystemExit(result.stderr.decode())
    if os.name == 'nt':
        # A pipe with no reader provides a real native write failure on Windows.
        reader, writer = os.pipe()
        os.close(reader)
        try:
            result = subprocess.run([str(root / "Tests/StandardLibraryConsoleWriteFailure/bin/Program")],
                stdout=writer, stderr=subprocess.PIPE, timeout=20)
        finally:
            os.close(writer)
    else:
        with open("/dev/full", "wb") as failed_output:
            result = subprocess.run([str(root / "Tests/StandardLibraryConsoleWriteFailure/bin/Program")],
                stdout=failed_output, stderr=subprocess.PIPE, timeout=20)
    require(result.returncode == 0 and not result.stderr, result.stderr.decode())
else:
    # The device is Linux-specific; all other checks remain portable.
    print("True")
