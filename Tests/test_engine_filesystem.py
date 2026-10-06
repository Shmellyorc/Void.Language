#!/usr/bin/env python3
"""Native ownership, error paths, partial transfers and incremental UTF-8."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
CC = shlex.split(os.environ.get("CC", "cc"))
FLAGS = ["-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-O2", "-IRuntime/include", "-pthread"]
EXPECTED = 67

def run(command, cwd=ROOT, env=None):
    return subprocess.run(command, cwd=cwd, env=env, capture_output=True, timeout=120)

def require(condition, result):
    if not condition:
        raise SystemExit(result.stdout.decode(errors="replace") + result.stderr.decode(errors="replace"))
    print("True")

def runtime(executable, directory, expected, arguments=(), env=None):
    result = run([str(executable), *arguments], cwd=directory, env=env)
    require(result.returncode == 0 and not result.stderr and result.stdout.splitlines() == [b"True"] * expected, result)
    print(result.stdout.decode(), end="")

for name in ("EngineTextStreams", "EngineNativeFailure"):
    for action in ("check", "build"):
        result = run([str(ROOT / "bin/voidc"), action, "Tests/" + name])
        require(result.returncode == 0, result)

with tempfile.TemporaryDirectory(prefix="void-filesystem-") as temporary:
    directory = Path(temporary)
    # Keep the existing guarded-check total while testing the accounting itself.
    probe = directory / 'allocation-probe'
    compiled = run(CC + FLAGS + ['Tests/EngineTextStreams/native/allocation_probe.c',
                               'Tests/EngineTextStreams/native/hooks.c',
                               'Runtime/src/vc_thread.c', '-o', str(probe)])
    assert compiled.returncode == 0 and not compiled.stderr, (compiled.stdout, compiled.stderr)
    for environment in (None, dict(os.environ, VOID_TEST_STATE_OOM='1')):
        balanced = run([str(probe), 'balanced'], env=environment)
        assert balanced.returncode == 0 and not balanced.stdout and not balanced.stderr, (balanced.returncode, balanced.stdout, balanced.stderr)
    for family in ('malloc', 'calloc', 'realloc'):
        leaked = run([str(probe), 'leak-' + family])
        assert leaked.returncode == 97 and not leaked.stdout, (family, leaked.returncode, leaked.stdout, leaked.stderr)
        assert leaked.stderr.replace(b'\r\n', b'\n') == b'native resource allocation leaked\n', (family, leaked.stderr)
    base = ["Runtime/src/vc_thread.c", "Runtime/src/vc_atomic.c", "Runtime/src/vc_memory.c"]
    # Prove that both native API paths inject denial and short transfers.
    io_probe = directory / 'io-probe'
    compiled = run(CC + FLAGS + ['Tests/EngineTextStreams/native/io_probe.c',
                               'Tests/EngineTextStreams/native/hooks.c',
                               'Runtime/src/vc_thread.c', '-o', str(io_probe)])
    assert compiled.returncode == 0 and not compiled.stderr, (compiled.stdout, compiled.stderr)
    for environment in (None, dict(os.environ, VOID_TEST_DENIED='1')):
        injected = run([str(io_probe)], cwd=directory, env=environment)
        assert injected.returncode == 0 and not injected.stdout and not injected.stderr, (injected.returncode, injected.stdout, injected.stderr)
    binaries = {}
    for name in ("EngineTextStreams", "EngineNativeFailure"):
        executable = directory / name
        result = run(CC + FLAGS + ["Tests/" + name + "/.void/" + name + ".c", *base,
                                    "Runtime/src/vc_filesystem.c", "-o", str(executable)])
        require(result.returncode == 0 and not result.stderr, result)
        binaries[name] = executable
    normal = directory / "normal"; normal.mkdir()
    runtime(binaries["EngineTextStreams"], normal, EXPECTED)
    require(not list(normal.iterdir()), result)
    backend = directory / "partial.o"
    defines = ["-include", "Tests/EngineTextStreams/native/hooks_redirect.h"]
    result = run(CC + FLAGS + defines + ["-c", "Runtime/src/vc_filesystem.c", "-o", str(backend)])
    require(result.returncode == 0 and not result.stderr, result)
    for name in ("EngineTextStreams", "EngineNativeFailure"):
        executable = directory / (name + "-partial")
        result = run(CC + FLAGS + ["Tests/" + name + "/.void/" + name + ".c", *base,
                                  "Tests/EngineTextStreams/native/hooks.c", str(backend), "-o", str(executable)])
        require(result.returncode == 0 and not result.stderr, result)
        binaries[name + "-partial"] = executable
    partial = directory / "partial"; partial.mkdir()
    runtime(binaries["EngineTextStreams-partial"], partial, EXPECTED)
    require(not list(partial.iterdir()), result)
    failures = directory / "failures"; failures.mkdir()
    (failures / "probe.txt").write_text("unchanged", encoding="utf-8")
    for variable, argument in (("VOID_TEST_STATE_OOM", "oom"), ("VOID_TEST_MONITOR_OOM", "oom"), ("VOID_TEST_DENIED", "denied")):
        environment = os.environ.copy(); environment[variable] = "1"
        marker = failures / "oom.expected"
        if argument == "oom": marker.write_text("OOM", encoding="utf-8")
        elif marker.exists(): marker.unlink()
        runtime(binaries["EngineNativeFailure-partial"], failures, 1, env=environment)
        require((failures / "probe.txt").read_text() == "unchanged", result)
result = run([str(ROOT / "bin/voidc"), "check", "Tests/EngineFilesystemDiagnostics"])
require(result.returncode != 0 and b"StandardLibrary primitive signature" in result.stderr, result)
