"""Execute already-built component probes sequentially, preserving all raw samples."""
from pathlib import Path
import json, os, sys, tempfile
import support as h
out=Path(sys.argv[1]).resolve()
env=dict(os.environ,VOID_STDLIB_ROOT=str(out/'stdlib'))
for name in sys.argv[2:] or ['void_log_host-timing','void_log_probe_optimized-instrumented']:
    exe=out/name
    mode='instrumented' if name.endswith('-instrumented') else 'timing'
    with tempfile.TemporaryDirectory(prefix='VOID-log-component-') as directory:
        result=h.run([exe,out/'performance.void',directory,'optimized',mode],small=True,
                     timeout=3600,env=env,sources=[out],context={'mode':'isolated-framing-components'})
    (out/(name+'.stdout')).write_bytes(result.stdout)
    rows=[json.loads(line[5:]) for line in result.stdout.decode().splitlines() if line.startswith('PERF ')]
    (out/(name+'.json')).write_text(json.dumps(rows,indent=2)+'\n')
    print(name,len(rows),'cold/warm observations',flush=True)
