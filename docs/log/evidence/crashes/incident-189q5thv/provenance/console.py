"""Actual redirected endpoints, default live UTC, and a native closed pipe peer."""
from pathlib import Path
from datetime import datetime,timezone
import os,re,sys,tempfile,subprocess
import support as h
compiler=Path(sys.argv[1]).resolve();checks=0
with tempfile.TemporaryDirectory(prefix='VOID-log-console-') as d:
    work=Path(d);source=work/'main.void'
    source.write_text('''use log
use convert
func main():
    convert.unwrap(log.info("live UTC"))
    convert.unwrap(log.shutdown())
''')
    for flags in (['--no-opt'],[]):
        before=datetime.now(timezone.utc);r=h.run([compiler,'run',source,'--no-cache',*flags],expected=b'',small=True);after=datetime.now(timezone.utc)
        match=re.fullmatch(rb'(\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3}Z) INFO "" "live UTC"\n',r.stderr)
        assert match,r.stderr
        sample=datetime.fromisoformat(match[1].decode().replace('Z','+00:00'))
        assert before.timestamp()-0.001<=sample.timestamp()<=after.timestamp(),(before,sample,after)
        checks+=2
    source.write_text('''use log
use console
use convert
func main() -> i32:
    var instance = convert.unwrap(log.open(log.config(timestamps = false,
        console = log.console_sink.stdout, file_path = "broken.log")))
    if instance.info("complete file") is Err(var problem):
        if not (problem.kind is log.failure_kind.console_io):
            return 1
        if not problem.console_attempted or problem.console_complete or not problem.file_complete:
            return 2
        if problem.console_cause is Ok(var cause):
            if not (cause.kind is console.io_error_kind.broken_pipe):
                return 3
        else:
            return 4
    else:
        return 5
    if instance.info("no replay") is Err(var problem):
        if not (problem.kind is log.failure_kind.faulted):
            return 6
    else:
        return 7
    convert.unwrap(instance.close())
    if console.stdout.write("borrowed endpoint still open") is Err(var cause):
        if cause.kind is console.io_error_kind.broken_pipe:
            return 0
    return 8
''')
    for optimized in (False,True):
        flags=[] if optimized else ['--no-opt'];obj=work/'console.o';exe=work/'console.app'
        h.run([compiler,'emit',source,'-o',obj,*flags],small=True);h.link(compiler,obj,exe,sources=[source]);checks+=2
        for mode in ('jit','aot'):
            cwd=work/(mode+str(optimized));cwd.mkdir()
            reader,writer=os.pipe();os.close(reader)
            try:
                command=[compiler,'run',source,'--no-cache',*flags] if mode=='jit' else [exe]
                r=h.run(command,cwd=cwd,small=True,capture_output=False,stdout=writer,stderr=subprocess.PIPE,sources=[source])
                assert (cwd/'broken.log').read_bytes()==b'INFO "" "complete file"\n'
                checks+=2
            finally:os.close(writer)
print(f'Logging actual console: {checks} redirected/live-UTC/closed-peer/file-independence observations')
