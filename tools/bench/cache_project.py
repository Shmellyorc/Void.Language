"""Real X11 TestGame runs, normal Escape exit, inclusive CLI timing receipts.

Explicitly supplied project is run with native authority for each invocation.
--edit-cycle briefly appends a comment and restores the exact original bytes.
No compiler installation, provider mutation, approval-store write or cache purge.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("compiler", type=Path)
parser.add_argument("project", type=Path)
parser.add_argument("evidence", type=Path)
parser.add_argument("--label", default="candidate")
parser.add_argument("--runs", type=int, default=4)
parser.add_argument("--edit-cycle", action="store_true")
parser.add_argument("--debug", action="store_true")
parser.add_argument("--approval-mode", choices=("invocation", "reuse"), default="invocation")
args = parser.parse_args()
compiler, project, evidence = (p.resolve() for p in (args.compiler, args.project, args.evidence))
evidence.mkdir(parents=True, exist_ok=True)

def small_stack():
    resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))

def run(label, extra=()):
    prefix = evidence / f"{args.label}-{label}"
    report = prefix.with_suffix(".json")
    authority = "--allow-native" if args.approval_mode == "invocation" else "--reuse-native-approval"
    command = [str(compiler), "run", ".", authority, "--timings", "--timings-format", "json",
               "--timings-output", str(report), *(["--profile", "debug"] if args.debug else []), *extra]
    start = time.monotonic()
    with prefix.with_suffix(".stdout").open("xb") as stdout, prefix.with_suffix(".stderr").open("xb") as stderr:
        child = subprocess.Popen(command, cwd=project, stdout=stdout, stderr=stderr, preexec_fn=small_stack)
        sent = False
        try:
            while child.poll() is None and time.monotonic() - start < 90:
                # Use this child's window only; allow initialization, then send
                # Escape through ordinary event handling. Verify assets at exit.
                if not sent:
                    windows = subprocess.run(["xdotool", "search", "--pid", str(child.pid), "--name", "VOID Image Test"],
                                             capture_output=True, text=True)
                    if windows.returncode == 0:
                        time.sleep(0.35)
                        result = subprocess.run(["xdotool", "keydown", "--window", windows.stdout.splitlines()[-1], "Escape"],
                                                capture_output=True)
                        assert result.returncode == 0, result.stderr
                        sent = True
                time.sleep(0.02)
            assert sent and child.wait(timeout=2) == 0, command
        finally:
            if child.poll() is None:
                child.kill(); child.wait()
    output = prefix.with_suffix(".stdout").read_text()
    assert ("Window closed successfully" in output and "Unloaded texture data" in output
            and "[assets/logo.png] File loaded successfully" in output), output
    result = json.loads(report.read_text())
    assert result["application"]["exit_code"] == 0
    result["receipt"] = {"argv": command, "stack_bytes": 1048576,
                         "process_wall_us": round((time.monotonic() - start) * 1e6)}
    return result

source = project / "app.void"
original = source.read_bytes()
initial_stat = source.stat()
edited = original + b"\n## G95 cache invalidation probe\n"
rows = []
try:
    for number in range(args.runs):
        rows.append(run(f"run{number}"))
    if args.edit_cycle:
        source.write_bytes(edited)
        rows.append(run("edited"))
        assert source.read_bytes() == edited, "project was edited concurrently; preserve the new source"
        source.write_bytes(original)
        rows.append(run("restored"))
    before = {p.name: (p.stat().st_mtime_ns, hashlib.sha256(p.read_bytes()).hexdigest())
              for p in (project / ".void/cache/v1").iterdir() if p.is_file()}
    rows.append(run("bypass", ["--no-cache"]))
    after = {p.name: (p.stat().st_mtime_ns, hashlib.sha256(p.read_bytes()).hexdigest())
             for p in (project / ".void/cache/v1").iterdir() if p.is_file()}
    assert before == after, "--no-cache changed persistent objects"
finally:
    if source.read_bytes() == edited:
        source.write_bytes(original)
    assert source.read_bytes() == original, "project was edited concurrently; preserve the new source"
    if args.edit_cycle:
        os.utime(source, ns=(initial_stat.st_atime_ns, initial_stat.st_mtime_ns))
assert source.read_bytes() == original
summary = {"project": str(project), "compiler": str(compiler),
           "compiler_sha256": hashlib.sha256(compiler.read_bytes()).hexdigest(),
           "source_sha256_before_after": hashlib.sha256(original).hexdigest(),
           "rows": rows}
with (evidence / f"{args.label}-summary.json").open("x") as output:
    json.dump(summary, output, indent=2)
for row in rows:
    print(row["receipt"]["argv"], row["cache"], row["build"], row["application"])
