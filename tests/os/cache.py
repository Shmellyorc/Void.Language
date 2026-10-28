"""Fresh processes reuse code and query their own environment/PID; bypass publishes nothing."""
from pathlib import Path
import os
import sys
import tempfile
import support as h
compiler=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='VOID OS cache ') as directory:
    work=Path(directory);source=work/'main.void'
    source.write_text('''use os
use console
var saved = os.env("VOID_OS_VALUE")
func main():
    if saved is Ok(var value):
        print(value)
    if os.env("VOID_OS_EXPECT_PID") is Ok(var expected):
        if expected == "__VOID_CAPTURE_PID__":
            print("__VOID_CAPTURE_PID__", os.process_id())
            var released = console.readline()
        else:
            print("{os.process_id()}" == expected)
''')
    base=dict(os.environ)
    for flags in (['--cache'],[]):
        for index in range(2):
            value='first' if index==0 else 'second'
            r=h.run([sys.executable,h.ROOT/'tests/os/exec_host.py',compiler,'run',source,*flags],work,(value+'\ntrue\n').encode(),env=dict(base,VOID_OS_VALUE=value))
            if flags and sys.platform!='win32':assert ('Debug cache: '+('miss' if index==0 else 'hit')).encode() in r.stderr,r.stderr
        before={p.relative_to(work):p.read_bytes() for p in (work/'.void').rglob('*') if p.is_file()}
        h.run([sys.executable,h.ROOT/'tests/os/exec_host.py',compiler,'run',source,'--no-cache',*(['--no-opt'] if flags else [])],work,b'bypass\ntrue\n',env=dict(base,VOID_OS_VALUE='bypass'))
        after={p.relative_to(work):p.read_bytes() for p in (work/'.void').rglob('*') if p.is_file()}
        assert before==after,'bypass changed persistent cache'
    # Two processes hold distinct native PIDs at a test-owned gate, then run the same cached code.
    from concurrent.futures import ThreadPoolExecutor
    import time
    for mode,flags in enumerate((['--cache'],[])):
        gate=work/f'gate{mode}';markers=[work/f'pid{mode}-{n}' for n in range(2)]
        def child(index):
            value='concurrent'+str(index)
            env=dict(base,VOID_OS_VALUE=value,VOID_OS_PID_GATE=str(gate),VOID_OS_PID_MARKER=str(markers[index]))
            return h.run([sys.executable,h.ROOT/'tests/os/exec_host.py',compiler,'run',source,*flags],work,(value+'\ntrue\n').encode(),env=env)
        with ThreadPoolExecutor(max_workers=2) as pool:
            futures=[pool.submit(child,index) for index in range(2)]
            deadline=time.monotonic()+30
            while not all(marker.exists() for marker in markers):
                assert time.monotonic()<deadline,'simultaneous PID markers unavailable'
                time.sleep(0.01)
            assert len({marker.read_text() for marker in markers})==2,'live child PIDs collided'
            gate.touch()
            for future in futures:future.result()
print(f'OS cache/fresh and simultaneously live process identity: {h.checks} process checks passed')
