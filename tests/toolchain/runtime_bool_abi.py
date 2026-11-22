"""Compare the shared Runtime ABI annotation against pinned Clang, including MSVC."""
from pathlib import Path
import re
import subprocess
import sys

fixture, clang = map(str, map(Path, sys.argv[1:3]))
source = '''extern "C" bool vd_abi_bool_probe(bool, bool, unsigned char);
extern "C" bool probe(bool a, bool b, unsigned char c) {
    return vd_abi_bool_probe(!a, b, c);
}
'''


def run(argv, text=None):
    result = subprocess.run(argv, input=text, text=True, capture_output=True, timeout=30)
    assert result.returncode == 0, (argv, result.stdout, result.stderr)
    return result.stdout


assert "clang version 23.1.4" in run([clang, "--version"])
for triple in ("x86_64-unknown-linux-gnu", "x86_64-pc-windows-msvc", "arm64-apple-macosx11.0.0"):
    actual = run([fixture, triple])
    for optimization in ("-O0", "-O2"):
        oracle = run([clang, "--driver-mode=g++", "-target", triple, optimization,
                      "-S", "-emit-llvm", "-x", "c++", "-", "-o", "-"], source)
        for text in (actual, oracle):
            lines = [line for line in text.splitlines()
                     if "@vd_abi_bool_probe(" in line and ("declare " in line or "call " in line)]
            assert len(lines) == 2, text
            for line in lines:
                assert re.search(r"(?:declare|call)\s+(?:dso_local\s+)?zeroext i1", line), line
                arguments = line.split("@vd_abi_bool_probe(", 1)[1].split(")", 1)[0].split(",")
                assert all(re.search(r"\bi1\b.*\bzeroext\b", arg) for arg in arguments[:2]), line
    print(f"PASS: {triple} declaration/callsite bool ABI agrees with Clang O0/O2")
run([fixture, "wasm32-unknown-unknown"])
print("PASS: unrelated declarations, source definitions, i8 and unsupported targets unchanged")
