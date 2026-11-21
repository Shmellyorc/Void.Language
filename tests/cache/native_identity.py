"""Linux native-content eligibility through real CLI processes and rebuilt DSOs."""
import hashlib
import json
import os
from pathlib import Path
import resource
import shutil
import subprocess
import sys
import tempfile

compiler, clang = (Path(arg).resolve() for arg in sys.argv[1:3])
fixtures = Path(__file__).resolve().parents[1] / "ffi"
environment = dict(os.environ)
environment.pop("VOID_NATIVE_CATALOG", None)
rows = []


def small_stack():
    resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))


def command(*args, status=0, env=None):
    result = subprocess.run(list(map(str, args)), env=environment if env is None else env,
                            capture_output=True, timeout=60, preexec_fn=small_stack)
    assert result.returncode == status, (args, result.returncode, result.stdout, result.stderr)
    assert not any(marker in result.stderr for marker in
                   (b"AddressSanitizer", b"UndefinedBehaviorSanitizer", b"LeakSanitizer")), result.stderr
    return result


def identity(path):
    data = path.read_bytes()
    return f"{len(data)}:{hashlib.sha256(data).hexdigest()}"


with tempfile.TemporaryDirectory(prefix="void-native-cache-identity-") as temporary:
    base = Path(temporary)
    raw = base / "unmanaged"
    command(compiler, "new", raw, "--source", "src/main.void")
    native = raw / "native"
    native.mkdir()
    descriptor = native / "provider.descriptor"
    content = (fixtures / "conventional.descriptor.in").read_text()
    for key, value in {
        "TARGET": "x86_64-unknown-linux-gnu sysv-x64", "LONG": "i64",
        "THUNK": hashlib.sha256((fixtures / "conventional_fixture.h").read_bytes()).hexdigest(),
        "FIXTURE": hashlib.sha256((fixtures / "conventional_fixture.c").read_bytes()).hexdigest(),
    }.items():
        content = content.replace(f"@{key}@", value)
    descriptor.write_text(content)
    generated = base / "adapter.c"
    command(compiler, "native-bind", descriptor, "conventional_fixture.h", "-o", generated)
    bias = base / "bias.c"
    bias.write_text("int g4_conventional_bias(void){return BIAS;}\nvoid g4_conventional_delay(void){}\n")
    revision = base / "adapter_revision.c"
    revision.write_text("int g95_adapter_revision = REVISION;\n")
    library = native / "libidentity_provider.so"
    adapter = native / "libidentity_adapter.so"
    flags = ["--no-default-config", "-std=c11", "-shared", "-fPIC", "-fuse-ld=lld", "-O1",
             "-I" + str(fixtures)]

    def rebuild_provider(value):
        command(clang, *flags, f"-DBIAS={value}", fixtures / "conventional_fixture.c", bias,
                "-Wl,-soname," + library.name, "-o", library)

    def rebuild_adapter(value):
        command(clang, *flags, f"-DREVISION={value}", generated, revision, library,
                "-Wl,-rpath,$ORIGIN", "-Wl,-soname," + adapter.name, "-o", adapter)

    rebuild_provider(7)
    rebuild_adapter(0)
    original_library, original_adapter = library.read_bytes(), adapter.read_bytes()
    manifest = raw / "void.project.toml"
    manifest.write_text(manifest.read_text() + '\n[native.linux]\ndescriptor="provider.descriptor"\n'
                        f'library="{library.name}"\nadapter="{adapter.name}"\n')
    source = 'use "/conventional_ffi.void" as ffi\nfunc main():\n    print(ffi.add(30, 5))\n'
    (raw / "src/main.void").write_text(source)

    def run(project, label, value=None, cache=None, *, env=None, status=0, authorize=True):
        report = base / f"timing-{len(rows)}.json"
        result = command(compiler, "run", "--project", project, *(["--allow-native"] if authorize else []),
                         "--timings", "--timings-format", "json", "--timings-output", report,
                         env=env, status=status)
        data = json.loads(report.read_text())
        assert result.stdout == (b"" if value is None else f"{value}\n".encode()), result
        if cache is not None:
            assert data["cache"]["status"] == cache, (label, data)
        if status:
            assert data["cache"]["status"] != "hit", (label, data)
        rows.append({"case": label, "value": value, "cache": data["cache"]["status"], "exit": status})
        return result

    # The exact previously missed path: no payload hashes, even after a rebuild.
    run(raw, "unmanaged first", 42, "unavailable")
    run(raw, "unmanaged unchanged", 42, "unavailable")
    rebuild_provider(9)
    assert library.read_bytes() != original_library
    run(raw, "unmanaged provider rebuilt", 44, "unavailable")
    rebuild_adapter(1)
    assert adapter.read_bytes() != original_adapter
    run(raw, "unmanaged adapter rebuilt", 44, "unavailable")
    assert not (raw / ".void/cache/v1").exists()

    # The same provider/adapter, now admitted by complete catalog identities.
    library.write_bytes(original_library)
    adapter.write_bytes(original_adapter)
    catalog = base / "catalog"
    catalog.mkdir()
    for path in (descriptor, library, adapter):
        shutil.copyfile(path, catalog / path.name)
    notice = catalog / "LICENSE.txt"
    shutil.copyfile(fixtures.parents[1] / "LICENSE", notice)
    managed = base / "managed"
    command(compiler, "new", managed, "--source", "src/main.void")
    manifest = managed / "void.project.toml"
    manifest.write_text(manifest.read_text() + '\n[native-requirements.fixture]\n'
                        'id="org.voidlang.g4-conventional"\nversion="=1.0.0"\nabi=3\n')
    (managed / "src/main.void").write_text(source)
    approved_env = environment | {"VOID_NATIVE_CATALOG": str(catalog)}

    def write_catalog(runtime=(library.name, adapter.name)):
        text = ('format=1\n[native]\nid="org.voidlang.g4-conventional"\nversion="1.0.0"\nabi=3\n'
                'target="x86_64-unknown-linux-gnu"\nrevision="g95-identity"\n'
                f'descriptor="{descriptor.name}"\ndescriptor-identity="{identity(catalog / descriptor.name)}"\n'
                f'library="{library.name}"\nadapter="{adapter.name}"\n[native.runtime]\n')
        text += "".join(f'"{name}"="{identity(catalog / name)}"\n' for name in runtime)
        text += f'[native.link]\n[native.notices]\n"{notice.name}"="{identity(notice)}"\n'
        (catalog / "void.native.toml").write_text(text)

    def resolve():
        command(compiler, "resolve", "--project", managed, "--refresh", env=approved_env)

    write_catalog()
    resolve()
    run(managed, "managed cold", 42, "miss", env=approved_env)
    run(managed, "managed unchanged", 42, "hit", env=approved_env)
    rejected = run(managed, "managed approval required", env=approved_env, status=2, authorize=False)
    assert b"allow-native" in rejected.stderr
    rebuild_provider(9)
    shutil.copyfile(library, catalog / library.name)
    run(managed, "unapproved provider content", env=approved_env, status=1)
    write_catalog()
    resolve()
    run(managed, "managed provider rebuilt", 44, "miss", env=approved_env)
    run(managed, "managed provider unchanged", 44, "hit", env=approved_env)
    (catalog / library.name).write_bytes(original_library)
    write_catalog()
    resolve()
    run(managed, "managed provider restored", 42, "hit", env=approved_env)
    rebuild_adapter(1)
    shutil.copyfile(adapter, catalog / adapter.name)
    run(managed, "unapproved adapter content", env=approved_env, status=1)
    write_catalog()
    resolve()
    run(managed, "managed adapter rebuilt", 42, "miss", env=approved_env)
    run(managed, "managed adapter unchanged", 42, "hit", env=approved_env)
    (catalog / adapter.name).write_bytes(original_adapter)
    write_catalog()
    resolve()
    run(managed, "managed adapter restored", 42, "hit", env=approved_env)
    for missing in (library.name, adapter.name):
        write_catalog(runtime=tuple(name for name in (library.name, adapter.name) if name != missing))
        run(managed, "missing identity " + missing, env=approved_env, status=1)
    assert len(list((managed / ".void/cache/v1").iterdir())) == 3

print(json.dumps(rows, indent=2))
print("Unfingerprinted native bypass; verified provider/adapter rebuild, warm reuse, restoration and integrity passed")
