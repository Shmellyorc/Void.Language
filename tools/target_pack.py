"""Assemble a Linux GNU target pack from verified native build inputs.

The system link inputs come from the named destination developer installation.
A local pack can be carried to another OS without carrying its compiler.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shlex
import shutil
import subprocess
import sys
import tarfile
import tempfile
from urllib.parse import urlsplit
from urllib.request import urlretrieve

ROOT = Path(__file__).resolve().parents[1]
TARGETS = {"x86_64": ("linux-x64", "ld-linux-x86-64.so.2"),
           "aarch64": ("linux-arm64", "ld-linux-aarch64.so.1")}
ARCHIVES = ("libvoid_runtime.a", "libvoid_native_fixture.a", "libarchive.a", "libsodium.a",
            "libcurl.a", "libpsl.a", "libnghttp2.a", "libssl.a", "libcrypto.a", "libcares.a", "libz.a")
SYSTEM = ("Scrt1.o", "crti.o", "crtbeginS.o", "libidn2.so.0", "libunistring.so.5", "libstdc++.so.6",
          "libm.so.6", "libgcc.a", "libgcc_s.so.1", "libc.so.6", "crtendS.o", "crtn.o")


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def command(*args):
    return subprocess.check_output(args, text=True).strip()


def runtime_files(build):
    header, *rows = (build / "void-native-resources.txt").read_text().splitlines()
    if header != "VOID native resources 1":
        raise ValueError("Expected the native build resource inventory")
    files = {}
    for record in rows:
        expected, raw_path = record.split("\t")
        path = Path(raw_path)
        if path.name in ARCHIVES:
            if path.name in files or not path.is_absolute() or sha(path) != expected:
                raise ValueError(f"Invalid native build input {path}")
            files[path.name] = path
    if set(files) != set(ARCHIVES):
        raise ValueError(f"Missing runtime archives: {sorted(set(ARCHIVES) - files.keys())}")
    return files


def license_files(build):
    header, *rows = (build / "void-package-notices.txt").read_text().splitlines()
    if header != "VOID package notices 1":
        raise ValueError("Expected the native build notice inventory")
    files = {}
    for record in rows:
        expected, name, raw_path = record.split("\t")
        path = Path(raw_path)
        if Path(name).name != name or name.casefold() in {item.casefold() for item in files} or sha(path) != expected:
            raise ValueError(f"Invalid native build notice {path}")
        files[name] = path
    return files


def system_file(cc, name):
    path = Path(command(cc, f"-print-file-name={name}"))
    if not path.is_absolute() or not path.is_file():
        raise ValueError(f"Missing destination developer library or startup object: {name}")
    return path


def populate(stage, header, files):
    (stage / "link").mkdir(parents=True)
    (stage / "licenses").mkdir()
    records = []
    for relative, source in sorted(files.items()):
        destination = stage / relative
        shutil.copyfile(source, destination)
        records.append(f"{sha(destination)}\t{relative}")
    (stage / "target-pack.txt").write_text("\n".join(header + records) + "\n", encoding="ascii", newline="\n")


def apt_record(source):
    # Bind to the exact installed Ubuntu package via APT's authenticated index.
    installed = source.resolve()
    package = command("dpkg-query", "-S", str(installed)).split(": ", 1)[0]
    version = command("dpkg-query", "-W", "-f=${Version}", package)
    url, _, size, value = shlex.split(command("apt-get", "--print-uris", "download", f"{package}={version}"))
    if url.startswith("http://"):
        url = "https://" + url.removeprefix("http://")
    if not value.startswith("SHA256:"):
        raise ValueError(f"Expected an authenticated APT package checksum for {package}")
    source_identity = command("dpkg-query", "-W", "-f=${source:Package}=${source:Version}", package)
    copyright_file = Path("/usr/share/doc") / package.split(":")[0] / "copyright"
    return {"url": url, "bytes": int(size), "sha256": value.removeprefix("SHA256:"),
            "path": installed.as_posix().lstrip("/"), "member_sha256": sha(installed),
            "package": package, "source": source_identity}, copyright_file


def assemble(args):
    if platform.system() != "Linux" or platform.machine() not in TARGETS:
        raise ValueError("Assemble from a native Linux GNU runtime build")
    target, loader = TARGETS[platform.machine()]
    if command(args.cc, "-dumpmachine").split("-")[0] != platform.machine():
        raise ValueError("The GCC system input architecture must match the runtime build")
    minimum = command("getconf", "GNU_LIBC_VERSION").replace(" ", "-")
    build = args.build.resolve()
    compiler = re.search(r"^project\(VoidLanguage VERSION ([\d.]+)", (ROOT / "CMakeLists.txt").read_text(), re.M)[1]
    revision = command("git", "-C", str(ROOT), "rev-parse", "HEAD")
    cache = (build / "CMakeCache.txt").read_text()
    if "CMAKE_BUILD_TYPE:STRING=Release\n" not in cache or "VOID_SANITIZERS:BOOL=OFF\n" not in cache:
        raise ValueError("Use a Release build without Runtime instrumentation")
    system = {name: system_file(args.cc, name) for name in (*SYSTEM, loader)}
    files = {f"link/{name}": source for name, source in runtime_files(build).items()}
    files.update({f"licenses/{name}": source for name, source in license_files(build).items()})
    output = fresh_output(args.output)
    header = ["VOID target pack 1", f"id {args.id}", f"version {args.version}", f"target {target}",
              f"compiler {compiler}", "llvm 23.1.4", "runtime void-aot-64-v1", f"source {revision}",
              f"minimum {minimum}"]
    with tempfile.TemporaryDirectory(dir=output.parent, prefix=".pack-") as work:
        stage = Path(work) / "pack"
        if args.ubuntu_inputs:
            stage.mkdir()
            records = {}
            for name, source in system.items():
                record, copyright_file = apt_record(source)
                records[name] = record
                license_name = record["package"].split(":")[0] + "-copyright.txt"
                files[f"licenses/{license_name}"] = copyright_file
            populate(stage / "runtime", header, files)
            (stage / "ubuntu-inputs.json").write_text(json.dumps(records, indent=2, sort_keys=True) + "\n")
        else:
            files.update({f"link/{name}": source for name, source in system.items()})
            populate(stage, header, files)
        os.rename(stage, output)
    print(output)


def fresh_output(path):
    output = path.resolve()
    if output.is_relative_to(ROOT) or output.exists():
        raise ValueError("Choose a fresh output directory outside the source tree")
    output.parent.mkdir(parents=True, exist_ok=True)
    return output


def compose(candidate, output, ar):
    output = fresh_output(output)
    runtime = candidate / "runtime"
    manifest = (runtime / "target-pack.txt").read_text().splitlines()
    if len(manifest) < 9 or manifest[0] != "VOID target pack 1" or not manifest[3].startswith("target "):
        raise ValueError("Expected a Linux Runtime candidate manifest")
    loaders = dict(TARGETS.values())
    loader = loaders[manifest[3].split()[1]]
    records = json.loads((candidate / "ubuntu-inputs.json").read_text())
    if set(records) != set((*SYSTEM, loader)):
        raise ValueError("Incomplete Ubuntu developer input recipe")
    files = {}
    for line in manifest[9:]:
        expected, relative = line.split("\t")
        path = runtime / relative
        if not path.resolve().is_relative_to(runtime.resolve()) or sha(path) != expected:
            raise ValueError(f"Runtime candidate checksum mismatch: {path}")
        role, leaf = relative.split("/")
        if role not in ("link", "licenses") or not re.fullmatch(r"[A-Za-z0-9_.+-]+", leaf) or leaf in (".", ".."):
            raise ValueError(f"Invalid Runtime candidate path: {relative}")
        files[relative] = path
    with tempfile.TemporaryDirectory(dir=output.parent, prefix=".pack-") as work:
        work = Path(work)
        for index, url in enumerate(sorted({r["url"] for r in records.values()})):
            location = urlsplit(url)
            host = location.hostname or ""
            if (location.scheme != "https" or not (host == "ubuntu.com" or host.endswith(".ubuntu.com"))
                    or location.username or location.password or location.query or location.fragment):
                raise ValueError(f"Expected an Ubuntu package mirror URL: {url}")
            group = {name: record for name, record in records.items() if record["url"] == url}
            deb = work / f"{index}.deb"
            package = next(iter(group.values()))
            urlretrieve(url, deb)
            if deb.stat().st_size != package["bytes"] or sha(deb) != package["sha256"]:
                raise ValueError(f"Ubuntu package checksum mismatch: {url}")
            members = command(ar, "t", str(deb)).splitlines()
            data = next(m for m in members if m.startswith("data.tar."))
            archive = work / data
            with archive.open("wb") as stream:
                subprocess.run([ar, "p", deb, data], stdout=stream, check=True)
            with tarfile.open(archive) as tar:
                for name, record in group.items():
                    path = work / name
                    member = tar.getmember("./" + record["path"])
                    if not member.isfile() or member.size > 64 * 1024 * 1024:
                        raise ValueError(f"Expected an ordinary Ubuntu package member: {member.name}")
                    with tar.extractfile(member) as source, path.open("wb") as destination:
                        shutil.copyfileobj(source, destination)
                    if sha(path) != record["member_sha256"]:
                        raise ValueError(f"Ubuntu package member differs from the build runner: {name}")
                    files[f"link/{name}"] = path
            archive.unlink()
        stage = work / "pack"
        populate(stage, manifest[:9], files)
        os.rename(stage, output)
    print(output)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    if len(sys.argv) > 1 and sys.argv[1] == "compose":
        parser.add_argument("command", choices=["compose"])
        parser.add_argument("candidate", type=Path)
        parser.add_argument("output", type=Path)
        parser.add_argument("--ar", default="llvm-ar")
        args = parser.parse_args()
        compose(args.candidate, args.output, args.ar)
        return
    parser.add_argument("build", type=Path, help="Native Release CMake build directory")
    parser.add_argument("output", type=Path)
    parser.add_argument("--id", required=True)
    parser.add_argument("--version", default="1.0.0")
    parser.add_argument("--cc", default="gcc", help="Destination GCC installation for GNU startup/library files")
    parser.add_argument("--ubuntu-inputs", action="store_true", help="Export Runtime plus data for provisioning exact Ubuntu system packages")
    args = parser.parse_args()
    if not re.fullmatch(r"custom-[a-z0-9](?:[a-z0-9-]{0,39}[a-z0-9])?", args.id):
        parser.error("Choose a custom-* pack ID")
    if not re.fullmatch(r"[0-9]+(?:\.[0-9]+)*", args.version) or len(args.version) > 24:
        parser.error("Expected a dotted numeric pack version")
    assemble(args)


if __name__ == "__main__":
    main()
