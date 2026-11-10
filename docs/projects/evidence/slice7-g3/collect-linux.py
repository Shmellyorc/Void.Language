from hashlib import file_digest
from pathlib import Path
import json
import os
import re
import shlex
import shutil
import subprocess
import sys

profile = sys.argv[1]
build = Path("build/llvm23-" + profile).resolve()
out = Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)
fixture = build / "ffi-g3"
source = Path('tests/ffi/resources.void').resolve()
root = Path('docs/projects/evidence/slice7-g3').resolve()
temporary = Path('build/slice7-g3/source-probes').resolve()
env = dict(os.environ, ASAN_OPTIONS="abort_on_error=1:halt_on_error=1:detect_leaks=1",
           UBSAN_OPTIONS="abort_on_error=1:halt_on_error=1:print_stacktrace=1")
entries = json.loads((build / "compile_commands.json").read_text())
commands = [entry for entry in entries if Path(entry["file"]).name in ("resources_thunk.c", "resources_fixture.c")]
assert len(commands) == 2
names = ["void_g3_tests", "void_g3_aot_normal", "void_g3_aot_optimized", "voidc",
         "ffi-g3/application-normal.o", "ffi-g3/application-optimized.o", "ffi-g3/resources.descriptor"]
names.extend(entry["output"] for entry in commands)

def hash_file(path):
    with path.open("rb") as stream:
        return file_digest(stream, "sha256").hexdigest()

data = {
    "profile": profile, "host": os.uname().machine, "c_compile_commands": commands,
    "artifacts": {name: hash_file(build / name) for name in names},
    "sanitizer_environment": {name: env[name] for name in ("ASAN_OPTIONS", "UBSAN_OPTIONS")},
    "processes": [],
    "clang_abi_probe": [],
}
descriptor = (fixture / "resources.descriptor").read_text()
exports = {line.split()[2] for line in descriptor.splitlines() if line.startswith("bind ")}
for field, file in (("thunk", "resources_thunk.c"), ("fixture", "resources_fixture.c")):
    assert re.search(rf"^{field} ([a-f0-9]+)$", descriptor, re.M)[1] == hash_file(Path("tests/ffi") / file)
for entry in commands:
    argv = shlex.split(entry["command"])
    output = argv.index("-o")
    argv[output:output + 2] = ["-o", "-"]
    argv[argv.index("-c"):argv.index("-c") + 1] = ["-S", "-emit-llvm"]
    compiled = subprocess.run(argv, cwd=entry["directory"], capture_output=True, text=True, check=True)
    lines = [line for line in compiled.stdout.splitlines()
             if re.match(r"(declare|define) .* @g3_", line) or line.startswith("target triple")]
    covered = {re.search(r"@(g3_\w+)\(", line)[1] for line in lines if "@g3_" in line}
    assert exports <= covered, exports - covered
    assert any(" ptr " in line and "@g3_create" in line for line in lines)
    assert any("ptr" in line and "@g3_close" in line for line in lines)
    data["clang_abi_probe"].append({"argv": argv, "C_IR_signatures": lines})

def run(label, args, status, expected):
    args = list(map(str, args))
    result = subprocess.run(args, text=True, capture_output=True, env=env, timeout=60)
    data["processes"].append({"case": label, "argv": args, "status": result.returncode,
                              "stdout": result.stdout, "stderr": result.stderr})
    assert result.returncode == status and expected in result.stdout + result.stderr, result

run("direct-jit-typed-ir-ownership", [build / "void_g3_tests", fixture / "resources.descriptor", source, fixture / "evidence-replay"], 0, "G3 JIT O2 returned 104")
for mode in ("normal", "optimized"):
    run("independently-linked-aot-" + mode, [sys.executable, Path('tests/ffi/resources_process.py').resolve(),
                                          build / ("void_g3_aot_" + mode)], 0, "7 0 104")
for label, expected in (("startup", "completed application startup"), ("forged", "nonforgeable")):
    copied = temporary / (label + ".void")
    assert hash_file(copied) == hash_file(root / (label + ".void"))
    output = temporary / (profile + "-" + label + ".o")
    run(label + "-source-denial", [build / "void_g3_tests", "--emit", fixture / "resources.descriptor", copied, output, "normal"],
        1, expected)
    assert not output.exists()
shutil.copyfile(fixture / "resources.descriptor", out / (profile + "-resources.descriptor"))
(out / (profile + "-artifacts.json")).write_text(json.dumps(data, indent=2) + "\n")
