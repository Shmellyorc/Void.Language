"""Primitive display/format behavior through normal/O2 JIT and independently linked AOT."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

compiler, clang, linker, runtime, fixture, sanitizer = sys.argv[1:]
flags = ["--driver-mode=g++", "-pthread"]
if linker:
    flags.append("--ld-path=" + linker)
if sanitizer == "ON":
    flags.append("-fsanitize=address,undefined")
if os.name == "nt":
    flags.extend(["-fms-runtime-lib=dll", "-llegacy_stdio_definitions"])
checks = 0


def run(args):
    global checks
    p = subprocess.run([str(a) for a in args], capture_output=True, text=True,
                       encoding="utf-8", timeout=60)
    assert p.returncode == 0, (args, p.returncode, p.stdout, p.stderr)
    checks += 1
    return p.stdout.strip()


with tempfile.TemporaryDirectory(prefix="VOID Stage10 display ") as directory:
    root = Path(directory)
    source = root / "main.void"
    source.write_text('''const expected = format(2.125d, "0.00")
struct record:
    private const value: i32 = 0
func (self record) init(n: i32):
    self.value = n
func (self record) string() -> string:
    return "{self.value:000}"
func (self record) format(spec: string) -> result<string, string>:
    return format(self.value, spec)
func (self record) render() -> string:
    return """{self.value:000}/{string(self.value)}"""
func entry(input: i32) -> i32:
    var spec = "000"
    var text = string(input)
    var joined = record(n = input).render()
    if format(value = input, spec = spec) is Ok(var padded):
        if expected is Ok(var constant_text):
            if constant_text == "2.12" and joined == padded + "/" + text and string(record(n = input)) == padded and "{record(n = input):000}" == padded:
                return (text.length + padded.length) as i32
    return 0
''', encoding="utf-8")
    for optimized in (False, True):
        switches = [] if optimized else ["--no-opt"]
        for value in (0, 4, -3):
            assert run([compiler, "run", source, value, *switches]) == str(len(str(value)) + max(3, len(str(abs(value)))) + (1 if value < 0 else 0))
        obj = root / ("optimized.o" if optimized else "normal.o")
        exe = obj.with_suffix(".exe" if os.name == "nt" else ".bin")
        run([compiler, "emit", source, "-o", obj, *switches])
        run([clang, *flags, obj, runtime, fixture, "-o", exe])
        for value in (0, 4, -3):
            assert run([exe, value]) == str(len(str(value)) + max(3, len(str(abs(value)))) + (1 if value < 0 else 0))
print(f"{checks} source display JIT/AOT checks passed")
