#!/usr/bin/env python3
"""Reproduce the isolated curl cookie/OOM gate; never builds or edits VOID."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import resource
import subprocess
import tarfile
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--profile", choices=["Release", "Debug", "strict"], required=True)
parser.add_argument("--archive", type=Path)
parser.add_argument("--compiler", type=Path)
args = parser.parse_args()
repo = Path(__file__).resolve().parents[4]
evidence = Path(__file__).resolve().parent
archive = (args.archive or repo / ".toolchains/http/downloads/curl-8.23.0.tar.xz").resolve()
compiler = (args.compiler or repo / ".toolchains/llvm-23.1.4/bin/clang").resolve()
expected_hash = "8dadd7d2536a314f982ad05a651b713f426bf7d02cc2c76cffb9796f2218e6c5"
assert hashlib.sha256(archive.read_bytes()).hexdigest() == expected_hash
stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
receipt = evidence / ("cookie-" + args.profile + "-" + stamp)
receipt.mkdir()
work = repo / "build/http-cookie-gate" / receipt.name
work.mkdir(parents=True)
report = {"profile": args.profile, "host": platform.platform(),
          "archive_sha256": expected_hash, "commands": [],
          "probe_sha256": hashlib.sha256((evidence / "cookie_accounting_probe.c").read_bytes()).hexdigest(),
          "candidate_patch_sha256": hashlib.sha256((evidence / "curl-8.23.0-cookie-oom-candidate.patch").read_bytes()).hexdigest(),
          "scope": "isolated cookie API; not the selected HTTP dependency profile"}
env = os.environ.copy()
if args.profile == "strict":
    env["ASAN_OPTIONS"] = "detect_leaks=1:halt_on_error=1"
    env["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
    report["sanitizer_environment"] = {key: env[key] for key in ("ASAN_OPTIONS", "UBSAN_OPTIONS")}

def stack_limit():
    resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))

def run(command, name, cwd=repo, constrained=False):
    command = [str(value) for value in command]
    started = time.monotonic()
    result = subprocess.run(command, cwd=cwd, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, timeout=180,
                            preexec_fn=stack_limit if constrained else None)
    (receipt / (name + ".stdout")).write_bytes(result.stdout)
    (receipt / (name + ".stderr")).write_bytes(result.stderr)
    report["commands"].append({"argv": command, "cwd": str(cwd), "name": name,
                               "exit_code": result.returncode,
                               "seconds": time.monotonic() - started,
                               "stack_1mib": constrained})
    (receipt / "commands.json").write_text(json.dumps(report, indent=2) + "\n")
    if result.returncode:
        raise RuntimeError(f"{name}: exit {result.returncode}; retained {receipt}")
    return result.stdout.decode()

run([compiler, "--version"], "compiler")
run(["cmake", "--version"], "cmake")
for variant in ("original", "candidate"):
    source = work / variant / "source"
    source.mkdir(parents=True)
    with tarfile.open(archive) as bundle:
        bundle.extractall(source, filter="data")
    source /= "curl-8.23.0"
    if variant == "candidate":
        run(["patch", "--batch", "--fuzz=0", "-p1", "-i",
             evidence / "curl-8.23.0-cookie-oom-candidate.patch"], "apply-candidate", cwd=source)
    build = work / variant / "build"
    configure = ["cmake", "-S", source, "-B", build,
                 "-DCMAKE_C_COMPILER=" + str(compiler),
                 "-DCMAKE_BUILD_TYPE=" + ("Debug" if args.profile == "strict" else args.profile),
                 "-DBUILD_SHARED_LIBS=OFF", "-DBUILD_STATIC_LIBS=ON", "-DBUILD_CURL_EXE=OFF",
                 "-DBUILD_TESTING=OFF", "-DCURL_DISABLE_INSTALL=ON", "-DCURL_ENABLE_SSL=OFF",
                 "-DCURL_USE_LIBPSL=OFF", "-DUSE_LIBIDN2=OFF", "-DUSE_NGHTTP2=OFF",
                 "-DCURL_ZLIB=OFF", "-DCURL_BROTLI=OFF", "-DCURL_ZSTD=OFF",
                 "-DENABLE_THREADED_RESOLVER=OFF"]
    flags = ["-std=c11", "-Wall", "-Wextra", "-Werror"]
    if args.profile == "strict":
        instrumentation = "-fsanitize=address,undefined -fno-omit-frame-pointer"
        configure += ["-DCMAKE_C_FLAGS=" + instrumentation,
                      "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined"]
        flags += instrumentation.split()
    run(configure, variant + "-configure")
    run(["cmake", "--build", build, "--target", "libcurl_static", "--parallel", "4"], variant + "-build")
    binary = build / "cookie-accounting-probe"
    run([compiler, *flags, "-I" + str(source / "include"),
         evidence / "cookie_accounting_probe.c", build / "lib/libcurl.a", "-o", binary], variant + "-compile")
    for mode in ("faults", "embedding"):
        command = [binary] + (["--embedding-host"] if mode == "embedding" else [])
        output = run(command, variant + "-" + mode, constrained=True)
        rows = [json.loads(line) for line in output.splitlines()]
        assert rows[0] == {"stack_soft": 1048576, "stack_hard": 1048576}
        if mode == "faults":
            expected = ("original_ambiguity_reproduced" if variant == "original"
                        else "checked_oom_in_candidate_patch")
            assert rows[-1] == {"result": expected, "oom_cases": 12}
            faults = [row for row in rows if row.get("fail_ordinal", 0)]
            assert len(faults) == 12
            assert all(row["injected_failures"] == 1 for row in faults)
            assert all(row["api_code"] == (0 if variant == "original" else 27) for row in faults)
        else:
            assert rows[-1]["replacement_allocator_calls"] == 0
    report[variant] = {"binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
                       "binary_bytes": binary.stat().st_size,
                       "provider_archive_bytes": (build / "lib/libcurl.a").stat().st_size}
report["result"] = "original ambiguity reproduced; candidate checked OOM; embedding hooks not replaced"
(receipt / "commands.json").write_text(json.dumps(report, indent=2) + "\n")
print(receipt)
