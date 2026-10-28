"""Existing visibility, readonly, worker and explicit-default contracts remain enforced."""
from pathlib import Path
import sys,tempfile
import support as h
compiler=Path(sys.argv[1]).resolve();checks=0
cases=[
('use log as l\nfunc main():\n    var instance = l.logger(l.config())\n','private'),
('use log as l\nuse convert\nfunc main():\n    var instance = convert.unwrap(l.open())\n    var hidden = instance._options\n','accessible'),
('use log as l\nuse convert\nfunc main():\n    const instance = convert.unwrap(l.open())\n    instance.info("readonly")\n','readonly'),
('use log as l\nfunc worker() -> i32:\n    l.info("worker")\n    return 1\nasync func entry(n: i32) -> i32:\n    return await spawn worker()\n','worker'),
('use log as l\nfunc worker(instance: l.logger) -> i32:\n    return 1\nasync func entry(n: i32) -> i32:\n    return await spawn worker(l.open())\n',''),
('use log as l\nasync func escaped(builder: func() -> string) -> i32:\n    return 1\n','callable'),
('use log as l\nfunc inspect(problem: l.failure):\n    problem.file_bytes = 1\n','const'),
('use log as l\nfunc inspect(problem: l.failure):\n    if problem.file_cause is Ok(var cause):\n        cause.bytes_transferred = 0\n','readonly'),
('enum choice:\n    first\nvalue struct missing:\n    selected: choice\nfunc main():\n    var value = missing()\n','explicit'),
('enum choice:\n    first\nstruct missing:\n    selected: choice\nfunc main():\n    var value = missing()\n','explicit'),
]
with tempfile.TemporaryDirectory(prefix='VOID-log-frontend-') as d:
    work=Path(d);source=work/'main.void'
    for text,needle in cases:
        source.write_text(text)
        result=h.run([compiler,'check',source],status=1,small=True)
        assert needle in result.stderr.decode().lower(),(text,result.stderr)
        checks+=1
    for name in ('defaults','semantics','async','native','publication','cancel'):
        h.run([compiler,'check',Path(__file__).with_name(name+'.void').resolve()],small=True);checks+=1
print(f'Logging frontend: {checks} visibility/readonly/worker/default/surface checks')
