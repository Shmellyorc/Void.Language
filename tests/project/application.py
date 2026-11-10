"""S4 compiled application identity in JIT, AOT, and a relocated installation."""
from pathlib import Path
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile

compiler = Path(sys.argv[1]).resolve()
evidence = Path(sys.argv[2]).resolve()
suffix = ".exe" if os.name == "nt" else ""
operations = ("data", "config", "cache", "state")
records = []
checks = 0
app_id = "org.test.context-game"
project_id = "org.test.context-project"


def check(condition, detail):
    global checks
    checks += 1
    assert condition, detail


def run(args, cwd, env, status=0):
    command = list(map(str, args))
    result = subprocess.run(command, cwd=cwd, env=env, capture_output=True, timeout=90)
    stdout = result.stdout.decode("utf-8", errors="backslashreplace").replace("\r\n", "\n")
    stderr = result.stderr.decode("utf-8", errors="backslashreplace").replace("\r\n", "\n")
    records.append({"argv": command, "cwd": str(cwd), "status": result.returncode, "stdout": stdout, "stderr": stderr})
    check(result.returncode == status, records[-1])
    return stdout, stderr


def manifest(name="Orbit 雪", version="1.0.0"):
    return f"""schema=1
[project]
id='{project_id}'
name='Project'
version='2.3.4'
kind='application'
[application]
id='{app_id}'
name='{name}'
version='{version}'
[build.release]
debug_info='none'
[build.debug]
debug_info='none'
"""


def encoded_metadata(name, version):
    fields = (project_id, "Project", "2.3.4", app_id, name, version)
    return struct.pack("<I", 1) + b"".join(struct.pack("<I", len(b)) + b for b in map(lambda s: s.encode(), fields))


def roots(base, use_xdg):
    env = dict(os.environ)
    locations = {}
    if sys.platform == "linux":
        home = base / "uncreated home 雪"
        env["HOME"] = str(home)
        for operation, fallback in zip(operations, (".local/share", ".config", ".cache", ".local/state")):
            root = base / f"uncreated {operation} 雪" if use_xdg else home / fallback
            env[f"XDG_{operation.upper()}_HOME"] = str(root) if use_xdg else "invalid-relative"
            locations[operation] = root
    elif os.name == "nt":
        # Ask the platform API independently of VOID and environment conventions.
        import ctypes
        import uuid
        from ctypes import wintypes

        def known_folder(identifier):
            guid = (ctypes.c_byte * 16).from_buffer_copy(uuid.UUID(identifier).bytes_le)
            location = ctypes.c_wchar_p()
            api = ctypes.windll.shell32.SHGetKnownFolderPath
            api.argtypes = (ctypes.c_void_p, wintypes.DWORD, wintypes.HANDLE, ctypes.POINTER(ctypes.c_wchar_p))
            api.restype = ctypes.c_long
            status = api(ctypes.byref(guid), 0x4000, None, ctypes.byref(location))  # KF_FLAG_DONT_VERIFY
            check(status == 0 and location.value, f"Known Folder failure {status}")
            try:
                return Path(location.value)
            finally:
                ctypes.windll.ole32.CoTaskMemFree(ctypes.cast(location, ctypes.c_void_p))

        local = known_folder("F1B32785-6FBA-4FCF-9D55-7B8E7F157091")
        roaming = known_folder("3EB685DB-65F9-4CF6-A03A-E3EF65729F3D")
        locations = {"data": local, "config": roaming, "cache": local, "state": local}
        env.update(LOCALAPPDATA=str(base / "poisoned-local"), APPDATA=str(base / "poisoned-roaming"))
    else:
        raise RuntimeError("This fixture has native Linux and Windows path oracles")
    return env, locations


def expected_paths(locations, label):
    rows = []
    for operation, root in locations.items():
        path = root / app_id
        if os.name == "nt" and operation in ("cache", "state"):
            path /= operation
        rows.append(f"{label}-{operation}|{path.as_posix()}")
    return rows


report = """use os as system

func show(label: string, response: result<string, system.error>):
    if response is Ok(var value):
        print("{label}|{value}")
    else if response is Err(var reason):
        var unavailable = false
        match reason.kind:
            system.error_kind.unavailable:
                unavailable = true
        print("{label}|{reason.code}|{reason.operation}|{reason.path.byte_length}|{reason.native_domain.byte_length}|{reason.os_code}|{reason.message}|{unavailable}")
"""
src = report + "\n" + "".join(f"var eager_{n} = system.{n}_dir()\n" for n in operations)
src += "\nfunc main():\n"
for kind in ("eager", "body"):
    for operation in operations:
        expression = f"eager_{operation}" if kind == "eager" else f"system.{operation}_dir()"
        src += f'    show("{kind}-{operation}", {expression})\n'
src += f'    var named: func(string) -> result<string, system.error> = system.data_dir\n'
src += f'    show("function-value", named("{app_id}"))\n'
src += f'    show("named-explicit", system.config_dir(app_id = "{app_id}"))\n'
src += '    show("invalid-explicit", system.data_dir("CON"))\n'

