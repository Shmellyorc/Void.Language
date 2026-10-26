"""Manual linked-O2 write benchmark; timed executions exclude compilation.

Usage: python3 tests/memory/benchmark.py /absolute/voidc /absolute/results.json
"""
from pathlib import Path
import json
import statistics
import subprocess
import sys
import tempfile
import time

compiler = Path(sys.argv[1]).resolve()
output = Path(sys.argv[2]).resolve()
records = []

with tempfile.TemporaryDirectory(prefix="VOID memory benchmark ") as directory:
    root = Path(directory)
    for operation in ("append", "overwrite"):
        for count in (20000, 40000, 80000):
            setup = ""
            if operation == "overwrite":
                setup = f"""    var seed: [u8] = []
    for index in 0..<{count}:
        seed.push(0)
    buffer = memory.stream(seed)
"""
            source = root / f"{operation}-{count}.void"
            source.write_text("""use io/memory

func main() -> i32:
    var buffer = memory.stream()
""" + setup + f"""    var byte: [u8] = [42]
    for index in 0..<{count}:
        if buffer.write_bytes(byte) is Err(var reason):
            return 1
    if buffer.position != {count} or buffer.length != {count}:
        return 2
    return 0
""")
            executable = source.with_suffix("")
            subprocess.run([str(compiler), "build", str(source), "-o", str(executable)],
                           check=True, capture_output=True)
            samples = []
            for trial in range(3):
                started = time.perf_counter()
                subprocess.run([str(executable)], check=True, capture_output=True)
                samples.append(time.perf_counter() - started)
            record = {
                "operation": operation,
                "count": count,
                "seconds": samples,
                "median_seconds": statistics.median(samples),
            }
            records.append(record)
            print(record, flush=True)

output.write_text(json.dumps(records, indent=2) + "\n")
