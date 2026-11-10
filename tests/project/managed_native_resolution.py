"""Resolve and lock managed native metadata without loading the fixture images."""
from hashlib import sha256
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import tomllib

compiler, clang = (Path(arg).absolute() for arg in sys.argv[1:3])
git = Path(sys.argv[3]).absolute() if len(sys.argv) > 3 and sys.argv[3] else None
target = "x86_64-unknown-linux-gnu"
provider = "org.example.native"
manifest_name = "void.project.toml"
application = """schema=1
[project]
id="org.example.application.sources"
name="Application"
version="1.0.0"
kind="application"
[application]
id="org.example.application"
"""
requirement = f"""
[native-requirements.fixture]
id="{provider}"
version="=6.0.0+local"
abi=1
"""
manual = """
[native.linux]
descriptor="manual.descriptor"
library="libmanual.so"
adapter="libmanual.so"
[native.windows]
descriptor="manual-windows.descriptor"
library="manual.dll"
adapter="manual.dll"
"""
library = f"""schema=1
[project]
id="org.example.native.sources"
name="Helper"
version="2.0.0"
kind="library"
[package]
id="{provider}"
name="helper"
entry="api.void"
"""
environment = {key: value for key, value in os.environ.items() if not key.startswith("GIT_")}
environment.pop("VOID_NATIVE_CATALOG", None)


def run(*args, env=None, cwd=None, status=0, message=None):
    command = list(map(str, args))
    result = subprocess.run(command, capture_output=True, cwd=cwd, env=environment if env is None else env, timeout=30)
    out = (result.stdout + result.stderr).decode("utf-8", errors="replace")
    assert result.returncode == status, (command, result.returncode, out)
    assert not message or message in out, (command, out)
    return out


def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(text.encode())


def identity(path):
    data = path.read_bytes()
    return f"{len(data)}:{sha256(data).hexdigest()}"


def state(root):
    return {
        entry.relative_to(root).as_posix(): (entry.stat().st_ino, entry.stat().st_mtime_ns,
            sha256(entry.read_bytes()).hexdigest() if entry.is_file() else None)
        for entry in [root, *root.rglob("*")]
    }


