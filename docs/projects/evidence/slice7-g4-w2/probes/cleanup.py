"""Remove only hash-verified duplicate transfer scripts after evidence capture."""
import csv
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

share = Path(r"\\vboxsvr\VM\g4w2")
home = Path.home()
transfer = home / "VoidS7-G4W2-io"
main = home / "VoidS7-G4W2-final"
acl = home / "VoidS7-G4W2-acl"
plan = json.loads((share / "cleanup-plan.json").read_bytes())
result = {"build": sys.getwindowsversion().build, "deleted": [], "retained": [],
          "free_before": shutil.disk_usage("C:/").free}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def local(path):
    return path.drive == "C:" and not path.is_symlink() and path.resolve(strict=True) == path


try:
    if not all(local(p) for p in (home, transfer, main, acl)) or result["build"] != 19045:
        raise RuntimeError("unexpected cleanup location or OS build")
    tasks = subprocess.check_output([r"C:\Windows\System32\tasklist.exe", "/fo", "csv", "/nh"],
                                    text=True, errors="replace")
    names = {row[0].lower() for row in csv.reader(tasks.splitlines()) if row}
    if names & {"probe.exe", "probe-embedded.exe", "probe-external.exe", "acl_probe.exe", "clang-cl.exe",
                "lld-link.exe", "llvm-readobj.exe"}:
        raise RuntimeError("a native probe or tool is still active")
    retained = {main / path: sha for path, sha in plan["main_artifacts"].items()}
    retained[main / "image_probe.cpp"] = plan["main_source"]
    retained[acl / "acl_probe.exe"] = plan["acl_binary"]
    retained[acl / "acl_probe.cpp"] = plan["acl_source"]
    retained[acl / "fixture" / "void_w2_gate.dll"] = plan["acl_fixture"]
    for path, sha in retained.items():
        if not local(path) or digest(path) != sha:
            raise RuntimeError(f"retained item mismatch: {path}")
        result["retained"].append({"path": str(path), "sha256": sha})
    names = {"space.cmd", "build.cmd", "native-run.py", "acl-run.cmd", "acl-run.py"}
    if set(plan["duplicates"]) != names:
        raise RuntimeError("cleanup targets do not match assignment transfer copies")
    for name in sorted(names):
        path = transfer / name
        if not local(path) or digest(path) != plan["duplicates"][name]:
            raise RuntimeError(f"transfer copy mismatch: {path}")
    for name in sorted(names):
        path = transfer / name
        path.unlink()
        result["deleted"].append({"path": str(path), "sha256": plan["duplicates"][name]})
    result["status"] = "pass"
except Exception as exc:
    result["status"] = "failed"
    result["error"] = str(exc)
    raise
finally:
    result["free_after"] = shutil.disk_usage("C:/").free
    (share / "cleanup-results.json").write_text(json.dumps(result, indent=2) + "\n")
