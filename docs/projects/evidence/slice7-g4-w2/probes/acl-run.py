"""Rerun only the file access control with a fully restricted token."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

share = Path(r"\\vboxsvr\VM\g4w2")
home = Path.home()
previous = home / "VoidS7-G4W2-final"
work = home / "VoidS7-G4W2-acl"
out = share / "acl-followup"
receipt = json.loads((share / previous.name / "results.json").read_bytes())


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


record = {"build": sys.getwindowsversion().build, "workspace": str(work),
          "prebuild_free_bytes": shutil.disk_usage("C:/").free, "commands": []}
if record["build"] != 19045 or record["prebuild_free_bytes"] < 12 * 1024**3 or home.drive != "C:":
    raise RuntimeError("guest platform, location or storage preflight")
out.mkdir()
work.mkdir()


def run(label, argv):
    argv = [str(value) for value in argv]
    result = subprocess.run(argv, cwd=work, capture_output=True, timeout=120)
    output = result.stdout + result.stderr
    (out / (label + ".txt")).write_bytes(output)
    record["commands"].append({"label": label, "argv": argv, "exit": result.returncode})
    return result.returncode, output


try:
    base = previous / "image_probe.cpp"
    record["base_source_sha256"] = sha(base)
    if record["base_source_sha256"] != receipt["inputs"]["sources"][base.name]:
        raise RuntimeError("baseline source hash changed")
    old = b"DISABLE_MAX_PRIVILEGE|WRITE_RESTRICTED"
    new = b"DISABLE_MAX_PRIVILEGE"
    content = base.read_bytes()
    if content.count(old) != 1:
        raise RuntimeError("expected exact write-only restriction flag")
    source = work / "acl_probe.cpp"
    source.write_bytes(content.replace(old, new))
    shutil.copyfile(source, out / source.name)
    record["compiled_source_sha256"] = sha(source)
    record["runner_sha256"] = sha(Path(__file__))
    clang = home / r"VoidCI42377496702-Native\.toolchains\llvm-23.1.4\bin\clang-cl.exe"
    program = work / "acl_probe.exe"
    code, _ = run("build-acl", [clang, "/nologo", "/std:c++20", "/EHsc", "/MT", "-fuse-ld=lld",
                                 source, "/Fe" + str(program), "/link", "bcrypt.lib", "advapi32.lib",
                                 "/manifest:embed"])
    if code:
        raise RuntimeError("ACL-only build failed")
    record["binary_sha256"] = sha(program)
    directory = work / "fixture"
    directory.mkdir()
    fixture = directory / "void_w2_gate.dll"
    shutil.copyfile(previous / "approved" / fixture.name, fixture)
    record["fixture_sha256"] = sha(fixture)
    if record["fixture_sha256"] != receipt["artifacts"]["approved/" + fixture.name]:
        raise RuntimeError("ACL fixture changed")
    code, output = run("case-acl-full", [program, "acl", fixture, directory, record["fixture_sha256"]])
    record["passed"] = code == 0 and b"read=1 write=0 delete=0 change-DACL=0 add-file=0" in output
    sys.exit(0 if record["passed"] else 1)
finally:
    (out / "results.json").write_text(json.dumps(record, indent=2) + "\n")