with tempfile.TemporaryDirectory(prefix="VOID D4 resolve 雪 ") as temporary:
    base = Path(temporary)
    root = base / "project"
    manifest, lock = root / manifest_name, root / "void.lock"
    catalog = base / "approved catalog"
    catalog.mkdir()
    catalog_file = catalog / "void.native.toml"

    def cli(*args, env=None, project=root, **options):
        return run(compiler, *args, "--project", project, env=env, **options)

    def resolve(*args, **options):
        return cli("resolve", *args, **options)

    write(manifest, application)
    unusable = dict(environment, VOID_NATIVE_CATALOG="relative")
    for content in (application, application + manual):
        write(manifest, content)
        resolve(env=unusable, message="No project dependencies.")
        resolve("--locked", env=unusable, message="No project dependencies.")
        assert not lock.exists()

    write(root / "helper" / manifest_name, library)
    write(root / "helper/api.void", "public func value() -> i32:\n    return 42\n")
    local = f'\n[dependencies.helper]\nid="{provider}"\npath="helper"\nversion="=2.0.0"\n'
    write(manifest, application + local)
    resolve(env=unusable, message="Created void.lock.")
    source_only = lock.read_bytes()
    assert tomllib.loads(source_only.decode())["format"] == 1
    write(manifest, application + local + manual)
    resolve("--locked", env=unusable, message="Verified void.lock.")
    assert lock.read_bytes() == source_only

    write(manifest, application + requirement)
    resolve(status=1, message="VOID_NATIVE_CATALOG")
    resolve(env=unusable, status=1, message="VOID_NATIVE_CATALOG")
    assert lock.read_bytes() == source_only
    lock.unlink()
    approved = dict(environment, VOID_NATIVE_CATALOG=str(catalog))
    resolve(env=approved, status=1, message="void.native.toml")
    resolve("--locked", env=approved, status=1, message="Missing void.lock")
    assert not lock.exists()

    source = base / "fixture.c"
    obj = base / "fixture.o"
    # If selection ever loads a fixture DSO, the process traps before returning.
    write(source, "__attribute__((constructor)) static void refuse_load(void) { __builtin_trap(); }\n"
                  "int fixture_value(void) { return 42; }\n")
    run(clang, "-fPIC", "-c", source, "-o", obj)
    images = [catalog / name for name in ("libprovider.so", "libadapter.so", "libdependency.so.1")]
    for image in images:
        run(clang, "-fuse-ld=lld", "-shared", "-nostdlib", "-Wl,-soname," + image.name, obj, "-o", image)
    link = catalog / "libfixture.a"
    run(clang.with_name("llvm-ar"), "rcsD", link, obj)
    notice = catalog / "LICENSE.txt"
    write(notice, "Test fixture. No third-party payload.\n")
    descriptor = catalog / "fixture.descriptor"
    descriptor_text = (f"void-c-primitive 1 1\nprovider {provider}\ntarget {target} sysv-x64\n"
        "source /fixture_ffi.void\nthunk " + "a" * 64 + "\nfixture " + "b" * 64 + "\n"
        "effects direct-sync nonblocking no-callbacks\ncall c-default\n"
        "aliases c_int:i32 c_long:i64 c_size:u64 c_bool:bool\nbind value fixture_value i32\n")
    write(descriptor, descriptor_text)

    def row(path):
        return f'"{path.name}" = "{identity(path)}"\n'

    def catalog_text(runtime=images):
        return (f'format=1\n[native]\nid="{provider}"\nversion="6.0.0+local"\nabi=1\n'
            f'target="{target}"\nrevision="fixture-r1"\ndescriptor="{descriptor.name}"\n'
            f'descriptor-identity="{identity(descriptor)}"\nlibrary="{images[0].name}"\nadapter="{images[1].name}"\n'
            '[native.runtime]\n' + "".join(row(path) for path in runtime)
            + '[native.link]\n' + row(link) + '[native.notices]\n' + row(notice))

    original = catalog_text()
    for bad, diagnostic in (
        (original.replace(f'id="{provider}"\n', ""), "missing its provider ID"),
        (original.replace(provider, "org.example.other"), "does not match required ID"),
        (original.replace("6.0.0+local", "6.0.0+other"), "does not match required exact version"),
        (original.replace("abi=1", "abi=2"), "does not match required ABI"),
        (original.replace(f'target="{target}"\n', ""), "missing its target"),
        (original.replace(target, "x86_64-pc-windows-msvc"), "does not match this host"),
        (original + "\n[native]\n", "Invalid managed native artifact catalog TOML"),
        (original + row(images[0]).replace(images[0].name, images[0].name.upper()), "ambiguous"),
        (original.replace(row(images[1]), ""), "Invalid or ambiguous managed native artifact catalog"),
        (original.replace(row(notice), ""), "Invalid or ambiguous managed native artifact catalog"),
    ):
        write(catalog_file, bad)
        output = resolve(env=approved, status=1, message=diagnostic)
        assert manifest_name in output and "VOID5006" in output and not lock.exists(), output

    write(catalog_file, original)
    extra = catalog / "other.so"
    extra.write_bytes(images[0].read_bytes())
    resolve(env=approved, status=1, message="Unexpected managed native catalog entry: other.so")
    extra.unlink()
    for path in (descriptor, images[1], images[2], link, notice):
        data = path.read_bytes()
        path.unlink()
        resolve(env=approved, status=1, message=f"Missing managed native artifact: {path.name}")
        path.write_bytes(data[:-1] + bytes([data[-1] ^ 1]))
        resolve(env=approved, status=1, message=f"artifact identity mismatch: {path.name}")
        path.write_bytes(data)

    changed_size = original.replace(identity(link), f"{link.stat().st_size + 1}:{sha256(link.read_bytes()).hexdigest()}")
    write(catalog_file, changed_size)
    resolve(env=approved, status=1, message=f"artifact identity mismatch: {link.name}")
    data = images[2].read_bytes()
    wrong_machine = bytearray(data)
    wrong_machine[18:20] = (183).to_bytes(2, "little")  # EM_AARCH64
    images[2].write_bytes(wrong_machine)
    write(catalog_file, catalog_text())
    resolve(env=approved, status=1, message=f"wrong target or image type: {images[2].name}")
    wrong_os = bytearray(data)
    wrong_os[7] = 9  # ELFOSABI_FREEBSD
    images[2].write_bytes(wrong_os)
    write(catalog_file, catalog_text())
    resolve(env=approved, status=1, message=f"wrong target or image type: {images[2].name}")
    images[2].write_bytes(data)

    write(descriptor, descriptor_text.replace(provider, "org.example.other"))
    write(catalog_file, catalog_text())
    resolve(env=approved, status=1, message="descriptor does not match the approved provider")
    write(descriptor, descriptor_text)
    write(catalog_file, original)
    assert not lock.exists()

    resolve(env=approved, message="Created void.lock.")
    initial = lock.read_bytes()
    native = tomllib.loads(initial.decode())
    assert native["format"] == native["resolver"] == 3 and "package" not in native
    assert native["root"]["dependencies"] == {}
    assert native["native"] == tomllib.loads(original)["native"]
    assert os.fsencode(base) not in initial
    resolve("--locked", status=1, message="VOID_NATIVE_CATALOG")
    for wrong_selection, diagnostic in ((original.replace(target, "x86_64-pc-windows-msvc"),
                                          "does not match this host"),
                                         (original.replace(provider, "org.example.other"),
                                          "does not match required ID")):
        write(catalog_file, wrong_selection)
        resolve("--refresh", env=approved, status=1, message=diagnostic)
        assert lock.read_bytes() == initial
    write(catalog_file, original)
    before = (state(root), state(catalog))
    resolve(env=approved, message="Verified void.lock.")
    resolve("--locked", env=approved, message="Verified void.lock.")
    resolve("--refresh", env=approved, message="Verified void.lock.")
    assert (state(root), state(catalog)) == before
    write(catalog_file, catalog_text(reversed(images)))
    resolve("--locked", env=approved, message="Verified void.lock.")
    assert lock.read_bytes() == initial
    moved_catalog = base / "relocated catalog"
    shutil.copytree(catalog, moved_catalog)
    moved_root = base / "relocated project"
    write(moved_root / manifest_name, application + requirement)
    resolve(project=moved_root, env=dict(environment, VOID_NATIVE_CATALOG=str(moved_catalog)))
    assert (moved_root / "void.lock").read_bytes() == initial

    changed = catalog_text().replace("fixture-r1", "fixture-r2")
    write(catalog_file, changed)
    for args in [(), ("--locked",)]:
        resolve(*args, env=approved, status=1, message="Stale or tampered void.lock")
        assert lock.read_bytes() == initial
    resolve("--refresh", env=approved, message="Refreshed void.lock.")
    refreshed = lock.read_bytes()
    assert refreshed != initial and tomllib.loads(refreshed.decode())["native"]["revision"] == "fixture-r2"
    forged = refreshed.replace(b"fixture-r2", b"fixture-r3")
    lock.write_bytes(forged)
    resolve("--locked", env=approved, status=1, message="Stale or tampered void.lock")
    assert lock.read_bytes() == forged
    resolve("--refresh", env=approved)
    assert lock.read_bytes() == refreshed

    lock.write_bytes(source_only)
    resolve("--locked", env=approved, status=1, message="Stale or tampered void.lock")
    assert lock.read_bytes() == source_only
    write(manifest, application + local + requirement)
    resolve("--refresh", env=approved)
    combined = tomllib.loads(lock.read_text(encoding="utf-8"))
    assert combined["format"] == 3 and combined["native"] == tomllib.loads(refreshed.decode())["native"]
    assert combined["package"] == tomllib.loads(source_only.decode())["package"]

    if git:
        origin = root / "source-origin"
        write(origin / manifest_name, library)
        write(origin / "api.void", "public func value() -> i32:\n    return 42\n")
        git_env = dict(environment, GIT_CONFIG_NOSYSTEM="1", GIT_CONFIG_GLOBAL=str(base / "no-config"))
        run(git, "init", "--quiet", "--object-format=sha1", cwd=origin, env=git_env)
        run(git, "add", ".", cwd=origin, env=git_env)
        run(git, "-c", "user.name=D4 Fixture", "-c", "user.email=d4@example.invalid",
            "commit", "--quiet", "--no-gpg-sign", "-m", "D4 source pin", cwd=origin, env=git_env)
        pin = run(git, "rev-parse", "HEAD", cwd=origin, env=git_env).strip()
        remote = local.replace('path="helper"', f'git="file:source-origin"\nrev="{pin}"')
        write(manifest, application + remote + requirement)
        cli("deps", "fetch", env=unusable, message="Verified Git source snapshots")
        resolve("--refresh", env=approved)
        resolve("--locked", env=approved)
        managed_git = tomllib.loads(lock.read_text(encoding="utf-8"))
        assert managed_git["format"] == 3 and managed_git["package"][0]["rev"] == pin
        assert managed_git["native"] == combined["native"]
        write(manifest, application + remote)
        resolve("--refresh", env=unusable)
        git_only = tomllib.loads(lock.read_text(encoding="utf-8"))
        assert git_only["format"] == 2 and "native" not in git_only
        assert git_only["package"] == managed_git["package"]

    write(manifest, application + local + manual)
    resolve("--refresh", env=unusable)
    assert lock.read_bytes() == source_only

print("Managed native artifact selection, no-load verification and v1/v2/v3 lock compatibility passed")
