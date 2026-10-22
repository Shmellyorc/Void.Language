"""Source time factories, shared hooks, zones, clocks and independently linked AOT."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
compiler, clang, linker, runtime, fixture, sanitizer = sys.argv[1:]
flags = ["--driver-mode=g++", "-pthread"]
if linker: flags.append("--ld-path=" + linker)
if sanitizer == "ON": flags.append("-fsanitize=address,undefined")
if os.name == "nt": flags.extend(["-fms-runtime-lib=dll", "-llegacy_stdio_definitions"])
checks = 0
def execute(args, success=True):
    global checks
    result = subprocess.run([str(a) for a in args], capture_output=True, text=True, encoding="utf-8", timeout=60)
    assert (result.returncode == 0) == success, (args, result.returncode, result.stdout, result.stderr)
    checks += 1
    return result.stdout.strip(), result.stderr
with tempfile.TemporaryDirectory(prefix="VOID Stage10 time ") as directory:
    root=Path(directory)
    source=root/"main.void"
    source.write_text(Path(__file__).with_name("time.void").read_text(encoding="utf-8"), encoding="utf-8")
    execute([compiler,"check",source])
    for optimized in (False,True):
        options=[] if optimized else ["--no-opt"]
        for value in (0,4,-3): assert execute([compiler,"run",source,value,*options])[0]==str(value)
        obj=root/("optimized.o" if optimized else "normal.o")
        exe=obj.with_suffix(".exe" if os.name=="nt" else ".bin")
        execute([compiler,"emit",source,"-o",obj,*options])
        execute([clang,*flags,obj,runtime,fixture,"-o",exe])
        for value in (0,4,-3): assert execute([exe,value])[0]==str(value)
    for body in ('var value = date(2024, 1, 1)', 'var value: date', 'var value = "{make_date(2024, 1, 1):yyyy}"',
                 'if make_date(2024, 1, 1) is Ok(var value):\n        var invalid = "{value:HH}"',
                 'if make_date(2024, 1, 1) is Ok(var value):\n        value.year = 0',
                 'if monotonic_now() is Ok(var value):\n        var raw = value.context'):
        source.write_text('use time\nfunc entry(input: i32) -> i32:\n    '+body+'\n    return input\n',encoding="utf-8")
        execute([compiler,"check",source],False)
print(f"{checks} source temporal JIT/AOT and rejection checks passed")
