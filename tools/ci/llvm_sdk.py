"""Install the complete official LLVM 19.1.7 archive for GitHub-hosted runners.

This helper never installs a compiler-only Windows .exe: the SDK must contain
headers, static development libraries, clang, and LLVMConfig.cmake together.
"""
from pathlib import Path
import os
import platform
import shutil
import subprocess
import tarfile
import tempfile
import urllib.request

VERSION = "19.1.7"
RELEASE = f"https://github.com/llvm/llvm-project/releases/download/llvmorg-{VERSION}"


def install(destination: Path) -> None:
    if destination.exists():
        return
    system = platform.system()
    arch = platform.machine().lower()
    if system == "Windows" and arch in ("amd64", "x86_64"):
        asset = f"clang+llvm-{VERSION}-x86_64-pc-windows-msvc.tar.xz"
    elif system == "Linux" and arch == "x86_64":
        asset = f"LLVM-{VERSION}-Linux-X64.tar.xz"
    elif system == "Darwin" and arch in ("arm64", "aarch64", "x86_64"):
        asset = f"LLVM-{VERSION}-macOS-{'X64' if arch == 'x86_64' else 'ARM64'}.tar.xz"
    else:
        raise RuntimeError(f"Unsupported CI host: {system} {arch}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=destination.parent) as scratch:
        scratch = Path(scratch)
        archive = scratch / asset
        print(f"Downloading official complete SDK: {asset}", flush=True)
        urllib.request.urlretrieve(f"{RELEASE}/{asset}", archive)
        extracted = scratch / "extracted"
        with tarfile.open(archive) as package:
            package.extractall(extracted, filter="data")
        configs = list(extracted.rglob("LLVMConfig.cmake"))
        if len(configs) != 1:
            raise RuntimeError("Expected exactly one LLVM SDK CMake configuration")
        shutil.move(str(configs[0].parents[3]), destination)


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
    clang = destination / f"bin/clang{suffix}"
    result = subprocess.check_output([clang, "--version"], text=True)
    if f"clang version {VERSION}" not in result:
        raise RuntimeError(f"Mismatched Clang: {result}")
    print(result)
    cc = destination / ("bin/clang-cl.exe" if os.name == "nt" else "bin/clang")
    cxx = destination / ("bin/clang-cl.exe" if os.name == "nt" else "bin/clang++")
    values = {"LLVM_DIR": destination / "lib/cmake/llvm", "CC": cc, "CXX": cxx, "VOID_CLANG": clang}
    if platform.system() == "Darwin":
        values["SDKROOT"] = subprocess.check_output(["xcrun", "--show-sdk-path"], text=True).strip()
    with open(os.environ["GITHUB_ENV"], "a", encoding="utf-8") as env:
        for key, value in values.items():
            env.write(f"{key}={value}\n")
    with open(os.environ["GITHUB_PATH"], "a", encoding="utf-8") as path:
        path.write(f"{destination / 'bin'}\n")


if __name__ == "__main__":
    main()
