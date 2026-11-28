"""Run the shipped application and resolve one failure against its private image data."""
from concurrent.futures import ThreadPoolExecutor
from hashlib import sha256
import json
import os
import plistlib
import shutil
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile

compiler, llvm = map(lambda p: Path(p).resolve(), sys.argv[1:])
windows = sys.platform == "win32"
macos = sys.platform == "darwin"
target = "windows-x64" if windows else "macos-arm64" if macos else "linux-x64"
exe = "app.exe" if windows else "app"
env = dict(os.environ)
for key in ("VOID_STDLIB_ROOT", "VOID_NATIVE_CATALOG", "LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH"):
    env.pop(key, None)
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from test_platform import relocate, windows_isolated_application, windows_package_imports


def invoke(args, *, cwd=None, environment=env):
    return subprocess.run(list(map(str, args)), cwd=cwd, env=environment, capture_output=True,
                          encoding="utf-8", timeout=90)


def run(*args, status=0, cwd=None):
    result = invoke(args, cwd=cwd)
    assert result.returncode == status, (args, result)
    return result


def publish(project, *args):
    return run(compiler, "publish", "--project", project, *args)


def archive(result):
    path = Path(next(line.removeprefix("Private diagnostics: ") for line in result.stdout.splitlines()
                     if line.startswith("Private diagnostics: ")))
    assert path.is_dir(), path
    if not windows:
        assert path.stat().st_mode & 0o077 == 0
    return path


def snapshot(directory):
    return {p.relative_to(directory).as_posix(): sha256(p.read_bytes()).hexdigest()
            for p in directory.rglob("*") if p.is_file()}


