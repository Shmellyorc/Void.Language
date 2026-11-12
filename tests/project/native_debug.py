"""Check emitted VOID DWARF and optionally exercise a native debugger."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile


MANIFEST = """schema=1
[project]
id='org.test.native.debug'
name='Source debug'
version='1.0.0'
kind='application'
[application]
id='org.test.native.debug.app'
"""
ENTRY = """use "./math source.void" as arithmetic
value struct box:
    value: i32
func (self box) init(initial: i32):
    var assigned = initial + 1
    self.value = assigned
func main() -> i32:
    var stored = box(initial = 7)
    return arithmetic.compute(stored.value)
"""
HELPER = """public func compute(input: i32) -> i32:
    var doubled = input * 2
    var result = doubled + 3
    return result
"""


def run(*args, status=0):
    result = subprocess.run(list(map(str, args)), capture_output=True, timeout=90,
                            text=True, encoding="utf-8", errors="replace")
    assert result.returncode == status, (args, result.returncode, result.stdout, result.stderr)
    return result


def verify_debugger(root, executable, lldb, gdb, mode):
    commands = root / "debugger commands.txt"
    if lldb:
        options = [
            "settings set target.disable-aslr false",
            'breakpoint set -f "main.void" -l 5',
            'breakpoint set -f "math source.void" -l 2',
            "run", "next",
        ]
        if mode == "full":
            options.append("frame variable initial assigned")
        options += ["continue", "next"]
        if mode == "full":
            options.append("frame variable input doubled")
        options += ["next", "continue"]
        commands.write_text("\n".join(options) + "\n", encoding="ascii")
        result = run(lldb, "--batch", "--no-lldbinit", "-s", commands, executable)
        assert result.stdout.count("stop reason = breakpoint") == 2, result.stdout
        for source in ("main.void:5", "main.void:6", "math source.void:2", "math source.void:3"):
            assert source in result.stdout, result.stdout
        if mode == "full":
            for name, value in (("initial", 7), ("assigned", 8), ("input", 8), ("doubled", 16)):
                assert re.search(rf"\b{name} = {value}\b", result.stdout), result.stdout
        assert "exited with status = 19" in result.stdout, result.stdout
    else:
        options = [
            f"break '{(root / 'main.void').as_posix()}':5",
            f"break '{(root / 'math source.void').as_posix()}':2",
            "run", "next",
        ]
        if mode == "full":
            options.extend(("print initial", "print assigned"))
        options += ["continue", "next"]
        if mode == "full":
            options.extend(("print input", "print doubled"))
        options += ["next", "continue"]
        commands.write_text("\n".join(options) + "\n", encoding="utf-8")
        result = run(gdb, "--batch", "-nx", "-x", commands, executable)
        assert re.search(r"Breakpoint 1, .*main\.void:5", result.stdout), result.stdout
        assert re.search(r"Breakpoint 2, .*math source\.void:2", result.stdout), result.stdout
        assert re.search(r"^6\s+self\.value = assigned", result.stdout, re.M), result.stdout
        assert re.search(r"^3\s+var result = doubled", result.stdout, re.M), result.stdout
        if mode == "full":
            assert re.findall(r"^\$\d+ = (\d+)$", result.stdout, re.M) == ["7", "8", "8", "16"], result.stdout
        assert "exited with code 023" in result.stdout, result.stdout
    print(f"{mode}: native debugger hit both source breakpoints, stepped and observed exit 19")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("compiler", type=Path)
    parser.add_argument("dwarfdump", type=Path)
    debugger = parser.add_mutually_exclusive_group()
    debugger.add_argument("--lldb", type=Path)
    debugger.add_argument("--gdb", type=Path)
    args = parser.parse_args()
    for tool in (args.compiler, args.dwarfdump, args.lldb, args.gdb):
        if tool is not None and (not tool.is_absolute() or not tool.is_file()):
            parser.error(f"Expected an existing absolute tool path: {tool}")
    if args.lldb is not None and "lldb version 23.1.4" not in run(args.lldb, "--version").stdout:
        parser.error("LLDB must be from the pinned 23.1.4 SDK")
    with tempfile.TemporaryDirectory(prefix="VOID native debug ") as temporary:
        root = Path(temporary)
        manifest = root / "void.project.toml"
        (root / "main.void").write_text(ENTRY, encoding="utf-8")
        (root / "math source.void").write_text(HELPER, encoding="utf-8")
        for profile, mode in (("debug", "none"), ("debug", "line"), ("debug", "full"),
                              ("release", "line"), ("release", "full")):
            manifest.write_text(MANIFEST + f"[build.{profile}]\ndebug_info='{mode}'\n", encoding="utf-8")
            selection = ("--project", root, "--profile", profile)
            obj = root / f"{profile}-{mode}.o"
            executable = root / (f"{profile}-{mode}" + (".exe" if os.name == "nt" else ""))
            for command, output in (("emit", obj), ("build", executable)):
                result = run(args.compiler, command, *selection, "-o", output)
                assert "VOID5012" not in result.stderr, result.stderr
                lines = run(args.dwarfdump, "--debug-line", output).stdout
                for file in ("math source.void", "main.void"):
                    assert (file in lines) == (mode != "none"), lines
                for name, tag in (("doubled", "DW_TAG_variable"), ("initial", "DW_TAG_formal_parameter")):
                    names = run(args.dwarfdump, f"--name={name}", output).stdout
                    assert (tag in names) == (profile == "debug" and mode == "full"), names
            run(executable, status=19)
            if mode != "none":
                run(args.dwarfdump, "--verify", obj)
            if mode == "full" and profile == "debug":
                repeated = root / "repeated output.o"
                run(args.compiler, "emit", *selection, "-o", repeated)
                assert obj.read_bytes() == repeated.read_bytes(), "Identical source and profile must emit identical objects"
            if os.name == "nt":
                assert not list(root.glob("*.pdb")), "Windows DWARF publication must stay single-file"
            if profile == "debug" and mode in ("line", "full") and (args.lldb or args.gdb):
                verify_debugger(root, executable, args.lldb, args.gdb, mode)
            print(f"{profile} {mode}: emitted object and native executable verified")
        manifest.write_text(MANIFEST + "[build.debug]\ndebug_info='other'\n", encoding="utf-8")
        assert "Unsupported debug information setting" in run(
            args.compiler, "check", "--project", root, "--profile", "debug", status=1).stderr


if __name__ == "__main__":
    main()
