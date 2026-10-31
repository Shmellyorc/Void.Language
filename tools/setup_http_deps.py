#!/usr/bin/env python3
"""Build the pinned HTTP dependency closure; curl itself is checked-in source.

Linux native recipe. Downloads, extraction, builds and prefixes are disposable.
No ambient curl, IDN, TLS, h2, PSL, DNS or compression library is selected.
"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tarfile
import time
import urllib.request
import sys

if platform.system() in ("Windows", "Darwin"):
    native_recipe = Path(__file__).with_name("setup_http_deps_native.py")
    raise SystemExit(subprocess.call([sys.executable, str(native_recipe), *sys.argv[1:]]))

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--profile", choices=["Release", "Debug", "strict"], default="Release")
parser.add_argument("--jobs", type=int, default=4)
args = parser.parse_args()
if platform.system() != "Linux" or platform.machine() not in ("x86_64", "amd64"):
    raise SystemExit("This native recipe is Linux-only; no system-library fallback is permitted")
root = Path(__file__).resolve().parents[1]
pins_path = root / "third_party/http-versions.json"
policy = json.loads(pins_path.read_text())
pins = policy["dependencies"]
base = root / ".toolchains/http"
prefix = base / "prefix" / policy["bundle"] / args.profile
build_root = base / "build" / policy["bundle"] / args.profile
cc = root / ".toolchains/llvm-23.1.4/bin/clang"
cxx = root / ".toolchains/llvm-23.1.4/bin/clang++"
stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
receipt = root / "docs/net/http/evidence" / f"dependencies-{args.profile}-{stamp}"
receipt.mkdir()
prefix.mkdir(parents=True, exist_ok=True)
build_root.mkdir(parents=True, exist_ok=True)
report = {"host": platform.platform(), "profile": args.profile,
          "pins_sha256": hashlib.sha256(pins_path.read_bytes()).hexdigest(), "commands": []}
env = os.environ.copy()
env.update(CC=str(cc), CXX=str(cxx), PKG_CONFIG="/bin/false")
flags = "-O2 -fPIC" if args.profile == "Release" else "-O0 -g -fPIC"
ldflags = ""
if args.profile == "strict":
    flags += " -fsanitize=address,undefined -fno-omit-frame-pointer"
    ldflags = "-fsanitize=address,undefined"
    env.update(ASAN_OPTIONS="detect_leaks=1:halt_on_error=1", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
env.update(CFLAGS=flags, LDFLAGS=ldflags)

def run(argv, name, cwd=build_root, extra_env=None):
    started = time.monotonic()
    with (receipt / f"{name}.stdout").open("wb") as out, (receipt / f"{name}.stderr").open("wb") as err:
        result = subprocess.run([str(x) for x in argv], cwd=cwd, env=env | (extra_env or {}),
                                stdout=out, stderr=err)
    report["commands"].append({"argv": [str(x) for x in argv], "cwd": str(cwd),
                               "exit_code": result.returncode, "seconds": time.monotonic() - started})
    (receipt / "commands.json").write_text(json.dumps(report, indent=2) + "\n")
    if result.returncode:
        raise SystemExit(f"{name} failed ({result.returncode}): {receipt}")
    print(f"{name}: passed", flush=True)

run([cc, "--version"], "compiler")
sources = {}
for pin in pins:
    archive = base / "downloads" / pin["archive"]
    archive.parent.mkdir(parents=True, exist_ok=True)
    if not archive.exists():
        temp = archive.with_suffix(archive.suffix + ".part")
        with urllib.request.urlopen(pin["url"], timeout=120) as response, temp.open("wb") as target:
            shutil.copyfileobj(response, target)
        temp.replace(archive)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != pin["sha256"]:
        raise SystemExit(f"Archive digest mismatch: {archive}")
    # Each profile gets its own pristine extraction, including configure inputs.
    source = build_root / "source" / pin.get("source_dir", pin["name"] + "-" + pin["version"])
    if not source.exists():
        source.parent.mkdir(parents=True, exist_ok=True)
        with tarfile.open(archive) as bundle:
            bundle.extractall(source.parent, filter="data")
    sources[pin["name"]] = source

def cmake(name, options):
    build = build_root / name
    run(["cmake", "-S", sources[name], "-B", build,
         f"-DCMAKE_C_COMPILER={cc}", f"-DCMAKE_CXX_COMPILER={cxx}",
         f"-DCMAKE_INSTALL_PREFIX={prefix}", "-DCMAKE_INSTALL_LIBDIR=lib",
         "-DCMAKE_BUILD_TYPE=" + ("Release" if args.profile == "Release" else "Debug"),
         f"-DCMAKE_C_FLAGS={flags}", f"-DCMAKE_CXX_FLAGS={flags}", f"-DCMAKE_EXE_LINKER_FLAGS={ldflags}",
         "-DCMAKE_POSITION_INDEPENDENT_CODE=ON", *options], name + "-configure")
    run(["cmake", "--build", build, "--parallel", args.jobs], name + "-build")
    run(["cmake", "--install", build], name + "-install")

cmake("nghttp2", ["-DENABLE_LIB_ONLY=ON", "-DBUILD_SHARED_LIBS=OFF", "-DBUILD_STATIC_LIBS=ON",
                  "-DBUILD_TESTING=OFF", "-DENABLE_DOC=OFF"])
cmake("c-ares", ["-DCARES_STATIC=ON", "-DCARES_SHARED=OFF", "-DCARES_BUILD_TESTS=OFF",
                 "-DCARES_BUILD_TOOLS=OFF", "-DCARES_INSTALL=ON"])
cmake("zlib", ["-DZLIB_BUILD_TESTING=OFF", "-DZLIB_BUILD_SHARED=OFF", "-DZLIB_BUILD_STATIC=ON"])
cmake("aws-lc", ["-DBUILD_SHARED_LIBS=OFF", "-DBUILD_TESTING=OFF", "-DBUILD_LIBSSL=ON",
                  "-DDISABLE_GO=ON", "-DENABLE_SOURCE_MODIFICATION=OFF"])

def configure_make(name, options, extra_env=None, subtree=None):
    build = build_root / name
    build.mkdir(exist_ok=True)
    run([sources[name] / "configure", f"--prefix={prefix}", f"--libdir={prefix / 'lib'}",
         "--disable-dependency-tracking", "--disable-rpath", *options], name + "-configure", build, extra_env)
    target = build / subtree if subtree else build
    run(["make", f"-j{args.jobs}"], name + "-build", target, extra_env)
    run(["make", "install"], name + "-install", target, extra_env)

configure_make("libunistring", ["--enable-shared", "--disable-static"], subtree="lib")
# Shared GNU libraries remain replaceable, and their unmodified corresponding
# source archives are installed alongside the license notices by VOID CMake.
unicode_env = {"CPPFLAGS": f"-I{prefix / 'include'}",
               "LDFLAGS": f"-L{prefix / 'lib'} {ldflags}",
               "LD_LIBRARY_PATH": str(prefix / "lib")}
configure_make("libidn2", ["--enable-shared", "--disable-static", "--disable-nls", "--disable-doc",
                            f"--with-libunistring-prefix={prefix}"], unicode_env)
psl_env = unicode_env | {"LIBIDN2_CFLAGS": f"-I{prefix / 'include'}",
                         "LIBIDN2_LIBS": f"-L{prefix / 'lib'} -lidn2 -lunistring"}
configure_make("libpsl", ["--disable-shared", "--enable-static", "--disable-nls", "--disable-man",
                         "--enable-runtime=libidn2", "--enable-builtin",
                         f"--with-psl-file={root / 'third_party/publicsuffix/public_suffix_list.dat'}",
                         "--with-psl-distfile=/dev/null"], psl_env, "src")
psl_header = sources["libpsl"] / "include/libpsl.h"
if not psl_header.exists():
    psl_header = build_root / "libpsl/include/libpsl.h"
shutil.copy2(psl_header, prefix / "include/libpsl.h")
report["result"] = "PASS (native Linux dependency build; transport acceptance separate)"
report["files"] = {str(path.relative_to(prefix)): hashlib.sha256(path.read_bytes()).hexdigest()
                   for folder in ("lib", "include", "bin") for path in sorted((prefix / folder).rglob("*"))
                   if path.is_file() and not path.is_symlink()}
(receipt / "commands.json").write_text(json.dumps(report, indent=2) + "\n")
(prefix / "VOID-HTTP-DEPENDENCIES.json").write_text(json.dumps(report, indent=2) + "\n")
print(prefix)
