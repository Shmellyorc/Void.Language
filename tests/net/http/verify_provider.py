#!/usr/bin/env python3
"""Clean builds from checked-in curl; no downloads or patch application."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
try:
    import resource
except ImportError:
    resource = None
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--profile", choices=["Release", "Debug", "strict", "no-cookies", "no-http"], required=True)
parser.add_argument("--compiler", type=Path)
parser.add_argument("--full", action="store_true", help="Use the pinned full HTTP dependency closure and URL checks")
args = parser.parse_args()
repo = Path(__file__).resolve().parents[3]
compiler = (args.compiler or repo / (".toolchains/llvm-23.1.4/bin/clang-cl.exe" if os.name == "nt" else ".toolchains/llvm-23.1.4/bin/clang")).resolve()
stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
receipt = repo / "docs/net/http/evidence" / f"vendored-{'full-' if args.full else ''}{args.profile}-{stamp}"
build = repo / "build/http-vendored-provider" / receipt.name
receipt.mkdir(parents=True)
report = {"profile": args.profile, "host": platform.platform(), "commands": [],
          "source": "third_party/curl (tracked source; no patch application)",
          "provenance_sha256": hashlib.sha256((repo / "third_party/curl/VOID-PROVENANCE.json").read_bytes()).hexdigest(),
          "scope": "full provider/URL and cookie regression; no network transport acceptance" if args.full else
                   "cookie/OOM regression; not full transport or native Windows/macOS acceptance"}
env = os.environ.copy()
strict = args.profile in ("strict", "no-cookies", "no-http")
if strict:
    env.update(ASAN_OPTIONS="detect_leaks=1:halt_on_error=1", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    report["sanitizer_environment"] = {key: env[key] for key in ("ASAN_OPTIONS", "UBSAN_OPTIONS")}

def stack_limit():
    resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))

def run(command, name, constrained=False):
    argv = [str(value) for value in command]
    started = time.monotonic()
    result = subprocess.run(argv, cwd=repo, env=env, capture_output=True, timeout=300,
                            preexec_fn=stack_limit if constrained and resource else None)
    (receipt / f"{name}.stdout").write_bytes(result.stdout)
    (receipt / f"{name}.stderr").write_bytes(result.stderr)
    report["commands"].append({"argv": argv, "exit_code": result.returncode,
                               "seconds": time.monotonic() - started, "stack_1mib": constrained})
    (receipt / "commands.json").write_text(json.dumps(report, indent=2) + "\n")
    if result.returncode:
        raise SystemExit(f"{name} failed ({result.returncode}); evidence: {receipt}")
    return result.stdout.decode()

run(["python3", repo / "tools/verify_curl_source.py"], "verify-source")
run([compiler, "--version"], "compiler")
profile = {"no-cookies": "NO_COOKIES", "no-http": "NO_HTTP"}.get(args.profile, "COOKIE")
if args.full:
    assert profile == "COOKIE", "disabled configurations are isolated regressions"
    profile = "HTTP"
native_flags = ["-G", "Ninja", "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded",
                "-DCMAKE_C_FLAGS_DEBUG=/Od /Zi /D_DEBUG", "-DCMAKE_CXX_FLAGS_DEBUG=/Od /Zi /D_DEBUG /D_ITERATOR_DEBUG_LEVEL=0"] if os.name == "nt" else []
cxx = compiler if os.name == "nt" else compiler.with_name('clang++')
run(["cmake", "-S", repo / "tests/net/http/provider", "-B", build, *native_flags,
     f"-DCMAKE_C_COMPILER={compiler}", f"-DCMAKE_CXX_COMPILER={cxx}", f"-DVOID_CURL_PROFILE={profile}",
     "-DCMAKE_BUILD_TYPE=" + ("Release" if args.profile == "Release" else "Debug"),
     "-DVOID_SANITIZERS=" + ("ON" if strict else "OFF")], "configure")
run(["cmake", "--build", build, "--parallel", "4"], "build")
binary = build / "provider_probe"
output = run([binary], "probe", constrained=True)
rows = [json.loads(line) for line in output.splitlines()]
assert rows[0]["stack_soft"] == rows[0]["stack_hard"] == 1048576
if profile in ("COOKIE", "HTTP"):
    assert rows[-1] == {"result": "checked_oom_in_candidate_patch", "oom_cases": 12}
    assert all(row["api_code"] == 27 and row["injected_failures"] == 1
               for row in rows if row.get("fail_ordinal", 0))
    host = run([binary, "--embedding-host"], "embedding", constrained=True)
    assert json.loads(host.splitlines()[-1])["replacement_allocator_calls"] == 0
    if args.full:
        run([build / "url_probe"], "url-provider", constrained=True)
else:
    assert rows[-1]["code"] == 0 and rows[-1]["list_null"] is True
archive = build / "curl/lib/libcurl.a"
report.update(result="PASS", binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
              archive_sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),
              archive_bytes=archive.stat().st_size)
(receipt / "commands.json").write_text(json.dumps(report, indent=2) + "\n")
print(receipt)
