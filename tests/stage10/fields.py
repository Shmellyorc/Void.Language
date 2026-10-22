"""Stored-field behavior through normal/O2 JIT and independently linked AOT."""
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


with tempfile.TemporaryDirectory(prefix="VOID Stage10 fields ") as directory:
    root = Path(directory)
    source = root / "main.void"
    source.write_text("""struct base:
    private const id: i32 = 0
func (self base) init(n: i32):
    self.id = n
    self.id = n + 1
func (self base) read() -> i32:
    var get = func() -> i32:
        return self.id
    return get()
struct derived extends base:
    const items: [i32]
func (self derived) init(n: i32):
    super.init(n)
    self.items = [n, n + 2]
value struct saved<t>:
    private const value: t
func (self saved<t>) read() -> t:
    return self.value
func (self saved<t>) take() -> t:
    return self.value
interface reader:
    func take() -> i32
struct wrapper:
    const using saved<i32>
func entry(input: i32) -> i32:
    var object = derived(n = input)
    var snapshot = saved<i32>()
    var view: reader = wrapper()
    return object.read() + object.items[1] + snapshot.read() + view.take()
""", encoding="utf-8")
    for optimized in (False, True):
        switches = [] if optimized else ["--no-opt"]
        for value in (0, 4, -3):
            assert run([compiler, "run", source, value, *switches]) == str(2 * value + 3)
        obj = root / ("optimized.o" if optimized else "normal.o")
        exe = obj.with_suffix(".exe" if os.name == "nt" else ".bin")
        run([compiler, "emit", source, "-o", obj, *switches])
        run([clang, *flags, obj, runtime, fixture, "-o", exe])
        for value in (0, 4, -3):
            assert run([exe, value]) == str(2 * value + 3)
print(f"{checks} field JIT/AOT checks passed")
