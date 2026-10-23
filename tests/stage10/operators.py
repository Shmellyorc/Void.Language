"""Exact owner operator behavior through normal/O2 JIT and independently linked AOT."""
import os
import json
import time
from pathlib import Path
import subprocess
import sys
import tempfile

compiler, clang, linker, runtime, fixture, sanitizer, integration = sys.argv[1:]
flags = ["--driver-mode=g++", "-pthread"]
if linker:
    flags.append("--ld-path=" + linker)
if sanitizer == "ON":
    flags.append("-fsanitize=address,undefined")
if os.name == "nt":
    flags.extend(["-fms-runtime-lib=dll", "-llegacy_stdio_definitions"])
checks = 0
measurements = []


def save_measurements():
    measurement_file=Path(runtime).resolve().parent/'step40-workloads.json'
    measurement_file.write_text(json.dumps(dict(configuration="strict ASan/UBSan/leaks" if sanitizer=="ON" else "Debug",clock="Python perf_counter process wall; C++ steady_clock LLVM compilation/invocation",memory="JIT getrusage process peak RSS includes analysis/compiler/Runtime; not incremental workload memory",measurements=measurements),indent=2)+'\n')


def run(args, status=0, diagnostics=()):
    global checks
    p = subprocess.run([str(a) for a in args], capture_output=True, text=True,
                       encoding="utf-8", timeout=60)
    assert p.returncode == status, (args, p.returncode, p.stdout, p.stderr)
    for message in diagnostics:
        assert message in p.stderr, (message, p.stderr)
    checks += 1
    return p.stdout.strip()


with tempfile.TemporaryDirectory(prefix="VOID Stage10 operators ") as directory:
    root = Path(directory)
    source = root / "main.void"
    source.write_text(Path(__file__).with_name("operators.void").read_text(encoding="utf-8"), encoding="utf-8")
    for optimized in (False, True):
        switches = [] if optimized else ["--no-opt"]
        for value in (0, 4, -3):
            assert run([compiler, "run", source, value, *switches]) == str(value)
        obj = root / ("optimized.o" if optimized else "normal.o")
        exe = obj.with_suffix(".exe" if os.name == "nt" else ".bin")
        run([compiler, "emit", source, "-o", obj, *switches])
        run([clang, *flags, obj, runtime, fixture, "-o", exe])
        for value in (0, 4, -3):
            assert run([exe, value]) == str(value)
    # Reuse the compiler-owned async/native admission harness for fixtures that
    # need explicit async roots or trusted native descriptors. Object files link
    # independently against the ordinary Runtime and fixture archive.
    for dependency in Path(__file__).parent.glob("operator_*.void"):
        (root / dependency.name).write_text(dependency.read_text(encoding="utf-8"), encoding="utf-8")
    cases = {
        "operator_systems": ("sync", [(0, "0"), (4, "4"), (-3, "-3")]),
        "operator_systems_async": ("async", [(0, "0"), (4, "4"), (-3, "-3")]),
        "operator_native_systems": ("sync", [(0, "0"), (4, "4"), (-3, None)]),
        "operator_systems_workload": ("sync", [(3, "81"), (-1000, "1003")]),
        "operator_format_cache": ("sync", [(0, "0"), (4, "4"), (-3, "-3")]),
        "operator_keys": ("sync", [(0, "0"), (4, "4"), (-3, "-3")]),
        "operator_native_compound_invalidation": ("sync", [(0, None), (1, None)]),
        "operator_compound_capture": ("sync", [(0, "0"), (4, "4"), (-3, "-3")]),
        "operator_compound_conversions": ("sync", [(0, "0"), (4, "4"), (-3, "-3")]),
        "operator_compounds": ("sync", [(0, "0"), (4, "4"), (-3, "-3")]),
        "operator_native_compounds": ("sync", [(0, "0"), (4, "4"), (-3, "-3")]),
        "operator_native_compound_failure": ("sync", [(4, "5"), (0, None)]),
        "operator_compound_suspension": ("async", [(0, "0"), (4, "4"), (-3, "-3")]),
        "operator_compound_workers": ("async", [(0, "3"), (4, "7"), (-3, "0")]),
        "operator_lifetimes": ("sync", [(0, "0"), (4, "4"), (-3, "-3")]),
        "operator_suspension": ("async", [(0, "0"), (4, "4"), (-3, "-3")]),
        "operator_workers": ("async", [(0, "0"), (4, "4"), (-3, "-3")]),
        "operator_failures": ("sync", [(4, "4"), (7, "7"), (0, None)]),
        "operator_errors": ("sync", [(7, None)]),
        "operator_worker_failures": ("async", [(7, None)]),
        "operator_cleanup": ("async", [(7, None)]),
        "operator_native": ("sync", [(4, "0"), (2, "1"), (0, None), (-1, None)]),
    }
    for name, (kind, inputs) in cases.items():
        source = root / (name + ".void")
        source.write_text(Path(__file__).with_name(name + ".void").read_text(encoding="utf-8"), encoding="utf-8")
        for optimized in (False, True):
            mode = "optimized" if optimized else "normal"
            obj = root / (name + mode + ".o")
            exe = obj.with_suffix(".exe" if os.name == "nt" else ".bin")
            run([integration, "--emit", source, obj, mode, kind])
            run([clang, *flags, obj, runtime, fixture, "-o", exe])
            for value, expected in inputs:
                diagnostics = () if expected is not None else ("Division by zero",)
                if name == "operator_native_systems" and expected is None:
                    diagnostics = ("Format rejected: cross--1", "Format rejected: cross--2", "Additional failure during defer", "operator +", "holder.target.amount += rhs(input)")
                if name in ("operator_errors", "operator_worker_failures"):
                    diagnostics = ("Format rejected: owned-1", "Format rejected: owned-2", "Additional failure during defer", "operator ==")
                if name == "operator_native_compound_invalidation":
                    diagnostics = ("structural",)
                if name == "operator_cleanup":
                    diagnostics = ("cannot enqueue during structured cleanup",)
                if name == "operator_failures" and expected is None:
                    diagnostics += ("Additional failure during defer", "operator ==", "return 10 / left.value == right.value", "var different = number(value = input) != number(value = 4)")
                for args in ([integration, "--run", source, value, mode, kind], [exe, value]):
                    output = run(args, 0 if expected is not None else 1, diagnostics)
                    if expected is not None:
                        assert output == expected, (args, output, expected)
            if name == "operator_systems_workload":
                for value, expected, description in [(10000,270000,"10000 numeric/custom/temporal interpolated records, 27 scalars each"),(-1000000,1000003,"1000000 decimal places, all 1000003 UTF-8 bytes checked")]:
                    started=time.perf_counter()
                    measured=json.loads(run([integration,"--measure",source,value,mode,kind]))
                    measured.update(engine="JIT",optimization=mode,workload=description,argument=value,wall_seconds=time.perf_counter()-started)
                    assert measured['result']==expected and measured['objects_after_collection']==0,measured
                    measurements.append(measured)
                    started=time.perf_counter()
                    assert run([exe,value])==str(expected)
                    measurements.append(dict(engine="linked_AOT",optimization=mode,workload=description,argument=value,result=expected,wall_seconds=time.perf_counter()-started,memory="not sampled by this process runner"))
                save_measurements()
        print(f"{name}: normal/O2 JIT and independently linked AOT verified",flush=True)
save_measurements()
print(f"{checks} source operator JIT/AOT checks passed")
