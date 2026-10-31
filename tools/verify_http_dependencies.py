#!/usr/bin/env python3
"""Validate a disposable HTTP prefix against the current immutable pin set."""
import hashlib
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
prefix = Path(sys.argv[1]).resolve()
report = json.loads((prefix / "VOID-HTTP-DEPENDENCIES.json").read_text())
pins = root / "third_party/http-versions.json"
if len(sys.argv) > 2 and report["profile"] != sys.argv[2]:
    raise SystemExit("HTTP dependency instrumentation/build profile mismatch")
if report["pins_sha256"] != hashlib.sha256(pins.read_bytes()).hexdigest():
    raise SystemExit("HTTP dependency pin set changed; rebuild the disposable prefix")
if len(sys.argv) > 3 and report.get("platform", "Linux") != sys.argv[3]:
    raise SystemExit("HTTP dependency native platform mismatch")
if len(sys.argv) > 4 and report.get("architecture") != sys.argv[4]:
    raise SystemExit("HTTP dependency architecture mismatch")
for name, digest in report.get("recipe_sha256", {}).items():
    if hashlib.sha256((root / name).read_bytes()).hexdigest() != digest:
        raise SystemExit(f"HTTP native dependency recipe changed: {name}; rebuild the prefix")
for name, digest in report["files"].items():
    path = prefix / name
    if not path.resolve().is_relative_to(prefix) or not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
        raise SystemExit(f"HTTP dependency missing or digest mismatch: {name}")
for name in (("libidn2.so", "libunistring.so") if report.get("platform", "Linux") == "Linux" else ()):
    target = (prefix / "lib" / name).resolve(strict=True)
    if not target.is_relative_to(prefix / "lib") or not target.name.startswith(name + "."):
        raise SystemExit(f"HTTP shared library alias escapes its pinned family: {name}")
    if target.relative_to(prefix).as_posix() not in report["files"]:
        raise SystemExit(f"HTTP shared library alias has an unverified target: {name}")
for path in (prefix / "lib").glob("*.so*"):
    if path.is_symlink() and not path.resolve(strict=True).is_relative_to(prefix / "lib"):
        raise SystemExit(f"HTTP shared library alias escapes prefix: {path.name}")
for pin in json.loads(pins.read_text())["dependencies"]:
    if pin["name"] not in report.get("selected_dependencies", [pin["name"]]):
        continue
    archive = root / ".toolchains/http/downloads" / pin["archive"]
    if not archive.is_file() or hashlib.sha256(archive.read_bytes()).hexdigest() != pin["sha256"]:
        raise SystemExit(f"HTTP corresponding source missing or digest mismatch: {pin['archive']}")
print(f"HTTP dependency prefix verified: {report['profile']}; {len(report['files'])} files")
