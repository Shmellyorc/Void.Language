"""Qualify producing and consuming a publication as separate operations."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import time

PROGRAM = """use os

struct mailbox:
    count: i32

generator func sync_values() -> i32:
    yield 2
    yield 3

async generator func async_values(owner: mailbox) -> i32:
    yield owner.count
    yield owner.count + 1

async func main() -> i32:
    var owner = mailbox(count = 5)
    var reference = weak(owner)
    var total: i32 = 0
    for value in sync_values():
        total += value
    for await value in async_values(owner):
        total += value
    if reference.lock() is Ok(var target):
        total += target.count
    if os.embedded_resource("assets/input.bin") is Ok(var data):
        if data.count == 4 and data[2] == 0 and data[3] == 255:
            print(total)
            return 0
    return 31
"""

HTTP_PROGRAM = """use archive
use convert
use io
use net/http
use os

func main() -> i32:
    with convert.unwrap(http.open_client()) as active_http_session:
        var directory = convert.unwrap(os.resource_dir())
        var contents = convert.unwrap(io.read_bytes(directory + "/assets/input.bin"))
        if contents.count != 4 or contents[3] != 255:
            return 31
        var zipped = convert.unwrap(archive.encode_zip([archive.zip_input(name = "a", bytes = contents)]))
        print(zipped.count)
        return 0
