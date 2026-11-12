"""Exercise typed returns through both JIT modes and independent native links."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
from tempfile import TemporaryDirectory

compiler, resources = sys.argv[1:]
fixtures = Path(__file__).resolve().parent
example = fixtures.parents[1] / "examples" / "multiple_returns" / "test_game.void"
windows = os.name == "nt"
version, clang, linker, *link_resources = Path(resources).read_text(encoding="utf-8").splitlines()
assert version == "VOID native 23.1.4 v2"
link_flags = ["--driver-mode=g++"]
if linker:
    link_flags.append("--ld-path=" + linker)
if windows:
    link_flags.append("--ld-path=" + str(Path(clang).with_name("lld-link.exe")))


def run(args, work, status=0):
    result = subprocess.run(list(map(str, args)), cwd=work, capture_output=True,
                            encoding="utf-8", timeout=60)
    assert result.returncode == status, (args, result.returncode, result.stdout, result.stderr)
    for failure in ("AddressSanitizer", "LeakSanitizer", "runtime error:", "PLEASE submit a bug report"):
        assert failure not in result.stderr, result.stderr
    return result


def qualify(work, source, cases):
    run([compiler, "check", source], work)
    for index, flags in enumerate((["--no-opt"], [])):
        obj = work / (f"{source.stem} {index}" + (".obj" if windows else ".o"))
        image = obj.with_suffix(".exe" if windows else ".bin")
        run([compiler, "emit", source, "-o", obj, *flags], work)
        run([clang, *link_flags, obj, *link_resources, "-o", image], work)
        for args, status, expected in cases:
            jit = run([compiler, "run", source, *flags, "--no-cache", "--", *args], work, status)
            aot = run([image, *args], work, status)
            assert jit.stdout == aot.stdout == expected + "\n", (jit.stdout, aot.stdout)
            assert not jit.stderr and not aot.stderr, (jit.stderr, aot.stderr)


pair = 'func pair() -> (i32, string):\n    return 1, "text"\n'
negative = {
    "mixed labels": ('func fail() -> (one: i32, string):\n    return 1, ""\n', "either all named"),
    "duplicate labels": ('func fail() -> (one: i32, one: string):\n    return 1, ""\n', "Duplicate return component"),
    "invalid type": ('func fail() -> (i32, absent):\n    return 1, ""\n', "Unsupported type 'absent'"),
    "bad generic header": ('func fail<t>(n: t) -> (t, absent):\n    return n, ""\n', "Unsupported type 'absent'"),
    "one component": ('func fail() -> (only: i32):\n    return 1\n', "at least two components"),
    "short return": ('func fail() -> (i32, bool):\n    return 1\n', "requires 2 values, received 1"),
    "long return": ('func fail() -> (i32, bool):\n    return 1, true, 3\n', "requires 2 values, received 3"),
    "empty return": ('func fail() -> (i32, bool):\n    return\n', "requires 2 values, received 0"),
    "type mismatch": ('func fail() -> (i32, bool):\n    return 1, "no"\n', "expected bool, received string"),
    "missing path": ('func fail(flag: bool) -> (i32, bool):\n    if flag:\n        return 1, true\n', "Missing return path"),
    "implicit output": ('func fail() -> (one: i32, two: bool):\n    one = 1\n    return 1, true\n', "Unknown identifier 'one'"),
    "single ABI": ('func fail() -> i32:\n    return 1, 2\n', "requires 1 value, received 2"),
    "single assignment": (pair + 'func fail():\n    var combined = pair()\n', "requires a destructuring declaration"),
    "standalone call": (pair + 'func fail():\n    pair()\n', "requires a destructuring declaration"),
    "defer call": (pair + 'func fail():\n    defer pair()\n', "requires a destructuring declaration"),
    "nested call": (pair + 'func fail():\n    print(pair())\n', "requires a destructuring declaration"),
    "ordinary value": ('func fail():\n    var one, two = 42\n', "requires a multiple-return function call"),
    "binding count": (pair + 'func fail():\n    var one, two, three = pair()\n', "requires 2 bindings, received 3"),
    "short binding list": ('func trio() -> (i32, i32, i32):\n    return 1, 2, 3\n'
                           'func fail():\n    var one, two = trio()\n', "requires 3 bindings, received 2"),
    "duplicate bindings": (pair + 'func fail():\n    var one, one = pair()\n    print(one)\n',
                           "Duplicate destructuring binding"),
    "atomic bindings": (pair + 'func fail():\n    var taken = 0\n'
                        '    var unused, taken = pair()\n    print(unused)\n', "Unknown identifier 'unused'"),
    "top-level bindings": (pair + 'var one, two = pair()\n', "requires a local declaration"),
    "readonly binding": (pair + 'func fail():\n    const one, two = pair()\n    one = 2\n', "Cannot mutate readonly"),
    "readonly component": ('struct cell:\n    value: i32\n'
                           'func alias(const x: cell) -> (cell, i32):\n    return x, 1\n'
                           'func fail():\n    var copied, marker = alias(cell())\n    copied.value = marker\n',
                           "readonly reference leaf"),
    "function value": (pair + 'func fail():\n    var indirect = pair\n', "cannot be used as a function value"),
    "callable type": ('func fail(callback: func() -> (i32, string)):\n    return\n',
                      "Function-value types do not support multiple returns"),
    "anonymous function": ('func fail():\n    var callback = func() -> (i32, string):\n        return 1, ""\n',
                           "not supported for anonymous functions"),
    "async function": ('async func fail() -> (i32, string):\n    return 1, ""\n',
                       "not supported for anonymous functions, async functions"),
    "virtual ABI": ('struct base:\n    number: i32\n'
                    'virtual func (b base) pair() -> (i32, bool):\n    return b.number, true\n'
                    'struct derived extends base:\n    own: i32\n'
                    'override func (d derived) pair() -> (i32, string):\n    return d.own, ""\n', "override"),
    "interface visibility": ('private struct secret:\n    number: i32\n'
                             'interface visible:\n    func read() -> (bool, secret)\n',
                             "cannot expose a less-visible type"),
}

with TemporaryDirectory(prefix="VOID returns 雪 ") as directory:
    work = Path(directory)
    for source in (fixtures / "multiple_returns.void", fixtures / "multiple_returns_helper.void", example):
        shutil.copyfile(source, work / source.name)
    qualify(work, work / "multiple_returns.void", [((), 0, "Multiple returns verified")])
    qualify(work, work / "test_game.void", [
        (("1",), 0, "Accepted"), (("10",), 0, "Accepted"),
        (("hello",), 1, "Invalid number"), (("11",), 1, "Number outside the allowed range"),
        ((), 2, "Usage: test_game <number>"),
    ])
    for name, (text, diagnostic) in negative.items():
        folder = work / name
        folder.mkdir()
        source = folder / "main.void"
        source.write_text(text + "\nfunc main() -> i32:\n    return 0\n", encoding="utf-8")
        result = run([compiler, "check", source], work, status=1)
        assert diagnostic in result.stderr, (name, result.stderr)
print(f"Named and unnamed returns passed JIT O0/O2, independent AOT O0/O2 and {len(negative)} rejection cases")
