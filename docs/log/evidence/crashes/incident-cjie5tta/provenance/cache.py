"""Miss/hit/bypass preserve code only; each Runtime initializes its own default."""
from pathlib import Path
import sys,tempfile
import support as h
compiler=Path(sys.argv[1]).resolve();checks=0
with tempfile.TemporaryDirectory(prefix='VOID-log-cache-') as d:
    work=Path(d);source=work/'main.void'
    source.write_text('''use log
use log as alias
use convert
func main():
    convert.unwrap(log.configure(log.config(timestamps = false, console = log.console_sink.stdout)))
    convert.unwrap(alias.info("fresh"))
    convert.unwrap(log.shutdown())
''')
    cache=work/'.void/cache/v1'
    for flags in (['--no-opt'],[]):
        before=set(cache.iterdir()) if cache.exists() else set()
        result=h.run([compiler,'run',source,*flags],expected=b'INFO "" "fresh"\n',small=True);checks+=1
        try:
            artifacts=set(cache.iterdir());assert len(artifacts-before)==1,'miss did not publish code'
            frozen={p:(p.stat().st_mtime_ns,p.read_bytes()) for p in artifacts}
            h.run([compiler,'run',source,*flags],expected=b'INFO "" "fresh"\n',small=True);checks+=1
            assert all((p.stat().st_mtime_ns,p.read_bytes())==value for p,value in frozen.items()),'hit republished cache'
            h.run([compiler,'run',source,'--no-cache',*flags],expected=b'INFO "" "fresh"\n',small=True);checks+=1
            assert set(cache.iterdir())==artifacts,'bypass published cache'
            assert not (work/'.void/logs').exists(),'default logger implicitly created a log directory'
        except AssertionError as error:
            h.crash.preserve(result.args,h.ROOT,result,source_inputs=[work],provenance_inputs=[Path(__file__)],failure_kind='test_assertion',evidence_context={'cache_assertion':str(error)})
            raise
print(f'Logging cache: {checks} miss/hit/bypass executions with fresh Runtime defaults')
