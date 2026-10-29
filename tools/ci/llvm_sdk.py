"""Install the complete official LLVM 23.1.4 archive for GitHub-hosted runners.

This helper never installs a compiler-only Windows .exe: the SDK must contain
headers, static development libraries, clang, and LLVMConfig.cmake together.
"""
from pathlib import Path
import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import tarfile
import tempfile
import urllib.request

VERSION = "23.1.4"
RELEASE = f"https://github.com/llvm/llvm-project/releases/download/llvmorg-{VERSION}"
# SHA-256 digests published on the official llvmorg-23.1.4 release assets.
# A cache hit must carry the same verified archive identity as a fresh install.
ARCHIVES = {
    ("Windows", "x86_64"): (f"clang+llvm-{VERSION}-x86_64-pc-windows-msvc.tar.xz",
                            "854bffcfc84528e0ee92490ff5c178920abd358a79c3e98f053e41e3e8e4c6d2"),
    ("Linux", "x86_64"): (f"LLVM-{VERSION}-Linux-X64.tar.xz",
                          "25d814fda50d5fb0084b42be09605d8b1595d49d5e2e03a3345d7e112ff6f9f3"),
    ("Darwin", "aarch64"): (f"LLVM-{VERSION}-macOS-ARM64.tar.xz",
                            "5d3179db39adeca5b17a34d82708fe976bc60e055b05fe557878fb3fb64d39ef"),
}


def archive_identity() -> dict:
    arch = platform.machine().lower()
    arch = {"amd64": "x86_64", "arm64": "aarch64"}.get(arch, arch)
    try:
        asset, sha256 = ARCHIVES[platform.system(), arch]
    except KeyError:
        raise RuntimeError(f"Unsupported CI host: {platform.system()} {arch}") from None
    return {"version": VERSION, "asset": asset, "sha256": sha256,
            "url": f"{RELEASE}/{asset}"}


def install(destination: Path) -> None:
    identity = archive_identity()
    if destination.exists():
        receipt = destination / "void-sdk-identity.json"
        if not receipt.is_file() or json.loads(receipt.read_text(encoding="utf-8")) != identity:
            raise RuntimeError(f"Unverified or mismatched cached SDK: {destination}")
        return
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=destination.parent) as scratch:
        scratch = Path(scratch)
        archive = scratch / identity["asset"]
        print(f"Downloading official complete SDK: {identity['asset']}", flush=True)
        urllib.request.urlretrieve(identity["url"], archive)
        with archive.open("rb") as stream:
            actual = hashlib.file_digest(stream, "sha256").hexdigest()
        if actual != identity["sha256"]:
            raise RuntimeError(f"LLVM archive checksum mismatch: {actual}")
        extracted = scratch / "extracted"
        with tarfile.open(archive) as package:
            package.extractall(extracted, filter="data")
        configs = list(extracted.rglob("LLVMConfig.cmake"))
        if len(configs) != 1:
            raise RuntimeError("Expected exactly one LLVM SDK CMake configuration")
        sdk = configs[0].parents[3]
        (sdk / "void-sdk-identity.json").write_text(json.dumps(identity, indent=2) + "\n", encoding="utf-8")
        shutil.move(str(sdk), destination)


def main() -> None:
    destination = Path(".toolchains") / f"llvm-{VERSION}"
    destination = destination.resolve()
    install(destination)
    suffix = ".exe" if os.name == "nt" else ""
    for relative in ("include/llvm/ExecutionEngine/Orc/LLJIT.h", "lib/cmake/llvm/LLVMConfig.cmake",
                     f"bin/clang{suffix}", f"bin/clang++{suffix}"):
        if not (destination / relative).is_file():
            raise RuntimeError(f"Incomplete SDK: missing {relative}")
    if not list((destination / "lib").glob("LLVMCore.*")) and not (destination / "lib/libLLVMCore.a").exists():
        raise RuntimeError("Incomplete SDK: LLVMCore development library missing")
    config = (destination / "include/llvm/Config/llvm-config.h").read_text(encoding="utf-8")
    if f'#define LLVM_VERSION_STRING "{VERSION}"' not in config:
        raise RuntimeError("Mismatched LLVM development headers")
    clang = destination / f"bin/clang{suffix}"
    result = subprocess.check_output([clang, "--version"], text=True)
    if not re.search(r"clang version " + re.escape(VERSION) + r"(?:\D|$)", result):
        raise RuntimeError(f"Mismatched Clang: {result}")
    print(result)
    cc = destination / ("bin/clang-cl.exe" if os.name == "nt" else "bin/clang")
    cxx = destination / ("bin/clang-cl.exe" if os.name == "nt" else "bin/clang++")
    values = {"LLVM_DIR": destination / "lib/cmake/llvm", "CC": cc, "CXX": cxx, "VOID_CLANG": clang}
    values["VOID_LINKER"] = ""
    linker_name = {"Linux": "ld.lld", "Windows": "lld-link.exe", "Darwin": "ld64.lld"}[platform.system()]
    linker = destination / "bin" / linker_name
    result = subprocess.check_output([linker, "--version"], text=True)
    if not re.search(r"LLD " + re.escape(VERSION) + r"(?:\D|$)", result):
        raise RuntimeError(f"Mismatched LLD: {result}")
    print(result)
    if platform.system() == "Linux":
        values["VOID_LINKER"] = linker
    if platform.system() == "Darwin":
        values["SDKROOT"] = subprocess.check_output(["xcrun", "--show-sdk-path"], text=True).strip()
    with open(os.environ["GITHUB_ENV"], "a", encoding="utf-8") as env:
        for key, value in values.items():
            env.write(f"{key}={value}\n")
    with open(os.environ["GITHUB_PATH"], "a", encoding="utf-8") as path:
        path.write(f"{destination / 'bin'}\n")


if __name__ == "__main__":
    main()
