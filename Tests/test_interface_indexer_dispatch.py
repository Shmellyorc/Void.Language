#!/usr/bin/env python3
"""General interface/virtual indexers and collection regressions, strict C."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
NAME = "InterfaceIndexerDispatch"
PROJECT = ROOT / "Tests" / NAME

def run(args):
    return subprocess.run(args, cwd=ROOT, capture_output=True, timeout=120)

def require(condition, result):
    if not condition:
        raise SystemExit(result.stdout.decode(errors="replace") + result.stderr.decode(errors="replace"))
    print("True")

for action in ("check", "build"):
    result = run([str(ROOT / "bin/voidc"), action, str(PROJECT)])
    require(result.returncode == 0, result)
result = run([str(PROJECT / "bin" / NAME)])
require(result.returncode == 0 and not result.stderr and result.stdout.splitlines() == [b"True"] * 38, result)
print(result.stdout.decode(), end="")
result = run([str(ROOT / "bin/voidc"), "check", "Tests/InterfaceIndexerDiagnostics"])
require(result.returncode != 0 and b"error:" in result.stderr and b"string" in result.stderr, result)
with tempfile.TemporaryDirectory(prefix="void-indexer-strict-") as temporary:
    result = run(shlex.split(os.environ.get("CC", "cc")) +
                 ["-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-O2",
                  "-IRuntime/include", "-pthread", "-c", str(PROJECT / ".void" / (NAME + ".c")),
                  "-o", str(Path(temporary) / "strict.o")])
    require(result.returncode == 0 and not result.stderr, result)