with tempfile.TemporaryDirectory(prefix="VOID publish 雪 ") as temp:
    root = Path(temp).resolve()
    outside = root / "launch cwd"
    outside.mkdir()
    project = root / "private producer path"
    run(compiler, "new", "app", project)
    manifest_path = project / "void.project.toml"
    manifest = manifest_path.read_text(encoding="utf-8").replace("schema = 1", "schema = 2")
    manifest = manifest.replace("[application]", '[application]\nresources = ["assets/input.bin", "assets/empty.bin"]')
    manifest_path.write_text(manifest + '\n[publish]\noutput="dist/production"\nresources="embed"\n', encoding="utf-8")
    assets = project / "assets"
    assets.mkdir()
    (assets / "input.bin").write_bytes(b"AZ\0\xff")
    (assets / "empty.bin").write_bytes(b"")
    (assets / "developer-token.txt").write_bytes(b"unselected development token")
    source = project / "src/app.void"
    source.write_text('''use os

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
    if os.embedded_resource("assets/input.bin") is Ok(var bytes):
        if bytes.count != 4 or bytes[2] != 0 or bytes[3] != 255:
            return 31
        bytes[3] = 0
    else:
        return 32
    if os.embedded_resource("assets/input.bin") is Ok(var again):
        if again[3] != 255:
            return 33
    if os.embedded_resource("assets/empty.bin") is Ok(var empty):
        if empty.count != 0:
            return 34
    else:
        return 35
    if os.embedded_resource("assets/developer-token.txt") is Ok(var unexpected):
        return 36
    print(total)
    return 0
''', encoding="utf-8")

    result = publish(project, "--portable")
    output = project / "dist/production"
    private = archive(result)
    files = snapshot(output)
    assert not any(file.startswith("assets/") for file in files), files
    assert not any(p.suffix == ".vdiag" for p in output.rglob("*"))
    assert private.name == files[exe]
    report = json.loads((output / "binary-audit.json").read_text(encoding="utf-8"))
    assert report["inspected"] and not report["debug"] and not report["static_symbols"], report
    assert not report["findings"] and not report["loader_paths"] and report["unwind"], report
    assert report["sha256"] == files[exe]
    assert "exports" in report
    if macos:
        assert not report["symbols"] and report["dynamic_symbols"], report
        assert "_main" in report["exports"] and set(report["exports"]) <= {"_main", "__mh_execute_header"}, report
    assert (output / "void-package.txt").read_text().splitlines()[1] == "target " + target
    run(compiler, "publish", "--verify", output)
    if macos:
        bundled = root / "app publication"
        bundle_result = publish(project, "--format", "app", "-o", bundled)
        app = next(bundled.glob("*.app"))
        plist = plistlib.loads((app / "Contents/Info.plist").read_bytes())
        assert plist["CFBundleIdentifier"] + ".app" == app.name
        assert plist["CFBundlePackageType"] == "APPL"
        assert plist["LSMinimumSystemVersion"] == "14.0"
        image = app / "Contents/MacOS" / plist["CFBundleExecutable"]
        assert archive(bundle_result).name == sha256(image.read_bytes()).hexdigest()
        run("/usr/bin/codesign", "--verify", "--strict", app)
        run(compiler, "publish", "--verify", bundled)
        assert run(image, cwd=outside).stdout.strip() == "21"
        for publication in (output, bundled):
            archive_file = root / (publication.name + ".zip")
            run(compiler, "publish", "--bundle", publication, "-o", archive_file)
            with zipfile.ZipFile(archive_file) as package:
                entry = package.getinfo(f"{app.name}/Contents/MacOS/app")
                assert entry.create_system == 3 and (entry.external_attr >> 16) & 0o111
                assert package.read(entry) == (publication / "app" if publication == output else image).read_bytes()
            extracted = root / ("zip " + publication.name)
            run("/usr/bin/ditto", "-x", "-k", archive_file, extracted)
            assert run(extracted / app.name / "Contents/MacOS/app", cwd=outside).stdout.strip() == "21"
    if target == "linux-x64":
        second_producer = root / "second producer path"
        second_producer.mkdir()
        shutil.copytree(project / "src", second_producer / "src")
        shutil.copytree(assets, second_producer / "assets")
        shutil.copyfile(manifest_path, second_producer / manifest_path.name)
        rebuilt = root / "reproducible"
        publish(second_producer, "-o", rebuilt)
        assert snapshot(rebuilt) == files
    rejected = invoke([compiler, "publish", "--project", project])
    assert rejected.returncode and "fresh directory" in rejected.stderr
    assert snapshot(output) == files
    missing = assets / "input.bin"
    missing.rename(assets / "removed.bin")
    invalid_output = root / "failed"
    rejected = invoke([compiler, "publish", "--project", project, "-o", invalid_output])
    assert rejected.returncode and not invalid_output.exists(), rejected
    (assets / "removed.bin").rename(missing)
    other = root / "relocated package"
    relocate(output, other)
    hidden = root / "parked producer"
    relocate(project, hidden)
    try:
        if windows:
            system = Path(os.environ["SystemRoot"]) / "System32"
            result = windows_isolated_application(other / exe, [], blocked=[(hidden, "src/app.void")],
                                                  cwd=outside, env=env | {"PATH": str(system)}, timeout=90)
            assert result.returncode == 0 and result.stdout.strip() == b"21", result
            windows_package_imports(llvm / "llvm-readobj.exe", other / exe)
        else:
            result = invoke([other / exe], cwd=outside, environment=env | {"PATH": "/usr/bin:/bin"})
            assert result.returncode == 0 and result.stdout.strip() == "21", result
        run(compiler, "publish", "--verify", other)
    finally:
        relocate(hidden, project)
    (other / "unexpected").write_bytes(b"tamper")
    assert invoke([compiler, "publish", "--verify", other]).returncode

    library = project / "libs/fixture"
    library.mkdir(parents=True)
    (library / "void.project.toml").write_text('''schema = 1
[project]
id = "org.fixture.source"
name = "Source dependency"
version = "1.0.0"
kind = "library"
[package]
id = "org.fixture.package"
name = "fixture"
entry = "main.void"
''', encoding="utf-8")
    (library / "main.void").write_text("public func number() -> i32:\n    return 17\n", encoding="utf-8")
    manifest_path.write_text(manifest + '''
[dependencies.fixture]
id = "org.fixture.package"
path = "libs/fixture"
version = "=1.0.0"
''', encoding="utf-8")
    run(compiler, "resolve", "--project", project)
    source.write_text('''use pkg/fixture
use os
use io

func main() -> i32:
    if os.resource_dir() is Ok(var directory):
        if io.read_bytes(directory + "/assets/input.bin") is Ok(var bytes):
            if bytes.count == 4 and bytes[3] == 255:
                print(fixture.number())
                return 0
    return 41
''', encoding="utf-8")
    distributed = root / "files"
    performance = ["--profile", "performance", "--resources", "files", "--symbols", "none"]
    if target == "linux-x64":
        performance += ["--lto", "thin"]
    private_files = archive(publish(project, *performance, "-o", distributed))
    assert snapshot(private_files).keys() == {"diagnostics.vdiag"}
    assert (distributed / "assets/input.bin").read_bytes() == b"AZ\0\xff"
    assert run(distributed / exe, cwd=outside).stdout.strip() == "17"
    assert json.loads((distributed / "binary-audit.json").read_text())["inspected"]

    source.write_text('''func private_failure_canary(value: i32) -> i32:
    var source_snippet_canary = value - value
    return value / source_snippet_canary

func main() -> i32:
    defer {
        print("cleanup")
    }
    return private_failure_canary(5)
''', encoding="utf-8")
    failure = root / "failure"
    private_failure = archive(publish(project, "--optimization", "none", "-o", failure))
    image = failure / exe
    result = run(image, status=1, cwd=outside)
    assert "cleanup" in result.stdout and "source_snippet_canary" not in result.stderr, result
    token = next(line for line in result.stderr.splitlines() if line.startswith("VOID-D1 "))
    mapping = private_failure / "diagnostics.vdiag"
    resolved = run(compiler, "publish", "--resolve-diagnostic", mapping, image, token).stdout
    assert "source_snippet_canary" in resolved and "private_failure_canary" in resolved, resolved
    assert invoke([compiler, "publish", "--resolve-diagnostic", mapping, distributed / exe, token]).returncode
    private_symbol = private_failure / ("app.unstripped.exe" if windows else "app.dwarf" if macos else "app.debug")
    dwarfdump = llvm / ("llvm-dwarfdump.exe" if windows else "llvm-dwarfdump")
    assert "private_failure_canary" in run(dwarfdump, "--debug-info", private_symbol).stdout
    if macos:
        assert run(dwarfdump, "--uuid", image).stdout.split()[1] == run(dwarfdump, "--uuid", private_symbol).stdout.split()[1]

    # Both writers stage privately. Only one can make the final directory visible.
    raced = root / "concurrent"
    command = [compiler, "publish", "--project", project, "--symbols", "none", "-o", raced]
    with ThreadPoolExecutor(max_workers=2) as workers:
        results = list(workers.map(invoke, [command, command]))
    assert sum(result.returncode == 0 for result in results) == 1, results
    run(compiler, "publish", "--verify", raced)

print(f"Native publication verified on {target} with relocated assets, locked source dependencies and private diagnostics.")