try:
    with tempfile.TemporaryDirectory(prefix="VOID S4 metadata 雪 ") as directory:
        base = Path(directory)
        root, install, cwd = (base / name for name in ("checkout", "installation", "unrelated"))
        for folder in (root, install, cwd):
            folder.mkdir()
        policy = root / "void.project.toml"
        policy.write_text(manifest(), encoding="utf-8")
        active = root / "launcher.void"
        active.write_text(src, encoding="utf-8")
        env, locations = roots(base, True)
        fallback, fallback_locations = roots(base, False)
        absent = cwd / "standalone.void"
        absent.write_text(src, encoding="utf-8")

        def success(output, locations):
            lines = output.splitlines()
            paths = expected_paths(locations, "eager") + expected_paths(locations, "body")
            paths += [paths[0].replace("eager-data", "function-value"), paths[1].replace("eager-config", "named-explicit")]
            check(lines[:-1] == paths, lines)
            check(lines[-1].startswith("invalid-explicit|2|os.data_dir|0|0|0|"), lines[-1])

        def missing(output):
            lines = output.splitlines()
            expected = [f"{kind}-{operation}|6|os.{operation}_dir|0|0|0|Application identity is unavailable.|true"
                        for kind in ("eager", "body") for operation in operations]
            check(lines[:8] == expected, lines)

        def unrelated_queries():
            # Invalid native root text would surface as a different error if
            # the adapter queried roots before detecting an absent context.
            if sys.platform != "linux":
                return env
            return {**env, **{f"XDG_{kind.upper()}_HOME": b"/poison\xff" for kind in operations}}

        outputs = []
        for profile in ("debug", "release"):
            output, _ = run([compiler, "run", "--project", root, "--profile", profile, "--no-cache"], cwd, env)
            success(output, locations)
            output, _ = run([compiler, "run", "--project", root, "--profile", profile, "--no-cache"], cwd, fallback)
            success(output, fallback_locations)
            flags = ["--no-opt"] if profile == "debug" else []
            output, _ = run([compiler, "run", absent, "--no-cache", *flags], cwd, unrelated_queries())
            missing(output)
            binary = base / ("output-" + profile + suffix)
            run([compiler, "build", "--project", root, "--profile", profile, "-o", binary], cwd, env)
            check(encoded_metadata("Orbit 雪", "1.0.0") in binary.read_bytes(), "AOT record missing")
            target = install / ("unrelated-image-" + profile + suffix)
            binary.rename(target)
            outputs.append(target)
            no_context = install / ("no-context-" + profile + suffix)
            run([compiler, "build", absent, "-o", no_context, *flags], cwd, env)
            missing(run([no_context], cwd, unrelated_queries())[0])

        policy.write_text(manifest("Orbit Renamed", "1.0.1-beta+build"), encoding="utf-8")
        changed = install / ("version-changed" + suffix)
        run([compiler, "build", "--project", root, "-o", changed], cwd, env)
        check(encoded_metadata("Orbit Renamed", "1.0.1-beta+build") in changed.read_bytes(), "updated metadata missing")
        check(encoded_metadata("Orbit 雪", "1.0.0") not in changed.read_bytes(), "old metadata retained")
        outputs.append(changed)

        # Scalar applications keep their record even when no Runtime is needed.
        active.write_text("func main() -> i32:\n    return 7\n", encoding="utf-8")
        scalar = install / ("scalar" + suffix)
        run([compiler, "build", "--project", root, "-o", scalar], cwd, env)
        check(encoded_metadata("Orbit Renamed", "1.0.1-beta+build") in scalar.read_bytes(), "scalar metadata eliminated")

        # Source-only binding and diagnostics. Local and imported lookalikes
        # retain their declared arity, and function values retain one parameter.
        shadow = cwd / "shadow.void"
        shadow.write_text("func data_dir() -> i32:\n    return 41\n", encoding="utf-8")
        control = cwd / "control.void"
        control.write_text('use "./shadow.void" as os\nfunc main() -> i32:\n    return os.data_dir()\n', encoding="utf-8")
        run([compiler, "run", control, "--no-cache"], cwd, env, 41)
        shadow.write_text("func data_dir(id: string) -> i32:\n    return 41\n", encoding="utf-8")
        check("required argument" in run([compiler, "check", control], cwd, env, 1)[1], "lookalike adapted")
        control.write_text("use os\nfunc main():\n    var f: func() -> result<string, os.error> = os.data_dir\n", encoding="utf-8")
        run([compiler, "check", control], cwd, env, 1)

        invalid = manifest().replace(app_id, "con.bad")
        policy.write_text(invalid, encoding="utf-8")
        check("VOID5004" in run([compiler, "run", "--project", root], cwd, env, 1)[1], "invalid ID ran application")
        shutil.rmtree(root)
        absent.unlink()
        for target in outputs:
            for active_env, expected in ((env, locations), (fallback, fallback_locations)):
                # Even an explicit compiler source root cannot affect the installed program.
                active_env = dict(active_env, VOID_STDLIB_ROOT=str(base / "missing compiler resources"))
                success(run([target], cwd, active_env)[0], expected)
        run([scalar], cwd, env, 7)
        for root_path in locations.values():
            if sys.platform == "linux":
                check(not root_path.exists(), f"location query created {root_path}")
        check(not (base / "uncreated home 雪").exists(), "location query created HOME")
    print(f"project_application_process: {checks} checks passed")
finally:
    evidence.write_text(json.dumps({"checks": checks, "processes": records}, indent=2) + "\n", encoding="utf-8")
