"""Run the same timer fixtures through both JIT modes and independent AOT links."""

import os
from pathlib import Path
import re
import subprocess
import sys
from tempfile import TemporaryDirectory


def invoke(command, work):
    return subprocess.run(list(map(str, command)), cwd=work, capture_output=True,
                          encoding="utf-8", timeout=45)


def run(command, work):
    result = invoke(command, work)
    assert result.returncode == 0 and not result.stderr, (command, result)
    return result.stdout.splitlines()


def link(object_file, application, work):
    flags = ["--driver-mode=g++"]
    if windows:
        sdk_linker = Path(clang).with_name("lld-link.exe").resolve(strict=True)
        assert run([sdk_linker, "--version"], work)[0].startswith("LLD 23.1.4")
        flags.extend(["-B" + str(sdk_linker.parent), "-fuse-ld=lld"])
    else:
        assert linker and "LLD 23.1.4" in run([linker, "--version"], work)[0]
        flags.append("--ld-path=" + linker)
    command = [clang, *flags, object_file, *resources, "-o", application]
    if windows:
        plan = invoke([*command, "-###"], work)
        launch = re.search(r'^\s*"([^"]*lld-link(?:\.exe)?)"', plan.stderr, re.M | re.I)
        assert plan.returncode == 0 and launch, plan
        actual = Path(launch[1].replace("\\\\", "\\")).with_suffix(".exe").resolve()
        assert actual == sdk_linker, plan
    run(command, work)


def exercise(source, expected, work):
    run([compiler, "check", source], work)
    lines = run([runner, "--async-one-worker", source], work)
    assert [line for line in lines if line not in {
        "Debug source execution passed", "O2 source execution passed"
    }] == expected * 2, lines
    assert lines.count("Debug source execution passed") == lines.count("O2 source execution passed") == 1, lines
    for mode in ("normal", "optimized"):
        stem = source.stem + "-" + mode
        object_file = work / (stem + (".obj" if windows else ".o"))
        application = work / (stem + (".exe" if windows else ".bin"))
        run([runner, "--emit-async", source, object_file, mode], work)
        link(object_file, application, work)
        assert run([application, "7"], work) == expected + ["7"], (source, mode)
    print(f"{source.name}: one-worker normal/O2 JIT and independent normal/O2 AOT passed")


def reject(name, code, position, message, source, work):
    fixture = work / (name + ".void")
    fixture.write_text("use time\n" + source, encoding="utf-8")
    result = invoke([compiler, "check", fixture], work)
    expected = f"{fixture.name}:{position}: error: {message}\n    code: {code}"
    assert result.returncode == 1 and expected in result.stderr, result


compiler, runner, manifest = (Path(arg).resolve(strict=True) for arg in sys.argv[1:4])
version, clang, linker, *resources = manifest.read_text(encoding="utf-8").splitlines()
assert version == "VOID native 23.1.4 v2"
fixtures = Path(__file__).resolve().parent
windows = os.name == "nt"
# The existing development adapter takes narrow argv on Windows.
with TemporaryDirectory(prefix="VOID async timers ") as directory:
    work = Path(directory).resolve()
    assert "clang version 23.1.4" in run([clang, "--version"], work)[0]
    if sys.argv[4:] == ["--network"]:
        exercise(fixtures / "timer_network.void", ["Timer, TCP and UDP controls passed"], work)
    else:
        assert not sys.argv[4:]
        exercise(fixtures / "timers.void", ["Timer source controls passed"], work)
        example = fixtures.parents[1] / "examples" / "async" / "turn_timer.void"
        exercise(example, ["Aim adjustment: 15", "Shot ready", "Turn drained"], work)
        forbidden = "await requires an async function and is not allowed in deferred code."
        reject("sync", "VOID1103", "3:5", forbidden,
               "func illegal(delay: time.duration):\n    await time.sleep(delay)\n", work)
        reject("cleanup", "VOID1103", "4:9", forbidden,
               "async func illegal(delay: time.duration):\n    defer {\n        await time.sleep(delay)\n    }\n", work)
        reject("private", "VOID2003", "3:16", "Unknown imported function 'time._sleep'.",
               "async func illegal():\n    await time._sleep(0, 0)\n", work)
        reject("type", "VOID2001", "3:22", "Type mismatch: expected /@stdlib/time.void::duration, received i32.",
               "async func illegal():\n    await time.sleep(1)\n", work)
        print("Synchronous, cleanup, private primitive and duration diagnostics passed")
