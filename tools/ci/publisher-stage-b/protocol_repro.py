import json, pathlib, runpy, subprocess, sys
root=pathlib.Path(sys.argv[1]).resolve();server=sys.argv[2];evidence=pathlib.Path(sys.argv[3]);evidence.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(root/'tools'))
from lsp_client import Client
original=Client.__exit__; records=[]
def retain(self, kind, value, tb):
    status=self.process.poll()
    if isinstance(value, EOFError) and status is None:
        try:status=self.process.wait(timeout=2)
        except subprocess.TimeoutExpired:pass
    if status is not None:self.logger.join(timeout=5)
    record=dict(argv=self.process.args,error_type=kind.__name__ if kind else None,error=str(value) if value else None,
                status_before_cleanup=status,stdin_closed=self.process.stdin.closed,stdout_closed=self.process.stdout.closed)
    try:return original(self,kind,value,tb)
    finally:
        record.update(status_after_cleanup=self.process.returncode,stderr=self.logs.decode('utf-8',errors='replace'))
        records.append(record);(evidence/'pipe-processes.json').write_text(json.dumps(records,indent=2)+'\n')
Client.__exit__=retain
sys.argv=[str(root/'tests/project/protocol.py'),server,str(evidence/'project-protocol-evidence.json')]
runpy.run_path(sys.argv[0],run_name='__main__')
