#!/usr/bin/env python3
"""Focused #313 TextReader/TextWriter and Console.In/Out/Error checks."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
compiler = root / "bin" / "voidc"
project = "Tests/ConsoleTextReaderWriter"


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

payload = b"Arest\n" + "β".encode("utf-8") + b"tail\n"
exe = root / project / "bin" / "ConsoleTextReaderWriter"
result = subprocess.run([str(exe)], input=payload, capture_output=True, timeout=20)
require(result.returncode == 0, result.stderr.decode(errors="replace"))
stdout_lines = result.stdout.splitlines()
require(len(stdout_lines) == 18, result.stdout.decode(errors="replace"))
for line in stdout_lines[:17]:
    require(line == b"True", result.stdout.decode(errors="replace"))
require(stdout_lines[17] == b"STDOUT|line", result.stdout.decode(errors="replace"))
newline = b'\r\n' if os.name == 'nt' else b'\n'
require(result.stderr == b"STDERR|line" + newline, repr(result.stderr))

result = invoke("check", "Tests/ConsoleTextReaderWriterDiagnostics/RuntimeStderrAccess")
require(
    result.returncode != 0
    and b"unknown identifier 'Runtime'" in result.stderr,
    result.stderr.decode(errors="replace"),
)

result = invoke("check", "Tests/ConsoleTextReaderWriterDiagnostics/ConsoleInReadOnly")
require(
    result.returncode != 0 and b"property 'In' is read-only" in result.stderr,
    result.stderr.decode(errors="replace"),
)

text_reader = (root / "StandardLibrary/Void/IO/TextReader.void").read_text()
text_writer = (root / "StandardLibrary/Void/IO/TextWriter.void").read_text()
console = (root / "StandardLibrary/Void/Console.void").read_text()
standard_output = (root / "StandardLibrary/Void/IO/StandardOutput.void").read_text()
compiler_source = (root / "Compiler/src/compiler.c").read_text()
semantic_source = (root / "Compiler/src/semantic.c").read_text()

require(
    "public abstract class TextReader" in text_reader
    and "public abstract int Read();" in text_reader
    and "public abstract string ReadLine();" in text_reader
    and "internal sealed class StandardTextReader : TextReader" in text_reader
)
require(
    "public abstract class TextWriter" in text_writer
    and "public abstract void Write(string value);" in text_writer
    and "public virtual void WriteLine()" in text_writer
    and "public virtual void WriteLine(string value)" in text_writer
    and "internal sealed class StandardTextWriter : TextWriter" in text_writer
)
require(
    "public static TextReader In" in console
    and "public static TextWriter Out" in console
    and "public static TextWriter Error" in console
)
require(
    "public static int Read() { return Console.In.Read(); }" in console
    and "public static string ReadLine() { return Console.In.ReadLine(); }" in console
    and "_outIsStandard" in console
    and "if (_outIsStandard) StandardOutput.Write(value);" in console
    and "else _out.Write(value);" in console
)
require(
    "internal static void WriteError(string value)" in standard_output
    and "Runtime.StandardStreamWriteUtf8(stream, value)" in standard_output
    and "StandardStreams.Flush(stream)" in standard_output
)
require("vc_standard_stream_write(" in compiler_source and "vc_standard_stream_write_utf8(" in compiler_source)
require("Runtime.StandardStreamWrite" in semantic_source and "Runtime.StandardStreamWriteUtf8" in semantic_source)
require(
    "Runtime.ConsoleRead" not in compiler_source + semantic_source + console + text_reader
    and "Runtime.ConsoleWrite" not in compiler_source + semantic_source + console + text_writer
)

# Recompile the generated translation unit under the project's strict C contract.
with tempfile.TemporaryDirectory(prefix="void313-strict-") as temporary:
    generated = root / project / ".void" / "ConsoleTextReaderWriter.c"
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
