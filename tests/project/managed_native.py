"""Managed C FFI through the project manifest, local catalog, and relocated AOT."""
from hashlib import sha256
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import tomllib
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from test_platform import relocate, windows_isolated_application, windows_package_imports

compiler, clang = (Path(arg).resolve() for arg in sys.argv[1:3])
sanitized = sys.argv[3] == "ON"
server = Path(sys.argv[4]).resolve() if len(sys.argv) > 4 and sys.argv[4] else None
packaging = '--package' in sys.argv[5:]
ffi = Path(__file__).resolve().parents[1] / "ffi"
windows = sys.platform == "win32"
native_host = windows or sys.platform.startswith("linux")
target = "x86_64-pc-windows-msvc" if windows else "x86_64-unknown-linux-gnu"
provider_id = "org.voidlang.g4-conventional"
requirement = f'\n[native-requirements.fixture]\nid="{provider_id}"\nversion="=1.0.0"\nabi=3\n'
environment = dict(os.environ)
environment.pop("VOID_NATIVE_CATALOG", None)
if sanitized:
    environment.update(ASAN_OPTIONS="halt_on_error=1:abort_on_error=1:detect_leaks=1",
                       UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")


def run(*args, status=0, message=None, env=None):
    result = subprocess.run(list(map(str, args)), cwd=cwd, env=environment if env is None else env,
                            capture_output=True, timeout=90)
    output = (result.stdout + result.stderr).decode("utf-8", errors="replace")
    assert result.returncode == status if status is not None else result.returncode != 0, (args, result, output)
    assert not message or message in output, (args, output)
    assert "AddressSanitizer" not in output and "UndefinedBehaviorSanitizer" not in output, (args, output)
    return output


def cli(*args, **options):
    return run(compiler, *args, "--project", project, **options)


def digest(path):
    return sha256(path.read_bytes()).hexdigest()


def identity(path):
    return f'"{path.name}" = "{path.stat().st_size}:{digest(path)}"\n'


def catalog_text(descriptor, library, adapter, runtime, link, notice, revision="fixture-r1", catalog_target=target):
    return (f'format = 1\n[native]\nid = "{provider_id}"\nversion = "1.0.0"\nabi = 3\n'
            f'target = "{catalog_target}"\nrevision = "{revision}"\n'
            f'descriptor = "{descriptor.name}"\nlibrary = "{library.name}"\nadapter = "{adapter.name}"\n'
            f'descriptor-identity = "{descriptor.stat().st_size}:{digest(descriptor)}"\n'
            '\n[native.runtime]\n' + "".join(identity(path) for path in runtime) +
            '\n[native.link]\n' + "".join(identity(path) for path in link) +
            '\n[native.notices]\n' + identity(notice))


def no_initializers():
    assert not marker.exists() and not released.exists()


def reset_markers():
    marker.unlink(missing_ok=True)
    released.unlink(missing_ok=True)


def editor(main, helper, text):
    sys.path.insert(0, str(ffi.parents[1] / "tools"))
    from lsp_client import Client

    def position(spelling):
        for number, line in enumerate(text.splitlines()):
            if spelling in line:
                return {"line": number, "character": line.index(spelling)}
        raise AssertionError(spelling)

    def query(client, method, path, point, **context):
        response, _ = client.request("textDocument/" + method, {
            "textDocument": {"uri": path.as_uri()}, "position": point, **context,
        })
        assert "result" in response, response
        return response["result"]

    native_call = position("add(30, 5)")
    generated = project / "conventional_ffi.void"
    approved_lock = lock.read_bytes(), lock.stat().st_mtime_ns
    with patch.dict(os.environ, environment, clear=True), Client(server, project, project=True, cwd=cwd) as client:
        assert not any(note["diagnostics"] for note in client.initialize())
        client.notify("textDocument/didOpen", {"textDocument": {
            "uri": main.as_uri(), "languageId": "void", "version": 1, "text": text,
        }})
        assert not any(note["diagnostics"] for note in client.sync())
        hover = query(client, "hover", main, native_call)
        assert "add" in str(hover) and "i32" in str(hover), hover
        native = query(client, "definition", main, native_call)
        assert native and native["uri"] == generated.as_uri(), native
        references = query(client, "references", generated, native["range"]["start"],
                           context={"includeDeclaration": False})
        assert any(location["uri"] == main.as_uri() and location["range"]["start"] == native_call
                   for location in references), references
        package = query(client, "definition", main, position("value()"))
        assert package and package["uri"] == helper.as_uri(), package
        client.shutdown()
    no_initializers()
    assert not generated.exists()

    write_catalog(revision="fixture-r3")
    try:
        with patch.dict(os.environ, environment, clear=True), Client(server, project, project=True, cwd=cwd) as client:
            diagnostics = client.initialize()
            assert any("Stale or tampered void.lock" in item["message"]
                       for note in diagnostics for item in note["diagnostics"]), diagnostics
            assert query(client, "definition", main, native_call) is None
            client.shutdown()
    finally:
        write_catalog(revision="fixture-r2")
    assert (lock.read_bytes(), lock.stat().st_mtime_ns) == approved_lock
    no_initializers()


with tempfile.TemporaryDirectory(prefix="VOID D4 雪 ") as temporary:
    base = Path(temporary).resolve()
    cwd = base / "working directory"
    cwd.mkdir()
    marker, released = cwd / "native-init.log", cwd / "native-release.log"
    modules = cwd / "native-modules.log"
    project = base / "application"
    run(compiler, "new", project, "--source", "src/main.void")
    manifest = project / "void.project.toml"
    initial = manifest.read_text(encoding="utf-8")
    if native_host:
        ordinary_output = base / "ordinary" / "native"
        cli("build", "-o", ordinary_output)
        assert ordinary_output.is_file()
    for invalid in (requirement.replace("=1.0.0", ">=1.0.0"), requirement.replace("abi=3", "abi=4"),
                    requirement + requirement.replace(".fixture", ".other"),
                    requirement + '\n[native.linux]\ndescriptor="p.txt"\nlibrary="p.so"\nadapter="a.so"\n'):
        manifest.write_text(initial + invalid, encoding="utf-8")
        cli("resolve", status=1, message="managed native")
    manifest.write_text(initial.replace('kind = "application"', 'kind = "library"') + requirement, encoding="utf-8")
    cli("resolve", status=1, message="managed native requirement in an application")
    manifest.write_text(initial + requirement, encoding="utf-8")
    if not native_host:
        cli("resolve", status=1, message="Managed native providers are unsupported on this host")
        assert not (project / "void.lock").exists()
        print("Managed native manifest and unsupported-host refusal passed")
        sys.exit(0)

    cli("resolve", status=1, message="VOID_NATIVE_CATALOG")
    catalog = base / "approved catalog"
    catalog.mkdir()
    c_sources = base / "C inputs"
    c_sources.mkdir()
    suffix = ".dll" if windows else ".so"
    stem = "" if windows else "lib"
    dependency, library, adapter = [catalog / (stem + name + suffix) for name in ("d4_dep", "d4_provider", "d4_adapter")]
    descriptor = catalog / "provider.descriptor"
    notice = catalog / "LICENSE.txt"
    shutil.copyfile(ffi.parents[1] / "LICENSE", notice)
    descriptor_bytes = (ffi / "conventional.descriptor.in").read_text(encoding="utf-8")
    for key, value in dict(TARGET=target + (" msvc-x64" if windows else " sysv-x64"),
                           LONG="i32" if windows else "i64", THUNK=digest(ffi / "conventional_fixture.h"),
                           FIXTURE=digest(ffi / "conventional_fixture.c")).items():
        descriptor_bytes = descriptor_bytes.replace(f"@{key}@", value)
    descriptor.write_bytes(descriptor_bytes.encode())
    generated = c_sources / "adapter.c"
    run(compiler, "native-bind", descriptor, "conventional_fixture.h", "-o", generated)

    def link(output, source, *imports, cxx=False):
        args = ["--driver-mode=g++", "-std=c++17"] if cxx else ["-std=c11"]
        args += ["-fuse-ld=lld", "-shared", "-O2", "-I" + str(ffi)]
        if windows:
            args += ["--target=x86_64-pc-windows-msvc", "-fms-runtime-lib=static", "-D_CRT_SECURE_NO_WARNINGS",
                     "-Wl,/implib:" + str(c_sources / (output.stem + ".lib"))]
            if packaging and output == dependency:
                args += ["-DVOID_D5_WINDOWS_MODULE_PROBE"]
            imports = tuple(c_sources / (item.stem + ".lib") for item in imports)
        else:
            args += ["-fPIC", "-fvisibility=hidden", "-Wl,-rpath,$ORIGIN", "-Wl,-soname," + output.name]
            if sanitized:
                args += ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"]
        run(clang, *args, source, *imports, "-o", output)

    link(dependency, ffi / "conventional_dependency.cpp", cxx=True)
    link(library, ffi / "conventional_fixture.c", dependency)
    link(adapter, generated, library)
    if windows:
        link_file = catalog / "d4_provider.lib"
        shutil.copyfile(c_sources / link_file.name, link_file)
        incompatible_link = b"not a native link input"
        imports = [link_file]
    else:
        implementation = c_sources / "link_value.c"
        implementation.write_text("int void_d4_link_value(void) { return 41; }\n", encoding="ascii")
        reference = c_sources / "link_reference.c"
        reference.write_text("int void_d4_link_value(void);\n"
                             "int void_d4_link_reference(void) { return void_d4_link_value(); }\n", encoding="ascii")
        member = c_sources / "link_value.o"
        probe = catalog / "d4_link_reference.o"
        link_file = catalog / "libd4_link.a"
        ar = clang.with_name("llvm-ar")
        run(clang, "-c", implementation, "-o", member)
        run(clang, "-c", reference, "-o", probe)
        run(ar, "rcs", link_file, member)
        wrong_member = c_sources / "wrong_target.o"
        wrong_archive = c_sources / "wrong_target.a"
        run(clang, "--target=aarch64-linux-gnu", "-c", implementation, "-o", wrong_member)
        run(ar, "rcs", wrong_archive, wrong_member)
        incompatible_link = wrong_archive.read_bytes()
        imports = [probe, link_file]
    images = [dependency, library, adapter]
    config = catalog / "void.native.toml"

    def write_catalog(runtime=images, **options):
        config.write_text(catalog_text(descriptor, library, adapter, runtime, imports, notice, **options), encoding="utf-8")

    write_catalog()
    environment["VOID_NATIVE_CATALOG"] = str(catalog)
    source = (ffi / "conventional.void.in").read_text(encoding="utf-8")
    (project / "src/main.void").write_text(source.replace("@LONG_ARG@", "-200" if windows else "4000000000 as i64")
        .replace("@LONG_RESULT@", "-193" if windows else "4000000007 as i64"), encoding="utf-8")
    cli("check", status=1, message="Missing void.lock")
    cli("resolve")
    lock = project / "void.lock"
    first = lock.read_bytes()
    decoded = tomllib.loads(first.decode())
    assert decoded["format"] == decoded["resolver"] == 3
    assert decoded["native"]["target"] == target and decoded["native"]["abi"] == 3
    assert decoded["native"]["runtime"][dependency.name] == f"{dependency.stat().st_size}:{digest(dependency)}"
    assert decoded["native"]["descriptor-identity"] == f"{descriptor.stat().st_size}:{digest(descriptor)}"
    assert decoded["native"]["link"] == {p.name: f"{p.stat().st_size}:{digest(p)}" for p in imports}
    assert os.fsencode(base) not in first
    write_catalog(list(reversed(images)))
    cli("resolve", "--locked")
    assert first == lock.read_bytes()
    cli("check")
    cli("run", status=2, message="--allow-native")
    cli("build", "-o", base / "denied", status=2, message="--allow-native")
    cli("emit", "-o", base / "denied.o", status=2, message="--allow-native")
    if packaging:
        cli("package", "-o", base / "denied package", status=2, message="--allow-native")
    no_initializers()

    for leaf in ("native", "Native"):
        reserved = base / "reserved"
        cli("build", "--allow-native", "-o", reserved / leaf, status=1,
            message="conflicts with the reserved native payload directory")
        assert not reserved.exists()

    rejected = base / "rejected link" / "app.exe"
    good_link = link_file.read_bytes()
    link_file.unlink()
    cli("build", "--allow-native", "-o", rejected, status=1,
        message=f"Missing managed native artifact: {link_file.name}")
    assert not rejected.parent.exists()
    link_file.write_bytes(incompatible_link)
    write_catalog()
    cli("resolve", "--refresh")
    diagnostic = cli("build", "--allow-native", "-o", rejected, status=1,
                     message="Incompatible managed native link input")
    assert link_file.name in diagnostic
    assert not rejected.exists() and not (rejected.parent / "native").exists()

    if not windows:
        # The remaining object needs a symbol from the archive. Omitting the
        # archive from an otherwise valid lock must fail at the final link.
        link_file.unlink()
        imports.remove(link_file)
        write_catalog()
        cli("resolve", "--refresh")
        cli("build", "--allow-native", "-o", rejected, status=1,
            message="undefined symbol: void_d4_link_value")
        assert not rejected.exists() and not (rejected.parent / "native").exists()
        imports.append(link_file)

    link_file.write_bytes(good_link)
    write_catalog()
    cli("resolve", "--refresh")
    assert first == lock.read_bytes()
    no_initializers()

    content = dependency.read_bytes()
    corrupted = content[:-1] + bytes([content[-1] ^ 1])
    dependency.write_bytes(corrupted)
    cli("run", "--allow-native", status=1, message="artifact identity mismatch")
    if packaging:
        cli("package", "--allow-native", "-o", base / "invalid package", status=1, message="artifact identity mismatch")
    dependency.write_bytes(b"not a dynamic library")
    write_catalog()
    cli("check", status=1, message="wrong target or image type")
    dependency.unlink()
    cli("build", "--allow-native", "-o", base / "missing", status=1, message="Missing managed native artifact")
    if packaging:
        cli("package", "--allow-native", "-o", base / "invalid package", status=1, message="Missing managed native artifact")
        assert not (base / "invalid package").exists()
    dependency.write_bytes(content)
    write_catalog(catalog_target="x86_64-unknown-linux-gnu" if windows else "x86_64-pc-windows-msvc")
    cli("resolve", "--refresh", status=1, message="catalog target")
    write_catalog()
    no_initializers()
    assert first == lock.read_bytes()

    cli("emit", "--allow-native", "-o", base / "before.o")
    write_catalog(revision="fixture-r2")
    cli("check", status=1, message="Stale or tampered void.lock")
    cli("resolve", "--refresh")
    assert first != lock.read_bytes()
    cli("emit", "--allow-native", "-o", base / "after.o")
    assert digest(base / "before.o") != digest(base / "after.o")
    no_initializers()

    def executed(*args):
        reset_markers()
        run(*args, message="VOID native application payload")
        assert marker.read_text().splitlines() == ["dependency initialized"]
        assert released.read_text().splitlines() == ["-2", "23"]

    for profile in ("debug", "release"):
        executed(compiler, "run", "--project", project, "--allow-native", "--no-cache", "--profile", profile,
                 "--", "payload")
        reset_markers()
        package = base / profile
        image = package / ("app.exe" if windows else "app")
        if packaging:
            cli("package", "--allow-native", "--profile", profile, "-o", package)
            if windows:
                cli("package", "--allow-native", "--profile", profile, "-o", package, "--replace", status=1,
                    message="Windows application packaging cannot replace an existing destination")
                for binary in (image, *sorted((package / "native").glob("*.dll"))):
                    windows_package_imports(clang.with_name("llvm-readobj.exe"), binary)
            else:
                cli("package", "--allow-native", "--profile", profile, "-o", package, "--replace")
            run(compiler, "package", "--verify", package)
        else:
            cli("build", "--allow-native", "--profile", profile, "-o", image)
            cli("build", "--allow-native", "--profile", profile, "-o", image)
        cli("build", "--allow-native", "-o", package / "native", status=1,
            message="conflicts with the reserved native payload directory")
        if not windows:
            run(clang.with_name("llvm-nm"), image, message="T void_d4_link_value")
        no_initializers()
        assert {p.name: digest(p) for p in (package / "native").iterdir()} == {
            p.name: digest(p) for p in [*images, notice]
        }
        moved = base / ("relocated 雪 " + profile)
        relocate(package, moved)
        parked_project, parked_catalog = base / "parked source", base / "parked catalog"
        relocate(project, parked_project)
        relocate(catalog, parked_catalog)
        try:
            image = moved / image.name
            if windows and packaging:
                modules.unlink(missing_ok=True)
                decoys = base / "DLL search decoys"
                decoys.mkdir(exist_ok=True)
                for dll in images:
                    for directory in (cwd, decoys):
                        (directory / dll.name).write_bytes(b"unexpected DLL search location")
                result = windows_isolated_application(image, ["payload"],
                    blocked=[(parked_project, "src/main.void"), (parked_catalog, dependency.relative_to(catalog))],
                    cwd=cwd, env=environment | {"PATH": str(decoys) + os.pathsep + environment["PATH"]}, timeout=90)
                assert result.returncode == 0 and b"VOID native application payload" in result.stdout, result
                assert marker.read_text().splitlines() == ["dependency initialized"]
                assert released.read_text().splitlines() == ["-2", "23"]
                loaded = [Path(path).resolve(strict=True) for path in modules.read_bytes().decode("utf-16-le").splitlines()]
                assert loaded == [(moved / "native" / dll.name).resolve(strict=True) for dll in images], loaded
                print(f"{profile} loaded all three selected DLLs from relocated native/")
                for dll in images:
                    (cwd / dll.name).unlink()
            else:
                executed(image, "payload")
            reset_markers()
            staged_dependency = moved / "native" / dependency.name
            staged_dependency.write_bytes(corrupted)
            run(image, "payload", status=1, message="managed native payload identity mismatch")
            if packaging:
                run(compiler, "package", "--verify", moved, status=1, message="inventory mismatch")
            no_initializers()
            staged_dependency.unlink()
            run(image, "payload", status=1, message="managed native payload missing or changed")
            if packaging:
                run(compiler, "package", "--verify", moved, status=1, message="inventory mismatch")
            no_initializers()
        finally:
            relocate(parked_project, project)
            relocate(parked_catalog, catalog)

    helper = project / "helper"
    helper.mkdir()
    (helper / "void.project.toml").write_text('schema=1\n[project]\nid="org.test.helper.project"\nname="helper"\n'
        'version="1.0.0"\nkind="library"\n[package]\nid="org.test.helper"\nname="helper"\nentry="api.void"\n')
    helper_source = helper / "api.void"
    helper_source.write_text("public func value() -> i32:\n    return 1\n")
    manifest.write_text(initial + requirement + '\n[dependencies.helper]\nid="org.test.helper"\n'
                        'path="helper"\nversion="=1.0.0"\n', encoding="utf-8")
    cli("resolve", "--refresh")
    cli("resolve", "--locked")
    cli("check")
    assert tomllib.loads(lock.read_text(encoding="utf-8"))["package"][0]["id"] == "org.test.helper"
    no_initializers()
    if server:
        main = project / "src/main.void"
        text = "use pkg/helper\n" + main.read_text(encoding="utf-8").replace(
            "func main() -> i32:\n", "func main() -> i32:\n    if helper.value() != 1:\n        return 101\n", 1)
        main.write_text(text, encoding="utf-8")
        editor(main, helper_source, text)

print("Managed native approval, deterministic locks, JIT/AOT relocation and pre-load refusal passed")
