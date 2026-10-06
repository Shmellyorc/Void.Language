#!/usr/bin/env python3
"""Focused #315 Stream/OpenStandard* compatibility and architecture checks."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
compiler = root / "bin" / "voidc"
project = "Tests/ConsoleStandardStreams"


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

exe = root / project / "bin" / "ConsoleStandardStreams"
result = subprocess.run([str(exe)], input=b"line\rZ\xf0\x9f\x98\x80", capture_output=True, timeout=20)
require(result.returncode == 0, result.stderr.decode(errors="replace"))
lines = result.stdout.splitlines()
require(lines == [
    b"True", b"True", b"True", b"True", b"True", b"True", b"True", b"True",
    b"True", b"True", b"True", b"True", b"True", b"True", b"True", b"True", b"True",
    b"STDOUT", b"True", b"True", b"True", b"True", b"True", b"True", b"True", b"X", b"True"
], result.stdout.decode(errors="replace"))
require(result.stderr == b"ERR\n", result.stderr.decode(errors="replace"))

result = invoke("check", "Tests/ConsoleStandardStreamsDiagnostics/RuntimeStandardStreamReadAccess")
require(
    result.returncode != 0
    and b"Runtime.StandardStreamRead is reserved for Void.IO.StandardInput" in result.stderr,
    result.stderr.decode(errors="replace"),
)

result = invoke("check", "Tests/ConsoleStandardStreamsDiagnostics/RuntimeStandardStreamWriteAccess")
require(
    result.returncode != 0
    and b"Runtime.StandardStreamWrite is reserved for Void.IO.StandardStreams" in result.stderr,
    result.stderr.decode(errors="replace"),
)

result = invoke("check", "Tests/ConsoleStandardStreamsDiagnostics/RuntimeStandardStreamFlushAccess")
require(
    result.returncode != 0
    and b"Runtime.StandardStreamFlush is reserved for Void.IO.StandardStreams" in result.stderr,
    result.stderr.decode(errors="replace"),
)

stream = (root / "StandardLibrary/Void/IO/Stream.void").read_text()
console = (root / "StandardLibrary/Void/Console.void").read_text()
input_source = (root / "StandardLibrary/Void/IO/StandardInput.void").read_text()
streams = (root / "StandardLibrary/Void/IO/StandardStreams.void").read_text()
compiler_source = (root / "Compiler/src/compiler.c").read_text()
semantic_source = (root / "Compiler/src/semantic.c").read_text()

require(
    "public abstract class Stream : IDisposable" in stream
    and "public abstract bool CanRead" in stream
    and "public abstract bool CanSeek" in stream
    and "public abstract bool CanWrite" in stream
    and "public abstract int Read(byte[] buffer, int offset, int count)" in stream
    and "public abstract void Write(byte[] buffer, int offset, int count)" in stream
    and "public abstract void Flush()" in stream
    and "public abstract long Seek(long offset, SeekOrigin origin)" in stream
    and "public abstract void SetLength(long value)" in stream
)
require(
    "public static Stream OpenStandardInput()" in console
    and "public static Stream OpenStandardInput(int bufferSize)" in console
    and "public static Stream OpenStandardOutput()" in console
    and "public static Stream OpenStandardOutput(int bufferSize)" in console
    and "public static Stream OpenStandardError()" in console
    and "public static Stream OpenStandardError(int bufferSize)" in console
)
require(
    "internal sealed class StandardStream : Stream" in stream
    and "StandardInput.ReadBytes(buffer, offset, count)" in stream
    and "StandardStreams.Write(_stream, buffer, offset, count)" in stream
    and "StandardStreams.Flush(_stream)" in stream
)
require(
    "internal static int ReadBytes(byte[] buffer, int offset, int count)" in input_source
    and "_pendingByte" in input_source
    and "Runtime.StandardStreamRead(0, buffer, offset, count)" in input_source
)
require(
    "Runtime.StandardStreamWrite(stream, buffer, offset, count)" in streams
    and "Runtime.StandardStreamFlush(stream)" in streams
)
require(
    "vc_standard_stream_read(" in compiler_source
    and "vc_standard_stream_write(" in compiler_source
    and "vc_standard_stream_flush(" in compiler_source
    and "fread(" in compiler_source
    and "fwrite(" in compiler_source
    and "fflush(" in compiler_source
)
require(
    "Runtime.StandardStreamRead is reserved for Void.IO.StandardInput" in semantic_source
    and "Runtime.StandardStreamWrite is reserved for Void.IO.StandardStreams" in semantic_source
    and "Runtime.StandardStreamFlush is reserved for Void.IO.StandardStreams" in semantic_source
)
require(
    "Runtime.ConsoleOpenStandardInput" not in compiler_source + semantic_source + console
    and "Runtime.ConsoleOpenStandardOutput" not in compiler_source + semantic_source + console
    and "Runtime.ConsoleOpenStandardError" not in compiler_source + semantic_source + console
)

# Recompile generated C under the project's strict C contract.
with tempfile.TemporaryDirectory(prefix="void315-strict-") as temporary:
    generated = root / project / ".void" / "ConsoleStandardStreams.c"
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
