#!/usr/bin/env python3
"""Reproduce an isolated TLS candidate review. Never changes production pins.

Linux x86_64 review harness; requires the matching built VOID Runtime and verified
production non-TLS dependencies. All candidate sources/builds stay disposable.
"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import resource
import shutil
import subprocess
import tarfile
import time
import urllib.request


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", choices=["Release", "Debug", "strict"], default="strict")
    parser.add_argument("--runtime-build", type=Path, required=True)
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    if platform.system() != "Linux" or platform.machine() != "x86_64":
        parser.error("This isolated review harness requires native Linux x86_64")
    root = Path(__file__).resolve().parents[1]
    project = root / "tests/net/http/tls-review"
    pin = json.loads((project / "candidate.json").read_text())
    runtime = args.runtime_build.resolve()
    for name in ["libvoid_runtime.a", "libvoid_native_fixture.a"]:
        if not (runtime / name).is_file():
            parser.error(f"Build the matching Runtime first: {runtime / name}")
    cache = (runtime / "CMakeCache.txt").read_text()
    expected_sanitizers = "ON" if args.profile == "strict" else "OFF"
    if f"VOID_SANITIZERS:BOOL={expected_sanitizers}\n" not in cache:
        parser.error("Runtime sanitizer profile does not match the candidate review")
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    evidence = root / f"docs/net/http/evidence/tls-review-{args.profile}-{stamp}"
    evidence.mkdir()
    base = root / ".toolchains/http/tls-review"
    work = base / f"reproduce-{args.profile}-{stamp}"
    work.mkdir(parents=True)
    cc = root / ".toolchains/llvm-23.1.4/bin/clang"
    cxx = cc.with_name("clang++")
    policy = json.loads((root / "third_party/http-versions.json").read_text())
    deps = root / ".toolchains/http/prefix" / policy["bundle"] / args.profile
    env = os.environ.copy()
    if args.profile == "strict":
        env.update(ASAN_OPTIONS="detect_leaks=1:halt_on_error=1", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    report = {"scope": pin["scope"], "pin": pin, "profile": args.profile,
              "stack_bytes": 1048576, "commands": [], "result": "INCOMPLETE"}
    report["runtime_files_sha256"] = {name: hashlib.sha256((runtime / name).read_bytes()).hexdigest()
        for name in ["libvoid_runtime.a", "libvoid_native_fixture.a", "CMakeCache.txt"]}

    def save():
        (evidence / "commands.json").write_text(json.dumps(report, indent=2) + "\n")

    def limited_stack():
        resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))

    def run(argv, name, probe=False):
        argv = [str(value) for value in argv]
        started = time.monotonic()
        with (evidence / f"{name}.stdout").open("wb") as out, (evidence / f"{name}.stderr").open("wb") as err:
            status = subprocess.run(argv, env=env, stdout=out, stderr=err,
                preexec_fn=limited_stack if probe else None, timeout=2400).returncode
        report["commands"].append({"argv": argv, "exit_code": status, "seconds": time.monotonic() - started,
            "soft_hard_stack_bytes": [1048576, 1048576] if probe else None})
        save()
        if status:
            raise SystemExit(f"{name} failed ({status}): {evidence}")
        print(f"{name}: passed", flush=True)

    run([cc, "--version"], "compiler")
    run(["python3", root / "tools/verify_http_dependencies.py", deps, args.profile], "original-dependencies")
    archive = base / "aws-lc-v5.12.0.tar.gz"
    if not archive.exists():
        temporary = archive.with_suffix(".part")
        with urllib.request.urlopen(pin["url"], timeout=120) as source, temporary.open("wb") as output:
            shutil.copyfileobj(source, output)
        temporary.replace(archive)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != pin["sha256"]:
        raise SystemExit("TLS review archive checksum mismatch")
    with tarfile.open(archive) as bundle:
        bundle.extractall(work / "source", filter="data")
    source = work / "source" / ("aws-lc-" + pin["revision"])
    prefix = work / "prefix"
    flags = "-fsanitize=address,undefined -fno-omit-frame-pointer" if args.profile == "strict" else ""
    common = [f"-DCMAKE_C_COMPILER={cc}", f"-DCMAKE_CXX_COMPILER={cxx}",
              "-DCMAKE_BUILD_TYPE=" + ("Release" if args.profile == "Release" else "Debug"),
              f"-DCMAKE_C_FLAGS={flags}", f"-DCMAKE_CXX_FLAGS={flags}", f"-DCMAKE_EXE_LINKER_FLAGS={flags}"]
    run(["cmake", "-S", source, "-B", work / "tls", *common,
         f"-DCMAKE_INSTALL_PREFIX={prefix}", "-DCMAKE_INSTALL_LIBDIR=lib", "-DBUILD_SHARED_LIBS=OFF",
         "-DBUILD_TESTING=OFF", "-DBUILD_LIBSSL=ON", "-DDISABLE_GO=ON", "-DENABLE_SOURCE_MODIFICATION=OFF",
         "-DCMAKE_POSITION_INDEPENDENT_CODE=ON"], "tls-configure")
    run(["cmake", "--build", work / "tls", "--parallel", args.jobs], "tls-build")
    run(["cmake", "--install", work / "tls"], "tls-install")
    run(["cmake", "-S", project, "-B", work / "curl", *common,
         f"-DVOID_TLS_REVIEW_PREFIX={prefix}", f"-DVOID_TLS_REVIEW_RUNTIME={runtime}",
         f"-DVOID_HTTP_DEPS_ROOT={deps}", "-DVOID_SANITIZERS=" + ("ON" if args.profile == "strict" else "OFF")], "curl-configure")
    run(["cmake", "--build", work / "curl", "--parallel", args.jobs], "curl-build")
    link = (work / "curl/CMakeFiles/candidate_native.dir/link.txt").read_text()
    for name in ["libssl.a", "libcrypto.a"]:
        if str(prefix / "lib" / name) not in link or str(deps / "lib" / name) in link:
            raise SystemExit("Mixed TLS dependency detected in isolated review link")
    (evidence / "candidate-native-link.txt").write_text(link)
    for name in ["LICENSE", "NOTICE"]:
        shutil.copy2(source / name, evidence / ("AWS-LC-" + name))
    report["binaries"] = {str(path.relative_to(work)): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in [prefix / "lib/libssl.a", prefix / "lib/libcrypto.a", work / "curl/candidate_native",
                     work / "curl/provider/provider_probe", work / "curl/provider/url_probe"]}
    run([work / "curl/provider/provider_probe"], "cookie-probe", True)
    run([work / "curl/provider/url_probe"], "url-probe", True)
    run(["python3", root / "tests/net/http/fixture.py", work / "curl/candidate_native", "--http2",
         "--certificate-tool", prefix / "bin/openssl", "--evidence", evidence / "native"], "native", True)
    report["result"] = "PASS (isolated Linux candidate only; not production or cross-platform acceptance)"
    save()
    print(evidence)


if __name__ == "__main__":
    main()
