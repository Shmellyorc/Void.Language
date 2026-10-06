#!/usr/bin/env python3
"""Post-320 focused contracts, negative binding and strict generated-C checks."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
GROUPS = {"list": ("EngineListOperations", 58, "List"),
          "set": ("EngineSetOperations", 2589, "Set"),
          "stream": ("EngineMemoryStream", 60, "MemoryStream")}
parser = argparse.ArgumentParser()
parser.add_argument("group", choices=GROUPS)
args = parser.parse_args()
name, expected, negative = GROUPS[args.group]
project = "Tests/" + name

def run(command):
    return subprocess.run(command, cwd=ROOT, capture_output=True, timeout=120)

def require(condition, result):
    if not condition:
        raise SystemExit(result.stdout.decode(errors="replace") + result.stderr.decode(errors="replace"))
    print("True")

for action in ("check", "build"):
    result = run([str(ROOT / "bin/voidc"), action, project])
    require(result.returncode == 0, result)
result = run([str(ROOT / project / "bin" / name)])
if result.returncode != 0 or result.stdout.splitlines() != [b"True"] * expected or result.stderr:
    raise SystemExit("runtime contract failure: " + str(result.returncode) + "\n" + result.stdout.decode(errors="replace") + result.stderr.decode(errors="replace"))
print(result.stdout.decode(), end="")
result = run([str(ROOT / "bin/voidc"), "check", "Tests/EngineReadinessDiagnostics/" + negative])
require(result.returncode != 0 and b"error:" in result.stderr, result)
with tempfile.TemporaryDirectory(prefix="void-engine-strict-") as temporary:
    result = run(shlex.split(os.environ.get("CC", "cc")) +
                 ["-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-O2",
                  "-ICompiler/include", "-IRuntime/include", "-pthread", "-c",
                  project + "/.void/" + name + ".c", "-o", str(Path(temporary) / "checked.o")])
    require(result.returncode == 0 and not result.stderr
            and "WriteUtf8Stdout" not in (ROOT / "Compiler/src/compiler.c").read_text()
            and "WriteUtf8Stderr" not in (ROOT / "Compiler/src/semantic.c").read_text(), result)

if args.group == "stream":
    fixture = "Tests/EngineMemoryStreamUnusedVirtual"
    result = run([str(ROOT / "bin/voidc"), "build", fixture])
    require(result.returncode == 0, result)
    result = run([str(ROOT / fixture / "bin/EngineMemoryStreamUnusedVirtual")])
    require(result.returncode == 0 and result.stdout.splitlines() == [b"True", b"True"] and not result.stderr, result)
    with tempfile.TemporaryDirectory(prefix="void-unused-virtual-") as temporary:
        result = run(shlex.split(os.environ.get("CC", "cc")) +
                     ["-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-O2",
                      "-ICompiler/include", "-IRuntime/include", "-pthread", "-c",
                      fixture + "/.void/EngineMemoryStreamUnusedVirtual.c", "-o", str(Path(temporary) / "checked.o")])
        require(result.returncode == 0 and not result.stderr, result)
