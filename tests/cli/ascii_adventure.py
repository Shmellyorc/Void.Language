"""Check the executable Adventure through its explicit line-input path."""

import os
from pathlib import Path
import re
import subprocess
import sys
from tempfile import TemporaryDirectory

compiler, manifest = (Path(argument).resolve() for argument in sys.argv[1:])
source = Path(__file__).resolve().parents[2] / "examples/ascii_adventure/main.void"
version, clang, linker, *libraries = manifest.read_text(encoding="utf-8").splitlines()
assert version == "VOID native 23.1.4 v2"
windows = os.name == "nt"
if windows:
    sdk_linker = Path(clang).with_name("lld-link.exe").resolve()
    flags = ["--driver-mode=g++", "-B" + str(sdk_linker.parent), "-fuse-ld=lld"]
else:
    sdk_linker = Path(linker)
    flags = ["--driver-mode=g++", "--ld-path=" + linker]

MAP = (
    "############",
    "#..........#",
    "#..###.....#",
    "#..........#",
    "#.....#....#",
    "#..$..#....#",
    "#..........#",
    "#....###...#",
    "#..........#",
    "#.....$....#",
    "#.........X#",
    "############",
)
BANNER = "=== VOID ASCII ADVENTURE ==="


def run(command, work, data=b"", status=0, diagnostic=b""):
    args = list(map(str, command))
    result = subprocess.run(args, cwd=work, input=data, capture_output=True, timeout=180)
    assert result.returncode == status, (args, result)
    if diagnostic is not None:
        assert result.stderr == diagnostic, (args, result)
    return result


def link_image(work, options):
    obj = work / ("adventure.obj" if windows else "adventure.o")
    image = work / ("adventure.exe" if windows else "adventure.bin")
    run([compiler, "emit", source, "-o", obj, *options], work)
    command = [clang, *flags, obj, *libraries, "-o", image]
    if windows:
        plan = run([*command, "-###"], work, diagnostic=None).stderr.decode("utf-8")
        launch = re.search(r'^\s*"([^"]*lld-link(?:\.exe)?)"', plan, re.M | re.I)
        assert launch and Path(launch[1].replace("\\\\", "\\")).with_suffix(".exe").resolve() == sdk_linker, plan
    run(command, work)
    return [image]


def board(x, y, first=False, second=False):
    rows = [list(row) for row in MAP]
    if first:
        rows[5][3] = "."
    if second:
        rows[9][6] = "."
    rows[y][x] = "@"
    return tuple("".join(row) for row in rows)


def frames(output):
    assert b"\x1b" not in output
    lines = output.decode("utf-8").splitlines()
    snapshots = [(tuple(lines[i + 1:i + 13]), lines[i + 13])
                 for i, line in enumerate(lines) if line == BANNER]
    assert all(len(rows) == 12 and all(len(row) == 12 for row in rows) for rows, _ in snapshots)
    return snapshots


def verify(command, work):
    line = [*command, "--line"]
    locked = run(line, work, b"\n?\nw\n" + b"d\n" * 9 + b"s\n" * 9 + b"Q\n").stdout
    states = frames(locked)
    start = (board(1, 1), "Treasure: 0/2   Turns: 0")
    assert states[:4] == [start] * 4
    assert len(states) == 22
    assert states[-2:] == [(board(10, 9), "Treasure: 0/2   Turns: 17")] * 2
    for expected in (b"Unknown command.", b"A wall blocks your path!", b"The exit is locked!", b"Goodbye!"):
        assert expected in locked, locked
    assert b"VICTORY!" not in locked

    victory = run(line, work, b"S\n" * 4 + b"d\n" * 2 + b"a\nd\n" +
                  b"s\n" * 4 + b"d\n" * 7 + b"s\nq\n").stdout
    states = frames(victory)
    assert len(states) == 21
    assert states[6] == (board(3, 5, first=True), "Treasure: 1/2   Turns: 6")
    assert states[7] == (board(2, 5, first=True), "Treasure: 1/2   Turns: 7")
    assert states[8] == (board(3, 5, first=True), "Treasure: 1/2   Turns: 8")
    assert states[15] == (board(6, 9, first=True, second=True), "Treasure: 2/2   Turns: 15")
    assert states[-1] == (board(10, 10, first=True, second=True), "Treasure: 2/2   Turns: 20")
    for collected in (b"You collected the first treasure!", b"You collected the second treasure!"):
        assert victory.count(collected) == 1
    assert victory.endswith(b"VICTORY! You escaped the dungeon in 20 moves!\n"), victory

    eof = run(line, work).stdout
    assert frames(eof) == [start] and eof.endswith(b"Input closed. Goodbye!\n")
    invalid = run(line, work, b"\xff\n", status=2, diagnostic=None)
    assert frames(invalid.stdout) == [start] and b"Console input failed:" in invalid.stderr
    refused = run(command, work, b"q\n", status=2,
                  diagnostic=b"Keyboard input needs an interactive terminal. Use --line for line input.\n")
    assert not refused.stdout
    usage = run([*command, "--bad"], work, status=2, diagnostic=b"Usage: ascii-adventure [--line]\n")
    assert not usage.stdout


with TemporaryDirectory(prefix="VOID ASCII Adventure ") as directory:
    work = Path(directory)
    assert b"LLVM 23.1.4\n" in run([compiler, "--version"], work).stdout.replace(b"\r\n", b"\n")
    assert run([clang, "--version"], work).stdout.startswith(b"clang version 23.1.4")
    assert run([sdk_linker, "--version"], work).stdout.startswith(b"LLD 23.1.4")
    run([compiler, "check", source], work)
    for mode, options in (("O0", ["--no-opt"]), ("O2", [])):
        lane = work / mode
        lane.mkdir()
        verify([compiler, "run", source, "--no-cache", *options, "--"], lane)
        print(f"{mode} JIT gameplay, EOF, error and redirection passed", flush=True)
        verify(link_image(lane, options), lane)
        print(f"{mode} independent AOT gameplay, EOF, error and redirection passed", flush=True)
