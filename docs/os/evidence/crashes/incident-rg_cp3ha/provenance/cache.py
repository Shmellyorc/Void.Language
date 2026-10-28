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
var saved = os.env("VOID_OS_VALUE")
func main():
    if saved is Ok(var value):
        print(value)
    if os.env("VOID_OS_EXPECT_PID") is Ok(var expected):
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
print(f'OS cache: {h.checks} process checks passed')
