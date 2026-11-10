"""Run the bounded G4-W2 controls on the existing Windows 10 guest."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import traceback

share = Path(r"\\vboxsvr\VM\g4w2")
home = Path.home()
name = "void_w2_gate.dll"
tools = home / r"VoidCI42377496702-Native\.toolchains\llvm-23.1.4\bin"
run = json.loads((share / "inputs.json").read_text())
work = home / run["workspace"]
out = share / run["workspace"]
out.mkdir()
record = {"workspace": str(work), "build": sys.getwindowsversion().build,
          "commands": [], "artifacts": {}, "inputs": run, "status": "incomplete"}


def save():
    (out / "results.json").write_text(json.dumps(record, indent=2) + "\n")


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def cmd(label, argv, expected=0, contains=()):
    argv = [str(value) for value in argv]
    print(label, subprocess.list2cmdline(argv), flush=True)
    result = subprocess.run(argv, cwd=work, capture_output=True, timeout=120)
    output = result.stdout + result.stderr
    (out / (label + ".txt")).write_bytes(output)
    passed = result.returncode == expected and all(item.encode() in output for item in contains)
    record["commands"].append({"label": label, "argv": argv, "exit": result.returncode,
                               "expected_exit": expected, "expected_output": list(contains), "passed": passed})
    save()
    return passed


def require(label, argv, expected=0, contains=()):
    if not cmd(label, argv, expected, contains):
        raise RuntimeError(f"{label} did not meet its control")


def copy_case(folder, source, filename=name):
    path = work / folder / filename
    path.parent.mkdir(exist_ok=True)
    shutil.copyfile(source, path)
    return path


def main():
    record["prebuild_free_bytes"] = shutil.disk_usage("C:/").free
    if record["build"] != 19045 or record["prebuild_free_bytes"] < 12 * 1024**3:
        raise RuntimeError("guest platform or storage preflight")
    if work.drive != "C:" or work.parent != home or not work.name.startswith("VoidS7-G4W2"):
        raise RuntimeError("workspace must be assignment-owned local C:")
    work.mkdir()
    for filename, digest in run["sources"].items():
        if Path(filename).name != filename:
            raise RuntimeError("unexpected source path")
        local = Path(__file__).parent / filename if filename in ("native-run.py", "build.cmd") else work / filename
        if filename not in ("native-run.py", "build.cmd"):
            shutil.copyfile(share / filename, local)
        if sha(local) != digest:
            raise RuntimeError(f"input hash mismatch: {filename}")
    clang = tools / "clang-cl.exe"
    lld = tools / "lld-link.exe"
    readobj = tools / "llvm-readobj.exe"
    require("toolchain-clang", [clang, "--version"])
    require("toolchain-lld", [lld, "--version"])
    require("toolchain-readobj", [readobj, "--version"])
    for folder, source, define in [
        ("approved", "dependency.c", "/DFIXTURE_VALUE=7"),
        ("collision", "dependency.c", "/DFIXTURE_VALUE=99"),
        ("sentinel121", "sentinel.c", "/DW2_SENTINEL=121"),
        ("sentinel122", "sentinel.c", "/DW2_SENTINEL=122"),
    ]:
        (work / folder).mkdir()
        obj = f"{folder}.obj"
        require(f"compile-{folder}", [clang, "/nologo", "/TC", "/c", "/GS-", "/Zl",
                                      define, source, "/Fo" + obj])
        entry = ["/entry:sentinel_entry", "kernel32.lib"] if source == "sentinel.c" else ["/noentry"]
        require(f"link-{folder}", [lld, "/dll", "/nodefaultlib", f"/out:{folder}/{name}", obj, *entry])
    good = work / "approved" / name
    bad121 = work / "sentinel121" / name
    bad122 = work / "sentinel122" / name
    digest = sha(good)
    probe = work / "probe.exe"
    (work / "embedded").mkdir()
    (work / "external").mkdir()
    embedded = work / "embedded" / "probe-embedded.exe"
    external = work / "external" / "probe-external.exe"
    for label, program, manifest in [
        ("probe", probe, ["/manifest:embed"]),
        ("embedded", embedded, ["/manifest:embed", "/manifestinput:redirect.manifest"]),
        ("external", external, ["/manifest:no"]),
    ]:
        require(f"build-{label}", [clang, "/nologo", "/std:c++20", "/EHsc", "/MT",
                                   "image_probe.cpp", "bcrypt.lib", "advapi32.lib", f"/Fo{label}.obj",
                                   f"/Fe{program}", "/link", *manifest])
    old = home / r"VoidS7-G4W-architecture-final\good\void_g4w_forwarder.dll"
    if sha(old) != run["forwarder_sha256"]:
        raise RuntimeError("W1 local forwarder does not match accepted receipt")
    forwarded = copy_case("forwarder", old)
    for label, image in [("approved", good), ("collision", work / "collision" / name),
                         ("sentinel121", bad121), ("sentinel122", bad122), ("forwarder", forwarded),
                         ("embedded", embedded), ("external", external), ("probe", probe)]:
        require(f"pe-{label}", [readobj, "--file-headers", "--coff-imports", "--coff-exports",
                                "--coff-resources", image])
    pin = copy_case("pin", good)
    replacement = copy_case("pin-replacement", bad121)
    acl = copy_case("acl", good)
    hard = copy_case("hardlink", good)
    os.link(hard, hard.with_name("second.dll"))
    junction = work / "junction"
    require("make-junction", [Path(r"C:\Windows\System32\cmd.exe"), "/d", "/c", "mklink", "/J",
                              junction, good.parent])
    if not junction.is_junction() or junction.resolve(strict=True) != good.parent:
        raise RuntimeError("junction fixture targets unexpected directory")
    symlink = work / "symlink" / name
    symlink.parent.mkdir()
    try:
        os.symlink(good, symlink)
    except OSError as error:
        if error.winerror != 1314:
            raise
        record["leaf_symlink"] = str(error)
    copy_case("embedded", bad122)
    copy_case("external", bad122)
    copy_case("external", work / "redirect.manifest", external.name + ".manifest")
    sidecar = copy_case("override", work / "redirect.manifest", "redirect.manifest")
    copy_case("override", bad122)
    cases = [
        ("clean", probe, "clean", good, "-", digest, 0, ("result=7",)),
        ("pin", probe, "pin", pin, replacement, digest, 0,
         ("pinned replacement error=", "ancestor rename error=", "result=7")),
        ("alias", probe, "alias", str(pin.parent) + r"\..\pin" + "\\" + name, "-", digest, 0,
         ("denied before executable load: path alias",)),
        ("junction", probe, "junction", junction / name, "-", digest, 0,
         ("denied before executable load: reparse ancestor",)),
        ("hardlink", probe, "hardlink", hard, "-", digest, 0,
         ("denied before executable load: hardlinked image",)),
        ("forward", probe, "forward", forwarded, "-", sha(forwarded), 0,
         ("denied before executable load: forwarded export",)),
        ("late", probe, "late", good, work / "collision" / name, digest, 0,
         ("initial namespace observation clear", "denied before executable load: loaded basename")),
        ("thread", probe, "thread", good, sidecar, digest, 0,
         ("active redirect=1", "denied before executable load: active manifest redirect")),
        ("embedded", embedded, "embedded", good, "-", digest, 0,
         ("embedded manifest=1", "denied before executable load: active manifest redirect")),
        ("external", external, "external", good, "-", digest, 0,
         ("embedded manifest=0", "denied before executable load: active manifest redirect")),
        ("acl", probe, "acl", acl, acl.parent, digest, 0,
         ("read=1 write=0 delete=0 change-DACL=0 add-file=0",)),
    ]
    if symlink.is_symlink():
        cases.append(("symlink", probe, "symlink", symlink, "-", digest, 0,
                      ("denied before executable load: reparse image",)))
    for label, mode, bad, code, message in [
        ("swap-open", "unguarded-substitute", bad121, 121, "approved no-entry image replaced"),
        ("calibrate122", "unguarded-substitute", bad122, 122, "approved no-entry image replaced"),
        ("swap-guarded", "guarded-substitute", bad122, 0, "denied before executable load: image digest"),
    ]:
        target = copy_case(label, good)
        alternate = copy_case(label, bad, "replacement.dll")
        cases.append((label, probe, mode, target, alternate, digest, code, (message,)))
    # Keep all controls even if another control fails. Each executable starts in a fresh process.
    results = [cmd("case-" + label, [program, mode, path, other, expected_sha], code, output)
               for label, program, mode, path, other, expected_sha, code, output in cases]
    for root, dirs, files in os.walk(work):
        dirs[:] = [d for d in dirs if not (Path(root) / d).is_junction()]
        for file in files:
            path = Path(root) / file
            if not path.is_symlink() and path.suffix.lower() in (".dll", ".exe", ".manifest"):
                record["artifacts"][path.relative_to(work).as_posix()] = sha(path)
    record["status"] = "pass" if all(results) else "failed controls"
    return 0 if all(results) else 1


try:
    sys.exit(main())
except Exception:
    record["status"] = "setup or build failure"
    record["failure"] = traceback.format_exc()
    raise
finally:
    save()
