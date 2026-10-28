"""Repeat the original workload into a fresh evidence file, never replace history."""
from pathlib import Path
import json, sys, tempfile
import support as h
output=Path(sys.argv[1]).resolve()
assert not output.exists(), 'Keep the earlier observations; choose a fresh result path'
output.parent.mkdir(parents=True,exist_ok=True)
rows=[]
for arg in sys.argv[2:]:
    exe=Path(arg).resolve()
    with tempfile.TemporaryDirectory(prefix='VOID-log-original-') as directory:
        result=h.run([exe,h.ROOT,directory,'--benchmark'],small=True,timeout=1800,
                     context={'mode':'isolated-original-workload','result':str(output)})
    for line in result.stdout.decode().splitlines():
        if line.startswith('BENCH '):
            row=json.loads(line[6:])
            row.update(executable=str(exe),executable_sha256=h.crash.digest(exe),
                       logger_sha256=h.crash.digest(h.ROOT/'stdlib/log.void'),gc_allocation_budget=256)
            rows.append(row)
    output.write_text(json.dumps(rows,indent=2)+'\n')
    print(exe.name,len(rows),'cumulative samples',flush=True)
