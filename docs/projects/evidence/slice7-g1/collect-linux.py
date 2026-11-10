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
fixture = build / "ffi-g1"
env = dict(os.environ, ASAN_OPTIONS="abort_on_error=1:halt_on_error=1:detect_leaks=1",
           UBSAN_OPTIONS="abort_on_error=1:halt_on_error=1:print_stacktrace=1")
entries = json.loads((build / "compile_commands.json").read_text())
commands = [entry for entry in entries if Path(entry["file"]).name in ("primitives_thunk.c", "primitives_fixture.c")]
assert len(commands) == 3
names = ["void_g1_tests", "void_g1_aot_normal", "void_g1_aot_optimized", "void_g1_aot_mismatch", "voidc",
         "ffi-g1/application-normal.o", "ffi-g1/application-optimized.o", "ffi-g1/primitives.descriptor", "ffi-g1/primitives.void"]
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
descriptor = (fixture / "primitives.descriptor").read_text()
exports = {line.split()[2] for line in descriptor.splitlines() if line.startswith("bind ")}
for field, file in (("thunk", "primitives_thunk.c"), ("fixture", "primitives_fixture.c")):
    assert re.search(rf"^{field} ([a-f0-9]+)$", descriptor, re.M)[1] == hash_file(Path("tests/ffi") / file)
for entry in commands:
    argv = shlex.split(entry["command"])
    if any(arg.startswith("-DG1_MISMATCHED_FIXTURE") for arg in argv):
        continue
    output = argv.index("-o")
    argv[output:output + 2] = ["-o", "-"]
    argv[argv.index("-c"):argv.index("-c") + 1] = ["-S", "-emit-llvm"]
    compiled = subprocess.run(argv, cwd=entry["directory"], capture_output=True, text=True, check=True)
    lines = [line for line in compiled.stdout.splitlines() if re.match(r"(declare|define) .* @g1_", line)]
    covered = {re.search(r"@(g1_\w+)\(", line)[1] for line in lines}
    assert exports <= covered, exports - covered
    data["clang_abi_probe"].append({"argv": argv, "C_IR_signatures": lines})

def run(label, args, status, expected):
    args = list(map(str, args))
    result = subprocess.run(args, text=True, capture_output=True, env=env, timeout=60)
    data["processes"].append({"case": label, "argv": args, "status": result.returncode,
                              "stdout": result.stdout, "stderr": result.stderr})
    assert result.returncode == status and expected in result.stdout + result.stderr, result

run("direct-jit-and-typed-ir", [build / "void_g1_tests", fixture / "primitives.descriptor",
    fixture / "primitives.void", fixture / "evidence-replay"], 0, "JIT O2 returned 73")
for mode in ("normal", "optimized"):
    run("independently-linked-aot-" + mode, [build / ("void_g1_aot_" + mode), 7], 0, "73\n")
run("mismatched-fixture-aot", [build / "void_g1_aot_mismatch", 7], 1,
    "C primitive provider rejected the exact descriptor or fixture identity")
for name in ("primitives.descriptor", "primitives.void"):
    shutil.copyfile(fixture / name, out / (profile + "-" + name))
(out / (profile + "-artifacts.json")).write_text(json.dumps(data, indent=2) + "\n")
