#!/usr/bin/env python3
"""Check real component installs and Windows notice-rule selection, without a build.

The Windows case exercises CMake file installation only, not native Windows code.
Each command log and the final receipt (including failures) stays in evidence-dir.
"""
import argparse
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--evidence-dir", required=True, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    evidence = args.evidence_dir.resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    inventory = json.loads((root / "docs/release/NOTICE_INVENTORY.json").read_text())
    results = []
    commands = []

    def command(name, argv):
        completed = subprocess.run(argv, text=True, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT)
        (evidence / (name + ".log")).write_text(completed.stdout)
        commands.append({"argv": argv, "exit_code": completed.returncode,
                         "log": name + ".log"})
        if completed.returncode:
            raise AssertionError(f"{name} exited {completed.returncode}; see log")

    def check(name, operation):
        try:
            detail = operation()
            results.append({"name": name, "result": "PASS", "detail": detail})
        except Exception as error:
            results.append({"name": name, "result": "FAIL", "error": str(error)})

    def matches(path, expected):
        if not path.is_file():
            raise AssertionError(f"Missing installed license/source: {path}")
        if digest(path) != expected:
            raise AssertionError(f"Installed bytes differ: {path}")

    def source_inventory():
        entries = inventory["entries"] + inventory["retained_entries"]
        for entry in entries:
            matches(root / entry["repository_path"], entry["sha256"])
        package = json.loads((root / "editors/vscode/package.json").read_text())
        lock = json.loads((root / "editors/vscode/package-lock.json").read_text())
        assert package["license"] == lock["packages"][""]["license"] == "MIT"
        return {"verified_inventory_entries": len(entries),
                "editor_original_license_metadata": "MIT"}

    def verify(prefix, component, platform, common_only=False):
        entries = inventory["entries"] + ([] if common_only else inventory["retained_entries"])
        checked = 0
        for entry in entries:
            if component not in entry["components"]:
                continue
            if entry.get("platform", "all") not in ("all", platform):
                continue
            if common_only and "/http/" in entry["installed_path"]:
                continue
            matches(prefix / entry["installed_path"], entry["sha256"])
            checked += 1
        matches(prefix / "share/void/licenses/NOTICE_INVENTORY.json",
                digest(root / "docs/release/NOTICE_INVENTORY.json"))
        if not common_only:
            for source in (root / "stdlib").rglob("*.void"):
                matches(prefix / "share/void/stdlib" / source.relative_to(root / "stdlib"),
                        digest(source))
            assert (prefix / "bin" / ("voidc" if component == "compiler" else "voidls")).is_file()
            if component == "compiler":
                for name in ("void-native.txt", "void-native-resources.txt"):
                    assert (prefix / "bin" / name).is_file(), name
                for dependency in inventory_http["dependencies"]:
                    if dependency["name"] in ("libidn2", "libunistring"):
                        matches(prefix / "share/void/licenses/http/corresponding-source" /
                                dependency["archive"], dependency["sha256"])
                assert (prefix / "lib/void/libvoid_runtime.a").is_file()
                assert (prefix / "lib/void/libvoid_native_fixture.a").is_file()
            else:
                assert not (prefix / "lib/void").exists()
        return {"verified_license_entries": checked,
                "stdlib_sources": 0 if common_only else len(list((root / "stdlib").rglob("*.void"))),
                "scope": "Windows file rules only; no native execution" if common_only
                         else "Linux existing component install; no requalification"}

    inventory_http = json.loads((root / "third_party/http-versions.json").read_text())
    check("source-inventory", source_inventory)
    with tempfile.TemporaryDirectory(prefix="VOID license installs ") as temporary:
        scratch = Path(temporary)
        for component in ("compiler", "tooling"):
            def install(component=component):
                prefix = scratch / component
                command("linux-" + component, ["cmake", "--install", str(args.build_dir.resolve()),
                        "--prefix", str(prefix), "--component", component])
                return verify(prefix, component, "Linux")
            check("linux-" + component + "-install", install)

        # Use the production helper itself; no duplicated Windows install rule.
        project = scratch / "windows-notice-rules"
        project.mkdir()
        (project / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.24)\nproject(NoticeRules NONE)\n'
            'set(WIN32 TRUE)\ninclude("' + (root / "cmake/InstallLicenses.cmake").as_posix() + '")\n'
            'void_install_licenses(compiler)\nvoid_install_licenses(tooling)\n')
        windows_configured = True
        try:
            command("windows-rules-configure", ["cmake", "-S", str(project), "-B", str(project / "build")])
        except Exception as error:
            windows_configured = False
            results.append({"name": "windows-rules-configure", "result": "FAIL", "error": str(error)})
        if windows_configured:
            for component in ("compiler", "tooling"):
                def install(component=component):
                    prefix = scratch / ("windows-" + component)
                    command("windows-rules-" + component,
                            ["cmake", "--install", str(project / "build"), "--prefix", str(prefix),
                             "--component", component])
                    return verify(prefix, component, "Windows", common_only=True)
                check("windows-notice-rules-" + component, install)
    receipt = {"scope": "Focused licensing checks, not platform/installation qualification",
               "commands": commands, "results": results,
               "native_windows_macos": "NOT RUN / UNKNOWN"}
    (evidence / "validation.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps(results, indent=2))
    return 1 if any(result["result"] != "PASS" for result in results) else 0


if __name__ == "__main__":
    raise SystemExit(main())
