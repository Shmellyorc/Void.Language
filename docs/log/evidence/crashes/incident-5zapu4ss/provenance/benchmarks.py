"""Three bounded samples; source execution is timed after compilation/admission."""
from pathlib import Path
import sys,tempfile,json,statistics
import support as h
records=[]
for arg in sys.argv[1:]:
    exe=Path(arg).resolve()
    with tempfile.TemporaryDirectory(prefix='VOID-log-bench-') as d:
        result=h.run([exe,h.ROOT,d,'--benchmark'],sources=[h.ROOT/'tests/log',Path(d)],small=True,timeout=1800,context={'mode':'bounded-benchmark'})
        for line in result.stdout.decode().splitlines():
            if line.startswith('BENCH '):
                row=json.loads(line[6:]);row.update(executable=str(exe),executable_sha256=h.crash.digest(exe))
                records.append(row)
    print('Benchmark completed:',exe.name)
target=h.EVIDENCE/'benchmarks.json'
previous=json.loads(target.read_text()) if target.exists() else []
identities={r['executable'] for r in records}
target.write_text(json.dumps([r for r in previous if r['executable'] not in identities]+records,indent=2)+'\n')
print(f'{len(records)} benchmark samples recorded')