"""


def run(*args, env=None, cwd=None, failure=None):
    start = time.perf_counter()
    result = subprocess.run(args, env=env, cwd=cwd, text=True, capture_output=True, timeout=90)
    seconds = time.perf_counter() - start
    print(f"{seconds:.3f}s {' '.join(map(str, args))}", flush=True)
    if failure:
        assert result.returncode and failure in result.stderr, result
    else:
        assert result.returncode == 0, result
    return result


def produce(compiler, pack, output):
    manifest = pack / "target-pack.txt"
    lines = manifest.read_text().splitlines()
    pack_id = lines[1].split()[1]
    target = lines[3].split()[1]
    expected_arch = "aarch64" if target.endswith("-arm64") else "x86_64"
    windows = target.startswith("windows-")
    output.mkdir()
    with tempfile.TemporaryDirectory(prefix="void target packs ") as temp:
        workspace = Path(temp).resolve()
        env = os.environ | {"VOID_TARGET_PACK_ROOT": str(workspace / "store")}
        project = workspace / "application"
        run(compiler, "new", "app", project)
        source = project / "src/app.void"
        config = project / "void.project.toml"
        text = config.read_text().replace("schema = 1", "schema = 2")
        config.write_text(text.replace("[application]", '[application]\nresources = ["assets/input.bin"]'))
        (project / "assets").mkdir()
        (project / "assets/input.bin").write_bytes(b"AZ\0\xff")
        source.write_text(PROGRAM)

        def publish(directory, *options, **checks):
            return run(compiler, "publish", "--project", project, "--target", pack_id,
                       "-o", directory, *options, env=env, **checks)

        publish(output / "missing", failure="is not installed")
        assert not (output / "missing").exists()
        altered = workspace / "altered"
        altered.mkdir()
        # Compatibility is checked before any pack payload is read.
        (altered / manifest.name).write_bytes(
            manifest.read_bytes().replace(b"runtime void-aot-64-v1", b"runtime future"))
        run(compiler, "target", "add", altered, env=env, failure="Incompatible target pack")
        staged = workspace / "pack-with-notices"
        shutil.copytree(pack, staged)
        notice_path = "licenses/providers/test/NOTICE.txt"
        notice = b"Target pack notice fixture\n"
        (staged / notice_path).parent.mkdir(parents=True)
        (staged / notice_path).write_bytes(notice)
        extended = manifest.read_bytes() + f"{hashlib.sha256(notice).hexdigest()}\t{notice_path}\n".encode()
        (staged / manifest.name).write_bytes(extended)
        result = run(compiler, "target", "add", staged, env=env)
        assert pack_id in result.stdout and f"sha256={hashlib.sha256(extended).hexdigest()}" in result.stdout
        assert run(compiler, "target", "list", env=env).stdout.splitlines() == [pack_id]
        run(compiler, "target", "verify", pack_id, env=env)
        publish(output / "async", "--resources", "embed", "--profile", "performance")
        publish(output / "async", failure="fresh directory")
        run(compiler, "publish", "--verify", output / "async")
        assert (output / "async" / notice_path).read_bytes() == notice
        report = json.loads((output / "async/binary-audit.json").read_text())
        assert report["inspected"] and report["architecture"] == expected_arch and not report["findings"], report

        source.write_text('func main() -> i32:\n    print(18)\n    return 0\n')
        publish(output / "sync", "--optimization", "none")
        if not windows:
            source.write_text(HTTP_PROGRAM)
            publish(output / "http", "--resources", "files")
            run(compiler, "publish", "--verify", output / "http")
            assert {path.name for path in (output / "http/native").iterdir()} == {"libidn2.so.0", "libunistring.so.5"}
            assert (output / "http/licenses/libidn2-2.3.8.tar.gz").is_file()
            assert (output / "http/licenses/libunistring-1.5.tar.xz").is_file()
        source.write_text('func main() -> i32:\n    var zero: i32 = 0\n    return 5 / zero\n')
        result = publish(output / "failure")
        private = Path(next(row.split(": ", 1)[1] for row in result.stdout.splitlines()
                            if row.startswith("Private diagnostics: ")))
        shutil.copyfile(private / "diagnostics.vdiag", output / "failure.vdiag")
        assert (private / ("app.unstripped.exe" if windows else "app.debug")).is_file()
        runtime = workspace / "store" / pack_id / ("link/void_runtime.lib" if windows else "link/libvoid_runtime.a")
        with runtime.open("ab") as stream:
            stream.write(b"bad")
        run(compiler, "target", "verify", pack_id, env=env, failure="checksum mismatch")
        publish(output / "corrupt", failure="checksum mismatch")
        assert not (output / "corrupt").exists()
        run(compiler, "target", "remove", pack_id, env=env)
        assert not run(compiler, "target", "list", env=env).stdout


def consume(output, compiler):
    with tempfile.TemporaryDirectory(prefix="void relocated pack ") as temp:
        root = Path(temp).resolve()
        relocated = root / "relocated"
        shutil.copytree(output, relocated)
        exe = "app.exe" if os.name == "nt" else "app"
        for application, expected in [("async", "21"), ("sync", "18")]:
            image = relocated / application / exe
            assert run(image, cwd=root).stdout.strip() == expected
        if os.name != "nt":
            assert run(relocated / "http/app", cwd=root).stdout.strip() == "104"
        image = relocated / "failure" / exe
        result = subprocess.run([image], cwd=root, capture_output=True, text=True, timeout=90)
        assert result.returncode == 1 and "VOID-" in result.stderr, result
        if compiler:
            resolved = run(compiler, "publish", "--resolve-diagnostic", relocated / "failure.vdiag",
                           image, result.stderr.splitlines()[0])
            assert "zero" in resolved.stdout and "app.void" in resolved.stdout, resolved
        assert "zero" not in result.stderr, result


def archive_linux_distributions(distributions, output):
    def metadata(member):
        # A Windows publishing filesystem cannot carry the Unix execute bits.
        if member.isfile() and member.name.endswith(("/async/app", "/sync/app", "/http/app", "/failure/app")):
            member.mode |= 0o111
        return member

    with tarfile.open(output, "x:gz") as archive:
        for target in sorted(distributions.iterdir()):
            archive.add(target, arcname=target.name, filter=metadata)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_subparsers(dest="mode", required=True)
    build = modes.add_parser("produce")
    build.add_argument("compiler", type=Path)
    build.add_argument("pack", type=Path)
    build.add_argument("output", type=Path)
    execute = modes.add_parser("consume")
    execute.add_argument("output", type=Path)
    execute.add_argument("--compiler", type=Path)
    archive = modes.add_parser("archive")
    archive.add_argument("distributions", type=Path)
    archive.add_argument("output", type=Path)
    args = parser.parse_args()
    if args.mode == "produce":
        produce(args.compiler.resolve(), args.pack.resolve(), args.output.resolve())
    elif args.mode == "consume":
        consume(args.output.resolve(), args.compiler.resolve() if args.compiler else None)
    else:
        archive_linux_distributions(args.distributions, args.output)


if __name__ == "__main__":
    main()
